#include "models.hpp"
#include "ElpMpp02.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <map>
#include <functional>

namespace celnav {
namespace {
uint32_t integer(std::istream& in,int bytes){uint32_t v=0;for(int i=0;i<bytes;i++){int c=in.get();if(c<0)throw std::runtime_error("Truncated lunar pack");v|=uint32_t(c)<<(8*i);}return v;}
double number(std::istream& in){uint64_t bits=0;for(int i=0;i<8;i++)bits|=uint64_t(integer(in,1))<<(8*i);double v;std::memcpy(&v,&bits,8);if(!std::isfinite(v))throw std::runtime_error("Nonfinite lunar coefficient");return v;}
struct Group {std::vector<std::array<int,13>> rows;std::vector<int*> pointers;std::vector<double> amplitude,phase;std::vector<unsigned> harmonic;std::vector<std::array<double,2>> phaseUnit;};
}
// A topologically ordered graph of exact integer harmonics. Fixed phases are
// prepared once; shared argument products are evaluated once per lunar epoch.
struct Harmonic { unsigned left, right; bool conjugate; };
struct MoonModel::Impl {
  Elp_paras parameters{};Elp_coefs coefficients{};std::array<Group,14> groups;
  bool reuse;std::vector<Harmonic> harmonics;
  void Prepare() {
    using Key=std::array<int,13>;
    std::map<Key,unsigned> indices;
    harmonics.resize(14);indices[Key{}]=0;
    for(unsigned i=0;i<13;++i){Key k{};k[i]=1;indices[k]=i+1;}
    std::function<unsigned(Key)> node=[&](Key key)->unsigned {
      auto found=indices.find(key);if(found!=indices.end())return found->second;
      int last=12;while(key[last]==0)--last;
      Key prefix=key;prefix[last]=0;
      unsigned left,right;bool conjugate=false;
      if(prefix!=Key{}) {
        Key part{};part[last]=key[last];left=node(prefix);right=node(part);
      } else if(key[last]<0) {
        Key positive=key;positive[last]=-positive[last];left=node(positive);right=0;conjugate=true;
      } else {
        Key half=key;half[last]/=2;left=node(half);
        Key remainder=key;remainder[last]-=half[last];right=node(remainder);
      }
      const unsigned result=harmonics.size();harmonics.push_back({left,right,conjugate});
      indices[key]=result;return result;
    };
    for(unsigned g=3;g<14;++g) {
      auto& group=groups[g];group.harmonic.reserve(group.rows.size());group.phaseUnit.reserve(group.rows.size());
      for(unsigned i=0;i<group.rows.size();++i) {
        group.harmonic.push_back(node(group.rows[i]));
        group.phaseUnit.push_back({std::cos(group.phase[i]),std::sin(group.phase[i])});
      }
    }
  }
  Vec Evaluate(double T) {
    if(!reuse){Vec result{};getX2000(T,parameters,coefficients,result[0],result[1],result[2]);return result;}
    Elp_args args;compute_Elp_arguments(T,parameters,args);
    const double angles[]={args.D,args.F,args.L,args.Lp,args.Me,args.Ve,args.EM,args.Ma,args.Ju,args.Sa,args.Ur,args.Ne,args.zeta};
    std::vector<std::array<double,2>> values(harmonics.size());values[0]={1,0};
    for(unsigned i=0;i<13;++i)values[i+1]={std::cos(angles[i]),std::sin(angles[i])};
    for(unsigned i=14;i<harmonics.size();++i) {
      const auto& n=harmonics[i];const auto& a=values[n.left];const auto& b=values[n.right];
      values[i]=n.conjugate?std::array<double,2>{a[0],-a[1]}:
          std::array<double,2>{a[0]*b[0]-a[1]*b[1],a[1]*b[0]+a[0]*b[1]};
    }
    double pert[14]{};
    for(unsigned g=3;g<14;++g) {
      const auto& group=groups[g];
      for(unsigned i=0;i<group.rows.size();++i) {
        const auto& h=values[group.harmonic[i]];const auto& phase=group.phaseUnit[i];
        pert[g]+=group.amplitude[i]*(h[1]*phase[0]+h[0]*phase[1]);
      }
    }
    auto& c=coefficients;
    const double main_long=Elp_main_sum(c.n_main_long,c.i_main_long,c.A_main_long,args,0);
    const double main_lat=Elp_main_sum(c.n_main_lat,c.i_main_lat,c.A_main_lat,args,0);
    const double main_dist=Elp_main_sum(c.n_main_dist,c.i_main_dist,c.A_main_dist,args,1);
    // Same ELP/MPP02 position and precession expressions as getX2000 in
    // third_party/elp/ElpMpp02.cpp (GPL-3.0). Only harmonic evaluation differs.
    const double T2=T*T,T3=T*T2,T4=T2*T2,T5=T2*T3;
    const double longM=args.W1+main_long+pert[3]+mod2pi(pert[4]*T)+mod2pi(pert[5]*T2)+mod2pi(pert[6]*T3);
    const double latM=main_lat+pert[7]+mod2pi(pert[8]*T)+mod2pi(pert[9]*T2);
    const double r=(384747.961370173/384747.980674318)*(main_dist+pert[10]+pert[11]*T+pert[12]*T2+pert[13]*T3);
    const double x0=r*cos(longM)*cos(latM),y0=r*sin(longM)*cos(latM),z0=r*sin(latM);
    const double P=0.10180391e-4*T+0.47020439e-6*T2-0.5417367e-9*T3-0.2507948e-11*T4+0.463486e-14*T5;
    const double Q=-0.113469002e-3*T+0.12372674e-6*T2+0.12654170e-8*T3-0.1371808e-11*T4-0.320334e-14*T5;
    const double sq=sqrt(1-P*P-Q*Q);
    return {(1-2*P*P)*x0+(2*P*Q)*y0+(2*P*sq)*z0,
            (2*P*Q)*x0+(1-2*Q*Q)*y0+(-2*Q*sq)*z0,
            (-2*P*sq)*x0+(2*Q*sq)*y0+(1-2*P*P-2*Q*Q)*z0};
  }
};
MoonModel::MoonModel(const std::string& path,int fit,bool reuse):impl_(new Impl){
  impl_->reuse=reuse;
  std::ifstream in(path,std::ios::binary);char magic[8];in.read(magic,8);
  if(!in||std::string(magic,8)!="CELP0001"||integer(in,4)!=unsigned(fit))throw std::runtime_error("Invalid lunar pack: "+path);
  Elp_facs factors;setup_parameters(fit,impl_->parameters,factors);
  unsigned total=0;
  for(auto& group:impl_->groups){unsigned n=integer(in,4);total+=n;if(n>40000||total>40000)throw std::runtime_error("Lunar pack count exceeds bound");
    group.rows.resize(n);group.amplitude.resize(n);group.phase.resize(n);group.pointers.resize(n);
    for(unsigned i=0;i<n;i++){for(auto& a:group.rows[i])a=int16_t(integer(in,2));group.amplitude[i]=number(in);group.phase[i]=number(in);group.pointers[i]=group.rows[i].data();}
  }
  if(in.peek()!=EOF)throw std::runtime_error("Trailing lunar pack bytes");
  for(int i=0;i<3;i++)if(impl_->groups[i].rows.empty())throw std::runtime_error("Missing main lunar coefficients");
  auto& c=impl_->coefficients;
#define BIND(name,index) c.n_##name=impl_->groups[index].rows.size();c.i_##name=impl_->groups[index].pointers.data();c.A_##name=impl_->groups[index].amplitude.data()
  BIND(main_long,0);BIND(main_lat,1);BIND(main_dist,2);
  BIND(pert_longT0,3);BIND(pert_longT1,4);BIND(pert_longT2,5);BIND(pert_longT3,6);
  BIND(pert_latT0,7);BIND(pert_latT1,8);BIND(pert_latT2,9);
  BIND(pert_distT0,10);BIND(pert_distT1,11);BIND(pert_distT2,12);BIND(pert_distT3,13);
#undef BIND
#define PHASE(name,index) c.ph_##name=impl_->groups[index].phase.data()
  PHASE(pert_longT0,3);PHASE(pert_longT1,4);PHASE(pert_longT2,5);PHASE(pert_longT3,6);
  PHASE(pert_latT0,7);PHASE(pert_latT1,8);PHASE(pert_latT2,9);
  PHASE(pert_distT0,10);PHASE(pert_distT1,11);PHASE(pert_distT2,12);PHASE(pert_distT3,13);
#undef PHASE
  if(reuse)impl_->Prepare();
}
MoonModel::~MoonModel()=default;
Vec MoonModel::Evaluate(double jd)const{if(!std::isfinite(jd))throw std::invalid_argument("Nonfinite lunar epoch");return impl_->Evaluate((jd-2451545.0)/36525.0);}
}
