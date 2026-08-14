/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Support
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2015 by Sean D'Epagnier                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 *
 */

// #include "wx/wxprec.h"

// #ifndef  WX_PRECOMP
//   #include "wx/wx.h"
// #endif //precompiled headers

#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <wx/wx.h>
#include <wx/progdlg.h>
#include <wx/listimpl.cpp>
#include <wx/fileconf.h>
#include <wx/filename.h>

#include "OcpnApiCompat.h"

#include "celestial_navigation_pi.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include "transform_star.hpp"
#include "moon.h"
#include "eclipse/astronomy.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"

WX_DEFINE_LIST(wxRealPointList);

double resolve_heading(double heading) {
  heading = std::fmod(heading + 180, 360);
  return heading >= 0 ? heading - 180 : heading + 180;
}

double resolve_heading_positive(double heading) {
  heading = std::fmod(heading, 360);
  return heading >= 0 ? heading : 360 + heading;
}

//-----------------------------------------------------------------------------
//          Sight Implementation
//-----------------------------------------------------------------------------

int Sight::s_lastsightcolor;

Sight::Sight()
    : Sight(ALTITUDE, _T("Sun"), LOWER, UtcDateTime::Now(), 0, 0, 10) {}

Sight::Sight(Type type, wxString body, BodyLimb bodylimb, wxDateTime datetime,
             double timecertainty, double measurement,
             double measurementcertainty)
    : m_bVisible(true),
      m_Type(type),
      m_Body(body),
      m_BodyLimb(bodylimb),
      m_DateTime(datetime),
      m_TimeCertainty(timecertainty),
      m_Measurement(measurement),
      m_MeasurementCertainty(measurementcertainty),
      m_LunarMoonAltitude(0),
      m_LunarBodyAltitude(0),
      m_LunarMoonLimb(LOWER),
      m_LunarBodyLimb(LOWER),
      m_LunarBodyDistanceLimb(LUNAR_NEAR),
      m_LunarMoonAltitudeUncertainty(0.2),
      m_LunarBodyAltitudeUncertainty(0.2),
      m_ShiftNm(0),
      m_ShiftBearing(0),
      m_bMagneticShiftBearing(true),
      m_bMagneticNorth(true),
      m_HorizonEvent(SUNRISE),
      m_HorizonBearingProvided(false),
      m_HorizonBearingMagnetic(true),
      m_HorizonBearing(90),
      m_HorizonVariation(0),
      m_HorizonDeviation(0),
      m_HorizonBearingUncertainty(2),
      m_HorizonAltitudeUncertainty(10),
      m_HorizonQuality(0),
      m_HorizonTimeSource(_T("System UTC")),
      m_HorizonEstimateValid(false),
      m_HorizonEstimateLat(0),
      m_HorizonEstimateLon(0),
      m_HorizonEstimateRadiusNm(0),
      m_TimeCorrection(0),
      m_LDC(NAN),
      m_LunarSolutionValid(false),
      m_LunarSelectedPosition(-1),
      m_LunarUsesDe440(false),
      m_DRLat(0),
      m_DRLon(0),
      m_DRBoatPosition(true),
      m_DRMagneticAzimuth(false) {
  wxFileConfig* pConf = GetOCPNConfigObject();
  pConf->SetPath(_T("/PlugIns/CelestialNavigation"));

  pConf->Read(_T("DefaultEyeHeight"), &m_EyeHeight, 2);
  pConf->Read(_T("DefaultTemperature"), &m_Temperature, 10);
  pConf->Read(_T("DefaultPressure"), &m_Pressure, 1013);
  pConf->Read(_T("DefaultIndexError"), &m_IndexError, 0);
  pConf->Read(_T("DefaultDIPShort"), &m_DipShort, 0);
  pConf->Read(_T("DefaultDIPShortDistance"), &m_DipShortDistance, 0);
  pConf->Read(_T("DefaultArtificialHorizon"), &m_ArtificialHorizon, 0);

  const wxString sightcolornames[] = {_T("MEDIUM VIOLET RED"),
                                      _T("MIDNIGHT BLUE"),
                                      _T("ORANGE"),
                                      _T("PLUM"),
                                      _T("PURPLE"),
                                      _T("RED"),
                                      _T("SALMON"),
                                      _T("SLATE BLUE"),
                                      _T("SPRING GREEN"),
                                      _T("ORANGE RED"),
                                      _T("ORCHID"),
                                      _T("PALE GREEN"),
                                      _T("PINK"),
                                      _T("BROWN"),
                                      _T("BLUE"),
                                      _T("GREEN YELLOW"),
                                      _T("GOLDENROD"),
                                      _T("BLUE VIOLET"),
                                      _T("AQUAMARINE"),
                                      _T("CADET BLUE"),
                                      _T("CORAL"),
                                      _T("CORNFLOWER BLUE"),
                                      _T("FOREST GREEN"),
                                      _T("GOLD"),
                                      _T("THISTLE"),
                                      _T("TURQUOISE"),
                                      _T("VIOLET"),
                                      _T("SEA GREEN"),
                                      _T("SKY BLUE"),
                                      _T("YELLOW GREEN"),
                                      _T("INDIAN RED"),
                                      _T("LIGHT BLUE"),
                                      _T("LIME GREEN"),
                                      _T("MAGENTA"),
                                      _T("MAROON"),
                                      _T("MEDIUM GOLDENROD"),
                                      _T("MEDIUM ORCHID"),
                                      _T("MEDIUM SEA GREEN"),
                                      _T("VIOLET RED"),
                                      _T("YELLOW")};

  m_ColourName = sightcolornames[s_lastsightcolor].Lower();
  m_Colour = wxColour(m_ColourName);
  if (m_Colour.IsOk())
    m_Colour.Set(m_Colour.Red(), m_Colour.Green(), m_Colour.Blue(), 150);
  else
    m_Colour.Set(25, 25, 112, 150);  // headless unit-test fallback

  if (++s_lastsightcolor ==
      (sizeof sightcolornames) / (sizeof *sightcolornames))
    s_lastsightcolor = 0;
  m_bCalculated = false;
  m_bSelected = false;
}

Sight::~Sight() {}

void Sight::SetVisible(bool visible) { m_bVisible = visible; }
void Sight::SetSelected(bool selected) { m_bSelected = selected; }

#include "astrolabe/astrolabe.hpp"

using namespace astrolabe;

using namespace astrolabe::calendar;
using namespace astrolabe::constants;
using namespace astrolabe::dynamical;
using namespace astrolabe::elp2000;
using namespace astrolabe::nutation;
using namespace astrolabe::sun;
using namespace astrolabe::vsop87d;
using astrolabe::util::ecl_to_equ;

/* calculate what position the body for this sight is directly over at a given
 * time */
void Sight::BodyLocation(wxDateTime time, double* lat, double* lon,
                         double* ghaast, double* rad, double* dist,
                         bool timeIsInstant) {
  astrolabe::globals::vsop87d_text_path = celestial_navigation_pi_DataDir();
  astrolabe::globals::vsop87d_text_path.append("/data/");
  astrolabe::globals::vsop87d_text_path.append("vsop87d.txt");

  if (!timeIsInstant) time.MakeFromUTC();
  double jdu = time.GetJulianDayNumber();
  // julian day dynamic
  double jdd = ut_to_dt(jdu);

  double l, b, r;
  double ra, dec, dra = 0., ddec = 0., radvel = 0., parallax = 0.;
  // nutation in longitude
  const double deltaPsi = nut_in_lon(jdd);

  // apparent obliquity
  const double eps = obliquity(jdd) + nut_in_obl(jdd);

  try {
    Sun sun;
    sun.dimension3(jdd, l, b, r);
  } catch (Error const& e) {
    static bool showonce = false;
    if (!showonce) {
      wxString err;
      const char* what = e.what();
      while (*what) err += *what++;
      wxMessageDialog mdlg(NULL,
                           _("Astrolab failed, data unavailable:\n") + err +
                               _("\nDid you forget to install vsop87d.txt?\n") +
                               _("The plugin will not work correctly"),
                           wxString(_("Failure Alert"), wxOK | wxICON_ERROR));
      mdlg.ShowModal();
      showonce = true;
    }
    return;
  }

  // correct vsop coordinates
  vsop_to_fk5(jdd, l, b);

  // nutation in longitude
  l += deltaPsi;

  // aberration
  l += aberration_low(r);

  if (!m_Body.Cmp(_T("Sun"))) {
    m_IsStar = false;
    m_IsPlanet = false;

    // equatorial coordinates
    ecl_to_equ(l, b, eps, ra, dec);

  } else if (!m_Body.Cmp(_T("Moon"))) {
    m_IsStar = false;
    m_IsPlanet = false;
    ELP2000 moon;
    moon.dimension3(jdd, l, b, r);

    // nutation in longitude
    l += deltaPsi;

    // equatorial coordinates
    ecl_to_equ(l, b, eps, ra, dec);
  } else {
    m_IsStar = false;
    m_IsPlanet = true;
    vPlanets planet;
    vPlanets* planetPtr = &planet;
    if (!m_Body.Cmp(_T("Mercury")))
      planet = vMercury;
    else if (!m_Body.Cmp(_T("Venus")))
      planet = vVenus;
    else if (!m_Body.Cmp(_T("Mars")))
      planet = vMars;
    else if (!m_Body.Cmp(_T("Jupiter")))
      planet = vJupiter;
    else if (!m_Body.Cmp(_T("Saturn")))
      planet = vSaturn;
    else { /* star maybe */
      m_IsStar = true;
      m_IsPlanet = false;
      planetPtr = NULL;
/* Numbers from http://simbad.u-strasbg.fr */
#define IFDEC_STAR(name, rahh, ramm, rass, drax, decdd, decmm, decss, ddecx, \
                   radvelx, parallaxx)                                       \
  if (!m_Body.Cmp(_T(name)))                                                 \
    ra = (rahh + (ramm + rass / 60.) / 60.) / 12. * pi, dra = drax,          \
    dec = (decdd > 0 ? 1. : -1.) *                                           \
          (abs(decdd) + (decmm + decss / 60.) / 60.) / 180. * pi,            \
    ddec = ddecx, radvel = radvelx, parallax = parallaxx;
      IFDEC_STAR("Alpheratz", 0, 8, 23.25988, 137.46, 29, 5, 25.5520, -163.44,
                 -10.10, 33.62)
      else IFDEC_STAR("Ankaa",0,26,17.05140,233.05,-42,18,21.55,-356.30,74.6,38.5)
      else IFDEC_STAR("Schedar",0,40,30.44107,50.88,56,32,14.3922,-32.13,-4.31,14.29)
      else IFDEC_STAR("Diphda",0,43,35.37090,232.55,-17,59,11.7827,31.99,13.32,33.86)
      else IFDEC_STAR("Achernar",1,37,42.84548,87.00,-57,14,12.31,-38.24,18.60,23.39)
      else IFDEC_STAR("Hamal",2,7,10.40570,188.55,23,27,44.7032,-148.08,-14.64,49.56)
      else IFDEC_STAR("Polaris",2,31,49.09456,44.48,89,15,50.7923,-11.85,-16.42,7.54)
      else IFDEC_STAR("Acamar",2,58,15.696,-44.6,-40,18,16.97,19.0,11.9,28.00)
      else IFDEC_STAR("Menkar",3,2,16.77307,-10.41,4,5,23.0596,-76.85,-26.08,13.09)
      else IFDEC_STAR("Mirfak",3,24,19.37009,23.75,49,51,40.2455,-26.23,-2.04,6.44)
      else IFDEC_STAR("Aldebaran",4,35,55.23907,63.45,16,30,33.4885,-188.94,54.26,48.94)
      else IFDEC_STAR("Rigel",5,14,32.27210,1.31,-8,12,5.8981,0.50,17.80,3.78)
      else IFDEC_STAR("Capella",5,16,41.35871,75.25,45,59,52.7693,-426.89,29.19,76.2)
      else IFDEC_STAR("Bellatrix",5,25,7.86325,-8.11,6,20,58.9318,-12.88,18.2,12.92)
      else IFDEC_STAR("Elnath",5,26,17.51312,22.76,28,36,26.8262,-173.58,9.2,24.36)
      else IFDEC_STAR("Alnilam",5,36,12.81335,1.44,-1,12,6.9089,-0.78,27.30,1.65)
      else IFDEC_STAR("Betelgeuse",5,55,10.30536,27.54,7,24,25.4304,11.30,21.91,6.55)
      else IFDEC_STAR("Canopus",6,23,57.10988,19.93,-52,41,44.3810,23.24,20.30,10.55)
      else IFDEC_STAR("Sirius",6,45,8.91728,-546.01,-16,42,58.0171,-1223.07,-5.50,379.21)
      else IFDEC_STAR("Adhara",6,58,37.54876,3.24,-28,58,19.5102,1.33,27.30,8.05)
      else IFDEC_STAR("Procyon",7,39,18.11950,-714.59,5,13,29.9552,-1036.80,-3.2,284.56)
      else IFDEC_STAR("Pollux",7,45,18.94987,-626.55,28,1,34.3160,-45.80,3.23,96.54)
      else IFDEC_STAR("Avior",8,22,30.83526,-25.52,-59,30,34.1431,22.06,11.60,5.39)
      else IFDEC_STAR("Suhail",9,7,59.75787,-24.01,-43,25,57.3273,13.52,17.60,5.99)
      else IFDEC_STAR("Miaplacidus",9,13,11.97746,-156.47,-69,43,1.9473,108.95,-5.10,28.82)
      else IFDEC_STAR("Alphard",9,27,35.24270,-15.23,-8,39,30.9583,34.37,-4.27,18.09)
      else IFDEC_STAR("Regulus",10,8,22.31099,-248.73,11,58,1.9516,5.59,5.9,41.13)
      else IFDEC_STAR("Dubhe",11,3,43.67152,-134.11,61,45,3.7249,-34.70,-9.40,26.54)
      else IFDEC_STAR("Denebola",11,49,3.57834,-497.68,14,34,19.4090,-114.67,-0.20,90.91)
      else IFDEC_STAR("Gienah",12,15,48.37081,-158.61,-17,32,30.9496,21.86,-4.2,21.23)
      else IFDEC_STAR("Acrux",12,26,35.871,-35.3,-63,5,56.58,-12.0,-11.2,0.0)
      else IFDEC_STAR("Gacrux",12,31,9.95961,28.23,-57,6,47.5684,-265.08,21.00,36.83)
      else IFDEC_STAR("Alioth",12,54,1.74959,111.91,55,57,35.3627,-8.24,-12.70,39.51)
      else IFDEC_STAR("Spica",13,25,11.57937,-42.35,-11,9,40.7501,-30.67,1.0,13.06)
      else IFDEC_STAR("Alkaid",13,47,32.43776,-121.17,49,18,47.7602,-14.91,-13.40,31.38)
      else IFDEC_STAR("Hadar",14,3,49.40535,-33.27,-60,22,22.9266,-23.16,5.90,8.32)
      else IFDEC_STAR("Menkent",14,6,40.94752,-520.53,-36,22,11.8371,-518.06,1.30,55.45)
      else IFDEC_STAR("Arcturus",14,15,39.67207,-1093.39,19,10,56.6730,-2000.06,-5.19,88.83)
      else IFDEC_STAR("Rigil",14,39,36.49400,-3679.25,-60,50,2.3737,473.67,-21.40,754.81)
      else IFDEC_STAR("Zubenelgenubi",14,50,52.71309,-105.68,-16,2,30.3955,-68.40,-10.,43.03)
      else IFDEC_STAR("Kochab",14,50,42.32580,-32.61,74,9,19.8142,11.42,16.96,24.91)
      else IFDEC_STAR("Alphecca",15,34,41.26800,120.27,26,42,52.8940,-89.58,1.7,43.46)
      else IFDEC_STAR("Antares",16,29,24.45970,-12.11,-26,25,55.2094,-23.30,-3.50,5.89)
      else IFDEC_STAR("Atria",16,48,39.89508,17.99,-69,01,39.7626,-31.58,-3.00,8.35)
      else IFDEC_STAR("Sabik",17,10,22.68689,40.13,-15,43,29.6639,99.17,-2.40,36.91)
      else IFDEC_STAR("Shaula",17,33,36.52012,-8.53,-37,6,13.7648,-30.80,-3.00,5.71)
      else IFDEC_STAR("Rasalhague",17,34,56.06945,108.07,12,33,36.1346,-221.57,11.70,67.13)
      else IFDEC_STAR("Eltanin",17,56,36.36988,-8.48,51,29,20.0242,-22.79,-27.91,21.14)
      else IFDEC_STAR("Kaus Australis",18,24,10.31840,-39.42,-34,23,4.6193,-124.20,-15.00,22.76)
      else IFDEC_STAR("Vega",18,36,56.33635,200.94,38,47,1.2802,286.23,-20.60,130.23)
      else IFDEC_STAR("Nunki",18,55,15.92650,15.14,-26,17,48.2068,-53.43,-11.2,14.32)
      else IFDEC_STAR("Altair",19,50,46.99855,536.23,8,52,5.9563,385.29,-26.60,194.95)
      else IFDEC_STAR("Peacock",20,25,38.85705,6.90,-56,44,6.3230,-86.02,2.0,18.24)
      else IFDEC_STAR("Deneb",20,41,25.91514,2.01,45,16,49.2197,1.85,-4.90,2.31)
      else IFDEC_STAR("Enif",21,44,11.15614,26.92,9,52,30.0311,0.44,3.39,4.73)
      else IFDEC_STAR("Al Na'ir",22,8,13.98473,126.69,-46,57,39.5078,-147.47,10.90,32.29)
      else IFDEC_STAR("Fomalhaut",22,57,39.04625,328.95,-29,37,20.0533,-164.67,6.50,129.81)
      else IFDEC_STAR("Scheat",23,3,46.45746,187.65,28,4,58.0336,136.93,7.99,16.64)
      else IFDEC_STAR("Markab",23,4,45.65345,60.40,15,12,18.9617,-41.30,-2.70,24.46)
      else {
        wxString s;
        s.Printf(_T ( "Unknown celestial body: " ) + m_Body);
        wxLogMessage(s);
      }
      proper_motion_parallax(jdd, ra, dec, dra, ddec, radvel, parallax);
      frame_bias(ra, dec);
      precess(jdd, ra, dec);
      nutate(jdd, ra, dec);
    }
    if (planetPtr != NULL) {
      double d;
      geocentric_planet(jdd, planet, deltaPsi, eps, days_per_second, ra, dec,
                        d);
      if (dist) {
        *dist = d;
      }
    }
  }

  // account for earth's hour angle

  double gmst = sidereal_time_greenwich(jdu);
  double eoe = deltaPsi * cos(eps);
  double gast = gmst + eoe;
  ra = ra - gast;

  if (lat) *lat = r_to_d(dec);
  if (lon) *lon = r_to_d(ra);
  if (ghaast) *ghaast = r_to_d(gast);
  if (rad) *rad = r;
}

std::list<wxRealPoint> Sight::GetPoints() {
  std::list<wxRealPoint> points;
  for (std::list<wxRealPointList*>::iterator it = polygons.begin();
       it != polygons.end(); it++)
    for (wxRealPointList::iterator it2 = (*it)->begin(); it2 != (*it)->end();
         it2++)
      points.push_back(**it2);
  return points;
}

/* Combine two lists of points by appending p2 to p1 */
wxRealPointList* Sight::MergePoints(wxRealPointList* p1, wxRealPointList* p2) {
  /* combine lists of points */
  wxRealPointList* p = new wxRealPointList;
  wxRealPointList::iterator it;
  for (it = p1->begin(); it != p1->end(); ++it)
    p->Append(new wxRealPoint(**it));
  for (it = p2->begin(); it != p2->end(); ++it)
    p->Append(new wxRealPoint(**it));
  return p;
}

/* give the angle between two points from 0 to 2 PI */
double Sight::CalcAngle(wxRealPoint p1, wxRealPoint p2) {
  /* rectangular coords */
  double phi = atan2(p1.y - p2.y, p1.x - p2.x);
  if (phi < 0) phi += 2 * pi;
  return phi;
}

/* take a list of points, and return a list of points
   which form a convex polygon which encompasses all the points with vertices at
   points. */
wxRealPointList* Sight::ReduceToConvexPolygon(wxRealPointList* points) {
  wxRealPointList* polygon = new wxRealPointList;
  wxRealPointList::iterator it, min;
  /* get min y point to start out at */
  for (min = it = points->begin(); it != points->end(); ++it)
    if ((*it)->y < (*min)->y) min = it;

  double theta = 0;
  while (!points->IsEmpty()) {
    polygon->Append(*min);
    points->DeleteObject(*min);

    /* delete duplicates (optimization) */
    it = points->begin();
    while (it != points->end())
      if (**it == *polygon->back()) {
        wxRealPointList::iterator l = it;
        ++it;
        points->DeleteObject(*l);
      } else
        ++it;

    double minphi = 2 * pi, maxdist = 0;
    for (min = it = points->begin(); it != points->end(); ++it) {
      double phi = CalcAngle(**it, *polygon->back());
      double dist =
          hypot((*it)->x - polygon->back()->x, (*it)->y - polygon->back()->y);
      if (maxdist == 0) maxdist = dist;

      if ((phi >= theta && phi < minphi) || (phi == minphi && dist > maxdist)) {
        min = it;
        minphi = phi;
        maxdist = dist;
      }
    }

    if (polygon->size() > 1 &&
        CalcAngle(*polygon->front(), *polygon->back()) < minphi)
      break;

    theta = minphi;
  }

  return polygon;
}

/* Draw a polygon or polyline (specified in lat/lon coords) to dc given a list
 * of points */
void Sight::DrawPolygon(PlugIn_ViewPort& VP, wxRealPointList& area, bool poly) {
  int n = area.size();
  wxPoint* ppoints = new wxPoint[n];
  bool rear1 = false, rear2 = false;
  wxRealPointList::iterator it = area.begin();

  double minx = 1000;
  double maxx = -1000;
  double miny = 1000;
  double maxy = -1000;

  for (int i = 0; i < n && it != area.end(); i++, it++) {
    wxPoint r;

    /* don't draw areas crossing opposite from center longitude */
    double lon = (*it)->y - VP.clon;
    lon = resolve_heading_positive(lon);

    if (lon > 90 && lon <= 180) rear1 = true;
    if (lon > 180 && lon < 270) rear2 = true;

    (*it)->y = resolve_heading((*it)->y);

    minx = wxMin(minx, (*it)->x);
    miny = wxMin(miny, (*it)->y);
    maxx = wxMax(maxx, (*it)->x);
    maxy = wxMax(maxy, (*it)->y);

    GetCanvasPixLL(&VP, &r, (*it)->x, (*it)->y);

    ppoints[i] = r;
  }

  if (!(rear1 && rear2)) {
    if (poly) {
      m_dc->DrawPolygon(n, ppoints);
    } else {
#if USE_ANDROID_GLES2
      for (int i = 0; i < n - 1; i++)
        m_dc->DrawLine(ppoints[i].x, ppoints[i].y, ppoints[i + 1].x,
                       ppoints[i + 1].y);
#else
      m_dc->DrawLines(n, ppoints);
#endif
    }
  }

  delete[] ppoints;
}

/* Compute trace areas for one dimension, given center certainty, and constant
 */
double Sight::ComputeStepSize(double certainty, double stepsize, double min,
                              double max) {
  return (max - min) / (floor(certainty / stepsize) + 1);
}

/* render the area of position for this sight */
void Sight::Render(piDC* dc, PlugIn_ViewPort& VP, double pix_per_mm) {
  if (!m_bVisible) return;

  m_dc = dc;

  dc->SetPen(wxPen(m_Colour, 0, wxPENSTYLE_TRANSPARENT));
  dc->SetBrush(wxBrush(m_Colour));

  std::list<wxRealPointList*>::iterator it = polygons.begin();
  while (it != polygons.end()) {
    DrawPolygon(VP, **it, true);
    ++it;
  }

  dc->SetPen(wxPen(m_Colour, (int)(0.5 * pix_per_mm)));
  DrawPolygon(VP, lines, false);

  if (m_Type == HORIZON && m_HorizonEstimateValid) {
    wxPoint centre;
    GetCanvasPixLL(&VP, &centre, m_HorizonEstimateLat, m_HorizonEstimateLon);
    const int marker = wxMax(4, static_cast<int>(1.5 * pix_per_mm));
    dc->SetPen(wxPen(m_Colour, wxMax(1, static_cast<int>(0.7 * pix_per_mm))));
    dc->StrokeLine(centre.x - marker, centre.y, centre.x + marker, centre.y);
    dc->StrokeLine(centre.x, centre.y - marker, centre.x, centre.y + marker);
    dc->StrokeCircle(centre.x, centre.y, marker + 2);
  }
}

void Sight::Recompute(int clock_offset) {
  m_CalcStr.clear();

  if (clock_offset)
    m_CalcStr += wxString::Format(
        _("Applying clock correction of %d seconds\n\n"), clock_offset);

  m_CorrectedDateTime = UtcDateTime::AddSeconds(m_DateTime, clock_offset);

  switch (m_Type) {
    case ALTITUDE:
      RecomputeAltitude();
      break;
    case AZIMUTH:
      RecomputeAzimuth();
      break;
    case LUNAR:
      RecomputeLunar();
      break;
    case HORIZON:
      RecomputeHorizon();
      break;
  }
}

void Sight::RebuildPolygons() {
  switch (m_Type) {
    case ALTITUDE:
      RebuildPolygonsAltitude();
      break;
    case AZIMUTH:
      RebuildPolygonsAzimuth();
      break;
    case LUNAR:
      return;  // lunar has no polygons
    case HORIZON:
      RebuildPolygonsHorizon();
      break;
  }

  /* now shift the vertices as needed */
  for (std::list<wxRealPointList*>::iterator it = polygons.begin();
       it != polygons.end(); it++) {
    wxRealPointList* area = *it;
    for (wxRealPointList::iterator it2 = area->begin(); it2 != area->end();
         it2++) {
      wxRealPoint* p = *it2;
      double lat = p->x, lon = p->y;

      double localbearing = m_ShiftBearing;
      if (m_bMagneticShiftBearing) {
        lon = resolve_heading(lon);
        localbearing += celestial_navigation_pi_GetWMM(lat, lon, m_EyeHeight,
                                                       m_CorrectedDateTime);
      }
      double localaltitude = 90 - m_ShiftNm / 60;
      *p = DistancePoint(localaltitude, localbearing, lat, lon);
    }
  }

  m_bCalculated = true;
}

wxString Sight::Alminac(wxDateTime time, double lat, double lon, double ghaast,
                        double rad, double SD, double HP) {
  double sha = 360 - lon - ghaast;
  sha = resolve_heading_positive(sha);

  double gha = -lon;
  gha = resolve_heading_positive(gha);

  double dec = lat;

  time.MakeFromUTC();
  double jdu = time.GetJulianDayNumber();
  double jdd = ut_to_dt(jdu);
  double deltaT = deltaT_seconds(jdu);

  return _("Almanac Data For ") + m_Body +
         wxString::Format(_("\n\
Date = %s\n\
JD = %.6f\n\
DeltaT = %.4f\n\
TT = %.6f\n\
Geographical Position (lat, lon) = %.4f%c %.4f%c = %s %s\n\
GHAAST = %.4f%c = %s\n\
SHA = %.4f%c = %s\n\
GHA = %.4f%c = %s\n\
Dec = %.4f%c = %s\n\
SD = %.4f'\n\
HP = %.4f'\n\n"),
                          time.Format("%Y-%m-%d %H:%M:%S", time.UTC), jdu,
                          deltaT, jdd, lat, 0x00B0, lon, 0x00B0,
                          toSDMM_PlugIn(1, lat, true),
                          toSDMM_PlugIn(2, lon, true), ghaast, 0x00B0,
                          toSDMM_PlugIn(0, ghaast, true), sha, 0x00B0,
                          toSDMM_PlugIn(0, sha, true), gha, 0x00B0,
                          toSDMM_PlugIn(0, gha, true), dec, 0x00B0,
                          toSDMM_PlugIn(1, dec, true), SD * 60, HP * 60);
}

void Sight::RecomputeAltitude() {
  double rad;
  double planet_dist;
  BodyLocation(m_CorrectedDateTime, 0, 0, 0, &rad, &planet_dist);

  m_CalcStr += _("Formulas used to calculate sight\n\n");

  m_CalcStr += wxString::Format(
      _("Altitude measurement (Hs) = %.4f%c = %s\n\n"), m_Measurement, 0x00B0,
      toSDMM_PlugIn(0, m_Measurement, true));

  /* correct for index error */
  double IndexCorrection = m_IndexError / 60.0;
  m_CalcStr +=
      wxString::Format(_("Index Error = %.4f%c = %s\n\n"), IndexCorrection,
                       0x00B0, toSDMM_PlugIn(0, IndexCorrection, true));

  double EyeHeightCorrection = 0;
  if (m_ArtificialHorizon) {
    m_CalcStr +=
        wxString::Format(_("Artificial horizon, no height correction\n"));
  } else {
    if (m_DipShort) {
      /* Dip Short sight.
         Bowditch: atan(h/(6076*d)+d/8268), h = HOE in ft, d = n.m. */
      if (m_DipShortDistance == 0) {
        m_CalcStr += wxString::Format(_("Dip Short Distance cannot be 0 !\n"));
        return;
      }
      EyeHeightCorrection =
          r_to_d(atan(m_EyeHeight / (0.3048 * 6076 * m_DipShortDistance) +
                      m_DipShortDistance / 8268));
      m_CalcStr += wxString::Format(
          _("Dip Short Distance = %.4f nm\n\
Eye Height = %.4f m = %.4f ft\n\
Height Correction = atan(Eye Height / (6076 * Dip Short Distance) + Dip Short Distance / 8268)\n\
Height Correction = atan(%.4f / (6076 * %.4f) + %.4f / 8268)\n\
Height Correction = %.4f%c = %s\n"),
          m_DipShortDistance, m_EyeHeight, m_EyeHeight / 0.3048,
          m_EyeHeight / 0.3048, m_DipShortDistance, m_DipShortDistance,
          EyeHeightCorrection, 0x00B0,
          toSDMM_PlugIn(0, EyeHeightCorrection, true));
    } else {
      /* correct for height of observer
         The dip of the sea horizon in minutes = 1.758*sqrt(height) */
      EyeHeightCorrection = 1.758 * sqrt(m_EyeHeight) / 60.0;
      m_CalcStr += wxString::Format(
          _("Eye Height = %.4f m\n\
Height Correction = (1.758 * sqrt(Eye Height)) / 60\n\
Height Correction = 1.758%c * sqrt(%.4f) / 60.0\n\
Height Correction = %.4f%c = %s\n"),
          m_EyeHeight, 0x00B0, m_EyeHeight, EyeHeightCorrection, 0x00B0,
          toSDMM_PlugIn(0, EyeHeightCorrection, true));
    }
  }

  /* Apparent Altitude Ha */
  double ApparentAltitude =
      m_Measurement - IndexCorrection - EyeHeightCorrection;
  m_CalcStr +=
      wxString::Format(_("\nApparent Altitude (Ha)\n\
ApparentAltitude = Hs - IndexCorrection - EyeHeightCorrection\n\
ApparentAltitude = %.4f%c - %.4f%c - %.4f%c\n\
ApparentAltitude = %.4f%c = %s\n"),
                       m_Measurement, 0x00B0, IndexCorrection, 0x00B0,
                       EyeHeightCorrection, 0x00B0, ApparentAltitude, 0x00B0,
                       toSDMM_PlugIn(0, ApparentAltitude, true));

  if (m_ArtificialHorizon) {
    m_CalcStr += wxString::Format(_("\nArtificial horizon\n\
ApparentAltitude = ApparentAltitude / 2\n\
ApparentAltitude = %.4f%c / 2 = %.4f%c\n"),
                                  ApparentAltitude, 0x00B0,
                                  ApparentAltitude / 2, 0x00B0);
    ApparentAltitude /= 2;
  }

  /* Backsight ? */
  if (ApparentAltitude > 90) {
    m_CalcStr +=
        wxString::Format(_("\nApparent Altitude (Ha) > 90, assuming backsight\n\
ApparentAltitude = 180%c - ApparentAltitude\n\
ApparentAltitude = 180%c - %.4f%c = %.4f%c\n"),
                         0x00B0, ApparentAltitude, 0x00B0, 0x00B0,
                         180 - ApparentAltitude, 0x00B0);
    ApparentAltitude = 180 - ApparentAltitude;
  }

  /* compensate for refraction */
  double RefractionCorrection;
#if 0
    /* old correction not used */
    double Ha = m_Measurement - m_EyeHeightCorrection;
    double Ref = 1/tan(d_to_r(Ha + (7.31/(Ha + 4.4))));
    double RefImp = Ref - .06 * sin(d_to_r(14.7*Ref + 13));

    RefractionCorrection = RefImp * .00467 * m_Pressure / (273.15 + m_Temperature);
#else
  double x = tan(d_to_r(ApparentAltitude) +
                 d_to_r(4.848e-2) / (tan(d_to_r(ApparentAltitude)) + .028));
  m_CalcStr += wxString::Format(_("\nRefraction Correction\n\
x = tan(ApparentAltitude + 4.848e-2 / (tan(ApparentAltitude) + .028))\n\
x = tan((%.4f + 4.848e-2) / (tan(%.4f) + .028))\n\
x = %.4f\n"),
                                ApparentAltitude, ApparentAltitude, x);
  RefractionCorrection =
      .267 * m_Pressure / (x * (m_Temperature + 273.15)) / 60.0;
  m_CalcStr += wxString::Format(_("\
RefractionCorrection = .267%c * Pressure / (x * (Temperature + 273.15)) / 60.0\n\
RefractionCorrection = .267%c * %.4f / (x * (%.4f + 273.15)) / 60.0\n\
RefractionCorrection = %.4f%c = %s\n"),
                                0x00B0, 0x00B0, m_Pressure, m_Temperature,
                                RefractionCorrection, 0x00B0,
                                toSDMM_PlugIn(0, RefractionCorrection, true));
#endif

  double SD = 0, topoSD = 0;
  double HP = 0;
  double lc = 0;

  if (!m_Body.Cmp(_T("Sun"))) {
    lc = 0.266564 / rad;
    SD = r_to_d(sin(d_to_r(lc)));
    topoSD = SD;

    m_CalcStr += wxString::Format(_("\nSun selected, Limb Correction\n\
ra = %.4f, lc = 0.266564/ra = %.4f%c = %s\n"),
                                  rad, lc, 0x00B0, toSDMM_PlugIn(0, lc, true));
  }

  if (!m_Body.Cmp(_T("Moon"))) {
    wxDateTime time = m_CorrectedDateTime;
    time.MakeFromUTC();
    double jdu = time.GetJulianDayNumber();
    double jdd = ut_to_dt(jdu);
    double moon_dist = moon_distance(jdd);
    HP = r_to_d(asin(EARTH_RADIUS / moon_dist));
    SD = r_to_d(asin(K_MOON * sin(d_to_r(HP))));
    // convert to topocentric SD, see Meeus (chapter 55)
    topoSD = SD * (1 + sin(d_to_r(ApparentAltitude)) * sin(d_to_r(HP)));
    lc = r_to_d(asin(d_to_r(topoSD)));
    m_CalcStr +=
        wxString::Format(_("\nMoon selected, Limb Correction\n\
SD = %.4f%c = %s\n\
topoSD = SD * (1 + sin(ApparentAltitude) * sin(HP))\n\
topoSD = %.4f\n\
lc = asin(topoSD)\n\
lc = %.4f%c = %s\n"),
                         SD, 0x00B0, toSDMM_PlugIn(0, SD, true), topoSD, lc,
                         0x00B0, toSDMM_PlugIn(0, lc, true));
  }

  double LimbCorrection = 0;
  if (lc) {
    if (m_BodyLimb == UPPER) {
      LimbCorrection = -lc;
      m_CalcStr += wxString::Format(_("Upper Limb"));
    } else if (m_BodyLimb == LOWER) {
      LimbCorrection = lc;
      m_CalcStr += wxString::Format(_("Lower Limb"));
    }

    m_CalcStr +=
        wxString::Format(_("\nLimbCorrection = %.4f%c = %s\n"), LimbCorrection,
                         0x00B0, toSDMM_PlugIn(0, LimbCorrection, true));
  }

  double CorrectedAltitude =
      ApparentAltitude - RefractionCorrection + LimbCorrection;
  m_CalcStr +=
      wxString::Format(_("\nCorrected Altitude (Hc)\n\
CorrectedAltitude = ApparentAltitude - RefractionCorrection + LimbCorrection\n\
CorrectedAltitude = %.4f%c - %.4f%c + %.4f%c\n\
CorrectedAltitude = %.4f%c = %s\n"),
                       ApparentAltitude, 0x00B0, RefractionCorrection, 0x00B0,
                       LimbCorrection, 0x00B0, CorrectedAltitude, 0x00B0,
                       toSDMM_PlugIn(0, CorrectedAltitude, true));

  /* correct for parallax shot */
  double ParallaxCorrection = 0;
  if (!m_Body.Cmp(_T("Sun"))) {
    HP = 0.002442 / rad;

    m_CalcStr += wxString::Format(_("\nSun selected, parallax correction\n\
rad = %.4f, HP = 0.002442/rad = %.4f%c = %s\n"),
                                  rad, HP, 0x00B0, toSDMM_PlugIn(0, HP, true));
  }

  if (!m_Body.Cmp(_T("Moon"))) {
    // HP calculated earlier
    m_CalcStr += wxString::Format(_("\nMoon selected, parallax correction\n\
HP = %.4f%c = %s\n"),
                                  HP, 0x00B0, toSDMM_PlugIn(0, HP, true));
  }

  if (m_IsPlanet) {
    HP = r_to_d(asin(EARTH_RADIUS / planet_dist));
    m_CalcStr += wxString::Format(_("\nPlanet selected, parallax correction\n\
HP = %.4f%c = %s\n"),
                                  HP, 0x00B0, toSDMM_PlugIn(0, HP, true));
  }

  if (HP) {
    ParallaxCorrection =
        r_to_d(asin(sin(d_to_r(HP)) * cos(d_to_r(CorrectedAltitude))));
    m_CalcStr +=
        wxString::Format(_("\
ParallaxCorrection = asin(sin(HP) * cos(CorrectedAltitude))\n\
ParallaxCorrection = asin(sin(%.4f) * cos(%.4f))\n\
ParallaxCorrection = %.4f%c = %s\n"),
                         HP, CorrectedAltitude, ParallaxCorrection, 0x00B0,
                         toSDMM_PlugIn(0, ParallaxCorrection, true));
  }

  m_ObservedAltitude = CorrectedAltitude + ParallaxCorrection;
  m_CalcStr += wxString::Format(_("\nObserved Altitude (Ho)\n\
ObservedAltitude = CorrectedAltitude + ParallaxCorrection\n\
ObservedAltitude = %.4f%c + %.4f%c\n\
ObservedAltitude = %.4f%c = %s\n"),
                                CorrectedAltitude, 0x00B0, ParallaxCorrection,
                                0x00B0, m_ObservedAltitude, 0x00B0,
                                toSDMM_PlugIn(0, m_ObservedAltitude, true));

  double lat, lon, ghaast;
  BodyLocation(m_CorrectedDateTime, &lat, &lon, &ghaast, &rad, 0);

  m_CalcStr =
      Alminac(m_CorrectedDateTime, lat, lon, ghaast, rad, SD, HP) + m_CalcStr;
}

void Sight::RecomputeAzimuth() {
  m_Measurement = resolve_heading_positive(m_Measurement);
  if (!std::isfinite(m_MeasurementCertainty) || m_MeasurementCertainty < 0.0) {
    m_CalcStr = _("Azimuth uncertainty must be zero or positive.\n");
    m_bCalculated = false;
    return;
  }
  double bodyLat = 0.0, bodyLon = 0.0, ghaast = 0.0, radius = 0.0;
  BodyLocation(m_CorrectedDateTime, &bodyLat, &bodyLon, &ghaast, &radius,
               nullptr);
  m_CalcStr = wxString::Format(
      _("Celestial azimuth line of position\n\n"
        "Body: %s\nUTC: %s\nObserved bearing: %.4f%c %s\n"
        "Bearing uncertainty: %.3f arcmin\n\n"),
      m_Body, UtcDateTime::FormatUtc(m_CorrectedDateTime,
                                     "%Y-%m-%d %H:%M:%S"),
      m_Measurement, 0x00B0,
      m_bMagneticNorth ? _T("magnetic") : _T("true"),
      m_MeasurementCertainty);
  if (m_bMagneticNorth)
    m_CalcStr += _(
        "The chart locus converts magnetic north to true north with the "
        "offline WMM at each trial position. Enter a magnetic bearing with "
        "compass deviation already removed; a raw compass bearing must first "
        "be corrected for deviation.\n");
  else
    m_CalcStr += _("No magnetic correction is applied to this true bearing.\n");
  m_CalcStr += _(
      "This sight type is a bearing to a celestial body. It is not a "
      "horizontal sextant angle between two terrestrial objects; use Coastal "
      "Sextant for that method.\n\n");
  m_CalcStr = Alminac(m_CorrectedDateTime, bodyLat, bodyLon, ghaast, radius,
                      0.0, 0.0) +
              m_CalcStr;
  m_bCalculated = true;
}

double Sight::HorizonTrueBearing() const {
  double bearing = m_HorizonBearing;
  if (m_HorizonBearingMagnetic)
    bearing += m_HorizonVariation + m_HorizonDeviation;
  return resolve_heading_positive(bearing);
}

wxString Sight::HorizonEventName() const {
  return m_HorizonEvent == SUNRISE ? _("Sunrise") : _("Sunset");
}

wxString Sight::HorizonMeasurementText() const {
  if (!m_HorizonBearingProvided) return _("Time only");
  if (m_HorizonBearingMagnetic)
    return wxString::Format(_T("%.1f%c M -> %.1f%c T"), m_HorizonBearing,
                            0x00B0, HorizonTrueBearing(), 0x00B0);
  return wxString::Format(_T("%.1f%c T"), HorizonTrueBearing(), 0x00B0);
}

void Sight::RecomputeHorizon() {
  m_Body = _T("Sun");
  m_bMagneticNorth = false;  // horizon corrections are explicitly applied
  m_Measurement = HorizonTrueBearing();
  m_HorizonEstimateValid = false;

  double rad = 1.0;
  BodyLocation(m_CorrectedDateTime, 0, 0, 0, &rad, 0);

  const double dip = m_EyeHeight > 0 ? 1.758 * sqrt(m_EyeHeight) / 60.0 : 0.0;
  const double refraction = (34.0 / 60.0) * (m_Pressure / 1010.0) *
                            (283.15 / (273.15 + m_Temperature));
  const double semidiameter = 0.266564 / rad;
  const double horizontalParallax = 0.002442 / rad;

  // At first/last upper-limb contact the sextant altitude is effectively 0.
  // Convert that event to the geocentric centre altitude used by the LOP code.
  m_ObservedAltitude = -dip - refraction - semidiameter + horizontalParallax;

  m_CalcStr += wxString::Format(
      _("Horizon Event: %s\n"
        "Date = %s UTC\n"
        "Time source = %s\n\n"
        "Upper limb at the visible horizon\n"
        "Dip = %.4f%c\n"
        "Refraction = %.4f%c\n"
        "Sun semidiameter = %.4f%c\n"
        "Horizontal parallax = %.4f%c\n"
        "Geocentric centre altitude = %.4f%c\n\n"),
      HorizonEventName(),
      UtcDateTime::FormatUtc(m_CorrectedDateTime,
                             "%Y-%m-%d %H:%M:%S"),
      m_HorizonTimeSource, dip, 0x00B0, refraction, 0x00B0, semidiameter,
      0x00B0, horizontalParallax, 0x00B0, m_ObservedAltitude, 0x00B0);

  if (m_HorizonBearingProvided) {
    m_CalcStr += wxString::Format(
        _("Observed bearing = %.2f%c %s\n"
          "Variation = %+.2f%c (east positive)\n"
          "Deviation = %+.2f%c (east positive)\n"
          "True bearing = %.2f%c\n"),
        m_HorizonBearing, 0x00B0,
        m_HorizonBearingMagnetic ? _("magnetic") : _("true"),
        m_HorizonBearingMagnetic ? m_HorizonVariation : 0.0, 0x00B0,
        m_HorizonBearingMagnetic ? m_HorizonDeviation : 0.0, 0x00B0,
        HorizonTrueBearing(), 0x00B0);
  } else {
    m_CalcStr +=
        _("No bearing supplied: this event produces an altitude "
          "line of position only.\n");
  }
}

bool Sight::HorizonEstimatedPosition(double* lat, double* lon) {
  if (m_Type != HORIZON || !m_HorizonBearingProvided || !lat || !lon)
    return false;

  double bodyLat, bodyLon;
  BodyLocation(m_CorrectedDateTime, &bodyLat, &bodyLon, 0, 0, 0);
  const double target = HorizonTrueBearing();

  double bestTrace = 0;
  double bestError = 361;
  for (double trace = -180; trace < 180; trace += 1.0) {
    const wxRealPoint point =
        DistancePoint(m_ObservedAltitude, trace, bodyLat, bodyLon);
    double altitude, bearing;
    AltitudeAzimuth(point.x, point.y, bodyLat, bodyLon, &altitude, &bearing);
    const double error = fabs(resolve_heading(bearing - target));
    if (error < bestError) {
      bestError = error;
      bestTrace = trace;
    }
  }

  double step = 0.5;
  for (int iteration = 0; iteration < 24; ++iteration) {
    double chosenTrace = bestTrace;
    for (int direction = -1; direction <= 1; direction += 2) {
      const double trace = bestTrace + direction * step;
      const wxRealPoint point =
          DistancePoint(m_ObservedAltitude, trace, bodyLat, bodyLon);
      double altitude, bearing;
      AltitudeAzimuth(point.x, point.y, bodyLat, bodyLon, &altitude, &bearing);
      const double error = fabs(resolve_heading(bearing - target));
      if (error < bestError) {
        bestError = error;
        chosenTrace = trace;
      }
    }
    bestTrace = chosenTrace;
    step *= 0.5;
  }

  const wxRealPoint estimate =
      DistancePoint(m_ObservedAltitude, bestTrace, bodyLat, bodyLon);
  *lat = estimate.x;
  *lon = estimate.y;
  if (*lon > 180) *lon -= 360;
  if (*lon < -180) *lon += 360;
  return bestError < 0.05;
}

double Sight::HorizonEstimateUncertaintyNm() const {
  if (!m_HorizonBearingProvided) return NAN;
  const double angularDistance = d_to_r(90.0 - m_ObservedAltitude);
  const double crossTrack =
      60.0 * fabs(sin(angularDistance)) * m_HorizonBearingUncertainty;
  const double radial = m_HorizonAltitudeUncertainty;
  const double timing = 0.25 * m_TimeCertainty;
  return sqrt(crossTrack * crossTrack + radial * radial + timing * timing);
}

void Sight::RecomputeLunar() {
  // A lunar recovers Greenwich time by clearing the observed limb distance
  // of dip, refraction, semidiameter and parallax, then matching the resulting
  // geocentric centre distance against the ephemeris.  The former code only
  // evaluated two endpoints and linearly extrapolated between them; it could
  // silently return a time even when no root existed or several roots existed.
  lunar_distance::Observation observation;
  observation.raw_distance_deg = m_Measurement;
  observation.moon_altitude_deg = m_LunarMoonAltitude;
  observation.body_altitude_deg = m_LunarBodyAltitude;
  auto altitude_limb = [](BodyLimb limb) {
    if (limb == LOWER) return lunar_distance::AltitudeLimb::Lower;
    if (limb == UPPER) return lunar_distance::AltitudeLimb::Upper;
    return lunar_distance::AltitudeLimb::Center;
  };
  auto body_distance_contact = [](BodyLimb limb) {
    if (limb == LUNAR_NEAR) return lunar_distance::DistanceContact::Near;
    if (limb == UPPER) return lunar_distance::DistanceContact::Far;
    return lunar_distance::DistanceContact::Center;
  };
  observation.moon_altitude_limb = altitude_limb(m_LunarMoonLimb);
  observation.body_altitude_limb = altitude_limb(m_LunarBodyLimb);
  observation.moon_contact = m_BodyLimb == LUNAR_NEAR
                                 ? lunar_distance::DistanceContact::Near
                                 : lunar_distance::DistanceContact::Far;
  observation.body_contact =
      !m_Body.Cmp(_T("Sun"))
          ? body_distance_contact(m_LunarBodyDistanceLimb)
          : lunar_distance::DistanceContact::Center;
  observation.index_error_arcmin = m_IndexError;
  observation.eye_height_m = m_EyeHeight;
  observation.pressure_hpa = m_Pressure;
  observation.temperature_c = m_Temperature;
  observation.artificial_horizon = m_ArtificialHorizon;
  observation.dip_short = m_DipShort;
  observation.dip_short_distance_m = m_DipShortDistance;
  observation.distance_uncertainty_arcmin =
      std::max(0.0, m_MeasurementCertainty);
  observation.moon_altitude_uncertainty_arcmin =
      std::max(0.0, m_LunarMoonAltitudeUncertainty);
  observation.body_altitude_uncertainty_arcmin =
      std::max(0.0, m_LunarBodyAltitudeUncertainty);

  const wxString selected_body = m_Body;
  m_LunarUsesDe440 = false;
  auto ephemeris = [this, selected_body](
                       double offset_seconds,
                       lunar_distance::EphemerisSample* sample,
                       std::string* error) {
    const wxDateTime time =
        UtcDateTime::AddSeconds(m_CorrectedDateTime, offset_seconds);
    if (!time.IsValid()) {
      if (error) *error = "The candidate UTC is outside the supported range";
      return false;
    }

    double body_dec = 0.0, body_hour_angle = 0.0, body_rad = 0.0;
    double body_distance = 0.0;
    m_Body = selected_body;
    BodyLocation(time, &body_dec, &body_hour_angle, nullptr, &body_rad,
                 &body_distance);
    const bool body_is_planet = m_IsPlanet;
    const bool body_is_star = m_IsStar;

    double moon_dec = 0.0, moon_hour_angle = 0.0, moon_rad = 0.0;
    m_Body = _T("Moon");
    BodyLocation(time, &moon_dec, &moon_hour_angle, nullptr, &moon_rad,
                 nullptr);
    m_Body = selected_body;

    const double delta_hour_angle =
        resolve_heading(moon_hour_angle - body_hour_angle);
    const double cosine =
        sin(d_to_r(moon_dec)) * sin(d_to_r(body_dec)) +
        cos(d_to_r(moon_dec)) * cos(d_to_r(body_dec)) *
            cos(d_to_r(delta_hour_angle));
    sample->predicted_distance_deg =
        r_to_d(acos(std::max(-1.0, std::min(1.0, cosine))));
    sample->body_geographic_latitude_deg = body_dec;
    sample->body_geographic_longitude_deg = body_hour_angle;
    sample->moon_geographic_latitude_deg = moon_dec;
    sample->moon_geographic_longitude_deg = moon_hour_angle;
    bool used_de440 = false;

    if (!selected_body.Cmp(_T("Sun"))) {
      static eclipse::SpkKernel kernel;
      static wxString opened_path;
      static bool attempted = false;
#ifdef UNIT_TESTS
      // wxStandardPaths requires a running wxApp.  The sight tests are
      // deliberately headless, so exercise the same kernel against the
      // source-tree test fixture instead of consulting the GUI profile.
      const wxString path = wxString::FromUTF8(ECLIPSE_DE440_TEST_PATH);
#else
      const wxString path = celestial_navigation_pi::StandardPath() +
                            _T("eclipse") + wxFileName::GetPathSeparator() +
                            _T("de440s.bsp");
#endif
      if (!attempted || opened_path != path) {
        attempted = true;
        opened_path = path;
        std::string open_error;
        kernel.Open(path.ToStdString(), &open_error);
      }
      if (kernel.IsOpen()) {
        eclipse::CalendarDateTime utc;
        utc.year = time.GetYear();
        utc.month = static_cast<int>(time.GetMonth()) + 1;
        utc.day = time.GetDay();
        utc.hour = time.GetHour();
        utc.minute = time.GetMinute();
        utc.second = time.GetSecond() + time.GetMillisecond() / 1000.0;
        double utc_jd = 0.0;
        std::string time_error;
        if (eclipse::CalendarToJulianDate(utc, &utc_jd, &time_error)) {
          double tai_minus_utc = eclipse::TaiMinusUtcSeconds(utc);
          if (!std::isfinite(tai_minus_utc)) tai_minus_utc = 37.0;
          const double tt_jd = utc_jd + (tai_minus_utc + 32.184) / 86400.0;
          const double tdb_jd =
              tt_jd + eclipse::TdbMinusTtSeconds(tt_jd, utc_jd) / 86400.0;
          const double et = (tdb_jd - 2451545.0) * 86400.0;
          eclipse::Vector3 moon_vector;
          eclipse::Vector3 sun_vector;
          std::string de_error;
          if (eclipse::AstrometricPosition(kernel, 301, 399, et,
                                           &moon_vector, &de_error) &&
              eclipse::AstrometricPosition(kernel, 10, 399, et, &sun_vector,
                                           &de_error)) {
            const double denominator = moon_vector.Norm() * sun_vector.Norm();
            if (denominator > 0.0) {
              sample->predicted_distance_deg = r_to_d(acos(std::max(
                  -1.0, std::min(1.0, eclipse::Dot(moon_vector, sun_vector) /
                                           denominator))));
              sample->moon_horizontal_parallax_deg =
                  r_to_d(asin(EARTH_RADIUS / moon_vector.Norm()));
              sample->moon_semidiameter_deg =
                  r_to_d(asin(1737.4 / moon_vector.Norm()));
              sample->body_horizontal_parallax_deg =
                  r_to_d(asin(EARTH_RADIUS / sun_vector.Norm()));
              sample->body_semidiameter_deg =
                  r_to_d(asin(695700.0 / sun_vector.Norm()));
              m_LunarUsesDe440 = true;
              used_de440 = true;
            }
          }
        }
      }
    }

    if (!used_de440) {
      wxDateTime instant = UtcDateTime::ToInstant(time);
      const double moon_distance_km = moon_distance(ut_to_dt(
          instant.GetJulianDayNumber()));
      sample->moon_horizontal_parallax_deg =
          r_to_d(asin(EARTH_RADIUS / moon_distance_km));
      sample->moon_semidiameter_deg = r_to_d(asin(
          K_MOON * sin(d_to_r(sample->moon_horizontal_parallax_deg))));

      sample->body_semidiameter_deg = 0.0;
      sample->body_horizontal_parallax_deg = 0.0;
      if (!selected_body.Cmp(_T("Sun"))) {
        if (!(body_rad > 0.0)) {
          if (error)
            *error = "The solar ephemeris returned an invalid distance";
          return false;
        }
        sample->body_semidiameter_deg = 0.266564 / body_rad;
        sample->body_horizontal_parallax_deg = 0.002442 / body_rad;
      } else if (body_is_planet && body_distance > EARTH_RADIUS) {
        sample->body_horizontal_parallax_deg =
            r_to_d(asin(EARTH_RADIUS / body_distance));
      }
    }
    m_IsPlanet = body_is_planet;
    m_IsStar = body_is_star;
    return std::isfinite(sample->predicted_distance_deg);
  };

  lunar_distance::SolveOptions options;
  const double search_span = m_TimeCertainty > 0.0 ? m_TimeCertainty : 86400.0;
  options.start_offset_seconds = -search_span / 2.0;
  options.end_offset_seconds = search_span / 2.0;
  options.scan_step_seconds = std::min(300.0, std::max(30.0, search_span / 288.0));
  const lunar_distance::SolveResult solution =
      lunar_distance::SolveTime(observation, ephemeris, options);
  m_LunarCandidates = solution.candidates;
  m_LunarSolutionValid = solution.valid;
  m_LunarSolutionError = wxString::FromUTF8(solution.error.c_str());
  m_LunarPositionResult = lunar_distance::PositionResult();
  m_LunarSelectedPosition = -1;
  m_TimeCorrection = 0;
  m_LDC = NAN;

  m_CalcStr = _(
      "Lunar-distance time recovery\n\n"
      "The measured limb distance is converted to an apparent centre distance. "
      "The two measured altitudes are then used to remove atmospheric refraction "
      "and geocentric parallax by spherical trigonometry. The cleared distance is "
      "matched numerically against the offline ephemeris.\n\n");
  m_CalcStr += wxString::Format(
      _("Reference UTC: %s\nSearch interval: %.1f hours (%.1f hours either side)\n"),
      UtcDateTime::FormatUtc(m_CorrectedDateTime, "%Y-%m-%d %H:%M:%S"),
      search_span / 3600.0, search_span / 7200.0);
  m_CalcStr += m_LunarUsesDe440
                   ? _("Ephemeris: local JPL DE440s (no network access)\n")
                   : _("Ephemeris: bundled analytical catalogue (offline fallback)\n");
  m_CalcStr += wxString::Format(
      _("Observed distance: %s\nMoon altitude: %s\n%s altitude: %s\n"),
      toSDMM_PlugIn(0, m_Measurement, true),
      toSDMM_PlugIn(0, m_LunarMoonAltitude, true), m_Body,
      toSDMM_PlugIn(0, m_LunarBodyAltitude, true));

  lunar_distance::EphemerisSample reference_sample;
  std::string reference_error;
  if (ephemeris(0.0, &reference_sample, &reference_error)) {
    const lunar_distance::Clearance clearance =
        lunar_distance::ClearDistance(observation, reference_sample);
    if (clearance.valid) {
      m_LDC = clearance.cleared_distance_deg;
      m_CalcStr += wxString::Format(
          _("\nApparent centre distance: %.6f%c\n"
            "Moon apparent/geocentric altitude: %.6f%c / %.6f%c\n"
            "%s apparent/geocentric altitude: %.6f%c / %.6f%c\n"
            "Relative azimuth: %.6f%c\nCleared distance at reference epoch: %.6f%c\n"),
          clearance.apparent_distance_deg, 0x00B0,
          clearance.moon_apparent_center_altitude_deg, 0x00B0,
          clearance.moon_geocentric_altitude_deg, 0x00B0, m_Body,
          clearance.body_apparent_center_altitude_deg, 0x00B0,
          clearance.body_geocentric_altitude_deg, 0x00B0,
          clearance.relative_azimuth_deg, 0x00B0,
          clearance.cleared_distance_deg, 0x00B0);
    }
  }

  if (!solution.valid) {
    m_CalcStr += _("\nNo valid UTC solution: ") + m_LunarSolutionError + _("\n");
    return;
  }

  std::size_t selected = 0;
  for (std::size_t index = 1; index < solution.candidates.size(); ++index) {
    if (fabs(solution.candidates[index].offset_seconds) <
        fabs(solution.candidates[selected].offset_seconds))
      selected = index;
  }
  const lunar_distance::TimeCandidate& chosen = solution.candidates[selected];
  m_TimeCorrection = static_cast<long>(lround(chosen.offset_seconds));
  m_LDC = chosen.cleared_distance_deg;
  m_CalcStr += wxString::Format(
      _("\nMatching UTC candidate%s:\n"),
      solution.candidates.size() == 1 ? _T("") : _T("s"));
  for (std::size_t index = 0; index < solution.candidates.size(); ++index) {
    const lunar_distance::TimeCandidate& candidate = solution.candidates[index];
    const wxDateTime candidate_time = UtcDateTime::AddSeconds(
        m_CorrectedDateTime, candidate.offset_seconds);
    m_CalcStr += wxString::Format(
        _("%s%s  slope %.3f arcmin/hour; estimated 1-sigma time uncertainty %.1f s\n"),
        index == selected ? _T("* ") : _T("  "),
        UtcDateTime::FormatUtc(candidate_time, "%Y-%m-%d %H:%M:%S.%l"),
        candidate.slope_arcmin_per_hour, candidate.time_uncertainty_seconds);
  }
  for (const std::string& warning : solution.warnings)
    m_CalcStr += _("Warning: ") + wxString::FromUTF8(warning.c_str()) + _("\n");

  lunar_distance::EphemerisSample chosen_sample;
  std::string chosen_error;
  if (ephemeris(chosen.offset_seconds, &chosen_sample, &chosen_error)) {
    const lunar_distance::Clearance chosen_clearance =
        lunar_distance::ClearDistance(observation, chosen_sample);
    if (chosen_clearance.valid) {
      m_LunarPositionResult = lunar_distance::IntersectAltitudeCircles(
          {chosen_sample.moon_geographic_latitude_deg,
           chosen_sample.moon_geographic_longitude_deg},
          chosen_clearance.moon_geocentric_altitude_deg,
          {chosen_sample.body_geographic_latitude_deg,
           chosen_sample.body_geographic_longitude_deg},
          chosen_clearance.body_geocentric_altitude_deg);
    }
  }
  if (m_LunarPositionResult.valid) {
    lunar_distance::GeographicPoint approximate{m_DRLat, m_DRLon};
    double nearest = INFINITY;
    for (std::size_t index = 0;
         index < m_LunarPositionResult.candidates.size(); ++index) {
      const double distance = lunar_distance::GreatCircleDistanceNm(
          approximate, m_LunarPositionResult.candidates[index]);
      if (distance < nearest) {
        nearest = distance;
        m_LunarSelectedPosition = static_cast<int>(index);
      }
    }
    for (std::size_t index = 0;
         index < m_LunarPositionResult.candidates.size(); ++index) {
      const auto& position = m_LunarPositionResult.candidates[index];
      m_CalcStr += wxString::Format(
          _("Position candidate %zu: %.6f%c, %.6f%c%s\n"), index + 1,
          position.latitude_deg, 0x00B0, position.longitude_deg, 0x00B0,
          static_cast<int>(index) == m_LunarSelectedPosition
              ? _T(" (nearest DR/boat position)")
              : _T(""));
    }
    m_CalcStr += wxString::Format(
        _("Altitude-circle crossing angle: %.2f%c.\n"),
        m_LunarPositionResult.circle_crossing_angle_deg, 0x00B0);
    if (m_LunarPositionResult.circle_crossing_angle_deg < 15.0)
      m_CalcStr += _("Warning: shallow altitude-circle crossing gives weak position geometry.\n");
  } else {
    m_CalcStr += _("\nThe two corrected altitude circles did not yield a position: ") +
                 wxString::FromUTF8(m_LunarPositionResult.error.c_str()) +
                 _("\nAdditional altitude sights can still use the recovered watch offset.\n");
  }
  m_CalcStr += _(
      "\nThe recovered correction is a constant watch offset. It may be "
      "applied to other sightings recorded while the watch retained interval "
      "accuracy. The position above comes from the two measured altitudes; it "
      "is not a 15-degrees-per-hour longitude shortcut.\n");
  return;

#if 0  // Historical implementation retained for algorithm provenance.
  double rad;
  double planet_dist;
  BodyLocation(m_CorrectedDateTime, 0, 0, 0, &rad, &planet_dist);

  m_CalcStr += _("Formulas used to calculate sight\n\n");

  m_CalcStr += wxString::Format(
      _("Moon altitude measurement (Hs) = %.4f%c = %s\n\n"),
      m_LunarMoonAltitude, 0x00B0, toSDMM_PlugIn(0, m_LunarMoonAltitude, true));

  /* correct for index error */
  double IndexCorrection = m_IndexError / 60.0;
  m_CalcStr +=
      wxString::Format(_("Index Error = %.4f%c = %s\n\n"), IndexCorrection,
                       0x00B0, toSDMM_PlugIn(0, IndexCorrection, true));

  double EyeHeightCorrection = 0;
  if (m_ArtificialHorizon) {
    m_CalcStr +=
        wxString::Format(_("Artificial horizon, no height correction\n"));
  } else {
    if (m_DipShort) {
      if (m_DipShortDistance == 0) {
        m_CalcStr += wxString::Format(_("Dip Short Distance cannot be 0 !\n"));
        return;
      }
      EyeHeightCorrection = 0.4156 * m_DipShortDistance +
                            1.856 * m_EyeHeight / m_DipShortDistance;
      m_CalcStr +=
          wxString::Format(_("Dip Short Distance = %.4f m\n\
Height Correction = 0.4156 * %.4f + 1.856 * %.5f / %.4f\n\
Height Correction = %.4f%c = %s\n"),
                           m_DipShortDistance, m_DipShortDistance, m_EyeHeight,
                           m_DipShortDistance, EyeHeightCorrection, 0x00B0,
                           toSDMM_PlugIn(0, EyeHeightCorrection, true));
    } else {
      /* correct for height of observer
         The dip of the sea horizon in minutes = 1.758*sqrt(height) */
      EyeHeightCorrection = 1.758 * sqrt(m_EyeHeight) / 60.0;
      m_CalcStr += wxString::Format(
          _("Eye Height = %.4f m\n\
Height Correction = 1.758%c * sqrt(%.4f) / 60.0\n\
Height Correction = %.4f%c = %s\n"),
          m_EyeHeight, 0x00B0, m_EyeHeight, EyeHeightCorrection, 0x00B0,
          toSDMM_PlugIn(0, EyeHeightCorrection, true));
    }
  }

  /* Apparent Altitude Ha */
  double ApparentAltitudeMoon =
      m_LunarMoonAltitude - IndexCorrection - EyeHeightCorrection;
  m_CalcStr +=
      wxString::Format(_("\nApparent Moon Altitude (Ha)\n\
ApparentAltitudeMoon = Hs - IndexCorrection - EyeHeightCorrection\n\
ApparentAltitudeMoon = %.4f%c - %.4f%c - %.4f%c\n\
ApparentAltitudeMoon = %.4f%c = %s\n"),
                       m_LunarMoonAltitude, 0x00B0, IndexCorrection, 0x00B0,
                       EyeHeightCorrection, 0x00B0, ApparentAltitudeMoon,
                       0x00B0, toSDMM_PlugIn(0, ApparentAltitudeMoon, true));

  if (m_ArtificialHorizon) {
    m_CalcStr += wxString::Format(_("\nArtificial horizon\n\
ApparentAltitudeMoon = ApparentAltitudeMoon / 2\n\
ApparentAltitudeMoon = %.4f%c / 2 = %.4f%c\n"),
                                  ApparentAltitudeMoon, 0x00B0,
                                  ApparentAltitudeMoon / 2, 0x00B0);
    ApparentAltitudeMoon /= 2;
  }

  /* compensate for refraction */
  double RefractionCorrectionMoon;

  double x = tan(d_to_r(ApparentAltitudeMoon) +
                 d_to_r(4.848e-2) / (tan(d_to_r(ApparentAltitudeMoon)) + .028));
  m_CalcStr += wxString::Format(_("\nRefraction Correction\n\
x = tan(ApparentAltitudeMoon + 4.848e-2 / (tan(ApparentAltitudeMoon) + .028))\n\
x = tan(%.4f + 4.848e-2 / (tan(%.4f) + .028))\n\
x = %.4f\n"),
                                ApparentAltitudeMoon, ApparentAltitudeMoon, x);
  RefractionCorrectionMoon =
      .267 * m_Pressure / (x * (m_Temperature + 273.15)) / 60.0;
  m_CalcStr += wxString::Format(
      _("\
RefractionCorrectionMoon = .267%c * Pressure / (x * (Temperature + 273.15)) / 60.0\n\
RefractionCorrectionMoon = .267%c * %.4f / (x * (%.4f + 273.15)) / 60.0\n\
RefractionCorrectionMoon = %.4f%c = %s\n"),
      0x00B0, 0x00B0, m_Pressure, m_Temperature, RefractionCorrectionMoon,
      0x00B0, toSDMM_PlugIn(0, RefractionCorrectionMoon, true));

  wxDateTime time = m_CorrectedDateTime;
  time.MakeFromUTC();
  double jdu = time.GetJulianDayNumber();
  double jdd = ut_to_dt(jdu);
  double moon_dist = moon_distance(jdd);
  double lunar_HP = r_to_d(asin(EARTH_RADIUS / moon_dist));
  double lunar_SD = r_to_d(asin(K_MOON * sin(d_to_r(lunar_HP))));
  // convert to topocentric SD, see Meeus (chapter 55)
  double lunar_topoSD = lunar_SD * (1 + sin(d_to_r(ApparentAltitudeMoon)) *
                                            sin(d_to_r(lunar_HP)));
  double lunar_lc = r_to_d(asin(d_to_r(lunar_topoSD)));
  m_CalcStr += wxString::Format(
      _("\nMoon selected, Limb Correction\n\
SD = %.4f%c = %s\n\
topoSD = SD * (1 + sin(ApparentAltitudeMoon) * sin(lunarHP))\n\
topoSD = %.4f\n\
lc = asin(topoSD)\n\
lc = %.4f%c = %s\n"),
      lunar_SD, 0x00B0, toSDMM_PlugIn(0, lunar_SD, true), lunar_topoSD,
      lunar_lc, 0x00B0, toSDMM_PlugIn(0, lunar_lc, true));

  double LimbCorrectionMoon = 0;
  if (lunar_lc) {
    if (m_LunarMoonLimb == UPPER) {
      LimbCorrectionMoon = -lunar_lc;
      m_CalcStr += wxString::Format(_("Upper Limb"));
    } else if (m_LunarMoonLimb == LOWER) {
      LimbCorrectionMoon = lunar_lc;
      m_CalcStr += wxString::Format(_("Lower Limb"));
    }

    m_CalcStr += wxString::Format(_("\nLimbCorrectionMoon = %.4f%c = %s\n"),
                                  LimbCorrectionMoon, 0x00B0,
                                  toSDMM_PlugIn(0, LimbCorrectionMoon, true));
  }

  double CorrectedAltitudeMoon =
      ApparentAltitudeMoon - RefractionCorrectionMoon + LimbCorrectionMoon;
  m_CalcStr += wxString::Format(
      _("\nCorrected Altitude (Hc)\n\
CorrectedAltitudeMoon = ApparentAltitudeMoon - RefractionCorrectionMoon + LimbCorrectionMoon\n\
CorrectedAltitudeMoon = %.4f%c - %.4f%c + %.4f%c\n\
CorrectedAltitudeMoon = %.4f%c = %s\n"),
      ApparentAltitudeMoon, 0x00B0, RefractionCorrectionMoon, 0x00B0,
      LimbCorrectionMoon, 0x00B0, CorrectedAltitudeMoon, 0x00B0,
      toSDMM_PlugIn(0, CorrectedAltitudeMoon, true));

  double ParallaxCorrectionMoon;
  m_CalcStr +=
      wxString::Format(_("\nMoon selected, parallax correction\n\
HP = %.4f%c = %s\n"),
                       lunar_HP, 0x00B0, toSDMM_PlugIn(0, lunar_HP, true));

  ParallaxCorrectionMoon =
      r_to_d(asin(sin(d_to_r(lunar_HP)) * cos(d_to_r(CorrectedAltitudeMoon))));
  m_CalcStr +=
      wxString::Format(_("\
ParallaxCorrectionMoon = asin(sin(HP) * cos(CorrectedAltitude))\n\
ParallaxCorrectionMoon = asin(sin(%.4f) * cos(%.4f))\n\
ParallaxCorrectionMoon = %.4f%c = %s\n"),
                       lunar_HP, CorrectedAltitudeMoon, ParallaxCorrectionMoon,
                       0x00B0, toSDMM_PlugIn(0, ParallaxCorrectionMoon, true));

  double ObservedAltitudeMoon = CorrectedAltitudeMoon + ParallaxCorrectionMoon;
  m_CalcStr +=
      wxString::Format(_("\nObserved Altitude (Ho)\n\
ObservedAltitudeMoon = CorrectedAltitudeMoon + ParallaxCorrectionMoon\n\
ObservedAltitudeMoon = %.4f%c + %.4f%c\n\
ObservedAltitudeMoon = %.4f%c = %s\n"),
                       CorrectedAltitudeMoon, 0x00B0, ParallaxCorrectionMoon,
                       0x00B0, ObservedAltitudeMoon, 0x00B0,
                       toSDMM_PlugIn(0, ObservedAltitudeMoon, true));

  // body

  m_CalcStr += wxString::Format(
      _("\n\n%s altitude measurement (Hs) = %.4f%c = %s\n"), m_Body,
      m_LunarBodyAltitude, 0x00B0, toSDMM_PlugIn(0, m_LunarBodyAltitude, true));

  /* Apparent Altitude Ha */
  double ApparentAltitude =
      m_LunarBodyAltitude - IndexCorrection - EyeHeightCorrection;
  m_CalcStr +=
      wxString::Format(_("\nApparent Altitude (Ha)\n\
ApparentAltitude = Hs - IndexCorrection - EyeHeightCorrection\n\
ApparentAltitude = %.4f%c - %.4f%c - %.4f%c\n\
ApparentAltitude = %.4f%c = %s\n"),
                       m_LunarBodyAltitude, 0x00B0, IndexCorrection, 0x00B0,
                       EyeHeightCorrection, 0x00B0, ApparentAltitude, 0x00B0,
                       toSDMM_PlugIn(0, ApparentAltitude, true));

  if (m_ArtificialHorizon) {
    m_CalcStr += wxString::Format(_("Artificial horizon\n\
ApparentAltitude = ApparentAltitudeMoon / 2\n\
ApparentAltitude = %.4f%c / 2 = %.4f%c"),
                                  ApparentAltitude, 0x00B0,
                                  ApparentAltitude / 2, 0x00B0);
    ApparentAltitude /= 2;
  }

  /* compensate for refraction */
  double RefractionCorrection;

  x = tan(d_to_r(ApparentAltitude) +
          d_to_r(4.848e-2) / (tan(d_to_r(ApparentAltitude)) + .028));
  m_CalcStr += wxString::Format(_("\nRefraction Correction\n\
x = tan(ApparentAltitude + 4.848e-2 / (tan(ApparentAltitude) + .028))\n\
x = tan(%.4f + 4.848e-2 / (tan(%.4f) + .028))\n\
x = %.4f\n"),
                                ApparentAltitude, ApparentAltitude, x);
  RefractionCorrection =
      .267 * m_Pressure / (x * (m_Temperature + 273.15)) / 60.0;
  m_CalcStr += wxString::Format(_("\
RefractionCorrection = .267%c * Pressure / (x * (Temperature + 273.15)) / 60.0\n\
RefractionCorrection = .267%c * %.4f / (x * (%.4f + 273.15)) / 60.0\n\
RefractionCorrection = %.4f%c = %s\n"),
                                0x00B0, 0x00B0, m_Pressure, m_Temperature,
                                RefractionCorrection, 0x00B0,
                                toSDMM_PlugIn(0, RefractionCorrection, true));

  double SD = 0;
  double lc = 0;

  if (!m_Body.Cmp(_T("Sun"))) {
    lc = 0.266564 / rad;
    SD = r_to_d(sin(d_to_r(lc)));

    m_CalcStr += wxString::Format(_("\nSun selected, Limb Correction\n\
ra = %.4f, lc = 0.266564/ra = %.4f%c = %s\n"),
                                  rad, lc, 0x00B0, toSDMM_PlugIn(0, lc, true));
  }

  double LimbCorrection = 0;
  if (lc) {
    if (m_LunarBodyLimb == UPPER) {
      LimbCorrection = -lc;
      m_CalcStr += wxString::Format(_("Upper Limb"));
    } else if (m_LunarBodyLimb == LOWER) {
      LimbCorrection = lc;
      m_CalcStr += wxString::Format(_("Lower Limb"));
    }

    m_CalcStr +=
        wxString::Format(_("\nLimbCorrection = %.4f%c = %s\n"), LimbCorrection,
                         0x00B0, toSDMM_PlugIn(0, LimbCorrection, true));
  }

  double CorrectedAltitude =
      ApparentAltitude - RefractionCorrection + LimbCorrection;
  m_CalcStr +=
      wxString::Format(_("\nCorrected Altitude\n\
CorrectedAltitude = ApparentAltitude - RefractionCorrection + LimbCorrection\n\
CorrectedAltitude = %.4f%c - %.4f%c - %.4f%c\n\
CorrectedAltitude = %.4f%c = %s\n"),
                       ApparentAltitude, 0x00B0, RefractionCorrection, 0x00B0,
                       LimbCorrection, 0x00B0, CorrectedAltitude, 0x00B0,
                       toSDMM_PlugIn(0, CorrectedAltitude, true));

  /* correct for parallax */
  double ParallaxCorrection = 0;
  double HP = 0;
  if (!m_Body.Cmp(_T("Sun"))) {
    HP = 0.002442 / rad;

    m_CalcStr += wxString::Format(_("\nSun selected, parallax correction\n\
rad = %.4f, HP = 0.002442/rad = %.4f%c = %s\n"),
                                  rad, HP, 0x00B0, toSDMM_PlugIn(0, HP, true));
  }

  if (m_IsPlanet) {
    HP = r_to_d(asin(EARTH_RADIUS / planet_dist));
    m_CalcStr += wxString::Format(_("\nStar selected, parallax correction\n\
HP = %.4f%c\n"),
                                  HP, 0x00B0, toSDMM_PlugIn(0, HP, true));
  }

  if (HP) {
    ParallaxCorrection =
        r_to_d(asin(sin(d_to_r(HP)) * cos(d_to_r(CorrectedAltitude))));
    m_CalcStr +=
        wxString::Format(_("\
ParallaxCorrection = asin(sin(HP) * cos(CorrectedAltitude))\n\
ParallaxCorrection = asin(sin(%.4f) * cos(%.4f))\n\
ParallaxCorrection = %.4f%c = %s\n"),
                         HP, CorrectedAltitude, ParallaxCorrection, 0x00B0,
                         toSDMM_PlugIn(0, ParallaxCorrection, true));
  }

  m_ObservedAltitude = CorrectedAltitude + ParallaxCorrection;
  m_CalcStr += wxString::Format(_("\nObserved Altitude (Ho)\n\
ObservedAltitude = CorrectedAltitude + ParallaxCorrection\n\
ObservedAltitude = %.4f%c + %.4f%c\n\
ObservedAltitude = %.4f%c = %s\n"),
                                CorrectedAltitude, 0x00B0, ParallaxCorrection,
                                0x00B0, m_ObservedAltitude, 0x00B0,
                                toSDMM_PlugIn(0, m_ObservedAltitude, true));

  double ApparentLunarDistance;
  m_CalcStr += _("\n\nApparent Lunar Distance (LDo)\n");
  if (!m_Body.Cmp(_T("Sun"))) {
    ApparentLunarDistance = m_Measurement - IndexCorrection + lunar_topoSD + SD,
    m_CalcStr += wxString::Format(
        _("\
LDo = LDO.pc - IndexCorrection + LunarTopoSD + SunSD\n\
LDo = %.4f%c - %.4f%c + %.4f%c + %.4f%c\n\
LDo = %.4f%c = %s\n"),
        m_Measurement, 0x00B0, IndexCorrection, 0x00B0, lunar_topoSD, 0x00B0,
        SD, 0x00B0, ApparentLunarDistance, 0x00B0,
        toSDMM_PlugIn(0, ApparentLunarDistance, true));
  } else {
    if (m_BodyLimb == LUNAR_NEAR) {
      ApparentLunarDistance = m_Measurement - IndexCorrection + lunar_topoSD,
      m_CalcStr +=
          wxString::Format(_("\
LDo is NEAR\n\
LDo = LDO.pc - IndexCorrection + LunarTopoSD\n\
LDo = %.4f%c - %.4f%c + %.4f%c\n\
LDo = %.4f%c = %s\n"),
                           m_Measurement, 0x00B0, IndexCorrection, 0x00B0,
                           lunar_topoSD, 0x00B0, ApparentLunarDistance, 0x00B0,
                           toSDMM_PlugIn(0, ApparentLunarDistance, true));
    } else {
      ApparentLunarDistance = m_Measurement - IndexCorrection - lunar_topoSD,
      m_CalcStr +=
          wxString::Format(_("\
LDo is FAR\n\
LDo = LDO.pc - IndexCorrection - LunarTopoSD\n\
LDo = %.4f%c - %.4f%c - %.4f%c\n\
LDo = %.4f%c = %s\n"),
                           m_Measurement, 0x00B0, IndexCorrection, 0x00B0,
                           lunar_topoSD, 0x00B0, ApparentLunarDistance, 0x00B0,
                           toSDMM_PlugIn(0, ApparentLunarDistance, true));
    }
  }

  double ham = ApparentAltitudeMoon + LimbCorrectionMoon;
  double hab = ApparentAltitude + LimbCorrection;
  double cosdz =
      cos(d_to_r(ApparentLunarDistance)) / cos(d_to_r(ham)) / cos(d_to_r(hab)) -
      tan(d_to_r(ham)) * tan(d_to_r(hab));
  double dz = r_to_d(acos(cosdz));
  m_CalcStr += wxString::Format(_("\nDZ Angle\n\
ha.m = ApparentAltitudeMoon + LimbCorrectionMoon\n\
ha.m = %.4f + %.4f = %.4f\n\
ha.b = ApparentAltitude + LimbCorrection\n\
ha.b = %.4f + %.4f = %.4f\n\
cos(LDo) = sin(ha.m) * sin(ha.b) + cos(ha.m) * cos(ha.b) * cos(DZ)\n\
cos(DZ) = cos(LDo) / cos(ha.m) / cos(ha.b) - tan(ha.m) * tan(ha.b)\n\
cos(DZ) = cos(%.4f) / cos(%.4f) / cos(%.4f) - tan(%.4f) * tan(%.4f)\n\
cos(DZ) = %.4f\n\
DZ = %.4f%c = %s\n"),
                                ApparentAltitudeMoon, LimbCorrectionMoon, ham,
                                ApparentAltitude, LimbCorrection, hab,
                                ApparentLunarDistance, ham, hab, ham, hab,
                                cosdz, dz, 0x00B0, toSDMM_PlugIn(0, dz, true));

  double hom = ham + ParallaxCorrectionMoon - RefractionCorrectionMoon;
  double hob = hab + ParallaxCorrection - RefractionCorrection;
  double cosldc = sin(d_to_r(hom)) * sin(d_to_r(hob)) +
                  cos(d_to_r(hom)) * cos(d_to_r(hob)) * cosdz;
  m_LDC = r_to_d(acos(cosldc));
  m_CalcStr += wxString::Format(
      _("\nLunar Distance Cleared (LDc)\n\
ho.m = ha.m + ParallaxCorrectionMoon - RefractionCorrectionMoon\n\
ho.m = %.4f + %.4f - %.4f = %.4f\n\
ho.b = ha.b + ParallaxCorrection - RefractionCorrection\n\
ho.b = %.4f + %.4f - %.4f = %.4f\n\
cos(LDc) = sin(ho.m) * sin(ho.b) + cos(ho.m) * cos(ho.b) * cos(DZ)\n\
cos(LDc) = sin(%.4f) * sin(%.4f) + cos(%.4f) * cos(%.4f) * cos(%.4f)\n\
cos(LDc) = %.4f\n\
LDc = %.4f%c = %s\n\n"),
      ham, ParallaxCorrectionMoon, RefractionCorrectionMoon, hom, hab,
      ParallaxCorrection, RefractionCorrection, hob, hom, hob, hom, hob, dz,
      cosldc, m_LDC, 0x00B0, toSDMM_PlugIn(0, m_LDC, true));

  wxDateTime startTime =
      UtcDateTime::AddSeconds(m_CorrectedDateTime, -m_TimeCertainty / 2.0);
  wxDateTime endTime =
      UtcDateTime::AddSeconds(m_CorrectedDateTime, m_TimeCertainty / 2.0);

  double startBodyLat, startBodyLon, startBodyGhaast, startBodyRad;
  BodyLocation(startTime, &startBodyLat, &startBodyLon, &startBodyGhaast,
               &startBodyRad, 0);

  m_CalcStr += Alminac(startTime, startBodyLat, startBodyLon, startBodyGhaast,
                       startBodyRad, SD, HP);
  startBodyLon = resolve_heading_positive(-startBodyLon);

  double endBodyLat, endBodyLon, endBodyGhaast, endBodyRad;
  BodyLocation(endTime, &endBodyLat, &endBodyLon, &endBodyGhaast, &endBodyRad,
               0);

  m_CalcStr += Alminac(endTime, endBodyLat, endBodyLon, endBodyGhaast,
                       endBodyRad, SD, HP);
  endBodyLon = resolve_heading_positive(-endBodyLon);

  wxString body = m_Body;
  m_Body = _T("Moon");
  double startMoonLat, startMoonLon, startMoonGhaast, startMoonRad;
  BodyLocation(startTime, &startMoonLat, &startMoonLon, &startMoonGhaast,
               &startMoonRad, 0);

  m_CalcStr += Alminac(startTime, startMoonLat, startMoonLon, startMoonGhaast,
                       startMoonRad, lunar_SD, lunar_HP);
  startMoonLon = resolve_heading_positive(-startMoonLon);

  double endMoonLat, endMoonLon, endMoonGhaast, endMoonRad;
  BodyLocation(endTime, &endMoonLat, &endMoonLon, &endMoonGhaast, &endMoonRad,
               0);

  m_CalcStr += Alminac(endTime, endMoonLat, endMoonLon, endMoonGhaast,
                       endMoonRad, lunar_SD, lunar_HP);
  endMoonLon = resolve_heading_positive(-endMoonLon);
  m_Body = body;

  double dgha = fabs(startMoonLon - startBodyLon);
  cosldc =
      sin(d_to_r(startMoonLat)) * sin(d_to_r(startBodyLat)) +
      cos(d_to_r(startMoonLat)) * cos(d_to_r(startBodyLat)) * cos(d_to_r(dgha));
  double startLdc = r_to_d(acos(cosldc));
  m_CalcStr +=
      wxString::Format(_("Lunar Distance Cleared (LDc) prediction for %s\n\
cos(LDc) = sin(dec.m) * sin(dec.b) + cos(dec.m) * cos(dec.b) * cos(Dgha)\n\
cos(LDc) = sin(%.4f) * sin(%.4f) + cos(%.4f) * cos(%.4f) * cos(%.4f)\n\
cos(LDc) = %.4f\n\
LDc = %.4f%c = %s\n"),
                       UtcDateTime::FormatUtc(startTime,
                                              "%Y-%m-%d %H:%M:%S"),
                       startMoonLat,
                       startBodyLat, startMoonLat, startBodyLat, dgha, cosldc,
                       startLdc, 0x00B0, toSDMM_PlugIn(0, startLdc, true));

  dgha = fabs(endMoonLon - endBodyLon);
  cosldc =
      sin(d_to_r(endMoonLat)) * sin(d_to_r(endBodyLat)) +
      cos(d_to_r(endMoonLat)) * cos(d_to_r(endBodyLat)) * cos(d_to_r(dgha));
  double endLdc = r_to_d(acos(cosldc));
  m_CalcStr += wxString::Format(
      _("\nLunar Distance Cleared (LDc) prediction for %s\n\
cos(LDc) = sin(dec.m) * sin(dec.b) + cos(dec.m) * cos(dec.b) * cos(Dgha)\n\
cos(LDc) = sin(%.4f) * sin(%.4f) + cos(%.4f) * cos(%.4f) * cos(%.4f)\n\
cos(LDc) = %.4f\n\
LDc = %.4f%c = %s\n"),
      UtcDateTime::FormatUtc(endTime, "%Y-%m-%d %H:%M:%S"), endMoonLat,
      endBodyLat, endMoonLat,
      endBodyLat, dgha, cosldc, endLdc, 0x00B0, toSDMM_PlugIn(0, endLdc, true));

  wxDateTime interpolatedTime = UtcDateTime::AddSeconds(
      startTime,
      round((m_LDC - startLdc) * m_TimeCertainty / (endLdc - startLdc)));
  m_CalcStr += wxString::Format(
      _("\nInterpolating Lunar Distance Cleared to find out UTC\n\
UTC = time.start + (LDc - LDc.start) * (time.end - time.start) / (LDc.end - LDc.start)\n\
UTC = %s + (%.4f - %.4f) * (%s - %s) / (%.4f - %.4f)\n\
UTC = %s\n"),
      UtcDateTime::FormatUtc(startTime, "%Y-%m-%d %H:%M:%S"), m_LDC,
      startLdc, UtcDateTime::FormatUtc(endTime, "%Y-%m-%d %H:%M:%S"),
      UtcDateTime::FormatUtc(startTime, "%Y-%m-%d %H:%M:%S"), endLdc,
      startLdc,
      UtcDateTime::FormatUtc(interpolatedTime, "%Y-%m-%d %H:%M:%S"));

  m_TimeCorrection = static_cast<long>(std::lround(
      UtcDateTime::SecondsBetween(interpolatedTime, m_CorrectedDateTime)));
  m_CalcStr +=
      wxString::Format(_("\nTime correction %ld seconds"), m_TimeCorrection);

  double lat, lon, ghaast;
  BodyLocation(m_CorrectedDateTime, &lat, &lon, &ghaast, &rad, 0);

  m_CalcStr =
      Alminac(m_CorrectedDateTime, lat, lon, ghaast, rad, SD, HP) + m_CalcStr;

  double lunar_lat, lunar_lon, lunar_ghaast, lunar_rad;
  body = m_Body;
  m_Body = _T("Moon");
  BodyLocation(m_CorrectedDateTime, &lunar_lat, &lunar_lon, &lunar_ghaast,
               &lunar_rad, 0);

  m_CalcStr = Alminac(m_CorrectedDateTime, lunar_lat, lunar_lon, lunar_ghaast,
                      lunar_rad, lunar_SD, lunar_HP) +
              m_CalcStr;
  m_Body = body;
#endif
}

void Sight::EstimateHs(double hc, double* hs, double* error) {
  *hs = NAN;
  *error = NAN;
  if (hc < 0) return;

  // first calculate HP and SD
  double SD = 0, topoSD = 0;
  double HP = 0;
  double planet_dist, rad;
  BodyLocation(m_CorrectedDateTime, 0, 0, 0, &rad, &planet_dist);

  if (!m_Body.Cmp(_T("Sun"))) {
    HP = 0.002442 / rad;
    double lc = 0.266564 / rad;
    SD = r_to_d(sin(d_to_r(lc)));
    topoSD = SD;
  }
  if (!m_Body.Cmp(_T("Moon"))) {
    wxDateTime time = m_CorrectedDateTime;
    time.MakeFromUTC();
    double jdu = time.GetJulianDayNumber();
    double jdd = ut_to_dt(jdu);
    double moon_dist = moon_distance(jdd);
    HP = r_to_d(asin(EARTH_RADIUS / moon_dist));
    SD = r_to_d(asin(K_MOON * sin(d_to_r(HP))));
  }
  if (m_IsPlanet) {
    HP = r_to_d(asin(EARTH_RADIUS / planet_dist));
  }

  double ca, ha, parallax = 0, dip, ic, refraction, lc, ho;
  double diff;

  // estimate CA
  ca = hc;
  diff = 0;
  if (HP > 0) {
    ca = hc;
    for (int i = 0; i < 11; i++) {
      ca -= diff;
      parallax = r_to_d(asin(sin(d_to_r(HP)) * cos(d_to_r(ca))));
      double ho_estimate = ca + parallax;
      diff = abs(ho_estimate - hc);
      if (diff == 0) break;
    }
  }

  // estimate HA
  ha = ca;
  diff = 0;
  for (int i = 0; i < 11; i++) {
    ha += diff;
    double topoSD = SD * (1 + sin(d_to_r(ha)) * sin(d_to_r(HP)));
    lc = r_to_d(asin(d_to_r(topoSD)));
    if (m_BodyLimb == UPPER) {
      lc = -lc;
    } else if (m_BodyLimb == CENTER) {
      lc = 0;
    }
    double x = tan(d_to_r(ha) + d_to_r(4.848e-2) / (tan(d_to_r(ha) + .028)));
    refraction = .267 * m_Pressure / (x * (m_Temperature + 273.15)) / 60.0;
    double ca_estimate = ha + lc - refraction;
    diff = ca - ca_estimate;
    if (diff == 0) break;
  }

  // final calculations
  if (m_ArtificialHorizon) {
    dip = 0;
  } else if (m_DipShort) {
    dip = r_to_d(atan(m_EyeHeight / (0.3048 * 6076 * m_DipShortDistance) +
                      m_DipShortDistance / 8268));
  } else {
    dip = 1.758 * sqrt(m_EyeHeight) / 60.0;
  }

  ic = m_IndexError / 60.0;

  if (m_ArtificialHorizon) {
    *hs = ha * 2 + ic;
  } else {
    *hs = ha + dip + ic;
  }

  if (m_ArtificialHorizon) {
    ho = (*hs - ic) / 2 - refraction + parallax + lc;
  } else {
    ho = *hs - dip - ic - refraction + parallax + lc;
  }
  *error = (ho - hc) * 60;
}

void Sight::RebuildPolygonsAltitude() {
  polygons.clear();
  lines.clear();

  double altitudemin, altitudemax, altitudestep;
  altitudemin = m_ObservedAltitude - m_MeasurementCertainty / 60;
  altitudemax = m_ObservedAltitude + m_MeasurementCertainty / 60;
  altitudestep =
      ComputeStepSize(m_MeasurementCertainty / 60, 1, altitudemin, altitudemax);

  double timemin, timemax, timestep;
  timemin = -m_TimeCertainty;
  timemax = +m_TimeCertainty;
  //      timestep = ComputeStepSize(m_TimeCertainty, 1, timemin, timemax);
  timestep = wxMax(2 * m_TimeCertainty, 1);
  BuildAltitudeLineOfPosition(1, altitudemin, altitudemax, altitudestep,
                              timemin, timemax, timestep);
}

void Sight::RebuildPolygonsHorizon() {
  const double savedCertainty = m_MeasurementCertainty;
  m_MeasurementCertainty = m_HorizonAltitudeUncertainty;
  RebuildPolygonsAltitude();
  m_MeasurementCertainty = savedCertainty;

  m_HorizonEstimateValid =
      HorizonEstimatedPosition(&m_HorizonEstimateLat, &m_HorizonEstimateLon);
  if (!m_HorizonEstimateValid) return;

  m_HorizonEstimateRadiusNm = HorizonEstimateUncertaintyNm();
  wxRealPointList* uncertainty = new wxRealPointList;
  const double altitude = 90.0 - m_HorizonEstimateRadiusNm / 60.0;
  for (int bearing = 0; bearing <= 360; bearing += 5) {
    uncertainty->Append(new wxRealPoint(DistancePoint(
        altitude, bearing, m_HorizonEstimateLat, m_HorizonEstimateLon)));
  }
  polygons.push_back(uncertainty);

  m_CalcStr += wxString::Format(
      _("\nBearing-derived estimate = %s %s\n"
        "Conservative uncertainty radius = %.1f NM\n"),
      toSDMM_PlugIn(1, m_HorizonEstimateLat, true),
      toSDMM_PlugIn(2, m_HorizonEstimateLon, true), m_HorizonEstimateRadiusNm);
}

/* Calculate latitude and longitude position for a sight taken with time,
   altitude, and trace angle */
wxRealPoint Sight::DistancePoint(double altitude, double trace, double lat,
                                 double lon) {
  double rlat, rlon, y, x;

  double dang_r = d_to_r(90 - altitude);
  double trace_r = d_to_r(trace);
  double lat_r = d_to_r(lat);
  double lon_r = d_to_r(lon);
  double rlat_r, rlon_r;

  rlat_r =
      asin(sin(lat_r) * cos(dang_r) + cos(lat_r) * sin(dang_r) * cos(trace_r));
  y = sin(trace_r) * sin(dang_r) * cos(lat_r);
  x = cos(dang_r) - sin(lat_r) * sin(rlat_r);
  rlon_r = lon_r + atan2(y, x);

  rlat = r_to_d(rlat_r);
  rlon = r_to_d(rlon_r);

  //    ll_gc_ll(lat, lon, trace, 60*(90-altitude),
  //             &rlat, &rlon);

  return wxRealPoint(rlat, rlon);
}

/* Calculate Hc and Zn from from one position to another */
void Sight::AltitudeAzimuth(double lat1, double lon1, double lat2, double lon2,
                            double* hc, double* zn) {
  lat1 = resolve_heading_positive(lat1);
  lat2 = resolve_heading_positive(lat2);
  double lat1_r = d_to_r(lat1);
  double lon1_r = d_to_r(lon1);
  double lat2_r = d_to_r(lat2);
  double lon2_r = d_to_r(lon2);

  double lha = lon1 - lon2;
  lha = resolve_heading_positive(lha);
  double lha_r = d_to_r(lha);

  double hc_r =
      asin(sin(lat1_r) * sin(lat2_r) + cos(lat1_r) * cos(lat2_r) * cos(lha_r));
  double zn_r =
      acos((sin(lat2_r) - sin(lat1_r) * sin(hc_r)) / (cos(lat1_r) * cos(hc_r)));

  *hc = r_to_d(hc_r);
  *zn = r_to_d(zn_r);
  if (lat1 > 0) {
    if (lha < 180) *zn = 360 - *zn;
  } else {
    if (lha > 180)
      *zn = 180 - *zn;
    else
      *zn = 180 + *zn;
  }
}

void Sight::BuildAltitudeLineOfPosition(double tracestep, double altitudemin,
                                        double altitudemax, double altitudestep,
                                        double timemin, double timemax,
                                        double timestep) {
  for (double time = timemin; time <= timemax; time += timestep) {
    double lat, lon;
    BodyLocation(UtcDateTime::AddSeconds(m_CorrectedDateTime, time), &lat,
                 &lon, 0, 0, 0);
    wxRealPointList *p, *l = new wxRealPointList;
    for (double trace = -180; trace <= 180; trace += tracestep) {
      p = new wxRealPointList;
      double mx = 0;
      double my = 0;
      int mc = 0;
      for (double altitude = altitudemin;
           altitude <= altitudemax && fabs(altitude) <= 90;
           altitude += altitudestep) {
        wxRealPoint* point =
            new wxRealPoint(DistancePoint(altitude, trace, lat, lon));
        p->Append(point);
        mx += point->x;
        my += point->y;
        mc++;
        if (altitudestep == 0) break;
      }
      if (mc > 0) lines.Append(new wxRealPoint(mx / mc, my / mc));
      wxRealPointList* m = MergePoints(l, p);
      wxRealPointList* n = ReduceToConvexPolygon(m);
      polygons.push_back(n);

      m->DeleteContents(true);
      delete m;
      l->DeleteContents(true);
      delete l;

      l = p;
    }
  }
}

void Sight::RebuildPolygonsAzimuth() {
  polygons.clear();
  lines.clear();

  double azimuthmin, azimuthmax, azimuthstep;
  azimuthmin = m_Measurement - m_MeasurementCertainty / 60;
  azimuthmax = m_Measurement + m_MeasurementCertainty / 60;
  azimuthstep =
      ComputeStepSize(m_MeasurementCertainty / 60, 1, azimuthmin, azimuthmax);

  double timemin, timemax, timestep;
  timemin = -m_TimeCertainty;
  timemax = +m_TimeCertainty;
  //    timestep = ComputeStepSize(m_TimeCertainty, 1, timemin, timemax);
  timestep = wxMax(2 * m_TimeCertainty, 1);

  BuildBearingLineOfPosition(1, azimuthmin, azimuthmax, azimuthstep, timemin,
                             timemax, timestep);
}

/* find latitude and longitude which sees the body at the time, altitude and
   bearing iterative method so we can easily support magnetic variation */
bool Sight::BearingPoint(double altitude, double bearing, double& rlat,
                         double& rlon, double& trace, double& lastlat,
                         double& lastlon, double lat, double lon) {
  double localbearing = bearing;

  localbearing = resolve_heading(localbearing);

  double rangle;
  double mdb = 1000;
  double mdl = 1001;
  double b;
  if (trace > 999) {
    lastlat = lat;
    lastlon = lon;

    /* apply magnetic correction to bearing */
    if (m_bMagneticNorth) {
      localbearing += celestial_navigation_pi_GetWMM(lat, lon, m_EyeHeight,
                                                     m_CorrectedDateTime);
    }
    trace = localbearing + 180;
  }

  trace = resolve_heading(trace);

  while ((fabs(mdb) < fabs(mdl)) && (fabs(mdb) > .001)) {
    //       ll_gc_ll(lat, lon, trace, 60*(90-altitude), &rlat, &rlon);
    //       ll_gc_ll_reverse(rlat, rlon, lat, lon, &b, 0);
    mdl = mdb;

    double y, x, yy, xx;
    double dang_r = d_to_r(1.0);
    double trace_r = d_to_r(trace);
    double lat_r = d_to_r(lat);
    double lon_r = d_to_r(lon);
    double rlat_r, rlon_r, backbearing_r;
    double lastlat_r = d_to_r(lastlat);
    double lastlon_r = d_to_r(lastlon);
    double rangle_r;

    rlat_r = asin(sin(lastlat_r) * cos(dang_r) +
                  cos(lastlat_r) * sin(dang_r) * cos(trace_r));
    y = sin(trace_r) * sin(dang_r) * cos(lastlat_r);
    x = cos(dang_r) - sin(lastlat_r) * sin(rlat_r);
    rlon_r = lastlon_r + atan2(y, x);

    yy = sin(lon_r - rlon_r) * cos(lat_r);
    xx = cos(rlat_r) * sin(lat_r) -
         sin(rlat_r) * cos(lat_r) * cos(lon_r - rlon_r);
    backbearing_r = atan2(yy, xx);

    rlat = r_to_d(rlat_r);
    rlon = r_to_d(rlon_r);

    rlon = resolve_heading(rlon);

    b = r_to_d(backbearing_r);

    rangle_r = acos(sin(lat_r) * sin(rlat_r) +
                    cos(lat_r) * cos(rlat_r) * cos(rlon_r - lon_r));
    rangle = r_to_d(rangle_r);

    /* apply magnetic correction to bearing */
    if (m_bMagneticNorth) {
      b -= celestial_navigation_pi_GetWMM(rlat, rlon, m_EyeHeight,
                                          m_CorrectedDateTime);
    }

    mdb = bearing - b;
    mdb = resolve_heading(mdb);

    trace += mdb;

    trace = resolve_heading(trace);
  }
  return ((fabs(mdb) < .1) && (fabs(rangle) < 90.0));
}

void Sight::BuildBearingLineOfPosition(double altitudestep, double azimuthmin,
                                       double azimuthmax, double azimuthstep,
                                       double timemin, double timemax,
                                       double timestep) {
  for (double time = timemin; time <= timemax; time += timestep) {
    double lasttrace[100];
    for (int i = 0; i < 100; i++) lasttrace[i] = 1000.0;

    double lastlat[100];
    double lastlon[100];
    double trace;

    double blat, blon;

    BodyLocation(UtcDateTime::AddSeconds(m_CorrectedDateTime, time), &blat,
                 &blon, 0, 0, 0);

    blon = resolve_heading(blon);

    /* sometimes it takes a long time to build magnetic azimuth sights */
    wxProgressDialog progressdialog(
        _("Celestial Navigation"), _("Building bearing Sight Positions"), 201,
        NULL, wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME);

    wxRealPointList *p, *l = new wxRealPointList;
    l->Append(new wxRealPoint(blat, blon));
    for (double altitude = 200; altitude >= 0; altitude -= 1) {
      if (m_bMagneticNorth && (int)altitude % 10 == 0)
        progressdialog.Update(200 - altitude);

      p = new wxRealPointList;
      int index = 0;
      double mx = 0;
      double my = 0;
      int mc = 0;
      double lat, lon, llat, llon;
      for (double azimuth = azimuthmin; azimuth <= azimuthmax;
           azimuth += azimuthstep) {
        trace = lasttrace[index];
        llat = lastlat[index];
        llon = lastlon[index];
        if (BearingPoint(altitude, azimuth, lat, lon, trace, llat, llon, blat,
                         blon)) {
          if (lat > 90)
            lat = 90.0;
          else if (lat < -90)
            lat = -90.0;

          {
            wxRealPoint* point = new wxRealPoint(lat, lon);
            mx += point->x;
            my += point->y;
            mc++;
            p->Append(point);
            lasttrace[index] = trace;

            lastlat[index] = lat;
            lastlon[index] = lon;
          }
        }
        index += 1;
      }
      if (mc > 0) lines.Append(new wxRealPoint(mx / mc, my / mc));
      wxRealPointList* m = MergePoints(l, p);
      wxRealPointList* n = ReduceToConvexPolygon(m);
      polygons.push_back(n);
      m->DeleteContents(true);
      delete m;
      l->DeleteContents(true);
      delete l;
      l = p;
    }
  }
}
