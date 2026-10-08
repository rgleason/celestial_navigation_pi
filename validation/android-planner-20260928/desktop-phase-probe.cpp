#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"
#include <wx/utils.h>
#include <wx/app.h>
#include <iostream>
int main() {
 wxApp::SetInstance(new wxAppConsole());
 int argc=1; char name[]="moon-probe"; char* argv[]={name,nullptr};
 if (!wxEntryStart(argc,argv)) return 3;
 wxSetEnv("CELNAV_TEST_DE440_ENABLE", "1");
 for (int month : {6,12}) {
  const auto utc = UtcDateTime::ToInstant(UtcDateTime::Create(2024,month,21,0,0,0,987));
  std::cout << UtcDateTime::FormatInstant(utc,"%Y-%m-%d %H:%M:%S.%l UTC").ToStdString() << "\n";
  for (const auto& phase : NextPrincipalMoonPhases(utc, 78, 15))
   std::cout << phase.name.ToStdString() << " " << UtcDateTime::FormatInstant(phase.utc,"%Y-%m-%d %H:%M:%S.%l UTC").ToStdString() << "\n";
 }
}
