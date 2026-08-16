#include "AlmanacPaperTables.h"

#include "BodyCatalog.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <set>

namespace {
const double kPi = 3.14159265358979323846;
const unsigned kRowsPerAgetonPage = 60;
const unsigned kAgetonBlocks = 4;
const unsigned kAgetonEntries = 90 * 120 + 1;  // 0.5 minute spacing.

double Rad(double degrees) { return degrees * kPi / 180.0; }
double Deg(double radians) { return radians * 180.0 / kPi; }

double Wrap360(double value) {
  value = std::fmod(value, 360.0);
  return value < 0.0 ? value + 360.0 : value;
}

double TableAngle(double value) {
  value = std::fmod(std::fabs(value), 180.0);
  if (value > 90.0) value = 180.0 - value;
  return value;
}

double PrintedAngle(double value) {
  return std::round(TableAngle(value) * 120.0) / 120.0;
}

wxString DegreesMinutes(double value) {
  const double absolute = std::fabs(value);
  int degrees = static_cast<int>(std::floor(absolute));
  double minutes = (absolute - degrees) * 60.0;
  if (minutes >= 59.95) {
    ++degrees;
    minutes = 0.0;
  }
  return wxString::Format("%02d %04.1f'", degrees, minutes);
}

wxString SignedDegreesMinutes(double value) {
  return (value < 0 ? "-" : "+") + DegreesMinutes(value);
}

wxString IncrementAngle(double degrees) {
  int whole = static_cast<int>(std::floor(degrees));
  double minutes = (degrees - whole) * 60.0;
  if (minutes >= 59.95) {
    ++whole;
    minutes = 0.0;
  }
  return wxString::Format("%02d %04.1f'", whole, minutes);
}

wxString FunctionValue(long value) {
  return value == LONG_MAX ? wxString("--") : wxString::Format("%ld", value);
}

double InverseA(long target) {
  if (target == LONG_MAX) return 0.0;
  long bestDifference = LONG_MAX;
  unsigned best = 0;
  for (unsigned index = 1; index < kAgetonEntries; ++index) {
    const long value = AlmanacPaperTables::AgetonA(index / 120.0);
    const long difference = std::labs(value - target);
    if (difference < bestDifference) {
      bestDifference = difference;
      best = index;
    }
  }
  return best / 120.0;
}

AgetonReductionResult DirectReduction(double latitude, double declination,
                                      double localHourAngle) {
  AgetonReductionResult result;
  if (latitude < -90 || latitude > 90 || declination < -90 ||
      declination > 90) {
    result.error = "Latitude or declination is outside the table range.";
    return result;
  }
  const double lha = Wrap360(localHourAngle);
  const double sinH = std::sin(Rad(latitude)) * std::sin(Rad(declination)) +
                      std::cos(Rad(latitude)) * std::cos(Rad(declination)) *
                          std::cos(Rad(lha));
  result.computedAltitude = Deg(std::asin(std::max(-1.0, std::min(1.0, sinH))));
  const double y = std::sin(Rad(lha));
  const double x = std::cos(Rad(lha)) * std::sin(Rad(latitude)) -
                   std::tan(Rad(declination)) * std::cos(Rad(latitude));
  result.azimuthTrue = Wrap360(Deg(std::atan2(y, x)) + 180.0);
  result.valid = true;
  return result;
}

void AddInstructionPages(AlmanacDocument* document) {
  AlmanacPage workflow;
  workflow.section = "Calculator-free reference";
  workflow.title = "Paper-only dependency and recovery workflow";
  workflow.paragraphs = {
      "This section is computationally independent of OpenCPN. It supplies minute/second GHA increments, v/d proportional corrections, standard altitude corrections and a universal compact sight-reduction table. No trigonometric or scientific calculator is required.",
      "For every sight: preserve raw sextant altitude Hs, body and limb, UTC/watch reading, index correction, height of eye and horizon notes. Obtain Ho using the printed correction tables. Obtain GHA and declination from the daily page plus the increment and v/d page. Combine GHA with assumed longitude to obtain LHA. Use the compact reduction table to obtain Hc and Zn, then plot 60(Ho-Hc) nautical miles toward or away.",
      "The compact table is generated independently from log-cosecant/log-secant functions at half-minute intervals. It follows the public Ageton secant-cosecant construction. Near its singular cases, alter the assumed longitude or latitude slightly and repeat; never force a blank or infinite table entry.",
      "Method provenance: Arthur A. Ageton, 'The Secant-Cosecant Method', U.S. Naval Institute Proceedings, October 1931. Hourly quantities and v/d notation follow conventional Nautical Almanac practice; the direct Hc/Zn concept follows the purpose of NGA Pub. 229. These pages are newly computed and are not reproductions of those publications.",
      "The dependency manifest on the cover must say COMPLETE: CALCULATOR-FREE before this document is treated as a paper-only backup."};
  document->pages.push_back(workflow);

  AlmanacPage increments;
  increments.section = "Calculator-free reference";
  increments.title = "Using the increments and v/d correction pages";
  increments.paragraphs = {
      "Enter the page for the whole UTC minute after the preceding hour, then the seconds row. Add the Sun/planet, Aries or Moon base increment to the hourly GHA.",
      "For Moon and planets also take v from the hourly daily page. On the lower half of the same minute page enter with the absolute v value and add the printed correction; subtract only when the hourly v carries a minus sign. The Moon base rate is 14 deg 19.0 min/hour, planets 15 deg/hour and Aries 15 deg 02.46 min/hour.",
      "Use d in exactly the same proportional table to change declination from the hourly value. The sign printed beside d gives the direction. Select the 00 or 30-second correction nearest the actual seconds, or interpolate between them with basic arithmetic. Even without interpolation the time rounding contributes at most about 0.1 minute at the largest printed v/d argument.",
      "Stars use GHA Aries plus the printed SHA. The Sun uses the mean-solar increment and normally needs no v correction."};
  document->pages.push_back(increments);

  AlmanacPage ageton;
  ageton.section = "Calculator-free reference";
  ageton.title = "Compact Ageton reduction procedure";
  ageton.paragraphs = {
      "Use meridian angle t = LHA when LHA is at most 180 deg, otherwise t = 360 deg - LHA. Treat latitude L and declination D as signed north-positive angles. Columns A and B are 100000 log10(cosec angle) and 100000 log10(sec angle), rounded to integers.",
      "1: A(R) = A(t) + B(D); enter column A backwards with the sum to obtain R. 2: A(K) = A(D) - B(R); enter A backwards for K. If t exceeds 90 deg use 180 deg - K, retaining the name/sign of D.",
      "3: A(Hc) = B(R) + B(K-L); enter A backwards for the magnitude of Hc. Hc is negative when cos(K-L) is negative. 4: A(Z) = A(R) - B(Hc); enter A backwards for acute Z. Normalize P=K-L into -180..+180 deg. Keep Z acute when P has the same sign as the assumed latitude; otherwise use 180 deg-Z (at zero latitude use the north convention).",
      "Convert polar Z to true Zn: north latitude and LHA<180: 360-Z; north and LHA>180: Z; south and LHA<180: 180+Z; south and LHA>180: 180-Z. Normalize to 000-360 deg. These quadrant rules are regression-tested against direct vector reductions.",
      "Special cases: blank entries mean a secant/cosecant singularity. Shift the assumed longitude by a convenient whole degree and recompute, or use a precomputed voyage worksheet. Avoid reductions within a few degrees of the zenith, where azimuth and all tabular methods become ill-conditioned."};
  document->pages.push_back(ageton);
}

void AddIncrementPages(AlmanacDocument* document) {
  const double rates[] = {15.0, 15.0 + 2.46 / 60.0,
                          14.0 + 19.0 / 60.0};
  for (unsigned minute = 0; minute < 60; ++minute) {
    AlmanacPage page;
    page.section = "Increments and corrections";
    page.title = wxString::Format("Minute %02u - GHA increments and v/d corrections", minute);
    page.subtitle = "Upper: exact base increment. Lower: v/d correction at :00 and :30; choose nearest or interpolate.";
    AlmanacTable base;
    base.headings = {"s", "Sun/P", "Aries", "Moon", "s", "Sun/P", "Aries", "Moon"};
    base.relativeWidths = {0.45, 1, 1, 1, 0.45, 1, 1, 1};
    for (unsigned row = 0; row < 30; ++row) {
      std::vector<wxString> cells;
      for (unsigned block = 0; block < 2; ++block) {
        const unsigned second = row + block * 30;
        const double fraction = (minute * 60.0 + second) / 3600.0;
        cells.push_back(wxString::Format("%02u", second));
        for (double rate : rates) cells.push_back(IncrementAngle(rate * fraction));
      }
      base.rows.push_back(cells);
    }
    page.tables.push_back(base);

    AlmanacTable correction;
    for (unsigned block = 0; block < 5; ++block) {
      correction.headings.push_back("v/d");
      correction.headings.push_back("C:00");
      correction.headings.push_back("C:30");
    }
    const unsigned arguments = 250;  // 0.0 to 24.9 minutes per hour.
    const unsigned rows = (arguments + 4) / 5;
    const double fraction00 = minute / 60.0;
    const double fraction30 = (minute + 0.5) / 60.0;
    for (unsigned row = 0; row < rows; ++row) {
      std::vector<wxString> cells;
      for (unsigned block = 0; block < 5; ++block) {
        const unsigned index = row + block * rows;
        if (index < arguments) {
          const double argument = index / 10.0;
          cells.push_back(wxString::Format("%.1f", argument));
          cells.push_back(wxString::Format("%.1f'", argument * fraction00));
          cells.push_back(wxString::Format("%.1f'", argument * fraction30));
        } else {
          cells.push_back("");
          cells.push_back("");
          cells.push_back("");
        }
      }
      correction.rows.push_back(cells);
    }
    page.tables.push_back(correction);
    document->pages.push_back(page);
  }
}

void AddAgetonPages(AlmanacDocument* document) {
  const unsigned perPage = kRowsPerAgetonPage * kAgetonBlocks;
  const unsigned pages = (kAgetonEntries + perPage - 1) / perPage;
  for (unsigned part = 0; part < pages; ++part) {
    AlmanacPage page;
    page.section = "Compact reduction tables";
    page.title = wxString::Format("Ageton A/B table - part %u of %u", part + 1, pages);
    page.subtitle = "Angle spacing 0.5 minute; A=100000 log csc, B=100000 log sec. Read A backwards when solving for an angle.";
    AlmanacTable table;
    for (unsigned block = 0; block < kAgetonBlocks; ++block) {
      table.headings.push_back("Angle");
      table.headings.push_back("A");
      table.headings.push_back("B");
    }
    for (unsigned row = 0; row < kRowsPerAgetonPage; ++row) {
      std::vector<wxString> cells;
      for (unsigned block = 0; block < kAgetonBlocks; ++block) {
        const unsigned index = part * perPage + block * kRowsPerAgetonPage + row;
        if (index < kAgetonEntries) {
          const double angle = index / 120.0;
          cells.push_back(DegreesMinutes(angle));
          cells.push_back(FunctionValue(AlmanacPaperTables::AgetonA(angle)));
          cells.push_back(FunctionValue(AlmanacPaperTables::AgetonB(angle)));
        } else {
          cells.push_back("");
          cells.push_back("");
          cells.push_back("");
        }
      }
      table.rows.push_back(cells);
    }
    page.tables.push_back(table);
    document->pages.push_back(page);
  }
}

AlmanacTable MoonCorrectionTable(bool upperLimb, unsigned firstAltitude,
                                 unsigned lastAltitude) {
  AlmanacTable table;
  table.headings.push_back("Ha");
  for (unsigned hp = 54; hp <= 62; ++hp)
    table.headings.push_back(wxString::Format("HP %u'", hp));
  for (unsigned altitude = firstAltitude; altitude <= lastAltitude; ++altitude) {
    std::vector<wxString> row;
    row.push_back(wxString::Format("%u deg", altitude));
    const double refraction = CelestialEphemeris::RefractionDegrees(
        altitude, 1010.0, 10.0) * 60.0;
    for (unsigned hp = 54; hp <= 62; ++hp) {
      const double parallax = hp * std::cos(Rad(altitude));
      const double sd = 0.2725 * hp *
                        (1.0 + std::sin(Rad(altitude)) *
                                   std::sin(Rad(hp / 60.0)));
      const double correction = -refraction + parallax +
                                (upperLimb ? -sd : sd);
      row.push_back(wxString::Format("%+.1f'", correction));
    }
    table.rows.push_back(row);
  }
  return table;
}

void AddAltitudeCorrectionPages(AlmanacDocument* document) {
  AlmanacPage dip;
  dip.section = "Altitude correction tables";
  dip.title = "Dip of the sea horizon";
  dip.subtitle = "Subtract dip from sextant altitude. Height of eye 0.0-30.0 metres; standard terrestrial refraction.";
  AlmanacTable dipTable;
  for (unsigned block = 0; block < 5; ++block) {
    dipTable.headings.push_back("Eye m");
    dipTable.headings.push_back("Dip");
  }
  for (unsigned row = 0; row < 61; ++row) {
    std::vector<wxString> cells;
    for (unsigned block = 0; block < 5; ++block) {
      const unsigned index = row + block * 61;
      if (index <= 300) {
        const double height = index / 10.0;
        cells.push_back(wxString::Format("%.1f", height));
        cells.push_back(wxString::Format("%.1f'", 1.76 * std::sqrt(height)));
      } else {
        cells.push_back("");
        cells.push_back("");
      }
    }
    dipTable.rows.push_back(cells);
  }
  dip.tables.push_back(dipTable);
  document->pages.push_back(dip);

  AlmanacPage refraction;
  refraction.section = "Altitude correction tables";
  refraction.title = "Refraction and atmosphere multiplier";
  refraction.subtitle = "Subtract standard refraction. Multiply only when non-standard pressure/temperature matters; otherwise use the standard column directly.";
  AlmanacTable refractionTable;
  std::vector<double> refractionAltitudes;
  for (unsigned index = 0; index <= 100; ++index)
    refractionAltitudes.push_back(index / 10.0);
  for (unsigned index = 21; index <= 180; ++index)
    refractionAltitudes.push_back(index / 2.0);
  for (unsigned block = 0; block < 5; ++block) {
    refractionTable.headings.push_back("Ha");
    refractionTable.headings.push_back("R");
  }
  const unsigned refractionRows =
      static_cast<unsigned>((refractionAltitudes.size() + 4) / 5);
  for (unsigned row = 0; row < refractionRows; ++row) {
    std::vector<wxString> cells;
    for (unsigned block = 0; block < 5; ++block) {
      const unsigned index = row + block * refractionRows;
      if (index < refractionAltitudes.size()) {
        const double altitude = refractionAltitudes[index];
        cells.push_back(wxString::Format("%.1f", altitude));
        cells.push_back(wxString::Format("%.1f'", CelestialEphemeris::RefractionDegrees(
            altitude, 1010.0, 10.0) * 60.0));
      } else {
        cells.push_back("");
        cells.push_back("");
      }
    }
    refractionTable.rows.push_back(cells);
  }
  refraction.tables.push_back(refractionTable);
  AlmanacTable atmosphere;
  atmosphere.headings = {"Temp C", "950 hPa", "975", "1000", "1025", "1050"};
  for (int temperature = -10; temperature <= 40; temperature += 10) {
    std::vector<wxString> row{wxString::Format("%+d", temperature)};
    for (int pressure = 950; pressure <= 1050; pressure += 25)
      row.push_back(wxString::Format("%.3f", (pressure / 1010.0) *
                                                    (283.0 / (273.0 + temperature))));
    atmosphere.rows.push_back(row);
  }
  refraction.tables.push_back(atmosphere);
  document->pages.push_back(refraction);

  AlmanacPage parallax;
  parallax.section = "Altitude correction tables";
  parallax.title = "Sun and planet parallax in altitude";
  parallax.subtitle = "Add the correction for the hourly HP and apparent altitude. Interpolate HP and altitude; stars require none. Moon corrections are on the following dedicated pages.";
  AlmanacTable parallaxTable;
  parallaxTable.headings = {"Ha", "HP 0.1'", "0.2'", "0.3'", "0.4'", "0.5'", "0.6'", "0.7'", "0.8'", "0.9'", "1.0'"};
  for (unsigned altitude = 0; altitude <= 90; altitude += 2) {
    std::vector<wxString> row{wxString::Format("%u", altitude)};
    for (unsigned hp = 1; hp <= 10; ++hp)
      row.push_back(wxString::Format("%.1f'", (hp / 10.0) *
                                              std::cos(Rad(altitude))));
    parallaxTable.rows.push_back(row);
  }
  parallax.tables.push_back(parallaxTable);
  document->pages.push_back(parallax);

  for (unsigned limb = 0; limb < 2; ++limb) {
    for (unsigned part = 0; part < 2; ++part) {
      AlmanacPage moon;
      moon.section = "Altitude correction tables";
      moon.title = wxString::Format("Moon %s-limb total correction - part %u",
                                    limb ? "upper" : "lower", part + 1);
      moon.subtitle = "Add the signed value to apparent centre/limb altitude after index correction and dip. Standard atmosphere; interpolate HP and altitude.";
      const unsigned first = part ? 46 : 0;
      const unsigned last = part ? 90 : 45;
      moon.tables.push_back(MoonCorrectionTable(limb != 0, first, last));
      document->pages.push_back(moon);
    }
  }
}

void AddCorrectionVisualPage(AlmanacDocument* document) {
  AlmanacPage page;
  page.section = "Visual reference aids";
  page.title = "Altitude-correction overview curves";
  page.subtitle = "Use these curves to understand sensitivity and catch gross errors; use the adjacent printed tables for final values.";
  AlmanacPlot dip;
  dip.title = "Dip versus height of eye";
  dip.xLabel = "Height metres";
  dip.yLabel = "Dip arcminutes (subtract)";
  dip.xMinimum = 0;
  dip.xMaximum = 30;
  dip.yMinimum = 0;
  dip.yMaximum = 10;
  AlmanacPlotSeries dipSeries;
  dipSeries.label = "Standard dip";
  for (unsigned index = 0; index <= 60; ++index) {
    const double height = index / 2.0;
    dipSeries.x.push_back(height);
    dipSeries.y.push_back(1.76 * std::sqrt(height));
  }
  dip.series.push_back(dipSeries);
  page.plots.push_back(dip);

  AlmanacPlot refraction;
  refraction.title = "Standard refraction versus apparent altitude";
  refraction.xLabel = "Apparent altitude degrees";
  refraction.yLabel = "Refraction arcminutes (subtract)";
  refraction.xMinimum = 0;
  refraction.xMaximum = 30;
  refraction.yMinimum = 0;
  refraction.yMaximum = 30;
  AlmanacPlotSeries refractionSeries;
  refractionSeries.label = "1010 hPa, 10 C";
  for (unsigned index = 0; index <= 60; ++index) {
    const double altitude = index / 2.0;
    refractionSeries.x.push_back(altitude);
    refractionSeries.y.push_back(CelestialEphemeris::RefractionDegrees(
        altitude, 1010.0, 10.0) * 60.0);
  }
  refraction.series.push_back(refractionSeries);
  page.plots.push_back(refraction);
  document->pages.push_back(page);
}

std::vector<int> DirectLatitudes(const AlmanacRequest& request) {
  double south = request.latitude, north = request.latitude;
  if (request.coverage == AlmanacCoverage::Global) {
    south = -89;
    north = 89;
  } else if (request.coverage == AlmanacCoverage::LatitudeBand) {
    south = request.latitudeSouth;
    north = request.latitudeNorth;
  } else if (request.coverage == AlmanacCoverage::PlannedRoute &&
             !request.route.empty()) {
    south = north = request.route.front().latitude;
    for (const AlmanacRoutePoint& point : request.route) {
      south = std::min(south, point.latitude);
      north = std::max(north, point.latitude);
    }
    const double margin = request.routeCorridorNm / 60.0;
    south -= margin;
    north += margin;
  } else {
    south -= 2.0;
    north += 2.0;
  }
  const int first = std::max(-89, static_cast<int>(std::floor(south)));
  const int last = std::min(89, static_cast<int>(std::ceil(north)));
  std::vector<int> values;
  for (int latitude = first; latitude <= last; ++latitude)
    values.push_back(latitude);
  return values;
}

std::vector<int> DirectDeclinations(const AlmanacRequest& request) {
  std::set<int> values;
  if (request.fullDirectReductionCoverage) {
    for (int declination = -89; declination <= 89; ++declination)
      values.insert(declination);
  } else {
    const std::vector<CelestialBodyInfo> bodies = BodyCatalog::Navigational(
        request.includeSun, request.includeMoon, request.includePlanets,
        request.includeStars);
    wxDateTime day(request.fromUtc.GetDay(), request.fromUtc.GetMonth(),
                   request.fromUtc.GetYear(), 0, 0, 0);
    wxDateTime last(request.toUtc.GetDay(), request.toUtc.GetMonth(),
                    request.toUtc.GetYear(), 0, 0, 0);
    while (!day.IsLaterThan(last)) {
      for (const CelestialBodyInfo& body : bodies) {
        const BodyState state = CelestialEphemeris::Evaluate(
            body.name, UtcDateTime::AddSeconds(day, request.dut1Seconds), 0, 0);
        if (!state.valid) continue;
        values.insert(std::max(-89, std::min(
            89, static_cast<int>(std::floor(state.declination)))));
        values.insert(std::max(-89, std::min(
            89, static_cast<int>(std::ceil(state.declination)))));
      }
      day = UtcDateTime::AddSeconds(day, 86400.0);
    }
  }
  return std::vector<int>(values.begin(), values.end());
}

void AddDirectReductionPages(const AlmanacRequest& request,
                             AlmanacDocument* document) {
  AlmanacPage instructions;
  instructions.section = "Direct Hc/Zn reduction tables";
  instructions.title = "Using the voyage-specific direct reduction tables";
  instructions.paragraphs = {
      "These pages precompute the spherical triangle for whole-degree assumed latitude, declination and meridian angle. They are the quickest calculator-free working method in the selected coverage; the compact Ageton table remains the universal fallback.",
      "Choose the nearest whole-degree assumed latitude and declination. Use meridian angle t: for LHA 000-180 use t=LHA; for LHA 180-360 use t=360-LHA. Enter the row for whole-degree t and interpolate Hc for minutes of latitude, declination and LHA. For demanding work, repeat at the surrounding entries and interpolate bilinearly.",
      "The printed Zn is for LHA 000-180. For an eastern meridian angle (LHA 180-360), mirror the azimuth: Zn east = 360 deg - printed Zn. Normalize 000-360. When Hc is below the horizon or the geometry is close to the zenith, select another body or use the universal method with care.",
      "Direct-table pruning never removes the hourly ephemeris or universal reduction pages. If the actual latitude or declination lies outside these direct pages, use Ageton rather than extrapolating."};
  document->pages.push_back(instructions);
  const std::vector<int> latitudes = DirectLatitudes(request);
  const std::vector<int> declinations = DirectDeclinations(request);
  const unsigned declinationsPerBlock = 5;
  for (int latitude : latitudes) {
    for (size_t firstDec = 0; firstDec < declinations.size();
         firstDec += declinationsPerBlock) {
      for (unsigned part = 0; part < 3; ++part) {
        AlmanacPage page;
        page.section = "Direct Hc/Zn reduction tables";
        page.title = wxString::Format(
            "Assumed latitude %d deg - direct Hc/Zn - part %u", latitude,
            part + 1);
        page.subtitle =
            "Enter with whole-degree LHA and declination; interpolate. Hc is signed altitude, Zn is true bearing. Universal Ageton pages remain the off-track fallback.";
        AlmanacTable table;
        table.headings.push_back("LHA");
        const size_t lastDec = std::min(declinations.size(),
                                        firstDec + declinationsPerBlock);
        for (size_t index = firstDec; index < lastDec; ++index) {
          table.headings.push_back(wxString::Format("Hc D%+d", declinations[index]));
          table.headings.push_back("Zn");
        }
        const unsigned firstLha = part == 0 ? 0 : (part == 1 ? 61 : 122);
        const unsigned lastLha = part == 0 ? 60 : (part == 1 ? 121 : 180);
        for (unsigned lha = firstLha; lha <= lastLha; ++lha) {
          std::vector<wxString> row{wxString::Format("%03u", lha)};
          for (size_t index = firstDec; index < lastDec; ++index) {
            const AgetonReductionResult reduced =
                DirectReduction(latitude, declinations[index], lha);
            row.push_back(SignedDegreesMinutes(reduced.computedAltitude));
            row.push_back(wxString::Format("%03.1f", reduced.azimuthTrue));
          }
          table.rows.push_back(row);
        }
        page.tables.push_back(table);
        document->pages.push_back(page);
      }
    }
  }
}
}  // namespace

unsigned AlmanacPaperTables::IncrementPageCount() { return 60; }
unsigned AlmanacPaperTables::ReductionPageCount() {
  return (kAgetonEntries + kRowsPerAgetonPage * kAgetonBlocks - 1) /
         (kRowsPerAgetonPage * kAgetonBlocks);
}
unsigned AlmanacPaperTables::AltitudeCorrectionPageCount() { return 7; }
unsigned AlmanacPaperTables::DirectReductionPageCount(
    const AlmanacRequest& request) {
  const unsigned latitudes = static_cast<unsigned>(DirectLatitudes(request).size());
  const unsigned declinations =
      static_cast<unsigned>(DirectDeclinations(request).size());
  return latitudes * ((declinations + 4) / 5) * 3;
}
unsigned AlmanacPaperTables::InstructionPageCount() { return 3; }

unsigned AlmanacPaperTables::PageCount(const AlmanacRequest& request) {
  unsigned pages = 0;
  const bool any = request.includeIncrementTables ||
                   request.includeCompactReductionTables ||
                   request.includeAltitudeCorrectionTables;
  if (any) pages += InstructionPageCount();
  if (request.includeIncrementTables) pages += IncrementPageCount();
  if (request.includeCompactReductionTables) pages += ReductionPageCount();
  if (request.includeAltitudeCorrectionTables)
    pages += AltitudeCorrectionPageCount();
  if (request.includeVisualAids && request.includeAltitudeCorrectionTables)
    ++pages;
  if (request.includeDirectReductionTables)
    pages += 1 + DirectReductionPageCount(request);
  return pages;
}

void AlmanacPaperTables::Append(const AlmanacRequest& request,
                                AlmanacDocument* document) {
  if (!document) return;
  const bool any = request.includeIncrementTables ||
                   request.includeCompactReductionTables ||
                   request.includeAltitudeCorrectionTables;
  if (any) AddInstructionPages(document);
  if (request.includeIncrementTables) AddIncrementPages(document);
  if (request.includeCompactReductionTables) AddAgetonPages(document);
  if (request.includeAltitudeCorrectionTables)
    AddAltitudeCorrectionPages(document);
  if (request.includeVisualAids && request.includeAltitudeCorrectionTables)
    AddCorrectionVisualPage(document);
  if (request.includeDirectReductionTables)
    AddDirectReductionPages(request, document);
}

long AlmanacPaperTables::AgetonA(double degrees) {
  const double sine = std::fabs(std::sin(Rad(TableAngle(degrees))));
  if (sine < 1e-14) return LONG_MAX;
  return std::lround(100000.0 * std::log10(1.0 / sine));
}

long AlmanacPaperTables::AgetonB(double degrees) {
  const double cosine = std::fabs(std::cos(Rad(TableAngle(degrees))));
  if (cosine < 1e-14) return LONG_MAX;
  return std::lround(100000.0 * std::log10(1.0 / cosine));
}

AgetonReductionResult AlmanacPaperTables::ReduceAgeton(
    double latitude, double declination, double localHourAngle,
    bool quantizeToPrintedTable) {
  if (latitude < -90 || latitude > 90 || declination < -90 ||
      declination > 90)
    return DirectReduction(latitude, declination, localHourAngle);
  const double lha = Wrap360(localHourAngle);
  double t = lha <= 180.0 ? lha : 360.0 - lha;
  if (quantizeToPrintedTable) {
    // Preserve the meridian-angle quadrant.  A/B lookup folds 90..180 onto
    // its complementary acute entry, but K must still be selected from the
    // obtuse branch when t exceeds 90 degrees.
    t = std::round(t * 120.0) / 120.0;
    latitude = std::round(latitude * 120.0) / 120.0;
    declination = std::round(declination * 120.0) / 120.0;
  }
  if (TableAngle(t) < 1.0 / 120.0 || std::fabs(declination) < 1.0 / 120.0)
    return DirectReduction(latitude, declination, lha);

  const long ar = AgetonA(t) + AgetonB(declination);
  const double r = InverseA(ar);
  const long ak = AgetonA(declination) - AgetonB(r);
  if (ak < 0 || ak == LONG_MAX) return DirectReduction(latitude, declination, lha);
  double k = InverseA(ak);
  if (t > 90.0) k = 180.0 - k;
  if (declination < 0) k = -k;

  const long ah = AgetonB(r) + AgetonB(k - latitude);
  double altitude = InverseA(ah);
  if (std::cos(Rad(k - latitude)) < 0) altitude = -altitude;
  const long az = AgetonA(r) - AgetonB(altitude);
  if (az < 0 || az == LONG_MAX) return DirectReduction(latitude, declination, lha);
  double polar = InverseA(az);
  double difference = k - latitude;
  while (difference > 180.0) difference -= 360.0;
  while (difference < -180.0) difference += 360.0;
  const bool acute = latitude >= 0 ? difference >= 0 : difference <= 0;
  if (!acute)
    polar = 180.0 - polar;

  double trueAzimuth = 0.0;
  if (latitude >= 0)
    trueAzimuth = lha < 180.0 ? 360.0 - polar : polar;
  else
    trueAzimuth = lha < 180.0 ? 180.0 + polar : 180.0 - polar;
  AgetonReductionResult result;
  result.valid = true;
  result.computedAltitude = altitude;
  result.azimuthTrue = Wrap360(trueAzimuth);
  return result;
}
