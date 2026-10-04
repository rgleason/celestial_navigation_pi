#include "celnav/engine.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
std::string quote(const std::string&s){std::string out="\"";for(char c:s){if(c=='"'||c=='\\')out+='\\';if(c=='\n')out+="\\n";else out+=c;}return out+'"';}
void number(const char*key,double v){std::cout<<','<<quote(key)<<':';if(std::isfinite(v))std::cout<<v;else std::cout<<"null";}
void output(const celnav::Result&r){
  std::cout<<std::setprecision(17)<<"{\"body\":"<<quote(r.body)<<",\"source\":"<<quote(r.source);
#define FIELD(name) number(#name,r.name)
  FIELD(ra_deg);FIELD(declination_deg);FIELD(gha_deg);FIELD(aries_gha_deg);
  FIELD(icrf_ra_deg);FIELD(icrf_declination_deg);FIELD(observer_icrf_ra_deg);FIELD(observer_icrf_declination_deg);
  FIELD(geometric_hc_deg);FIELD(airless_altitude_deg);FIELD(azimuth_deg);FIELD(distance_km);
  FIELD(horizontal_parallax_deg);FIELD(geocentric_semidiameter_deg);FIELD(observer_semidiameter_deg);
#undef FIELD
  number("tt_jd",r.epoch.tt_jd);number("tdb_jd",r.epoch.tdb_jd);number("ut1_jd",r.epoch.ut1_jd);
  number("dut1_seconds",r.epoch.dut1_seconds);number("tai_minus_utc",r.epoch.tai_minus_utc);
  std::cout<<",\"dut1_available\":"<<(r.epoch.dut1_available?"true":"false")<<",\"azimuth_defined\":"<<(r.azimuth_defined?"true":"false")<<",\"warnings\":[";
  for(size_t i=0;i<r.epoch.warnings.size();i++){if(i)std::cout<<',';std::cout<<quote(r.epoch.warnings[i]);}std::cout<<"]}\n";
}
std::vector<std::string> split(const std::string&line){std::vector<std::string>out;std::istringstream s(line);std::string v;while(std::getline(s,v,'\t'))out.push_back(v);return out;}
std::string defaultData(const char* executable){
  namespace fs=std::filesystem;
  // Installed/exported CLI is relocatable; --data remains authoritative.
  std::error_code error;auto path=fs::weakly_canonical(executable,error);
  if(!error){
    for(const auto& candidate:{path.parent_path()/"../share/celnav-compact",path.parent_path()/"../data"})
      if(fs::is_regular_file(candidate/"vsop2013.bin",error))return candidate.lexically_normal().string();
  }
  return COMPACT_DEFAULT_DATA;
}
}
int main(int argc,char**argv){try{
  celnav::Options options;options.data_directory=defaultData(argv[0]);
  celnav::Request request;std::string mode="candidate",batch;int planet=0,benchmark=0;double modelJd=0;bool moon=false,list=false;
  auto value=[&](int&i){if(++i>=argc)throw std::invalid_argument("Missing option value");return std::string(argv[i]);};
  for(int i=1;i<argc;i++){std::string a=argv[i];
    if(a=="--data")options.data_directory=value(i);else if(a=="--mode")mode=value(i);
    else if(a=="--body")request.body=value(i);else if(a=="--utc")request.utc=value(i);
    else if(a=="--latitude")request.latitude_deg=std::stod(value(i));else if(a=="--longitude")request.longitude_deg=std::stod(value(i));
    else if(a=="--height")request.height_m=std::stod(value(i));else if(a=="--dut1")request.dut1_seconds=std::stod(value(i));
    else if(a=="--tai")request.tai_minus_utc=std::stod(value(i));else if(a=="--batch")batch=value(i);
    else if(a=="--venus-phase")request.venus_phase=true;else if(a=="--full")options.full_series=true;
    else if(a=="--fit")options.lunar_fit=std::stoi(value(i));else if(a=="--planet"){planet=std::stoi(value(i));modelJd=std::stod(value(i));}
    else if(a=="--moon"){moon=true;modelJd=std::stod(value(i));}else if(a=="--benchmark")benchmark=std::stoi(value(i));
    else if(a=="--list")list=true;else throw std::invalid_argument("Unknown option: "+a);
  }
  if(mode!="candidate"&&mode!="baseline"&&mode!="modern-legacy")throw std::invalid_argument("Unknown engine mode");
  if(options.lunar_fit<0||options.lunar_fit>1)throw std::invalid_argument("Invalid lunar fit");
  options.legacy_orbits=mode=="modern-legacy";celnav::Engine engine(options);
  if(list){for(const auto&body:engine.Bodies())std::cout<<body<<'\n';return 0;}
  if(planet||moon){auto v=moon?engine.MoonEcliptic(modelJd):engine.PlanetEcliptic(planet,modelJd);std::cout<<std::setprecision(17)<<"["<<v[0]<<','<<v[1]<<','<<v[2]<<"]\n";return 0;}
  auto evaluate=[&](const celnav::Request&r){
#if COMPACT_WITH_BASELINE
    if(mode=="baseline")return celnav::EvaluateBaseline(r,engine.ResolveEpoch(r),options.data_directory+"/../baseline/vsop87d.txt");
#else
    if(mode=="baseline")throw std::invalid_argument("Baseline comparison disabled in this build");
#endif
    return engine.Evaluate(r);
  };
  if(benchmark){if(benchmark<1||benchmark>100000)throw std::invalid_argument("Invalid benchmark count");evaluate(request);auto start=std::chrono::steady_clock::now();double checksum=0;
    for(int i=0;i<benchmark;i++)checksum+=evaluate(request).gha_deg;
    auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<std::setprecision(17)<<"{\"iterations\":"<<benchmark<<",\"microseconds_per_evaluation\":"<<elapsed*1e6/benchmark<<",\"checksum\":"<<checksum<<"}\n";return 0;}
  if(batch.empty()){output(evaluate(request));return 0;}
  std::ifstream input(batch);if(!input)throw std::runtime_error("Cannot open batch file");std::string line;std::getline(input,line);
  if(line!="utc\tbody\tlatitude\tlongitude\theight\tdut1\tvenus_phase")throw std::invalid_argument("Invalid batch header");
  while(std::getline(input,line)){auto f=split(line);if(f.size()!=7)throw std::invalid_argument("Invalid batch row");
    auto r=request;r.utc=f[0];r.body=f[1];r.latitude_deg=std::stod(f[2]);r.longitude_deg=std::stod(f[3]);r.height_m=std::stod(f[4]);r.dut1_seconds=f[5].empty()?NAN:std::stod(f[5]);r.venus_phase=f[6]=="1";output(evaluate(r));}
  return 0;
}catch(const std::exception&e){std::cerr<<"compact-engine: "<<e.what()<<'\n';return 2;}}
