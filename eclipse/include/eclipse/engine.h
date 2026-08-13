#ifndef CELESTIAL_ECLIPSE_ENGINE_H
#define CELESTIAL_ECLIPSE_ENGINE_H

#include "eclipse/geometry.h"
#include "eclipse/spk.h"
#include "eclipse/time.h"

#include <string>
#include <vector>

namespace eclipse {

class LunarLimbGrid;
class PckKernel;

enum EclipseType {
  kPartialEclipse,
  kAnnularEclipse,
  kTotalEclipse,
  kHybridEclipse
};

struct EclipseEvent {
  EclipseType type;
  double maximum_tt_jd;
  double delta_t_seconds;
  GeoPoint greatest_position;
  double axis_distance_km;
  double magnitude;

  EclipseEvent()
      : type(kPartialEclipse),
        maximum_tt_jd(0.0),
        delta_t_seconds(0.0),
        axis_distance_km(0.0),
        magnitude(0.0) {}
};

struct PathPoint {
  double tt_jd;
  GeoPoint northern_limit;
  GeoPoint southern_limit;
  GeoPoint central_line;
  double width_km;
  double magnitude;
  bool total;
};

struct ContactTime {
  bool valid;
  double tt_jd;
  double sun_altitude_deg;
  double sun_azimuth_deg;

  ContactTime()
      : valid(false), tt_jd(0.0), sun_altitude_deg(0.0), sun_azimuth_deg(0.0) {}
};

struct LocalContacts {
  ContactTime c1;
  ContactTime c2;
  ContactTime maximum;
  ContactTime c3;
  ContactTime c4;
  EclipseType type;
  double magnitude;
  double obscuration;
  double central_duration_seconds;
  bool limb_adjusted;

  LocalContacts()
      : type(kPartialEclipse),
        magnitude(0.0),
        obscuration(0.0),
        central_duration_seconds(0.0),
        limb_adjusted(false) {}
};

struct ContourSegment {
  GeoPoint first;
  GeoPoint second;
};

struct MagnitudeContour {
  double magnitude;
  std::vector<ContourSegment> segments;
};

class EclipseEngine {
public:
  bool OpenEphemeris(const std::string& path, std::string* error);
  bool State(double tt_jd, double delta_t_seconds, SolarLunarState* state,
             EarthOrientation* orientation, std::string* error) const;
  bool Footprint(double tt_jd, double delta_t_seconds, int angular_samples,
                 ShadowFootprint* footprint, std::string* error) const;
  bool Local(double tt_jd, double delta_t_seconds, const GeoPoint& observer,
             double height_metres, LocalCircumstances* circumstances,
             std::string* error) const;

  bool FindEvents(double start_tt_jd, double end_tt_jd,
                  std::vector<EclipseEvent>* events, std::string* error,
                  double delta_t_override_seconds = 0.0) const;
  bool EvaluateEvent(double maximum_tt_jd, double delta_t_seconds,
                     EclipseEvent* event, std::string* error) const;
  bool BuildCentralPath(const EclipseEvent& event, double interval_seconds,
                        std::vector<PathPoint>* path, std::string* error) const;
  bool SolveLocalContacts(const EclipseEvent& event, const GeoPoint& observer,
                          double height_metres, LocalContacts* contacts,
                          std::string* error) const;
  bool RefineContactsWithLola(const EclipseEvent& event,
                              const GeoPoint& observer, double height_metres,
                              const PckKernel& lunar_orientation,
                              const LunarLimbGrid& limb_grid,
                              LocalContacts* contacts,
                              std::string* error) const;
  bool BuildMagnitudeContours(const EclipseEvent& event,
                              const std::vector<double>& levels,
                              double grid_spacing_deg, double time_step_seconds,
                              std::vector<MagnitudeContour>* contours,
                              std::string* error) const;

private:
  double AxisDistance(double tt_jd, double delta_t_seconds,
                      std::string* error) const;
  bool Classify(double maximum_tt_jd, double delta_t_seconds,
                EclipseEvent* event, std::string* error) const;
  double ContactFunction(double tt_jd, double delta_t_seconds,
                         const GeoPoint& observer, double height_metres,
                         bool internal, LocalCircumstances* circumstances,
                         std::string* error) const;
  double ContactFunctionRadius(double tt_jd, double delta_t_seconds,
                               const GeoPoint& observer, double height_metres,
                               bool internal, double moon_radius_km,
                               std::string* error) const;

  SpkKernel kernel_;
  ReferenceEllipsoid ellipsoid_;
  PhysicalConstants constants_;
};

const char* EclipseTypeName(EclipseType type);

}  // namespace eclipse

#endif
