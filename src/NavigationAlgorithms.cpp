#include "NavigationAlgorithms.h"

#include "BodyCatalog.h"
#include "Sight.h"
#include "geodesic.h"
#include "moon.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

double Clamp(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}

double Wrap360(double value) {
  value = std::fmod(value, 360.0);
  return value < 0.0 ? value + 360.0 : value;
}

double Wrap180(double value) {
  value = Wrap360(value + 180.0) - 180.0;
  return value;
}

double SecondsBetween(const wxDateTime& a, const wxDateTime& b) {
  return static_cast<double>((a - b).GetMilliseconds().GetValue()) / 1000.0;
}

wxDateTime AddSeconds(const wxDateTime& utc, double seconds) {
  return utc + wxTimeSpan::Milliseconds(
                   static_cast<long long>(std::llround(seconds * 1000.0)));
}

double AngularSeparation(double lat1, double lon1, double lat2, double lon2) {
  const double a = lat1 * kDeg;
  const double b = lat2 * kDeg;
  const double dl = Wrap180(lon1 - lon2) * kDeg;
  return std::acos(Clamp(std::sin(a) * std::sin(b) +
                             std::cos(a) * std::cos(b) * std::cos(dl),
                         -1.0, 1.0)) /
         kDeg;
}

double EventValue(const wxString& body, const wxDateTime& utc,
                  const ObserverMotion& observer, double threshold,
                  bool limbEvent, double dipDegrees, BodyState* output) {
  double lat, lon;
  observer.PositionAt(utc, &lat, &lon);
  BodyState state = CelestialEphemeris::Evaluate(body, utc, lat, lon);
  if (output) *output = state;
  if (!state.valid) return std::numeric_limits<double>::quiet_NaN();
  if (limbEvent)
    return state.apparentAltitude + state.semidiameter + dipDegrees;
  return state.geometricAltitude - threshold;
}

bool RefineRoot(const wxString& body, const ObserverMotion& observer,
                const wxDateTime& left, const wxDateTime& right,
                double threshold, bool limbEvent, double dipDegrees,
                wxDateTime* root, BodyState* rootState) {
  wxDateTime lo = left, hi = right;
  double flo = EventValue(body, lo, observer, threshold, limbEvent, dipDegrees,
                          nullptr);
  double fhi = EventValue(body, hi, observer, threshold, limbEvent, dipDegrees,
                          nullptr);
  if (!std::isfinite(flo) || !std::isfinite(fhi) || flo * fhi > 0.0)
    return false;
  for (int i = 0; i < 20 && std::abs(SecondsBetween(hi, lo)) > 1.0; ++i) {
    wxDateTime mid = AddSeconds(lo, SecondsBetween(hi, lo) / 2.0);
    const double fm = EventValue(body, mid, observer, threshold, limbEvent,
                                 dipDegrees, nullptr);
    if (!std::isfinite(fm)) return false;
    if ((flo <= 0.0 && fm >= 0.0) || (flo >= 0.0 && fm <= 0.0)) {
      hi = mid;
      fhi = fm;
    } else {
      lo = mid;
      flo = fm;
    }
  }
  *root = AddSeconds(lo, SecondsBetween(hi, lo) / 2.0);
  double lat, lon;
  observer.PositionAt(*root, &lat, &lon);
  *rootState = CelestialEphemeris::Evaluate(body, *root, lat, lon);
  return rootState->valid;
}

void FindCrossings(const wxString& body, const wxDateTime& start,
                   const ObserverMotion& observer, double threshold,
                   bool limbEvent, double dipDegrees,
                   HorizonEventKind risingKind, HorizonEventKind settingKind,
                   std::vector<HorizonEventResult>* events,
                   bool* alwaysAbove, bool* alwaysBelow) {
  const int stepSeconds = 300;
  wxDateTime previousTime = start;
  double previous = EventValue(body, previousTime, observer, threshold,
                               limbEvent, dipDegrees, nullptr);
  double minimum = previous, maximum = previous;
  for (int seconds = stepSeconds; seconds <= 86400; seconds += stepSeconds) {
    const wxDateTime currentTime = AddSeconds(start, seconds);
    const double current = EventValue(body, currentTime, observer, threshold,
                                      limbEvent, dipDegrees, nullptr);
    if (std::isfinite(current)) {
      minimum = std::min(minimum, current);
      maximum = std::max(maximum, current);
    }
    if (std::isfinite(previous) && std::isfinite(current) &&
        ((previous < 0.0 && current >= 0.0) ||
         (previous > 0.0 && current <= 0.0))) {
      wxDateTime root;
      BodyState state;
      if (RefineRoot(body, observer, previousTime, currentTime, threshold,
                     limbEvent, dipDegrees, &root, &state)) {
        double lat, lon;
        observer.PositionAt(root, &lat, &lon);
        HorizonEventResult event;
        event.kind = previous < current ? risingKind : settingKind;
        event.utc = root;
        event.bearingTrue = state.azimuthTrue;
        event.observerLatitude = lat;
        event.observerLongitude = lon;
        events->push_back(event);
      }
    }
    previous = current;
    previousTime = currentTime;
  }
  if (alwaysAbove) *alwaysAbove = minimum > 0.0;
  if (alwaysBelow) *alwaysBelow = maximum < 0.0;
}

HorizonEventResult FindTransit(const wxString& body, const wxDateTime& start,
                               const ObserverMotion& observer,
                               HorizonEventKind kind) {
  int bestSeconds = 0;
  double bestAltitude = -1000.0;
  for (int seconds = 0; seconds <= 86400; seconds += 600) {
    double lat, lon;
    const wxDateTime utc = AddSeconds(start, seconds);
    observer.PositionAt(utc, &lat, &lon);
    const BodyState state = CelestialEphemeris::Evaluate(body, utc, lat, lon);
    if (state.valid && state.geometricAltitude > bestAltitude) {
      bestAltitude = state.geometricAltitude;
      bestSeconds = seconds;
    }
  }
  double lo = std::max(0, bestSeconds - 900);
  double hi = std::min(86400, bestSeconds + 900);
  for (int i = 0; i < 24; ++i) {
    const double m1 = lo + (hi - lo) / 3.0;
    const double m2 = hi - (hi - lo) / 3.0;
    const double h1 = EventValue(body, AddSeconds(start, m1), observer, 0.0,
                                 false, 0.0, nullptr);
    const double h2 = EventValue(body, AddSeconds(start, m2), observer, 0.0,
                                 false, 0.0, nullptr);
    if (h1 < h2)
      lo = m1;
    else
      hi = m2;
  }
  HorizonEventResult result;
  result.kind = kind;
  result.utc = AddSeconds(start, (lo + hi) / 2.0);
  observer.PositionAt(result.utc, &result.observerLatitude,
                      &result.observerLongitude);
  const BodyState state = CelestialEphemeris::Evaluate(
      body, result.utc, result.observerLatitude, result.observerLongitude);
  result.bearingTrue = state.azimuthTrue;
  return result;
}

double Median(std::vector<double> values) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const size_t middle = values.size() / 2;
  return values.size() % 2 ? values[middle]
                           : (values[middle - 1] + values[middle]) / 2.0;
}

double Hc(const FixObservation& observation, const ObserverMotion& baseMotion,
          double epochLat, double epochLon) {
  ObserverMotion motion = baseMotion;
  motion.latitude = epochLat;
  motion.longitude = epochLon;
  double lat, lon;
  motion.PositionAt(observation.utc, &lat, &lon);
  return CelestialEphemeris::Evaluate(observation.body, observation.utc, lat,
                                     lon)
      .geometricAltitude;
}

}  // namespace

void ObserverMotion::PositionAt(const wxDateTime& utc, double* lat,
                                double* lon) const {
  *lat = latitude;
  *lon = longitude;
  if (!moving || speedKnots == 0.0 || !referenceUtc.IsValid()) return;
  double distance = speedKnots * SecondsBetween(utc, referenceUtc) / 3600.0;
  double course = courseTrue;
  if (distance < 0.0) {
    distance = -distance;
    course = Wrap360(course + 180.0);
  }
  ll_gc_ll(latitude, longitude, course, distance, lat, lon);
  *lon = Wrap180(*lon);
}

double CelestialEphemeris::RefractionDegrees(double altitudeDeg,
                                             double pressureMb,
                                             double temperatureC) {
  if (altitudeDeg < -1.2 || altitudeDeg > 89.9) return 0.0;
  const double argument =
      (altitudeDeg + 10.3 / (altitudeDeg + 5.11)) * kDeg;
  if (std::abs(std::tan(argument)) < 1e-8) return 0.0;
  return (1.02 / std::tan(argument)) / 60.0 * (pressureMb / 1010.0) *
         (283.0 / (273.0 + temperatureC));
}

BodyState CelestialEphemeris::Evaluate(const wxString& body,
                                       const wxDateTime& utc,
                                       double observerLat,
                                       double observerLon, double pressureMb,
                                       double temperatureC) {
  BodyState result;
  result.body = body;
  result.utc = utc;
  const CelestialBodyInfo* info = BodyCatalog::Find(body);
  if (!info) {
    result.error = "Unsupported celestial body";
    return result;
  }

  Sight sight(Sight::ALTITUDE, info->name, Sight::CENTER, utc, 0.0, 0.0, 1.0);
  double ghaast = 0.0, radius = 0.0, distance = 0.0;
  sight.BodyLocation(utc, &result.latitude, &result.longitude, &ghaast,
                     &radius, &distance, true);
  sight.AltitudeAzimuth(observerLat, observerLon, result.latitude,
                        result.longitude, &result.geometricAltitude,
                        &result.azimuthTrue);
  result.azimuthTrue = Wrap360(result.azimuthTrue);
  result.declination = result.latitude;
  result.gha = Wrap360(-result.longitude);
  result.sha = Wrap360(result.gha - ghaast);
  result.distance = distance != 0.0 ? distance : radius;
  result.visualMagnitude = info->visualMagnitude;
  result.isStar = info->kind == CelestialBodyKind::Star;
  result.isPlanet = info->kind == CelestialBodyKind::Planet;

  double topocentricAltitude = result.geometricAltitude;
  if (info->kind == CelestialBodyKind::Sun) {
    result.semidiameter = 0.266564 / radius;
    result.horizontalParallax = 0.002442 / radius;
  } else if (info->kind == CelestialBodyKind::Moon && radius > EARTH_RADIUS) {
    result.horizontalParallax =
        std::asin(EARTH_RADIUS / radius) / kDeg;
    result.semidiameter =
        std::asin(MOON_MEAN_RADIUS / radius) / kDeg;
  } else if (info->kind == CelestialBodyKind::Planet && distance > 0.0) {
    // Planet distance is in AU; equatorial horizontal parallax at 1 AU.
    result.horizontalParallax = 0.002442 / distance;
  }
  if (result.horizontalParallax != 0.0)
    topocentricAltitude -=
        result.horizontalParallax * std::cos(result.geometricAltitude * kDeg);
  result.apparentAltitude =
      topocentricAltitude + RefractionDegrees(topocentricAltitude, pressureMb,
                                              temperatureC);
  result.valid = std::isfinite(result.geometricAltitude) &&
                 std::isfinite(result.azimuthTrue);
  if (!result.valid) result.error = "Ephemeris calculation failed";
  return result;
}

DailyEventsResult HorizonEventCalculator::Calculate(
    const wxDateTime& dayUtc, const ObserverMotion& observer,
    double eyeHeightMetres) {
  wxDateTime start(dayUtc);
  start.SetHour(0);
  start.SetMinute(0);
  start.SetSecond(0);
  start.SetMillisecond(0);
  DailyEventsResult result;
  const double dip = 1.76 * std::sqrt(std::max(0.0, eyeHeightMetres)) / 60.0;

  FindCrossings("Sun", start, observer, -18.0, false, 0.0,
                HorizonEventKind::AstronomicalDawn,
                HorizonEventKind::AstronomicalDusk, &result.events, nullptr,
                nullptr);
  FindCrossings("Sun", start, observer, -12.0, false, 0.0,
                HorizonEventKind::NauticalDawn,
                HorizonEventKind::NauticalDusk, &result.events, nullptr,
                nullptr);
  FindCrossings("Sun", start, observer, -6.0, false, 0.0,
                HorizonEventKind::CivilDawn, HorizonEventKind::CivilDusk,
                &result.events, nullptr, nullptr);
  FindCrossings("Sun", start, observer, 0.0, true, dip,
                HorizonEventKind::Sunrise, HorizonEventKind::Sunset,
                &result.events, &result.sunAlwaysAbove,
                &result.sunAlwaysBelow);
  FindCrossings("Moon", start, observer, 0.0, true, dip,
                HorizonEventKind::Moonrise, HorizonEventKind::Moonset,
                &result.events, &result.moonAlwaysAbove,
                &result.moonAlwaysBelow);
  result.events.push_back(FindTransit("Sun", start, observer,
                                      HorizonEventKind::UpperTransit));
  result.events.push_back(FindTransit("Moon", start, observer,
                                      HorizonEventKind::MoonTransit));
  std::sort(result.events.begin(), result.events.end(),
            [](const HorizonEventResult& a, const HorizonEventResult& b) {
              return a.utc.IsEarlierThan(b.utc);
            });
  return result;
}

wxString HorizonEventCalculator::Name(HorizonEventKind kind) {
  switch (kind) {
    case HorizonEventKind::AstronomicalDawn:
      return "Astronomical dawn";
    case HorizonEventKind::NauticalDawn:
      return "Nautical dawn";
    case HorizonEventKind::CivilDawn:
      return "Civil dawn";
    case HorizonEventKind::Sunrise:
      return "Sunrise";
    case HorizonEventKind::UpperTransit:
      return "Local apparent noon";
    case HorizonEventKind::Sunset:
      return "Sunset";
    case HorizonEventKind::CivilDusk:
      return "Civil dusk";
    case HorizonEventKind::NauticalDusk:
      return "Nautical dusk";
    case HorizonEventKind::AstronomicalDusk:
      return "Astronomical dusk";
    case HorizonEventKind::Moonrise:
      return "Moonrise";
    case HorizonEventKind::MoonTransit:
      return "Moon transit";
    case HorizonEventKind::Moonset:
      return "Moonset";
  }
  return "Event";
}

MoonInformation CalculateMoonInformation(const wxDateTime& utc,
                                          double observerLat,
                                          double observerLon) {
  const BodyState sun =
      CelestialEphemeris::Evaluate("Sun", utc, observerLat, observerLon);
  const BodyState moon =
      CelestialEphemeris::Evaluate("Moon", utc, observerLat, observerLon);
  const wxDateTime later = AddSeconds(utc, 6.0 * 3600.0);
  const BodyState laterSun =
      CelestialEphemeris::Evaluate("Sun", later, observerLat, observerLon);
  const BodyState laterMoon =
      CelestialEphemeris::Evaluate("Moon", later, observerLat, observerLon);
  MoonInformation result;
  result.elongationDegrees = AngularSeparation(
      sun.latitude, sun.longitude, moon.latitude, moon.longitude);
  result.illuminatedFraction =
      (1.0 - std::cos(result.elongationDegrees * kDeg)) / 2.0;
  const double laterElongation =
      AngularSeparation(laterSun.latitude, laterSun.longitude,
                        laterMoon.latitude, laterMoon.longitude);
  const double laterFraction = (1.0 - std::cos(laterElongation * kDeg)) / 2.0;
  result.waxing = laterFraction >= result.illuminatedFraction;
  const double phaseDegrees = result.waxing
                                  ? result.elongationDegrees
                                  : 360.0 - result.elongationDegrees;
  result.ageDays = phaseDegrees / 360.0 * 29.530588853;
  if (result.illuminatedFraction < 0.02)
    result.phaseName = "New Moon";
  else if (result.illuminatedFraction > 0.98)
    result.phaseName = "Full Moon";
  else if (std::abs(result.illuminatedFraction - 0.5) < 0.03)
    result.phaseName = result.waxing ? "First quarter" : "Last quarter";
  else if (result.illuminatedFraction < 0.5)
    result.phaseName = result.waxing ? "Waxing crescent" : "Waning crescent";
  else
    result.phaseName = result.waxing ? "Waxing gibbous" : "Waning gibbous";
  return result;
}

std::vector<MoonPhaseEvent> NextPrincipalMoonPhases(const wxDateTime& utc,
                                                    double observerLat,
                                                    double observerLon) {
  struct Target {
    const char* name;
    double age;
    double elongation;
  };
  static const Target targets[] = {{"New Moon", 0.0, 0.0},
                                   {"First quarter", 29.530588853 / 4.0, 90.0},
                                   {"Full Moon", 29.530588853 / 2.0, 180.0},
                                   {"Last quarter", 3.0 * 29.530588853 / 4.0,
                                    90.0}};
  const MoonInformation current =
      CalculateMoonInformation(utc, observerLat, observerLon);
  std::vector<MoonPhaseEvent> result;
  for (const auto& target : targets) {
    double days = target.age - current.ageDays;
    if (days <= 0.02) days += 29.530588853;
    wxDateTime guess = AddSeconds(utc, days * 86400.0);
    wxDateTime best = guess;
    double bestError = 1000.0;
    for (int hour = -36; hour <= 36; ++hour) {
      const wxDateTime candidate = AddSeconds(guess, hour * 3600.0);
      const MoonInformation info =
          CalculateMoonInformation(candidate, observerLat, observerLon);
      const double error = std::abs(info.elongationDegrees - target.elongation);
      if (error < bestError) {
        bestError = error;
        best = candidate;
      }
    }
    double lo = -3600.0, hi = 3600.0;
    for (int i = 0; i < 20; ++i) {
      const double a = lo + (hi - lo) / 3.0;
      const double b = hi - (hi - lo) / 3.0;
      const double ea = std::abs(
          CalculateMoonInformation(AddSeconds(best, a), observerLat, observerLon)
              .elongationDegrees -
          target.elongation);
      const double eb = std::abs(
          CalculateMoonInformation(AddSeconds(best, b), observerLat, observerLon)
              .elongationDegrees -
          target.elongation);
      if (ea > eb)
        lo = a;
      else
        hi = b;
    }
    MoonPhaseEvent event;
    event.name = target.name;
    event.utc = AddSeconds(best, (lo + hi) / 2.0);
    result.push_back(event);
  }
  std::sort(result.begin(), result.end(),
            [](const MoonPhaseEvent& a, const MoonPhaseEvent& b) {
              return a.utc.IsEarlierThan(b.utc);
            });
  return result;
}

std::vector<RankedBody> SightRanker::VisibleBodies(
    const wxDateTime& utc, double lat, double lon, double minAltitude,
    double maxAltitude, double maxMagnitude) {
  std::vector<RankedBody> result;
  const BodyState sun = CelestialEphemeris::Evaluate("Sun", utc, lat, lon);
  for (const auto& info : BodyCatalog::All()) {
    const BodyState state = CelestialEphemeris::Evaluate(info.name, utc, lat, lon);
    if (!state.valid || state.geometricAltitude < minAltitude ||
        state.geometricAltitude > maxAltitude ||
        (info.kind != CelestialBodyKind::Sun &&
         info.visualMagnitude > maxMagnitude))
      continue;
    RankedBody candidate;
    candidate.state = state;
    const double altitudeScore =
        1.0 - std::abs(state.geometricAltitude - 40.0) / 40.0;
    const double brightnessScore =
        Clamp((3.0 - info.visualMagnitude) / 5.0, 0.0, 1.0);
    const double twilightPenalty =
        info.kind == CelestialBodyKind::Star
            ? Clamp((sun.geometricAltitude + 12.0) / 12.0, 0.0, 1.0)
            : 0.0;
    candidate.score =
        100.0 * Clamp(0.65 * altitudeScore + 0.35 * brightnessScore -
                          0.55 * twilightPenalty,
                      0.0, 1.0);
    candidate.reason = wxString::Format("Hc %.1f%c, Zn %.0f%c, mag %.1f",
                                        state.geometricAltitude, 0x00b0,
                                        state.azimuthTrue, 0x00b0,
                                        info.visualMagnitude);
    result.push_back(candidate);
  }
  std::sort(result.begin(), result.end(),
            [](const RankedBody& a, const RankedBody& b) {
              return a.score > b.score;
            });
  return result;
}

std::vector<RankedCombination> SightRanker::BestCombinations(
    const std::vector<RankedBody>& bodies, unsigned count,
    unsigned maximumResults) {
  std::vector<RankedCombination> combinations;
  if ((count != 2 && count != 3) || bodies.size() < count) return combinations;
  const size_t limit = std::min<size_t>(bodies.size(), 18);
  for (size_t i = 0; i < limit; ++i) {
    for (size_t j = i + 1; j < limit; ++j) {
      const double d1 = std::abs(Wrap180(bodies[i].state.azimuthTrue -
                                         bodies[j].state.azimuthTrue));
      if (count == 2) {
        RankedCombination c;
        c.bodies = {bodies[i], bodies[j]};
        const double geometry = 1.0 - std::abs(d1 - 90.0) / 90.0;
        c.score = 0.55 * (bodies[i].score + bodies[j].score) / 2.0 +
                  45.0 * Clamp(geometry, 0.0, 1.0);
        c.reason = wxString::Format("Crossing angle %.0f%c", d1, 0x00b0);
        combinations.push_back(c);
      } else {
        for (size_t k = j + 1; k < limit; ++k) {
          const double d2 = std::abs(Wrap180(bodies[i].state.azimuthTrue -
                                             bodies[k].state.azimuthTrue));
          const double d3 = std::abs(Wrap180(bodies[j].state.azimuthTrue -
                                             bodies[k].state.azimuthTrue));
          const double smallest = std::min(d1, std::min(d2, d3));
          // The determinant of the 2-D unit-normal information matrix,
          // normalised to its ideal three-bearing value (2.25).
          double xx = 0, xy = 0, yy = 0;
          for (const RankedBody* b : {&bodies[i], &bodies[j], &bodies[k]}) {
            const double z = b->state.azimuthTrue * kDeg;
            const double x = std::cos(z), y = std::sin(z);
            xx += x * x;
            xy += x * y;
            yy += y * y;
          }
          const double geometry = Clamp((xx * yy - xy * xy) / 2.25, 0.0, 1.0);
          RankedCombination c;
          c.bodies = {bodies[i], bodies[j], bodies[k]};
          c.score = 0.5 * (bodies[i].score + bodies[j].score + bodies[k].score) /
                        3.0 +
                    50.0 * geometry;
          c.reason = wxString::Format("Geometry %.0f%%; closest bearings %.0f%c",
                                      geometry * 100.0, smallest, 0x00b0);
          combinations.push_back(c);
        }
      }
    }
  }
  std::sort(combinations.begin(), combinations.end(),
            [](const RankedCombination& a, const RankedCombination& b) {
              return a.score > b.score;
            });
  if (combinations.size() > maximumResults)
    combinations.resize(maximumResults);
  return combinations;
}

RunningFixResult RunningFixSolver::Solve(
    const std::vector<FixObservation>& sights, const ObserverMotion& motion,
    double initialLat, double initialLon) {
  RunningFixResult result;
  result.epochUtc = motion.referenceUtc;
  if (sights.size() < 2 || !motion.referenceUtc.IsValid()) {
    result.error = "At least two sights and a valid common epoch are required";
    return result;
  }
  double lat = initialLat, lon = initialLon;
  double normal00 = 0, normal01 = 0, normal11 = 0;
  for (unsigned iteration = 0; iteration < 20; ++iteration) {
    normal00 = normal01 = normal11 = 0.0;
    double rhs0 = 0, rhs1 = 0;
    for (const auto& sight : sights) {
      const double hc = Hc(sight, motion, lat, lon);
      const double epsilon = 1e-4;
      const double dLat = (Hc(sight, motion, lat + epsilon, lon) -
                           Hc(sight, motion, lat - epsilon, lon)) /
                          (2.0 * epsilon);
      const double dLon = (Hc(sight, motion, lat, lon + epsilon) -
                           Hc(sight, motion, lat, lon - epsilon)) /
                          (2.0 * epsilon);
      const double residual = sight.observedAltitude - hc;
      const double sigma = std::max(0.1, sight.uncertaintyMinutes) / 60.0;
      const double weight = 1.0 / (sigma * sigma);
      normal00 += weight * dLat * dLat;
      normal01 += weight * dLat * dLon;
      normal11 += weight * dLon * dLon;
      rhs0 += weight * dLat * residual;
      rhs1 += weight * dLon * residual;
    }
    const double determinant = normal00 * normal11 - normal01 * normal01;
    if (std::abs(determinant) < 1e-12) {
      result.error = "Sight geometry is singular; choose better-spaced azimuths";
      return result;
    }
    const double deltaLat = (normal11 * rhs0 - normal01 * rhs1) / determinant;
    const double deltaLon = (normal00 * rhs1 - normal01 * rhs0) / determinant;
    lat = Clamp(lat + deltaLat, -89.9, 89.9);
    lon = Wrap180(lon + deltaLon);
    result.iterations = iteration + 1;
    if (std::hypot(deltaLat, deltaLon * std::cos(lat * kDeg)) * 60.0 <
        0.001)
      break;
  }

  double sumSquares = 0.0;
  for (const auto& sight : sights) {
    FixResidual residual;
    residual.label = sight.label;
    residual.body = sight.body;
    residual.utc = sight.utc;
    residual.calculatedAltitude = Hc(sight, motion, lat, lon);
    residual.interceptMinutes =
        60.0 * (sight.observedAltitude - residual.calculatedAltitude);
    sumSquares += residual.interceptMinutes * residual.interceptMinutes;
    result.residuals.push_back(residual);
  }
  result.rmsMinutes = std::sqrt(sumSquares / sights.size());
  result.latitude = lat;
  result.longitude = lon;
  const double determinant = normal00 * normal11 - normal01 * normal01;
  if (determinant > 0.0) {
    // Convert covariance in degrees to an approximate nautical-mile tangent
    // plane before extracting ellipse axes.
    const double variance =
        sights.size() > 2 ? sumSquares / (sights.size() - 2) / 3600.0 : 1.0;
    const double c00 = variance * normal11 / determinant * 3600.0;
    const double c01 = -variance * normal01 / determinant * 3600.0 *
                       std::cos(lat * kDeg);
    const double c11 = variance * normal00 / determinant * 3600.0 *
                       std::pow(std::cos(lat * kDeg), 2);
    const double trace = c00 + c11;
    const double disc = std::sqrt(std::max(
        0.0, (c00 - c11) * (c00 - c11) + 4.0 * c01 * c01));
    result.semiMajorNm = std::sqrt(std::max(0.0, (trace + disc) / 2.0));
    result.semiMinorNm = std::sqrt(std::max(0.0, (trace - disc) / 2.0));
    result.ellipseBearing = Wrap360(
        0.5 * std::atan2(2.0 * c01, c00 - c11) / kDeg);
  }
  result.valid = true;
  return result;
}

SequenceStatistics SightSequenceAnalyzer::Analyze(
    const std::vector<FixObservation>& sights, const ObserverMotion& motion,
    double knownLat, double knownLon) {
  SequenceStatistics result;
  if (sights.size() < 2 || !motion.referenceUtc.IsValid()) return result;
  std::vector<double> values;
  for (const auto& sight : sights) {
    FixResidual residual;
    residual.label = sight.label;
    residual.body = sight.body;
    residual.utc = sight.utc;
    residual.calculatedAltitude = Hc(sight, motion, knownLat, knownLon);
    residual.interceptMinutes =
        60.0 * (sight.observedAltitude - residual.calculatedAltitude);
    values.push_back(residual.interceptMinutes);
    result.residuals.push_back(residual);
  }
  result.count = values.size();
  result.meanMinutes =
      std::accumulate(values.begin(), values.end(), 0.0) / values.size();
  result.medianMinutes = Median(values);
  std::vector<double> deviations;
  double sumSquares = 0.0;
  for (double value : values) {
    sumSquares += (value - result.meanMinutes) * (value - result.meanMinutes);
    deviations.push_back(std::abs(value - result.medianMinutes));
  }
  result.standardDeviationMinutes =
      std::sqrt(sumSquares / std::max<size_t>(1, values.size() - 1));
  result.madMinutes = Median(deviations);
  const double threshold = std::max(2.0, 3.0 * 1.4826 * result.madMinutes);
  for (auto& residual : result.residuals)
    residual.outlier =
        std::abs(residual.interceptMinutes - result.medianMinutes) > threshold;

  double meanTime = 0.0, inlierMean = 0.0;
  size_t inlierCount = 0;
  for (size_t i = 0; i < sights.size(); ++i) {
    if (result.residuals[i].outlier) continue;
    meanTime += SecondsBetween(sights[i].utc, motion.referenceUtc) / 3600.0;
    inlierMean += values[i];
    ++inlierCount;
  }
  if (!inlierCount) return result;
  meanTime /= inlierCount;
  inlierMean /= inlierCount;
  double covariance = 0.0, timeVariance = 0.0;
  for (size_t i = 0; i < sights.size(); ++i) {
    if (result.residuals[i].outlier) continue;
    const double t = SecondsBetween(sights[i].utc, motion.referenceUtc) / 3600.0;
    covariance += (t - meanTime) * (values[i] - inlierMean);
    timeVariance += (t - meanTime) * (t - meanTime);
  }
  result.trendMinutesPerHour =
      timeVariance > 0.0 ? covariance / timeVariance : 0.0;
  result.personalBiasMinutes = result.medianMinutes;
  result.valid = true;
  return result;
}

std::vector<AlmanacRow> BuildAlmanac(const wxDateTime& startUtc,
                                     unsigned hours,
                                     const std::vector<wxString>& bodies,
                                     const ObserverMotion& observer) {
  std::vector<AlmanacRow> rows;
  for (unsigned hour = 0; hour <= hours; ++hour) {
    const wxDateTime utc = AddSeconds(startUtc, hour * 3600.0);
    double lat, lon;
    observer.PositionAt(utc, &lat, &lon);
    for (const auto& body : bodies) {
      const BodyState state = CelestialEphemeris::Evaluate(body, utc, lat, lon);
      if (!state.valid) continue;
      AlmanacRow row;
      row.utc = utc;
      row.body = body;
      row.gha = state.gha;
      row.sha = state.sha;
      row.declination = state.declination;
      row.altitude = state.geometricAltitude;
      row.azimuth = state.azimuthTrue;
      rows.push_back(row);
    }
  }
  return rows;
}

wxString AlmanacToCsv(const std::vector<AlmanacRow>& rows) {
  wxString result = "UTC,Body,GHA_deg,SHA_deg,Declination_deg,Hc_deg,Zn_true_deg\n";
  for (const auto& row : rows)
    result += wxString::Format("%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                               row.utc.Format("%Y-%m-%dT%H:%M:%SZ",
                                              wxDateTime::UTC)
                                   .c_str(),
                               row.body.c_str(),
                               row.gha, row.sha, row.declination, row.altitude,
                               row.azimuth);
  return result;
}

double SolveLatitudeFromAltitude(const wxString& body, const wxDateTime& utc,
                                 double longitude, double observedAltitude,
                                 double initialLatitude) {
  double latitude = Clamp(initialLatitude, -89.0, 89.0);
  for (int i = 0; i < 20; ++i) {
    const double value = CelestialEphemeris::Evaluate(body, utc, latitude,
                                                      longitude)
                             .geometricAltitude -
                         observedAltitude;
    const double epsilon = 1e-4;
    const double derivative =
        (CelestialEphemeris::Evaluate(body, utc, latitude + epsilon, longitude)
             .geometricAltitude -
         CelestialEphemeris::Evaluate(body, utc, latitude - epsilon, longitude)
             .geometricAltitude) /
        (2.0 * epsilon);
    if (std::abs(derivative) < 1e-8) break;
    const double delta = value / derivative;
    latitude = Clamp(latitude - delta, -89.9, 89.9);
    if (std::abs(delta) < 1e-8) break;
  }
  return latitude;
}
