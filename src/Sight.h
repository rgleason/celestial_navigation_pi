/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Support
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2013 by Sean D'Epagnier                                 *
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

#ifndef _CELESTIAL_NAVIGATION_SIGHT_H_
#define _CELESTIAL_NAVIGATION_SIGHT_H_

#include <list>
#include <vector>
#include "pidc.h"
#include "LunarDistanceEngine.h"

#ifdef __MSVC__
#define _USE_MATH_DEFINES
#include <float.h>
#include <iostream>
#include <limits>
#include <cmath>

#ifndef NAN
#define NAN std::numeric_limits<double>::quiet_NaN()
#endif

#ifndef INFINITY
#define INFINITY std::numeric_limits<double>::infinity()
#endif

#define isnan _isnan
#define isinf(x) (!_finite(x) && !_isnan(x))

#define trunc(d) (((d) > 0) ? floor(d) : ceil(d))
#endif

WX_DECLARE_LIST(wxRealPoint, wxRealPointList);

//    Sight
//----------------------------------------------------------------------------

const wxString SightType[] = {_("Altitude"), _("Azimuth"), _("Lunar"),
                              _("Horizon")};

class Sight : public wxObject {
public:
  enum Type { ALTITUDE, AZIMUTH, LUNAR, HORIZON };
  enum HorizonEvent { SUNRISE, SUNSET };
  enum BodyLimb {
    LOWER = 0,
    LUNAR_NEAR = 0,
    CENTER = 1,
    LUNAR_FAR = 1,
    UPPER = 2
  };

  Sight();
  Sight(Type type, wxString body, BodyLimb bodylimb, wxDateTime datetime,
        double timecertainty, double measurement, double measurementcertainty);

  ~Sight();

  void SetVisible(bool visible = true);  ///< set visibility and make points
                                         ///< selectable accordingly
  void SetSelected(bool selected = true);
  bool IsVisible() const { return m_bVisible; }
  bool IsCalculated() const { return m_bCalculated; }
  bool IsSelected() const { return m_bSelected; }

  void Recompute(int clock_offset);
  void RebuildPolygons();

  wxString Alminac(wxDateTime time, double lat, double lon, double ghaast,
                   double rad, double SD, double HP);
  void RecomputeAltitude();
  void RecomputeAzimuth();
  void RecomputeLunar();
  void RecomputeHorizon();

  void RebuildPolygonsAltitude();
  void RebuildPolygonsAzimuth();
  void RebuildPolygonsHorizon();

  double HorizonTrueBearing() const;
  bool HorizonEstimatedPosition(double* lat, double* lon);
  double HorizonEstimateUncertaintyNm() const;
  wxString HorizonEventName() const;
  wxString HorizonMeasurementText() const;

  bool m_bVisible;  // should this sight be drawn?
  bool m_bCalculated;
  bool m_bSelected;

  Type m_Type;
  wxString m_Body;
  bool m_IsStar;  // for stars, except the Sun
  bool m_IsPlanet;
  BodyLimb m_BodyLimb;

  wxDateTime m_DateTime;  // Time for the sight
  double m_TimeCertainty;

  double m_Measurement;  // Measurement angle in degrees (NaN is valid for all)
  double m_MeasurementCertainty;
  double m_LunarMoonAltitude, m_LunarBodyAltitude;
  BodyLimb m_LunarMoonLimb, m_LunarBodyLimb;
  BodyLimb m_LunarBodyDistanceLimb;
  double m_LunarMoonAltitudeUncertainty;
  double m_LunarBodyAltitudeUncertainty;
  bool m_LunarSeparateTimes;
  int m_LunarMoonTimeOffsetSeconds;
  int m_LunarBodyTimeOffsetSeconds;
  bool m_LunarMovingObserver;
  double m_LunarCourseTrue;
  double m_LunarSpeedKnots;

  double m_EyeHeight;         // Height above sea in meters
  double m_Temperature;       // Temperature in degrees celcius
  double m_Pressure;          // Pressure in millibars
  double m_IndexError;        // Error of measurement in degrees
  bool m_DipShort;            // DIP Short ?
  double m_DipShortDistance;  // DIP Short distance
  bool m_ArtificialHorizon;   // Artificial Horizon ?

  double m_ShiftNm;              // direction to move points
  double m_ShiftBearing;         // direction to move points
  bool m_bMagneticShiftBearing;  // use magnetic or true for shift

  wxString m_ColourName;
  wxColour m_Colour;  // Color of the sight

  virtual void Render(piDC* dc, PlugIn_ViewPort& pVP, double pix_per_mm);

  void BodyLocation(wxDateTime time, double* lat, double* lon, double* ghaash,
                    double* rad, double* dist, bool timeIsInstant = false);
  void AltitudeAzimuth(double lat1, double lon1, double lat2, double lon2,
                       double* hc, double* zn);
  void EstimateHs(double hc, double* hs, double* error);
  std::list<wxRealPoint> GetPoints();

  wxString m_CalcStr;

  wxDateTime m_CorrectedDateTime;

  /* for altitude */
  double m_ObservedAltitude; /* after all corrections are applied */

  /* for azimuth */
  bool m_bMagneticNorth;  // if azimuth angle is in magnetic coordinates

  /* for sunrise/sunset horizon events */
  HorizonEvent m_HorizonEvent;
  bool m_HorizonBearingProvided;
  bool m_HorizonBearingMagnetic;
  double m_HorizonBearing;
  double m_HorizonVariation;            // degrees, east positive
  double m_HorizonDeviation;            // degrees, east positive
  double m_HorizonBearingUncertainty;   // degrees
  double m_HorizonAltitudeUncertainty;  // arcminutes
  int m_HorizonQuality;                 // 0 clear, 1 hazy, 2 obstructed
  wxString m_HorizonTimeSource;
  bool m_HorizonEstimateValid;
  double m_HorizonEstimateLat;
  double m_HorizonEstimateLon;
  double m_HorizonEstimateRadiusNm;

  /* for lunar */
  long m_TimeCorrection;
  double m_LDC;
  bool m_LunarSolutionValid;
  wxString m_LunarSolutionError;
  std::vector<lunar_distance::TimeCandidate> m_LunarCandidates;
  lunar_distance::PositionResult m_LunarPositionResult;
  int m_LunarSelectedPosition;
  bool m_LunarUsesDe440;

  /* DR info */
  double m_DRLat;
  double m_DRLon;
  bool m_DRBoatPosition;
  bool m_DRMagneticAzimuth;

protected:
  double CalcAngle(wxRealPoint p1, wxRealPoint p2);
  double ComputeStepSize(double certainty, double stepsize, double min,
                         double max);

  wxRealPointList* MergePoints(wxRealPointList* p1, wxRealPointList* p2);
  wxRealPointList* ReduceToConvexPolygon(wxRealPointList* points);

  std::list<wxRealPointList*> polygons;
  wxRealPointList lines;

private:
  wxRealPoint DistancePoint(double altitude, double trace, double lat,
                            double lon);
  void BuildAltitudeLineOfPosition(double altitudemin, double altitudemax,
                                   double altitudestep, double tracestep,
                                   double timemin, double timemax,
                                   double timestep);
  bool BearingPoint(double altitude, double trace, double& rlat, double& rlon,
                    double& lasttrace, double& llat, double& llon, double lat,
                    double lon);
  void BuildBearingLineOfPosition(double altitudestep, double azimuthmin,
                                  double azimuthmax, double azimuthstep,
                                  double timemin, double timemax,
                                  double timestep);

  void DrawPolygon(PlugIn_ViewPort& VP, wxRealPointList& area, bool poly);

  piDC* m_dc;

  static int s_lastsightcolor;
};

double resolve_heading(double heading);
double resolve_heading_positive(double heading);

#endif
