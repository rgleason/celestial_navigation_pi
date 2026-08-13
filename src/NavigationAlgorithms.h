/******************************************************************************
 * Offline planning and numerical navigation algorithms.
 ******************************************************************************/

#ifndef CELESTIAL_NAVIGATION_ALGORITHMS_H
#define CELESTIAL_NAVIGATION_ALGORITHMS_H

#include <wx/datetime.h>
#include <wx/string.h>

#include <vector>

struct ObserverMotion {
  wxDateTime referenceUtc;
  double latitude = 0.0;
  double longitude = 0.0;
  double courseTrue = 0.0;
  double speedKnots = 0.0;
  bool moving = false;

  void PositionAt(const wxDateTime& utc, double* lat, double* lon) const;
};

struct BodyState {
  bool valid = false;
  wxString body;
  wxDateTime utc;
  double latitude = 0.0;       // geographic position (declination)
  double longitude = 0.0;      // geographic position
  double gha = 0.0;
  double sha = 0.0;
  double declination = 0.0;
  double geometricAltitude = 0.0;
  double apparentAltitude = 0.0;
  double azimuthTrue = 0.0;
  double semidiameter = 0.0;
  double horizontalParallax = 0.0;
  double distance = 0.0;
  double visualMagnitude = 0.0;
  bool isStar = false;
  bool isPlanet = false;
  wxString error;
};

class CelestialEphemeris {
public:
  static BodyState Evaluate(const wxString& body, const wxDateTime& utc,
                            double observerLat, double observerLon,
                            double pressureMb = 1010.0,
                            double temperatureC = 10.0);
  static double RefractionDegrees(double altitudeDeg, double pressureMb,
                                  double temperatureC);
};

enum class HorizonEventKind {
  AstronomicalDawn,
  NauticalDawn,
  CivilDawn,
  Sunrise,
  UpperTransit,
  Sunset,
  CivilDusk,
  NauticalDusk,
  AstronomicalDusk,
  Moonrise,
  MoonTransit,
  Moonset
};

struct HorizonEventResult {
  HorizonEventKind kind;
  wxDateTime utc;
  double bearingTrue = 0.0;
  double observerLatitude = 0.0;
  double observerLongitude = 0.0;
};

struct DailyEventsResult {
  std::vector<HorizonEventResult> events;
  bool sunAlwaysAbove = false;
  bool sunAlwaysBelow = false;
  bool moonAlwaysAbove = false;
  bool moonAlwaysBelow = false;
};

class HorizonEventCalculator {
public:
  static DailyEventsResult Calculate(const wxDateTime& dayUtc,
                                     const ObserverMotion& observer,
                                     double eyeHeightMetres = 0.0);
  static wxString Name(HorizonEventKind kind);
};

struct MoonInformation {
  double illuminatedFraction = 0.0;
  double elongationDegrees = 0.0;
  double ageDays = 0.0;
  bool waxing = false;
  wxString phaseName;
};

MoonInformation CalculateMoonInformation(const wxDateTime& utc,
                                          double observerLat,
                                          double observerLon);

struct MoonPhaseEvent {
  wxString name;
  wxDateTime utc;
};

std::vector<MoonPhaseEvent> NextPrincipalMoonPhases(const wxDateTime& utc,
                                                    double observerLat,
                                                    double observerLon);

struct RankedBody {
  BodyState state;
  double score = 0.0;
  wxString reason;
};

struct RankedCombination {
  std::vector<RankedBody> bodies;
  double score = 0.0;
  wxString reason;
};

class SightRanker {
public:
  static std::vector<RankedBody> VisibleBodies(const wxDateTime& utc,
                                               double lat, double lon,
                                               double minAltitude = 10.0,
                                               double maxAltitude = 75.0,
                                               double maxMagnitude = 3.0);
  static std::vector<RankedCombination> BestCombinations(
      const std::vector<RankedBody>& bodies, unsigned count,
      unsigned maximumResults = 10);
};

struct FixObservation {
  wxString label;
  wxString body;
  wxDateTime utc;
  double observedAltitude = 0.0;
  double uncertaintyMinutes = 1.0;
};

struct FixResidual {
  wxString label;
  wxString body;
  wxDateTime utc;
  double calculatedAltitude = 0.0;
  double interceptMinutes = 0.0;
  bool outlier = false;
};

struct RunningFixResult {
  bool valid = false;
  wxDateTime epochUtc;
  double latitude = 0.0;
  double longitude = 0.0;
  double rmsMinutes = 0.0;
  double semiMajorNm = 0.0;
  double semiMinorNm = 0.0;
  double ellipseBearing = 0.0;
  unsigned iterations = 0;
  wxString error;
  std::vector<FixResidual> residuals;
};

class RunningFixSolver {
public:
  static RunningFixResult Solve(const std::vector<FixObservation>& sights,
                                const ObserverMotion& motion,
                                double initialLat, double initialLon);
};

struct SequenceStatistics {
  bool valid = false;
  unsigned count = 0;
  double meanMinutes = 0.0;
  double standardDeviationMinutes = 0.0;
  double medianMinutes = 0.0;
  double madMinutes = 0.0;
  double trendMinutesPerHour = 0.0;
  double personalBiasMinutes = 0.0;
  std::vector<FixResidual> residuals;
};

class SightSequenceAnalyzer {
public:
  static SequenceStatistics Analyze(const std::vector<FixObservation>& sights,
                                    const ObserverMotion& motion,
                                    double knownLat, double knownLon);
};

struct AlmanacRow {
  wxDateTime utc;
  wxString body;
  double gha = 0.0;
  double sha = 0.0;
  double declination = 0.0;
  double altitude = 0.0;
  double azimuth = 0.0;
};

std::vector<AlmanacRow> BuildAlmanac(const wxDateTime& startUtc,
                                     unsigned hours,
                                     const std::vector<wxString>& bodies,
                                     const ObserverMotion& observer);
wxString AlmanacToCsv(const std::vector<AlmanacRow>& rows);

// Special-purpose workflows used by the planner's Noon & Polaris page.
double SolveLatitudeFromAltitude(const wxString& body, const wxDateTime& utc,
                                 double longitude, double observedAltitude,
                                 double initialLatitude);

#endif
