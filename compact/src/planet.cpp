#include "models.hpp"
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>

namespace celnav {
namespace {
constexpr double pi=3.14159265358979323846;
double number(std::istream& stream) {
  uint64_t bits=0; for(int i=0;i<8;i++){int c=stream.get();if(c<0)throw std::runtime_error("Truncated planetary pack");bits|=uint64_t(c)<<(8*i);}
  double v; std::memcpy(&v,&bits,8); if(!std::isfinite(v))throw std::runtime_error("Invalid planetary coefficient"); return v;
}
}
PlanetModel::PlanetModel(const std::string& path) {
  std::ifstream in(path,std::ios::binary); char magic[8]; in.read(magic,8);
  if(!in || std::string(magic,8)!="CVSOP001")throw std::runtime_error("Invalid planetary pack: "+path);
  uint32_t n=0;for(int i=0;i<4;i++){int c=in.get();if(c<0)throw std::runtime_error("Truncated pack header");n|=uint32_t(c)<<(8*i);}
  bool covered[7][6]={};
  std::array<std::map<std::pair<double,double>,unsigned>,7> arguments;
  if(n>2000000)throw std::runtime_error("Planetary pack count exceeds bound");
  for(uint32_t i=0;i<n;i++){
    int p=in.get(),v=in.get(),power=in.get(),reserved=in.get();
    if(p<1||p>6||v<0||v>5||power<0||power>20||reserved!=0)throw std::runtime_error("Invalid planetary term");
    PlanetTerm t{unsigned(p),unsigned(v),unsigned(power),number(in),number(in),number(in),number(in)};
    // Identical harmonics occur in several elements and powers. Evaluate
    // their trigonometry once, retaining the original term/summation order.
    const auto key=std::make_pair(t.phase,t.rate);
    auto inserted=arguments[p].emplace(key,arguments_[p].size());
    if(inserted.second)arguments_[p].push_back({t.phase,t.rate});
    t.argument=inserted.first->second;
    covered[p][v]=true;
    terms_[p].push_back(t);
  }
  if(in.peek()!=EOF)throw std::runtime_error("Trailing planetary pack bytes");
  for(int p=1;p<7;p++)for(int v=0;v<6;v++)if(!covered[p][v])throw std::runtime_error("Missing planetary coefficients");
}
Vec PlanetModel::Evaluate(int planet,double jd) const {
  if(planet<1||planet>6||!std::isfinite(jd))throw std::invalid_argument("Invalid planetary model request");
  const double t=(jd-2451545.0)/365250.0;
  std::array<double,21> powers{};powers[0]=1;for(int i=1;i<21;i++)powers[i]=powers[i-1]*t;
  double el[6]={}, compensation[6]={};
  std::vector<std::array<double,2>> trig;
  trig.reserve(arguments_[planet].size());
  for(const auto& argument:arguments_[planet]){
    const double arg=argument[0]+argument[1]*t;
    trig.push_back({std::sin(arg),std::cos(arg)});
  }
  for(const auto& term:terms_[planet]){
    const auto& harmonic=trig[term.argument];
    double add=powers[term.power]*(term.sine*harmonic[0]+term.cosine*harmonic[1]);
    double y=add-compensation[term.variable],z=el[term.variable]+y;
    compensation[term.variable]=(z-el[term.variable])-y;el[term.variable]=z;
  }
  constexpr double freq[]={0,26087.90314068555,10213.28554743445,6283.075850353215,3340.612434145457,529.690961562325,213.299086108488};
  el[1]=std::remainder(el[1]+freq[planet]*t,2*pi);
  const double a=el[0],k=el[2],h=el[3],q=el[4],p=el[5];
  if(a<=0||k*k+h*h>=1||p*p+q*q>=1)throw std::runtime_error("Invalid elliptic elements");
  double e=el[1];bool done=false;
  for(int i=0;i<25;i++){
    double residual=e-k*std::sin(e)+h*std::cos(e)-el[1];
    double step=residual/(1-k*std::cos(e)-h*std::sin(e));e-=step;
    if(std::abs(step)<2e-15){done=true;break;}
  }
  if(!done)throw std::runtime_error("Elliptic solve did not converge");
  const std::complex<double> z(k,h),zt(std::cos(e),std::sin(e)),z3=std::conj(z)*zt;
  double rsa=1-std::real(z3),u=1/(1+std::sqrt(1-k*k-h*h));
  auto z1=u*z*std::imag(z3);
  auto zto=(-z+zt+std::complex<double>(std::imag(z1),-std::real(z1)))/rsa;
  double cw=std::real(zto),sw=std::imag(zto),xm=p*cw-q*sw,r=a*rsa;
  return {r*(cw-2*p*xm),r*(sw+2*q*xm),-2*r*std::sqrt(1-q*q-p*p)*xm};
}
Vec VsopEclipticToIcrf(const Vec& v){
  constexpr double eps=(23+26/60.0+21.41136/3600.0)*pi/180.0,phi=-.05188*pi/648000.0;
  return {std::cos(phi)*v[0]-std::sin(phi)*std::cos(eps)*v[1]+std::sin(phi)*std::sin(eps)*v[2],
          std::sin(phi)*v[0]+std::cos(phi)*std::cos(eps)*v[1]-std::cos(phi)*std::sin(eps)*v[2],
          std::sin(eps)*v[1]+std::cos(eps)*v[2]};
}
Vec MoonEclipticToIcrf(const Vec& v){
  // ELP defines dynamical J2000. Transform to mean equator, then undo ICRS frame bias.
  constexpr double eps=84381.448*pi/648000.0;
  return {v[0],std::cos(eps)*v[1]-std::sin(eps)*v[2],std::sin(eps)*v[1]+std::cos(eps)*v[2]};
}
}
