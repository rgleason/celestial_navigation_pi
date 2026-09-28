#include "Sight.h"
#include <wx/init.h>
#include <wx/app.h>
#include <wx/utils.h>
#include <cstdio>
struct InspectSight:Sight {using Sight::Sight;using Sight::lines;};
int main() {
 int argc=0;char** argv=nullptr;wxApp::SetInstance(new wxApp);if(!wxEntryStart(argc,argv))return 1;wxTheApp->CallOnInit();
 wxDateTime utc(21,wxDateTime::Jun,2024,22,0,0,0);
 InspectSight s(Sight::AZIMUTH,"Spica",Sight::CENTER,utc,0,220.63953512345679,0);
 s.m_bMagneticNorth=false;s.m_ShiftNm=0;s.m_TimeCertainty=0;s.m_MeasurementCertainty=1;s.Recompute(0);s.RebuildPolygons();
 double lat,lon;s.BodyLocation(utc,&lat,&lon,0,0,0);
 printf("GP %.17g %.17g\n",lat,lon);
 for(auto node=s.lines.GetFirst();node;node=node->GetNext()) {auto p=node->GetData();printf("POINT %.17g %.17g\n",p->x,p->y);}

 return 0;
}
