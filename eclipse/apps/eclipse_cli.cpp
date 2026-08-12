#include "eclipse/besselian.h"
#include "eclipse/astronomy.h"
#include "eclipse/geometry.h"
#include "eclipse/engine.h"
#include "eclipse/data_pack.h"
#include "eclipse/spk.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void Usage(std::ostream& stream) {
  stream << "Offline Celestial Eclipse Engine\n"
         << "Usage:\n"
         << "  eclipse-cli nasa-2027-central <UT-hours>\n"
         << "  eclipse-cli spk-position <kernel> <target> <center> <ET-seconds>\n"
         << "  eclipse-cli de440-2027-axis <kernel>\n"
         << "  eclipse-cli de440-axis <kernel> <TT-JD> <Delta-T-seconds>\n"
         << "  eclipse-cli de440-2027-row <kernel> <UT-hours>\n"
         << "  eclipse-cli find <kernel> <start-year> <end-year>\n"
         << "  eclipse-cli path-2027 <kernel> [interval-seconds]\n"
         << "  eclipse-cli local-2027 <kernel> <latitude> <longitude> [height-m]\n"
         << "  eclipse-cli verify-data <kernel>\n"
         << "\n"
         << "Example: eclipse-cli nasa-2027-central 10.1104722222\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "verify-data") {
    const eclipse::DataPackStatus status = eclipse::VerifyDe440s(argv[2]);
    if (!status.valid) {
      std::cerr << status.error << '\n';
      return 1;
    }
    std::cout << "valid,de440s,bytes," << status.bytes << ",sha256,"
              << status.sha256 << '\n';
    return 0;
  }

  if ((argc == 3 || argc == 4) &&
      std::string(argv[1]) == "path-2027") {
    double interval = 60.0;
    char* end = NULL;
    if (argc == 4) {
      interval = std::strtod(argv[3], &end);
      if (!end || *end != '\0' || interval <= 0.0) return 2;
    }
    eclipse::EclipseEngine engine;
    std::string error;
    if (!engine.OpenEphemeris(argv[2], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::EclipseEvent event;
    event.type = eclipse::kTotalEclipse;
    event.maximum_tt_jd = 2461619.9221;
    event.delta_t_seconds = 71.7;
    std::vector<eclipse::PathPoint> path;
    if (!engine.BuildCentralPath(event, interval, &path, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    std::cout << "ut,north_lat,north_lon,south_lat,south_lon,"
                 "central_lat,central_lon,width_km,magnitude\n";
    for (std::size_t index = 0; index < path.size(); ++index) {
      const eclipse::PathPoint& point = path[index];
      const eclipse::CalendarDateTime utc = eclipse::JulianDateToCalendar(
          point.tt_jd - event.delta_t_seconds / 86400.0);
      std::cout << std::setfill('0') << std::setw(2) << utc.hour << ':'
                << std::setw(2) << utc.minute << ':' << std::fixed
                << std::setprecision(3) << std::setw(6) << utc.second << ','
                << std::setprecision(7) << point.northern_limit.latitude_deg
                << ',' << point.northern_limit.longitude_deg << ','
                << point.southern_limit.latitude_deg << ','
                << point.southern_limit.longitude_deg << ','
                << point.central_line.latitude_deg << ','
                << point.central_line.longitude_deg << ','
                << std::setprecision(3) << point.width_km << ','
                << std::setprecision(6) << point.magnitude << '\n';
    }
    return 0;
  }

  if ((argc == 5 || argc == 6) &&
      std::string(argv[1]) == "local-2027") {
    char* end = NULL;
    const double latitude = std::strtod(argv[3], &end);
    if (!end || *end != '\0' || latitude < -90.0 || latitude > 90.0)
      return 2;
    const double longitude = std::strtod(argv[4], &end);
    if (!end || *end != '\0' || longitude < -180.0 || longitude > 180.0)
      return 2;
    double height = 0.0;
    if (argc == 6) {
      height = std::strtod(argv[5], &end);
      if (!end || *end != '\0') return 2;
    }
    eclipse::EclipseEngine engine;
    std::string error;
    if (!engine.OpenEphemeris(argv[2], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::EclipseEvent event;
    event.type = eclipse::kTotalEclipse;
    event.maximum_tt_jd = 2461619.9221;
    event.delta_t_seconds = 71.7;
    eclipse::LocalContacts contacts;
    if (!engine.SolveLocalContacts(event,
          eclipse::GeoPoint(latitude, longitude), height, &contacts, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    const eclipse::ContactTime* values[] = {
        &contacts.c1, &contacts.c2, &contacts.maximum,
        &contacts.c3, &contacts.c4};
    const char* names[] = {"C1", "C2", "MAX", "C3", "C4"};
    std::cout << "contact,ut,altitude_deg,azimuth_deg\n";
    for (int index = 0; index < 5; ++index) {
      if (!values[index]->valid) continue;
      const eclipse::CalendarDateTime utc = eclipse::JulianDateToCalendar(
          values[index]->tt_jd - event.delta_t_seconds / 86400.0);
      std::cout << names[index] << ',' << std::setfill('0')
                << std::setw(2) << utc.hour << ':' << std::setw(2)
                << utc.minute << ':' << std::fixed << std::setprecision(3)
                << std::setw(6) << utc.second << ',' << std::setprecision(4)
                << values[index]->sun_altitude_deg << ','
                << values[index]->sun_azimuth_deg << '\n';
    }
    std::cout << "type," << eclipse::EclipseTypeName(contacts.type)
              << "\nmagnitude," << std::setprecision(7)
              << contacts.magnitude << "\nobscuration,"
              << contacts.obscuration << "\ncentral_duration_seconds,"
              << std::setprecision(3) << contacts.central_duration_seconds
              << '\n';
    return contacts.c1.valid ? 0 : 1;
  }

  if (argc == 5 && std::string(argv[1]) == "find") {
    char* end = NULL;
    const long start_year = std::strtol(argv[3], &end, 10);
    if (!end || *end != '\0') return 2;
    const long end_year = std::strtol(argv[4], &end, 10);
    if (!end || *end != '\0' || end_year <= start_year) return 2;
    eclipse::CalendarDateTime start_calendar;
    start_calendar.year = static_cast<int>(start_year);
    eclipse::CalendarDateTime end_calendar;
    end_calendar.year = static_cast<int>(end_year);
    double start_jd = 0.0;
    double end_jd = 0.0;
    std::string error;
    if (!eclipse::CalendarToJulianDate(start_calendar, &start_jd, &error) ||
        !eclipse::CalendarToJulianDate(end_calendar, &end_jd, &error)) {
      std::cerr << error << '\n';
      return 2;
    }
    eclipse::EclipseEngine engine;
    if (!engine.OpenEphemeris(argv[2], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    std::vector<eclipse::EclipseEvent> events;
    if (!engine.FindEvents(start_jd, end_jd, &events, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    std::cout << "date,type,maximum_ut_jd,delta_t_seconds,latitude_deg,"
                 "longitude_deg,magnitude,axis_distance_km\n";
    for (std::size_t index = 0; index < events.size(); ++index) {
      const eclipse::EclipseEvent& event = events[index];
      const double ut_jd = event.maximum_tt_jd -
                           event.delta_t_seconds / 86400.0;
      const eclipse::CalendarDateTime date =
          eclipse::JulianDateToCalendar(ut_jd);
      std::cout << std::setfill('0') << std::setw(4) << date.year << '-'
                << std::setw(2) << date.month << '-' << std::setw(2) << date.day
                << 'T' << std::setw(2) << date.hour << ':' << std::setw(2)
                << date.minute << ':' << std::fixed << std::setprecision(3)
                << std::setw(6) << date.second << 'Z' << ','
                << eclipse::EclipseTypeName(event.type) << ','
                << std::setprecision(9) << ut_jd << ','
                << std::setprecision(3) << event.delta_t_seconds << ','
                << std::setprecision(6) << event.greatest_position.latitude_deg
                << ',' << event.greatest_position.longitude_deg << ','
                << event.magnitude << ',' << event.axis_distance_km << '\n';
    }
    return 0;
  }

  if (argc == 4 && std::string(argv[1]) == "de440-2027-row") {
    char* end = NULL;
    const double ut_hours = std::strtod(argv[3], &end);
    if (!end || *end != '\0' || ut_hours < 0.0 || ut_hours >= 24.0) return 2;
    const double delta_t = 71.7;
    const double tt_jd = 2461619.5 +
                         (ut_hours + delta_t / 3600.0) / 24.0;
    const double et = (tt_jd - 2451545.0) * 86400.0;
    eclipse::SpkKernel kernel;
    std::string error;
    if (!kernel.Open(argv[2], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::SolarLunarState state;
    if (!eclipse::AstrometricPosition(kernel, 301, 399, et,
                                      &state.moon_from_earth_km, &error) ||
        !eclipse::AstrometricPosition(kernel, 10, 399, et,
                                      &state.sun_from_earth_km, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::EarthOrientation orientation;
    orientation.tt_jd = tt_jd;
    orientation.ut1_jd = tt_jd - delta_t / 86400.0;
    const eclipse::ShadowFootprint footprint =
        eclipse::CentralShadowFootprint(
            state, orientation, eclipse::ReferenceEllipsoid(),
            eclipse::PhysicalConstants(), 7200);
    if (!footprint.central || footprint.boundary.empty()) {
      std::cerr << "No central footprint at this time\n";
      return 1;
    }
    eclipse::GeoPoint axis_before;
    eclipse::GeoPoint axis_after;
    for (int direction = -1; direction <= 1; direction += 2) {
      eclipse::SolarLunarState adjacent;
      if (!eclipse::AstrometricPosition(kernel, 301, 399, et + direction,
                                         &adjacent.moon_from_earth_km, &error) ||
          !eclipse::AstrometricPosition(kernel, 10, 399, et + direction,
                                         &adjacent.sun_from_earth_km, &error)) {
        std::cerr << error << '\n';
        return 1;
      }
      eclipse::EarthOrientation adjacent_orientation = orientation;
      adjacent_orientation.tt_jd += direction / 86400.0;
      adjacent_orientation.ut1_jd += direction / 86400.0;
      eclipse::GeoPoint* output = direction < 0 ? &axis_before : &axis_after;
      if (!eclipse::ShadowAxisPosition(adjacent, adjacent_orientation,
                                       eclipse::ReferenceEllipsoid(), output)) {
        std::cerr << "Unable to determine adjacent shadow axis\n";
        return 1;
      }
    }
    eclipse::GeoPoint north;
    eclipse::GeoPoint south;
    if (!eclipse::SelectCrossTrackLimits(footprint, axis_before, axis_after,
                                         &north, &south)) {
      std::cerr << "Unable to select cross-track limits\n";
      return 1;
    }
    const eclipse::LocalCircumstances local =
        eclipse::EvaluateLocalCircumstances(
            state, orientation, footprint.axis, 0.0,
            eclipse::ReferenceEllipsoid(), eclipse::PhysicalConstants());
    std::cout << std::fixed << std::setprecision(8)
              << "ut_hours,north_lat,north_lon,south_lat,south_lon,"
                 "axis_lat,axis_lon,magnitude,obscuration,sun_altitude,"
                 "sun_azimuth,path_width_km,boundary_points\n"
              << ut_hours << ',' << north.latitude_deg << ','
              << north.longitude_deg << ',' << south.latitude_deg << ','
              << south.longitude_deg << ',' << footprint.axis.latitude_deg
              << ',' << footprint.axis.longitude_deg << ',' << local.magnitude
              << ',' << local.obscuration << ',' << local.sun_altitude_deg
              << ',' << local.sun_azimuth_deg << ','
              << eclipse::SurfaceDistanceKm(north, south) << ','
              << footprint.boundary.size() << '\n';
    return 0;
  }

  if (argc == 5 && std::string(argv[1]) == "de440-axis") {
    char* end = NULL;
    const double tt_jd = std::strtod(argv[3], &end);
    if (!end || *end != '\0') return 2;
    const double delta_t = std::strtod(argv[4], &end);
    if (!end || *end != '\0') return 2;
    eclipse::SpkKernel kernel;
    std::string error;
    if (!kernel.Open(argv[2], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    const double et = (tt_jd - 2451545.0) * 86400.0;
    eclipse::SolarLunarState state;
    if (!eclipse::AstrometricPosition(kernel, 301, 399, et,
                                      &state.moon_from_earth_km, &error) ||
        !eclipse::AstrometricPosition(kernel, 10, 399, et,
                                      &state.sun_from_earth_km, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::EarthOrientation orientation;
    orientation.tt_jd = tt_jd;
    orientation.ut1_jd = tt_jd - delta_t / 86400.0;
    eclipse::GeoPoint point;
    if (!eclipse::ShadowAxisPosition(state, orientation,
                                     eclipse::ReferenceEllipsoid(), &point)) {
      std::cerr << "DE440 shadow axis misses WGS 84\n";
      return 1;
    }
    std::cout << std::fixed << std::setprecision(8)
              << "tt_jd,delta_t_seconds,latitude_deg,longitude_deg\n"
              << tt_jd << ',' << delta_t << ',' << point.latitude_deg << ','
              << point.longitude_deg << '\n';
    return 0;
  }

  if (argc == 3 && std::string(argv[1]) == "de440-2027-axis") {
    eclipse::SpkKernel kernel;
    std::string error;
    if (!kernel.Open(argv[2], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    const double tt_jd = 2461619.9221;
    const double et = (tt_jd - 2451545.0) * 86400.0;
    eclipse::SolarLunarState state;
    if (!eclipse::AstrometricPosition(kernel, 301, 399, et,
                                      &state.moon_from_earth_km, &error) ||
        !eclipse::AstrometricPosition(kernel, 10, 399, et,
                                      &state.sun_from_earth_km, &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::EarthOrientation orientation;
    orientation.tt_jd = tt_jd;
    orientation.ut1_jd = tt_jd - 71.7 / 86400.0;
    eclipse::GeoPoint point;
    if (!eclipse::ShadowAxisPosition(state, orientation,
                                     eclipse::ReferenceEllipsoid(), &point)) {
      std::cerr << "DE440 shadow axis misses WGS 84\n";
      return 1;
    }
    std::cout << std::fixed << std::setprecision(8)
              << "tt_jd,delta_t_seconds,latitude_deg,longitude_deg\n"
              << tt_jd << ",71.70000000," << point.latitude_deg << ','
              << point.longitude_deg << '\n';
    return 0;
  }

  if (argc == 6 && std::string(argv[1]) == "spk-position") {
    char* end = NULL;
    const long target = std::strtol(argv[3], &end, 10);
    if (!end || *end != '\0') return 2;
    const long center = std::strtol(argv[4], &end, 10);
    if (!end || *end != '\0') return 2;
    const double et = std::strtod(argv[5], &end);
    if (!end || *end != '\0') return 2;

    eclipse::SpkKernel kernel;
    std::string error;
    if (!kernel.Open(argv[2], &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    eclipse::Vector3 position;
    if (!kernel.Position(static_cast<std::int32_t>(target),
                         static_cast<std::int32_t>(center), et, &position,
                         &error)) {
      std::cerr << error << '\n';
      return 1;
    }
    std::cout << std::fixed << std::setprecision(9)
              << "target,center,et_seconds,x_km,y_km,z_km,distance_km\n"
              << target << ',' << center << ',' << et << ',' << position.x
              << ',' << position.y << ',' << position.z << ','
              << position.Norm() << '\n';
    return 0;
  }

  if (argc != 3 || std::string(argv[1]) != "nasa-2027-central") {
    Usage(argc == 1 ? std::cout : std::cerr);
    return argc == 1 ? 0 : 2;
  }

  char* end = NULL;
  const double ut_hours = std::strtod(argv[2], &end);
  if (!end || *end != '\0' || ut_hours < 0.0 || ut_hours >= 24.0) {
    std::cerr << "Invalid UT decimal hour: " << argv[2] << "\n";
    return 2;
  }

  const eclipse::BesselianElements elements =
      eclipse::Nasa2027Aug02Reference();
  const double tt_hours = ut_hours + elements.delta_t_seconds / 3600.0;
  const double tt_jd = 2461619.5 + tt_hours / 24.0;
  const eclipse::EvaluatedElements evaluated = eclipse::Evaluate(elements, tt_jd);

  eclipse::GeoPoint position;
  if (!eclipse::CentralLinePosition(evaluated,
                                    eclipse::ReferenceEllipsoid(), &position)) {
    std::cerr << "The shadow axis does not intersect WGS 84 at this time.\n";
    return 1;
  }

  std::cout << std::fixed << std::setprecision(8)
            << "ut_hours,latitude_deg,longitude_deg\n"
            << ut_hours << ',' << position.latitude_deg << ','
            << position.longitude_deg << '\n';
  return 0;
}
