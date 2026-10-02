#include "models.hpp"
#include "ElpMpp02.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace celnav {
namespace {
uint32_t integer(std::istream& in,int bytes){uint32_t v=0;for(int i=0;i<bytes;i++){int c=in.get();if(c<0)throw std::runtime_error("Truncated lunar pack");v|=uint32_t(c)<<(8*i);}return v;}
double number(std::istream& in){uint64_t bits=0;for(int i=0;i<8;i++)bits|=uint64_t(integer(in,1))<<(8*i);double v;std::memcpy(&v,&bits,8);if(!std::isfinite(v))throw std::runtime_error("Nonfinite lunar coefficient");return v;}
struct Group {std::vector<std::array<int,13>> rows;std::vector<int*> pointers;std::vector<double> amplitude,phase;};
}
struct MoonModel::Impl {Elp_paras parameters{};Elp_coefs coefficients{};std::array<Group,14> groups;};
MoonModel::MoonModel(const std::string& path,int fit):impl_(new Impl){
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
}
MoonModel::~MoonModel()=default;
Vec MoonModel::Evaluate(double jd)const{if(!std::isfinite(jd))throw std::invalid_argument("Nonfinite lunar epoch");Vec result{};getX2000((jd-2451545.0)/36525.0,impl_->parameters,impl_->coefficients,result[0],result[1],result[2]);return result;}
}
