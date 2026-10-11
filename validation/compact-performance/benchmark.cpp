#include <wx/app.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/utils.h>
#include <chrono>
#include <iostream>
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include "CelestialNavigationDialog.h"
#include "PlannerDialog.h"
#include "celestial_navigation_pi.h"
#include <wx/frame.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <memory>
#include <iomanip>
#ifdef ISSUE365_COMPACT
#include "CompactEphemerisProvider.h"
#endif
void SetTestPluginDataRoot(const wxString&);
void SetTestPrivateDataPath(const wxString&);
bool compactSource(const BodyState& s) {
#ifdef ISSUE365_COMPACT
  return s.usedCompact;
#else
  return false;
#endif
}
wxChoice* timeSource(wxWindow* root) {
  for(auto* child:root->GetChildren()) {
    if(auto* choice=dynamic_cast<wxChoice*>(child))
      if(choice->GetCount()==3 && choice->GetString(0)=="Now") return choice;
    if(auto* found=timeSource(child))return found;
  }
  return nullptr;
}

template<typename F> void measure(const std::string& name, F f) {
  auto start = std::chrono::steady_clock::now();
  auto count = f();
  double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::cout << name << " ms=" << ms << " count=" << count << std::endl;
}

int main(int argc,char**argv) {
  const std::string mode = argc>1 ? argv[1] : "classic";
  const std::string scenario = argc>2 ? argv[2] : "components";
  int wxargc=1;char app[]="celnav365-benchmark";char* wxargv[]={app,nullptr};
  wxApp::SetInstance(new wxApp);
  if(!wxEntryStart(wxargc,wxargv)||!wxTheApp->CallOnInit()) return 2;
  wxInitAllImageHandlers();
  wxString privatePath=wxFileName::CreateTempFileName("celnav365-");
  wxRemoveFile(privatePath);
  wxFileName::Mkdir(privatePath+"/plugins/celestial_navigation",0700,wxPATH_MKDIR_FULL);
  SetTestPrivateDataPath(privatePath);
  SetTestPluginDataRoot(wxString::FromUTF8(ISSUE365_SOURCE));
  wxSetEnv("CELNAV_TEST_DE440_ENABLE",mode.find("de440")==0?"1":"0");
  GetOCPNConfigObject()->Write("/PlugIns/CelestialNavigation/UseCompactEphemeris",mode=="compact"||mode=="de440-compact");
  GetOCPNConfigObject()->Write("/PlugIns/CelestialNavigation/ShowTimeIntegrity",false);
  ObserverMotion motion;
  motion.referenceUtc=UtcDateTime::ToInstant(wxDateTime(16,wxDateTime::Jul,2025,12,0,0));
  motion.latitude=41.0;motion.longitude=-71.0;
  std::cout << "mode=" << mode << " scenario=" << scenario << std::endl;
  if(scenario=="accuracy") {
#ifdef ISSUE365_COMPACT
    auto engine=celestial_navigation::CompactEngine();
    const char* dates[]={"1972-01-01T00:00:00.125","1982-11-12T19:17:35.750","2003-03-11T12:00:00.000","2025-07-16T12:00:00.000","2026-03-08T02:30:00.125","2100-12-31T23:59:59.999"};
    std::cout<<std::setprecision(17);
    for(const char* date:dates) for(double lat:{-89.9,0.,41.,89.9}) for(const auto& body:engine->Bodies()) {
      if(body=="Aries")continue;
      celnav::Request r;r.body=body;r.utc=date;r.latitude_deg=lat;r.longitude_deg=-179.75;r.height_m=3.7;r.venus_phase=body=="Venus";
      auto s=engine->Evaluate(r);
      std::cout<<"STATE\t"<<body<<'\t'<<date<<'\t'<<lat<<'\t'<<s.gha_deg<<'\t'<<s.declination_deg<<'\t'<<s.distance_km<<'\t'<<s.airless_altitude_deg<<'\t'<<s.azimuth_deg<<'\t'<<s.geometric_hc_deg<<'\t'<<s.observer_semidiameter_deg<<std::endl;
    }
    for(const char* date:{"1982-11-12T19:17:35","2025-07-16T12:00:00","2026-03-08T02:30:00"}) {
      wxDateTime fields;fields.ParseISOCombined(date);const auto utc=UtcDateTime::ToInstant(fields);
      for(const auto& phase:NextPrincipalMoonPhases(utc,41,-71))
        std::cout<<"PHASE\t"<<date<<'\t'<<phase.name<<'\t'<<UtcDateTime::FormatInstant(phase.utc,"%Y-%m-%dT%H:%M:%S.%l")<<std::endl;
      ObserverMotion m;m.referenceUtc=utc;m.latitude=41;m.longitude=-71;
      for(const auto& event:HorizonEventCalculator::Calculate(utc,m,3.7).events)
        std::cout<<"EVENT\t"<<date<<'\t'<<int(event.kind)<<'\t'<<UtcDateTime::FormatInstant(event.utc,"%Y-%m-%dT%H:%M:%S.%l")<<'\t'<<event.bearingTrue<<std::endl;
    }
#endif
  } else if(scenario=="ui") {
    wxFrame frame(nullptr,wxID_ANY,"Issue 365 benchmark");
    celestial_navigation_pi plugin(nullptr);
    CelestialNavigationDialog main(&frame,&plugin);
    std::unique_ptr<PlannerDialog> planner;
    measure("planner_construct",[&]{planner.reset(new PlannerDialog(&main));return 1;});
    planner->Show();
    auto wait=[&]{
      auto* status=dynamic_cast<wxStaticText*>(planner->FindWindowByName("PlannerStatus"));
      const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(30);
      while(status && status->GetLabel()!="Planning context updated; calculations remain fully offline." && std::chrono::steady_clock::now()<end){wxTheApp->Yield();wxMilliSleep(5);}
      return status && status->GetLabel()=="Planning context updated; calculations remain fully offline.";
    };
    if(mode!="classic")measure("initial_results_ready",wait);
    auto* source=timeSource(planner.get());
    if(!source)return 3;
    measure("switch_now_to_manual",[&]{
      source->SetSelection(2);
      wxCommandEvent event(wxEVT_CHOICE,source->GetId());source->ProcessWindowEvent(event);
      for(int i=0;i<90;i++){wxTheApp->Yield();wxMilliSleep(5);}
      if(mode!="classic")wait();
      return 1;
    });
#ifdef ISSUE365_COMPACT
    measure("latitude_edit",[&]{
      auto* latitude=planner->FindWindowByName("PlannerLatitude");
      static_cast<wxTextCtrl*>(latitude)->SetValue("41.1");
      for(int i=0;i<90;i++){wxTheApp->Yield();wxMilliSleep(5);}
      if(mode!="classic")wait();
      return 1;
    });
#endif
    planner->Hide();
  } else {
    measure("cold_sun",[&]{auto s=CelestialEphemeris::Evaluate("Sun",motion.referenceUtc,motion.latitude,motion.longitude);
      std::cout << "sun_valid="<<s.valid<<" usedCompact="<<compactSource(s)<<" usedDe440="<<s.usedDe440<<std::endl;return 1;});
    auto moon=CelestialEphemeris::Evaluate("Moon",motion.referenceUtc,motion.latitude,motion.longitude);
    auto star=CelestialEphemeris::Evaluate("Capella",motion.referenceUtc,motion.latitude,motion.longitude);
    std::cout<<"moonCompact="<<compactSource(moon)<<" moonDe440="<<moon.usedDe440<<" starCompact="<<compactSource(star)<<std::endl;
    for(int repeat=0;repeat<2;repeat++) {
      std::string prefix="run"+std::to_string(repeat)+"_";
      measure(prefix+"daily_events",[&]{return HorizonEventCalculator::Calculate(motion.referenceUtc,motion,3.7).events.size();});
      measure(prefix+"moon_phases",[&]{return NextPrincipalMoonPhases(motion.referenceUtc,motion.latitude,motion.longitude).size();});
      measure(prefix+"moon_info",[&]{auto m=CalculateMoonInformation(motion.referenceUtc,motion.latitude,motion.longitude);return m.ageDays;});
#ifdef ISSUE365_COMPACT
      measure(prefix+"rank",[&]{auto result=PlannerRecommendations::Calculate(motion.referenceUtc,motion.latitude,motion.longitude);return result.bodies.size();});
#else
      measure(prefix+"rank",[&]{return SightRanker::VisibleBodies(motion.referenceUtc,motion.latitude,motion.longitude).size();});
#endif
      measure(prefix+"almanac",[&]{return BuildAlmanac(motion.referenceUtc,24,{"Sun","Moon","Venus","Mars","Jupiter","Saturn","Polaris"},motion).size();});
      measure(prefix+"duplicate_noon_events",[&]{return HorizonEventCalculator::Calculate(motion.referenceUtc,motion).events.size();});
      motion.latitude+=0.1;
    }
  }
  return 0;
}
