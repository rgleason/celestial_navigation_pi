#include "celnav/engine.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv) {
  if(argc!=3)return 2;
  celnav::Options o;o.data_directory=argv[1];celnav::Engine fast(o);
  std::ifstream fixture(argv[2]);std::string line;int count=0;double maxAngle=0,maxRange=0;
  std::ofstream capture("/data/local/tmp/celnav295-compact-check/optimized-states.tsv");capture<<std::setprecision(17);
  while(std::getline(fixture,line)) {
    std::istringstream stream(line);std::vector<std::string> f;std::string v;
    while(std::getline(stream,v,'\t'))f.push_back(v);
    if(f.empty()||f[0]!="STATE")continue;
    celnav::Request r;r.body=f[1];r.utc=f[2];r.latitude_deg=std::stod(f[3]);r.longitude_deg=-179.75;r.height_m=3.7;r.venus_phase=r.body=="Venus";
    const auto a=fast.Evaluate(r);
    const double values[]={a.gha_deg,a.declination_deg,a.distance_km,a.airless_altitude_deg,a.azimuth_deg,a.geometric_hc_deg,a.observer_semidiameter_deg};
    for(int i=0;i<7;++i) {
      const double error=std::abs(values[i]-std::stod(f[i+4]));
      if(i==2)maxRange=std::max(maxRange,error);else maxAngle=std::max(maxAngle,error);
      if(error>(i==2?1e-4:1e-8)){std::cerr<<"Coordinate mismatch "<<r.body<<' '<<r.utc<<' '<<i<<' '<<error<<'\n';return 1;}
    }
    capture<<r.body<<"\t"<<r.utc<<"\t"<<r.latitude_deg;for(double value:values)capture<<"\t"<<value;capture<<"\n";
    ++count;
  }
  if(count!=1584)return 3;
  double lunarDifference=0;int lunarSamples=0;
  for(int fit:{0,1}) {
    o.lunar_fit=fit;o.reuse_lunar_arguments=true;celnav::Engine optimized(o);
    o.reuse_lunar_arguments=false;celnav::Engine original(o);
    for(int y=1972;y<=2100;++y)for(int m=1;m<=12;++m)for(int d:{1,15}) {
      std::ostringstream utc;utc<<y<<'-'<<std::setw(2)<<std::setfill('0')<<m<<'-'<<std::setw(2)<<d<<"T12:17:35.125";
      celnav::Request r;r.utc=utc.str();double jd=optimized.ResolveEpoch(r).tdb_jd;
      auto a=optimized.MoonEcliptic(jd),b=original.MoonEcliptic(jd);
      for(int i=0;i<3;++i)lunarDifference=std::max(lunarDifference,std::abs(a[i]-b[i]));
      ++lunarSamples;
    }
  }
  if(lunarDifference>1e-7)return 4;
  celnav::Request r;r.body="Moon";r.utc="2025-07-16T12:00:00.125";r.latitude_deg=41;r.longitude_deg=-71;
  fast.Evaluate(r);auto begin=Clock::now();
  for(int i=0;i<1000;++i)fast.Evaluate(r);
  double reuse=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
  o.lunar_fit=1;o.reuse_lunar_arguments=true;o.cache_epoch_context=false;celnav::Engine uncached(o);
  begin=Clock::now();for(int i=0;i<100;++i)uncached.Evaluate(r);
  double recompute=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
  std::cout<<std::setprecision(17)<<"PASS frozen_states="<<count<<" max_angle_deg="<<maxAngle<<" max_range_km="<<maxRange
           <<" lunar_samples="<<lunarSamples<<" lunar_rounding_mm="<<lunarDifference*1e6
           <<" repeated_request_ms="<<reuse/1000<<" recomputed_request_ms="<<recompute/100<<'\n';
}
