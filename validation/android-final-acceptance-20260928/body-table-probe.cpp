// Desktop model reference for Android sort binding, not new astronomy accuracy.
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include <wx/app.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
int main() {
  setenv("TZ", "UTC", 1); tzset();
  wxApp::SetInstance(new wxAppConsole());
  int argc=1; char name[]="body-table"; char* argv[]={name,nullptr};
  if (!wxEntryStart(argc,argv)) return 2;
  wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
  const auto utc=UtcDateTime::ToInstant(UtcDateTime::Create(2100,12,31,23,0,0,987));
  const auto bodies=SightRanker::VisibleBodies(utc,49.99635,-5.12052,-90,90,
                             std::numeric_limits<double>::infinity());
  std::cout << std::setprecision(17);
  for (const auto& b:bodies) std::cout << b.state.body.ToStdString() << '\t'
    << b.state.geometricAltitude << '\t' << b.state.azimuthTrue << '\t'
    << b.state.gha << '\t' << b.state.declination << '\t'
    << b.state.visualMagnitude << '\t' << b.score << '\t'
    << b.reason.ToUTF8().data() << '\n';
}
