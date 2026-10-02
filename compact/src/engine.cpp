#include "celnav/engine.hpp"
#include "models.hpp"
#include "erfa.h"
#include "eclipse/mutex.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <locale>
#include <mutex>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace celnav {
namespace {
constexpr double pi=3.14159265358979323846,deg=pi/180,au=149597870.7,cAuDay=173.144632674240,emrat=81.30056907419062;
Vec add(const Vec&a,const Vec&b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
Vec mul(const Vec&a,double k){return {a[0]*k,a[1]*k,a[2]*k};}
Vec sub(const Vec&a,const Vec&b){return add(a,mul(b,-1));}
double norm(const Vec&v){return std::hypot(v[0],std::hypot(v[1],v[2]));}
double dot(const Vec&a,const Vec&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec unit(const Vec&v){double n=norm(v);if(!(n>0))throw std::runtime_error("Zero direction");return mul(v,1/n);}
double wrap(double x){return std::fmod(std::fmod(x,360)+360,360);}
struct Star{double ra,dec,pmra,pmdec,rv,parallax;};
struct Day{int mjd;double dut1;char quality;};
struct Context{Vec earth,earthHelio,earthVelocity,sun;};
struct Calendar {int y,m,d,h,min;double sec;};
Calendar parse(const std::string&s){
  static const std::regex format(R"(^([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2}(?:\.[0-9]+)?)(?:Z)?$)");
  std::smatch match;if(!std::regex_match(s,match,format))throw std::invalid_argument("Expected ISO UTC calendar");
  std::istringstream seconds_input(match[6].str());seconds_input.imbue(std::locale::classic());
  double seconds;seconds_input>>seconds;
  if(!seconds_input||!seconds_input.eof())throw std::invalid_argument("Invalid UTC seconds");
  Calendar c{std::stoi(match[1]),std::stoi(match[2]),std::stoi(match[3]),std::stoi(match[4]),std::stoi(match[5]),seconds};
  if(c.h>23||c.min>59||c.sec>=60)throw std::invalid_argument("Invalid clock or unsupported leap-second instant");
  double z,j;if(eraCal2jd(c.y,c.m,c.d,&z,&j))throw std::invalid_argument("Invalid calendar date");return c;
}
Vec solarBarycentre(double jd){double h[2][3],b[2][3];eraEpv00(2451545,jd-2451545,h,b);return {b[0][0]-h[0][0],b[0][1]-h[0][1],b[0][2]-h[0][2]};}
void angles(const Vec&v,double&ra,double&dec){ra=wrap(std::atan2(v[1],v[0])/deg);dec=std::atan2(v[2],std::hypot(v[0],v[1]))/deg;}
}
struct Engine::Impl{
  Options options;
  PlanetModel planets;
  MoonModel moon;
  std::map<std::string,Star> stars;
  std::vector<Day> days;
  double moon_bias[3][3]{};
  mutable eclipse::Mutex context_mutex;
  mutable std::vector<std::pair<double,Context>> contexts;
  explicit Impl(Options opts):options(std::move(opts)),
    planets(options.data_directory+(options.full_series?"/../.work/vsop-full.bin":"/vsop2013.bin")),
    moon(options.data_directory+(options.full_series?"/../.work/moon-full-":"/moon-")+std::to_string(options.lunar_fit)+".bin",options.lunar_fit){
    std::ifstream input(options.data_directory+"/stars.tsv");input.imbue(std::locale::classic());std::string line;
    while(std::getline(input,line)){
      std::istringstream stream(line);stream.imbue(std::locale::classic());std::string name;std::getline(stream,name,'\t');Star s;
      if(!(stream>>s.ra>>s.dec>>s.pmra>>s.pmdec>>s.rv>>s.parallax)||!std::isfinite(s.ra)||!std::isfinite(s.dec)||!std::isfinite(s.pmra)||!std::isfinite(s.pmdec)||!std::isfinite(s.rv)||!std::isfinite(s.parallax)||s.ra<0||s.ra>=360||std::abs(s.dec)>=90||s.parallax<0||name.empty()||!stars.emplace(name,s).second)throw std::runtime_error("Invalid star catalogue");
    }
    if(stars.size()!=59)throw std::runtime_error("Incomplete star catalogue (expected the 59 frozen plugin entries)");
    std::ifstream time(options.data_directory+"/dut1.tsv");time.imbue(std::locale::classic());Day day;
    while(time>>day.mjd>>day.dut1>>day.quality){
      if(!std::isfinite(day.dut1)||std::abs(day.dut1)>1||(!days.empty()&&day.mjd!=days.back().mjd+1))throw std::runtime_error("Invalid DUT1 data");
      days.push_back(day);
    }
    if(days.empty()||!time.eof())throw std::runtime_error("Missing or invalid DUT1 table");
    double rp[3][3],rbp[3][3];eraBp00(2451545,0,moon_bias,rp,rbp);
  }
  Vec moonIcrf(double jd)const{
#if COMPACT_WITH_BASELINE
    if(options.legacy_orbits)return LegacyMoonIcrf(jd);
#endif
    Vec eq=MoonEclipticToIcrf(moon.Evaluate(jd)),out{};
    eraTrxp(const_cast<double(*)[3]>(moon_bias),eq.data(),out.data());return out;
  }
  Vec earthHelio(double jd)const{
#if COMPACT_WITH_BASELINE
    if(options.legacy_orbits)return LegacyHeliocentric(3,jd,options.data_directory+"/../baseline/vsop87d.txt");
#endif
    return sub(VsopEclipticToIcrf(planets.Evaluate(3,jd)),mul(moonIcrf(jd),1/(au*(emrat+1))));
  }
  Vec earthBary(double jd)const{return add(earthHelio(jd),solarBarycentre(jd));}
  Vec target(const std::string&body,double jd)const{
    if(body=="Sun")return solarBarycentre(jd);
    if(body=="Moon"){
      const Vec lunar=moonIcrf(jd);
#if COMPACT_WITH_BASELINE
      if(options.legacy_orbits)return add(earthBary(jd),mul(lunar,1/au));
#endif
      // EMB to lunar barycentric position: evaluate the lunar series once.
      return add(add(VsopEclipticToIcrf(planets.Evaluate(3,jd)),solarBarycentre(jd)),
                 mul(lunar,(1-1/(emrat+1))/au));
    }
    static const std::map<std::string,int> ids={{"Mercury",1},{"Venus",2},{"Mars",4},{"Jupiter",5},{"Saturn",6}};
    auto it=ids.find(body);if(it==ids.end())throw std::invalid_argument("Unknown solar-system body: "+body);
    auto helio=VsopEclipticToIcrf(planets.Evaluate(it->second,jd));
#if COMPACT_WITH_BASELINE
    if(options.legacy_orbits)helio=LegacyHeliocentric(it->second,jd,options.data_directory+"/../baseline/vsop87d.txt");
#endif
    return add(helio,solarBarycentre(jd));
  }
  Context context(const Epoch&e)const{
    if(options.cache_epoch_context){
      eclipse::MutexGuard lock(context_mutex);
      for(const auto& entry:contexts)if(entry.first==e.tdb_jd)return entry.second;
    }
    constexpr double step=.005;
    Context c;c.earthHelio=earthHelio(e.tdb_jd);c.sun=solarBarycentre(e.tdb_jd);c.earth=add(c.earthHelio,c.sun);
    c.earthVelocity=mul(sub(earthBary(e.tdb_jd+step),earthBary(e.tdb_jd-step)),1/(2*step));
    if(options.cache_epoch_context){
      eclipse::MutexGuard lock(context_mutex);
      for(const auto& entry:contexts)if(entry.first==e.tdb_jd)return entry.second;
      if(contexts.size()==16)contexts.erase(contexts.begin());
      contexts.emplace_back(e.tdb_jd,c);
    }
    return c;
  }
  Vec direction(const Request&r,const Epoch&e,const Context&context,const Vec&station,const Vec&stationVelocity,double&range,
                std::map<double,Vec>& targets,std::map<double,Vec>& suns)const{
    const Vec observer=add(context.earth,station);
    const Vec velocity=add(context.earthVelocity,stationVelocity);
    auto star=stars.find(r.body);
    if(star!=stars.end()){
      const auto&s=star->second;eraASTROM astrom{};
      double pv[2][3]={{station[0],station[1],station[2]},{stationVelocity[0],stationVelocity[1],stationVelocity[2]}};
      // eraApcs requires terrestrial station metres and metres/second.
      for(int k=0;k<3;k++){pv[0][k]*=au*1000;pv[1][k]*=au*1000/86400;}
      const auto&earth=context.earth;const auto&ev=context.earthVelocity;auto eh=context.earthHelio;
      double ebpv[2][3]={{earth[0],earth[1],earth[2]},{ev[0],ev[1],ev[2]}};
      eraApcs(2451545,e.tdb_jd-2451545,pv,ebpv,eh.data(),&astrom);
      double ra,dec;
      eraAtciq(s.ra*deg,s.dec*deg,s.pmra/1000*deg/3600/std::cos(s.dec*deg),s.pmdec/1000*deg/3600,s.parallax/1000,s.rv,&astrom,&ra,&dec);
      range=0;return {std::cos(ra)*std::cos(dec),std::sin(ra)*std::cos(dec),std::sin(dec)};
    }
    double emission=e.tdb_jd;Vec targetPosition{},delta{};bool converged=false;
    for(int i=0;i<12;i++){
      auto pos=targets.find(emission);
      if(pos==targets.end())pos=targets.emplace(emission,target(r.body,emission)).first;
      targetPosition=pos->second;delta=sub(targetPosition,observer);
      double next=e.tdb_jd-norm(delta)/cAuDay;
      if(std::abs(next-emission)<5e-10){converged=true;break;}emission=next;
    }
    if(!converged)throw std::runtime_error("Light-time solve did not converge");
    range=norm(delta)*au;Vec natural=unit(delta),deflected=natural;
    auto solar=[&](double jd){auto it=suns.find(jd);if(it==suns.end())it=suns.emplace(jd,solarBarycentre(jd)).first;return it->second;};
    Vec sunToObserver=sub(observer,context.sun);double sunRange=norm(sunToObserver);
    if(r.body!="Sun"){
      Vec q=unit(sub(targetPosition,solar(emission))),fromSun=unit(sunToObserver);
      eraLd(1,natural.data(),q.data(),fromSun.data(),sunRange,1e-6,deflected.data());
    }
    Vec beta=mul(velocity,1/cAuDay),apparent{};
    eraAb(deflected.data(),beta.data(),sunRange,std::sqrt(1-dot(beta,beta)),apparent.data());
    if(r.body=="Venus"&&r.venus_phase){
      Vec sp=sub(targetPosition,solar(emission)),usp=unit(sp),p=unit(apparent);
      const double pRange=range/au,spRange=norm(sp),sunGeo=norm(sunToObserver);
      double illuminated=std::clamp(((spRange+pRange)*(spRange+pRange)-sunGeo*sunGeo)/(4*spRange*pRange),0.0,1.0);
      Vec tangent=sub(mul(p,dot(p,usp)),usp);
      if(norm(tangent)>1e-15)apparent=unit(add(p,mul(unit(tangent),8*(8.41*pi/(pRange*648000))*(1-illuminated)/(3*pi))));
    }
    return unit(apparent);
  }
};
Engine::Engine(Options options){
  // ERFA initializes its process-wide leap table lazily. Initialize before
  // an Engine can be shared between calculation threads.
  static std::once_flag leapTable;
  std::call_once(leapTable,[]{double offset;eraDat(2026,1,1,0,&offset);});
#if !COMPACT_WITH_BASELINE
  if(options.legacy_orbits)throw std::invalid_argument("Legacy comparison is disabled in this standalone build");
#endif
  if(options.lunar_fit<0||options.lunar_fit>1)throw std::invalid_argument("Invalid lunar fit");
  impl_.reset(new Impl(std::move(options)));
}
Engine::~Engine()=default;Engine::Engine(Engine&&)noexcept=default;Engine&Engine::operator=(Engine&&)noexcept=default;
Epoch Engine::ResolveEpoch(const Request&r)const{
  auto calendar=parse(r.utc);if(calendar.y<1972||calendar.y>2100)throw std::out_of_range("Candidate date coverage is 1972-2100");
  double z,mjd;eraCal2jd(calendar.y,calendar.m,calendar.d,&z,&mjd);
  Epoch e;double fraction=(calendar.h*3600+calendar.min*60+calendar.sec)/86400;
  e.utc_jd=z+mjd+fraction;
  double calendar_tai;
  int dat=eraDat(calendar.y,calendar.m,calendar.d,fraction,&calendar_tai);
  e.tai_minus_utc=calendar_tai;
  if(dat<0)throw std::invalid_argument("Cannot resolve leap-second offset");
  if(std::isinf(r.tai_minus_utc)||std::isinf(r.dut1_seconds))throw std::invalid_argument("Infinite time offset");
  if(std::isfinite(r.tai_minus_utc))e.tai_minus_utc=r.tai_minus_utc;
  else if(dat>0||calendar.y>2027||(calendar.y==2027&&calendar.m>=7))
    e.warnings.push_back("Future leap seconds unknown beyond IERS Bulletin C 72; last published TAI-UTC assumed");
  if(std::isfinite(r.dut1_seconds)){e.dut1_seconds=r.dut1_seconds;e.dut1_available=true;}
  else{
    int i=int(mjd)-impl_->days.front().mjd;
    if(i>=0&&i<int(impl_->days.size())&&(fraction==0||i+1<int(impl_->days.size()))){
      auto d=impl_->days[i];e.dut1_seconds=d.dut1;e.dut1_available=true;
      if(fraction>0){int y,m,day;double f,tai;eraJd2cal(z,mjd+1,&y,&m,&day,&f);eraDat(y,m,day,0,&tai);
        e.dut1_seconds+=fraction*(impl_->days[i+1].dut1-d.dut1-(tai-calendar_tai));}
      if(d.quality=='P')e.warnings.push_back("DUT1 uses dated IERS prediction");
    }else e.warnings.push_back("DUT1 unavailable; UT1=UTC fallback");
  }
  if(!std::isfinite(e.tai_minus_utc)||e.tai_minus_utc<0||e.tai_minus_utc>1000||std::abs(e.dut1_seconds)>2)throw std::invalid_argument("Invalid time offset");
  e.tt_jd=e.utc_jd+(e.tai_minus_utc+32.184)/86400;e.ut1_jd=e.utc_jd+e.dut1_seconds/86400;
  // Geocentric periodic TT/TDB relation, without observer-dependent term.
  e.tdb_jd=e.tt_jd+eraDtdb(2451545,e.tt_jd-2451545,fraction,0,0,0)/86400;
  if(std::abs(e.tdb_jd-2451545.0)>36525)
    e.warnings.push_back("Small ERFA epv00 solar barycentric correction is outside its recommended 1900-2100 interval");
  return e;
}
Result Engine::Evaluate(const Request&r)const{
  for(double value:{r.latitude_deg,r.longitude_deg,r.height_m,r.polar_x_arcsec,r.polar_y_arcsec})if(!std::isfinite(value))throw std::invalid_argument("Nonfinite observer input");
  if(std::abs(r.latitude_deg)>90||std::abs(r.longitude_deg)>180||r.height_m<=-6370000)throw std::invalid_argument("Invalid observer coordinates");
  Result out;out.body=r.body;out.epoch=ResolveEpoch(r);const auto&e=out.epoch;
  out.source=impl_->options.legacy_orbits?"Modern astrometry / legacy orbits":impl_->stars.count(r.body)?"ERFA / frozen stellar catalogue":"Compact VSOP2013 / ELP-MPP02";
  out.aries_gha_deg=wrap(eraGst06a(2451545,e.ut1_jd-2451545,2451545,e.tt_jd-2451545)/deg);
  if(r.body=="Aries"){out.gha_deg=out.aries_gha_deg;out.source="IAU 2006/2000A Earth rotation";return out;}
  Vec zero{};double range;
  std::map<double,Vec> targets,suns;
  const auto context=impl_->context(e);
  Vec geocentric=impl_->direction(r,e,context,zero,zero,range,targets,suns);
  out.distance_km=range;angles(geocentric,out.icrf_ra_deg,out.icrf_declination_deg);
  double bpn[3][3];eraPnm06a(2451545,e.tt_jd-2451545,bpn);Vec equinox{};eraRxp(bpn,geocentric.data(),equinox.data());
  angles(equinox,out.ra_deg,out.declination_deg);out.gha_deg=wrap(out.aries_gha_deg-out.ra_deg);
  double radius=r.body=="Sun"?695700:r.body=="Moon"?1737.4:0;
  if(range>6378.137){out.horizontal_parallax_deg=std::asin(6378.137/range)/deg;if(radius>0)out.geocentric_semidiameter_deg=std::asin(radius/range)/deg;}
  double cirs[3][3],stationPV[2][3];eraC2i06a(2451545,e.tt_jd-2451545,cirs);
  eraPvtob(r.longitude_deg*deg,r.latitude_deg*deg,r.height_m,r.polar_x_arcsec*deg/3600,r.polar_y_arcsec*deg/3600,
           eraSp00(2451545,e.tt_jd-2451545),eraEra00(2451545,e.ut1_jd-2451545),stationPV);
  Vec station{},stationVelocity{};eraTrxp(cirs,stationPV[0],station.data());eraTrxp(cirs,stationPV[1],stationVelocity.data());
  station=mul(station,1/(au*1000));stationVelocity=mul(stationVelocity,86400/(au*1000));
  Vec observed=impl_->direction(r,e,context,station,stationVelocity,range,targets,suns);
  angles(observed,out.observer_icrf_ra_deg,out.observer_icrf_declination_deg);
  if(radius>0)out.observer_semidiameter_deg=std::asin(radius/range)/deg;
  double fixedMatrix[3][3];eraC2t06a(2451545,e.tt_jd-2451545,2451545,e.ut1_jd-2451545,r.polar_x_arcsec*deg/3600,r.polar_y_arcsec*deg/3600,fixedMatrix);
  auto horizon=[&](const Vec&direction,double&alt,double*az){
    Vec fixed{};eraRxp(fixedMatrix,const_cast<double*>(direction.data()),fixed.data());
    double lat=r.latitude_deg*deg,lon=r.longitude_deg*deg;
    double east=-sin(lon)*fixed[0]+cos(lon)*fixed[1];
    double north=-sin(lat)*cos(lon)*fixed[0]-sin(lat)*sin(lon)*fixed[1]+cos(lat)*fixed[2];
    double up=cos(lat)*cos(lon)*fixed[0]+cos(lat)*sin(lon)*fixed[1]+sin(lat)*fixed[2];
    alt=atan2(up,hypot(east,north))/deg;
    if(az){out.azimuth_defined=hypot(east,north)>1e-12;*az=out.azimuth_defined?wrap(atan2(east,north)/deg):std::numeric_limits<double>::quiet_NaN();}
  };
  horizon(geocentric,out.geometric_hc_deg,nullptr);horizon(observed,out.airless_altitude_deg,&out.azimuth_deg);
  return out;
}
std::vector<Result> Engine::EvaluateMany(const std::vector<Request>& requests)const{
  std::vector<Result> results;results.reserve(requests.size());
  for(const auto& request:requests)results.push_back(Evaluate(request));
  return results;
}
Coverage Engine::CheckCoverage(const std::string& body,const std::string& first,const std::string& last)const{
  const auto a=parse(first),b=parse(last);
  auto jd=[](const Calendar& c){double z,d;eraCal2jd(c.y,c.m,c.d,&z,&d);return z+d+(c.h*3600+c.min*60+c.sec)/86400;};
  if(jd(a)>jd(b))throw std::invalid_argument("Reversed search interval");
  const auto bodies=Bodies();
  if(std::find(bodies.begin(),bodies.end(),body)==bodies.end())return {false,"Unsupported body"};
  if(a.y<1972||b.y>2100)return {false,"Search interval extends outside 1972-01-01 through 2100-12-31 UTC"};
  return {true,""};
}
const char* Engine::Version()noexcept{return "0.2.0";}
std::vector<std::string>Engine::Bodies()const{std::vector<std::string>b={"Sun","Moon","Mercury","Venus","Mars","Jupiter","Saturn","Aries"};for(const auto&s:impl_->stars)b.push_back(s.first);return b;}
std::array<double,3>Engine::PlanetEcliptic(int p,double jd)const{return impl_->planets.Evaluate(p,jd);}
std::array<double,3>Engine::MoonEcliptic(double jd)const{return impl_->moon.Evaluate(jd);}
}
