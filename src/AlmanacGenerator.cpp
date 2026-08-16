#include "AlmanacGenerator.h"

#include "AlmanacPaperTables.h"
#include "BodyCatalog.h"
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"

#include <wx/filefn.h>
#include <wx/filename.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace {
const double kPi = 3.14159265358979323846;

double Rad(double value) { return value * kPi / 180.0; }
double Deg(double value) { return value * 180.0 / kPi; }
double Wrap360(double value) {
  value = std::fmod(value, 360.0);
  return value < 0.0 ? value + 360.0 : value;
}

wxDateTime AtHour(const wxDateTime& day, int hour) {
  wxDateTime result(day.GetDay(), day.GetMonth(), day.GetYear(), hour, 0, 0);
  return result;
}

unsigned InclusiveDays(const AlmanacRequest& request) {
  const wxDateTime from = UtcDateTime::ToInstant(AtHour(request.fromUtc, 0));
  const wxDateTime to = UtcDateTime::ToInstant(AtHour(request.toUtc, 0));
  if (!from.IsValid() || !to.IsValid() || to.IsEarlierThan(from)) return 0;
  return static_cast<unsigned>((to - from).GetDays()) + 1;
}

wxDateTime DayAt(const AlmanacRequest& request, unsigned index) {
  return UtcDateTime::FromInstant(
      UtcDateTime::ToInstant(AtHour(request.fromUtc, 0)) +
      wxTimeSpan::Days(index));
}

wxString Angle(double degrees, bool signedValue = false) {
  const wxString sign = signedValue ? (degrees < 0.0 ? "S " : "N ") : "";
  const double value = signedValue ? std::fabs(degrees) : Wrap360(degrees);
  int whole = static_cast<int>(std::floor(value));
  double minutes = (value - whole) * 60.0;
  if (minutes >= 59.95) {
    ++whole;
    minutes = 0.0;
  }
  return wxString::Format("%s%03d %04.1f'", sign, whole, minutes);
}

wxString ShortAngle(double degrees) {
  return wxString::Format("%.1f deg", degrees);
}

wxString Time(const wxDateTime& utc) {
  return UtcDateTime::FormatUtc(utc, "%H:%M");
}

double GreatCircleNm(const AlmanacRoutePoint& a,
                     const AlmanacRoutePoint& b) {
  const double p1 = Rad(a.latitude), p2 = Rad(b.latitude);
  const double dp = p2 - p1, dl = Rad(b.longitude - a.longitude);
  const double h = std::sin(dp / 2) * std::sin(dp / 2) +
                   std::cos(p1) * std::cos(p2) *
                       std::sin(dl / 2) * std::sin(dl / 2);
  return 3440.065 * 2.0 * std::atan2(std::sqrt(h), std::sqrt(1.0 - h));
}

AlmanacRoutePoint Interpolate(const AlmanacRoutePoint& a,
                              const AlmanacRoutePoint& b, double fraction) {
  AlmanacRoutePoint p;
  p.latitude = a.latitude + fraction * (b.latitude - a.latitude);
  double delta = b.longitude - a.longitude;
  if (delta > 180.0) delta -= 360.0;
  if (delta < -180.0) delta += 360.0;
  p.longitude = a.longitude + fraction * delta;
  if (p.longitude > 180.0) p.longitude -= 360.0;
  if (p.longitude < -180.0) p.longitude += 360.0;
  return p;
}

double Separation(const BodyState& a, const BodyState& b) {
  const double cosDistance =
      std::sin(Rad(a.declination)) * std::sin(Rad(b.declination)) +
      std::cos(Rad(a.declination)) * std::cos(Rad(b.declination)) *
          std::cos(Rad(a.gha - b.gha));
  return Deg(std::acos(std::max(-1.0, std::min(1.0, cosDistance))));
}

wxString CoverageText(const AlmanacRequest& request) {
  switch (request.coverage) {
    case AlmanacCoverage::PlannedRoute:
      return wxString::Format("Route: %s; %.0f NM planning corridor",
                              request.routeName, request.routeCorridorNm);
    case AlmanacCoverage::FixedPosition:
      return wxString::Format("Fixed planning position: %.4f, %.4f",
                              request.latitude, request.longitude);
    case AlmanacCoverage::LatitudeBand:
      return wxString::Format("Planning latitude band: %.1f to %.1f deg",
                              request.latitudeSouth, request.latitudeNorth);
    case AlmanacCoverage::Global:
      return "Global ephemeris; planning examples use 0 deg N, 0 deg E";
  }
  return wxString();
}

double ForwardGhaChange(double from, double to) {
  double change = Wrap360(to - from);
  if (change > 180.0) change -= 360.0;
  return change;
}

std::vector<AlmanacTable> UniversalTables(const AlmanacRequest& request,
                                          const wxDateTime& day) {
  AlmanacTable sunTable;
  sunTable.headings.push_back("UTC");
  if (request.includeAries) sunTable.headings.push_back("Aries GHA");
  if (request.includeSun) {
    sunTable.headings.push_back("Sun GHA");
    sunTable.headings.push_back("Sun Dec");
    sunTable.headings.push_back("d");
    sunTable.headings.push_back("HP");
    sunTable.headings.push_back("SD");
  }
  AlmanacTable moonTable;
  if (request.includeMoon)
    moonTable.headings = {"UTC", "Moon GHA", "v", "Moon Dec", "d", "HP", "SD"};
  for (int hour = 0; hour < 24; ++hour) {
    const wxDateTime utc = AtHour(day, hour);
    const wxDateTime ut1 = UtcDateTime::AddSeconds(utc, request.dut1Seconds);
    const wxDateTime nextUt1 = UtcDateTime::AddSeconds(ut1, 3600.0);
    const BodyState sun = CelestialEphemeris::Evaluate("Sun", ut1, 0, 0);
    const BodyState moon = CelestialEphemeris::Evaluate("Moon", ut1, 0, 0);
    const BodyState nextSun = CelestialEphemeris::Evaluate("Sun", nextUt1, 0, 0);
    const BodyState nextMoon = CelestialEphemeris::Evaluate("Moon", nextUt1, 0, 0);
    const BodyState polaris =
        CelestialEphemeris::Evaluate("Polaris", ut1, 0, 0);
    const double aries = Wrap360(polaris.gha - polaris.sha);
    std::vector<wxString> sunRow{wxString::Format("%02d", hour)};
    if (request.includeAries) sunRow.push_back(Angle(aries));
    if (request.includeSun) {
      sunRow.push_back(Angle(sun.gha));
      sunRow.push_back(Angle(sun.declination, true));
      sunRow.push_back(wxString::Format("%+.1f'",
          (nextSun.declination - sun.declination) * 60.0));
      sunRow.push_back(wxString::Format("%.1f'", sun.horizontalParallax * 60.0));
      sunRow.push_back(wxString::Format("%.1f'", sun.semidiameter * 60.0));
    }
    sunTable.rows.push_back(sunRow);
    if (request.includeMoon) {
      const double v = ForwardGhaChange(moon.gha, nextMoon.gha) * 60.0 -
                       (14.0 * 60.0 + 19.0);
      moonTable.rows.push_back(
          {wxString::Format("%02d", hour), Angle(moon.gha),
           wxString::Format("%+.1f'", v), Angle(moon.declination, true),
           wxString::Format("%+.1f'",
                            (nextMoon.declination - moon.declination) * 60.0),
           wxString::Format("%.1f'", moon.horizontalParallax * 60.0),
           wxString::Format("%.1f'", moon.semidiameter * 60.0)});
    }
  }
  std::vector<AlmanacTable> tables;
  if (request.includeAries || request.includeSun) tables.push_back(sunTable);
  if (request.includeMoon) tables.push_back(moonTable);
  return tables;
}

AlmanacTable PlanetTable(const AlmanacRequest& request,
                         const wxDateTime& day) {
  AlmanacTable table;
  table.headings = {"Body", "00h GHA", "00h Dec", "12h GHA", "12h Dec"};
  const char* names[] = {"Venus", "Mars", "Jupiter", "Saturn"};
  for (const char* name : names) {
    const BodyState at0 = CelestialEphemeris::Evaluate(
        name, UtcDateTime::AddSeconds(AtHour(day, 0), request.dut1Seconds), 0,
        0);
    const BodyState at12 = CelestialEphemeris::Evaluate(
        name, UtcDateTime::AddSeconds(AtHour(day, 12), request.dut1Seconds), 0,
        0);
    table.rows.push_back({name, Angle(at0.gha), Angle(at0.declination, true),
                          Angle(at12.gha),
                          Angle(at12.declination, true)});
  }
  return table;
}

std::vector<AlmanacTable> PlanetHourlyTables(const AlmanacRequest& request,
                                             const wxDateTime& day) {
  const char* names[] = {"Venus", "Mars", "Jupiter", "Saturn"};
  std::vector<AlmanacTable> tables;
  for (unsigned pair = 0; pair < 2; ++pair) {
    AlmanacTable table;
    table.headings.push_back("UTC");
    for (unsigned offset = 0; offset < 2; ++offset) {
      const wxString name = names[pair * 2 + offset];
      table.headings.push_back(name + " GHA");
      table.headings.push_back("v");
      table.headings.push_back("Dec");
      table.headings.push_back("d");
      table.headings.push_back("HP/SD");
    }
    for (int hour = 0; hour < 24; ++hour) {
      std::vector<wxString> row{wxString::Format("%02d", hour)};
      const wxDateTime utc = UtcDateTime::AddSeconds(
          AtHour(day, hour), request.dut1Seconds);
      for (unsigned offset = 0; offset < 2; ++offset) {
        const char* name = names[pair * 2 + offset];
        const BodyState state = CelestialEphemeris::Evaluate(name, utc, 0, 0);
        const BodyState next = CelestialEphemeris::Evaluate(
            name, UtcDateTime::AddSeconds(utc, 3600.0), 0, 0);
        const double v = ForwardGhaChange(state.gha, next.gha) * 60.0 - 900.0;
        row.push_back(Angle(state.gha));
        row.push_back(wxString::Format("%+.1f'", v));
        row.push_back(Angle(state.declination, true));
        row.push_back(wxString::Format("%+.1f'",
                                       (next.declination - state.declination) * 60.0));
        row.push_back(wxString::Format("%.1f/%.1f'",
            state.horizontalParallax * 60.0, state.semidiameter * 60.0));
      }
      table.rows.push_back(row);
    }
    tables.push_back(table);
  }
  return tables;
}

const std::vector<wxString>& OfficialNavigationalStars() {
  static const std::vector<wxString> names = {
      "Alpheratz", "Ankaa", "Schedar", "Diphda", "Achernar", "Hamal",
      "Acamar", "Menkar", "Mirfak", "Aldebaran", "Rigel", "Capella",
      "Bellatrix", "Elnath", "Alnilam", "Betelgeuse", "Canopus", "Sirius",
      "Adhara", "Procyon", "Pollux", "Avior", "Suhail", "Miaplacidus",
      "Alphard", "Regulus", "Dubhe", "Denebola", "Gienah", "Acrux",
      "Gacrux", "Alioth", "Spica", "Alkaid", "Hadar", "Menkent",
      "Arcturus", "Rigil", "Zubenelgenubi", "Kochab", "Alphecca",
      "Antares", "Atria", "Sabik", "Shaula", "Rasalhague", "Eltanin",
      "Kaus Australis", "Vega", "Nunki", "Altair", "Peacock", "Deneb",
      "Enif", "Al Na'ir", "Fomalhaut", "Markab"};
  return names;
}

void AddReferencePage(AlmanacDocument* document, const wxString& title,
                      const std::vector<wxString>& paragraphs) {
  AlmanacPage page;
  page.section = "Reference";
  page.title = title;
  page.paragraphs = paragraphs;
  document->pages.push_back(page);
}

void AddFormPages(AlmanacDocument* document, const wxString& title,
                  unsigned count, const std::vector<wxString>& headings) {
  for (unsigned i = 0; i < count; ++i) {
    AlmanacPage page;
    page.section = "Forms";
    page.title = title;
    page.subtitle = wxString::Format("Sheet %u of %u", i + 1, count);
    page.form = true;
    AlmanacTable table;
    table.headings = headings;
    for (int row = 0; row < 14; ++row) {
      std::vector<wxString> cells(headings.size(), "");
      table.rows.push_back(cells);
    }
    page.tables.push_back(table);
    document->pages.push_back(page);
  }
}

std::string Ascii(const wxString& value) {
  std::string output;
  for (wxString::const_iterator it = value.begin(); it != value.end(); ++it) {
    const unsigned long c = static_cast<unsigned long>(*it);
    if (c >= 32 && c <= 126)
      output.push_back(static_cast<char>(c));
    else if (c == 0x00b0)
      output += " deg";
    else if (c == 0x2013 || c == 0x2014)
      output.push_back('-');
    else if (c == 0x2018 || c == 0x2019)
      output.push_back('\'');
    else if (c == 0x201c || c == 0x201d)
      output.push_back('"');
    else
      output.push_back(' ');
  }
  return output;
}

std::string PdfEscape(const wxString& value) {
  const std::string ascii = Ascii(value);
  std::string result;
  for (char c : ascii) {
    if (c == '(' || c == ')' || c == '\\') result.push_back('\\');
    result.push_back(c);
  }
  return result;
}

std::vector<wxString> Wrap(const wxString& input, size_t width) {
  std::vector<wxString> lines;
  wxString remaining = input;
  while (remaining.length() > width) {
    size_t split = width;
    while (split > width / 2 && remaining[split] != ' ') --split;
    if (split <= width / 2) split = width;
    lines.push_back(remaining.Left(split));
    remaining = remaining.Mid(split);
    remaining.Trim(false);
  }
  if (!remaining.empty()) lines.push_back(remaining);
  return lines;
}

void PdfText(std::ostringstream& stream, double x, double y, double size,
             const wxString& text, bool bold = false) {
  stream << "BT /" << (bold ? "F2" : "F1") << " " << size << " Tf "
         << std::fixed << std::setprecision(1) << x << " " << y
         << " Td (" << PdfEscape(text) << ") Tj ET\n";
}

void PdfSeriesDash(std::ostringstream& stream, size_t seriesIndex) {
  switch (seriesIndex % 4) {
    case 1:
      stream << "[5 3] 0 d\n";
      break;
    case 2:
      stream << "[2 2] 0 d\n";
      break;
    case 3:
      stream << "[8 2 2 2] 0 d\n";
      break;
    default:
      stream << "[] 0 d\n";
      break;
  }
}

std::string RenderPage(const AlmanacPage& page, double width, double height,
                       unsigned pageNumber, unsigned totalPages,
                       bool compact) {
  std::ostringstream stream;
  const double left = compact ? 36.0 : 42.0;
  const size_t wrapWidth = static_cast<size_t>(std::max(48.0,
      (width - 2.0 * left) / 5.5));
  double y = height - 44.0;
  PdfText(stream, left, y, compact ? 15 : 16, page.title, true);
  y -= 19.0;
  if (!page.subtitle.empty()) {
    for (const wxString& line : Wrap(page.subtitle, wrapWidth)) {
      PdfText(stream, left, y, 9, line);
      y -= 11.5;
    }
    y -= 3.5;
  }
  stream << "0.65 w " << left << " " << y << " m " << width - left
         << " " << y << " l S\n";
  y -= 15.0;
  for (const wxString& paragraph : page.paragraphs) {
    for (const wxString& line : Wrap(paragraph, wrapWidth)) {
      PdfText(stream, left, y, 9, line);
      y -= 11.5;
    }
    y -= 5.0;
  }
  size_t tableLines = 0;
  for (const AlmanacTable& table : page.tables)
    tableLines += table.rows.size() + 1;
  const double chartReserve = page.chart.empty() ? 0.0 : 155.0;
  const double fittedRowHeight = tableLines
      ? std::max(compact ? 7.0 : 7.5,
                 std::min(compact ? 11.5 : 13.0,
                          (y - 55.0 - chartReserve) / tableLines))
      : 13.0;
  for (const AlmanacTable& table : page.tables) {
    if (y < 100) break;
    const size_t columns = table.headings.size();
    if (!columns) continue;
    std::vector<double> weights = table.relativeWidths;
    if (weights.size() != columns) weights.assign(columns, 1.0);
    double sum = 0.0;
    for (double weight : weights) sum += weight;
    const double available = width - 2 * left;
    std::vector<double> x(columns + 1, left);
    for (size_t col = 0; col < columns; ++col)
      x[col + 1] = x[col] + available * weights[col] / sum;
    const double rowHeight = page.form
                                 ? std::min(31.0, (y - 60.0) /
                                                        (table.rows.size() + 1))
                                 : fittedRowHeight;
    const double tableFont = std::max(5.8, std::min(7.2, rowHeight - 2.0));
    stream << "0.35 w ";
    for (size_t col = 0; col < columns; ++col)
      PdfText(stream, x[col] + 2, y - 8, tableFont, table.headings[col], true);
    stream << left << " " << y << " m " << width - left << " " << y
           << " l S\n";
    y -= rowHeight;
    for (const std::vector<wxString>& row : table.rows) {
      if (y - rowHeight < 55) break;
      for (size_t col = 0; col < columns && col < row.size(); ++col)
        PdfText(stream, x[col] + 2, y - 8, tableFont, row[col]);
      y -= rowHeight;
      stream << left << " " << y << " m " << width - left << " " << y
             << " l S\n";
    }
    y -= 8.0;
  }
  if (!page.plots.empty() && y > 150) {
    const double plotHeight = std::min(230.0,
        (y - 50.0 - 18.0 * page.plots.size()) / page.plots.size());
    for (size_t plotIndex = 0; plotIndex < page.plots.size(); ++plotIndex) {
      const AlmanacPlot& plot = page.plots[plotIndex];
      if (plotHeight < 90.0 || y - plotHeight < 40.0) break;
      const double x0 = left + 35.0, x1 = width - left - 10.0;
      const double top = y - 18.0, bottom = top - plotHeight;
      PdfText(stream, left, y - 8.0, 9.0, plot.title, true);
      stream << "0.35 w " << x0 << " " << bottom << " m " << x1 << " "
             << bottom << " l " << x1 << " " << top << " l " << x0 << " "
             << top << " l h S\n";
      for (unsigned tick = 0; tick <= 4; ++tick) {
        const double px = x0 + (x1 - x0) * tick / 4.0;
        const double value = plot.xMinimum +
                             (plot.xMaximum - plot.xMinimum) * tick / 4.0;
        stream << "0.15 w " << px << " " << bottom << " m " << px << " "
               << top << " l S\n";
        PdfText(stream, px - 8.0, bottom - 12.0, 6.5,
                wxString::Format("%.1f", value));
        const double py = bottom + (top - bottom) * tick / 4.0;
        const double yValue = plot.yMinimum +
                              (plot.yMaximum - plot.yMinimum) * tick / 4.0;
        stream << x0 << " " << py << " m " << x1 << " " << py << " l S\n";
        PdfText(stream, left, py - 2.0, 6.5,
                wxString::Format("%.1f", yValue));
      }
      PdfText(stream, (x0 + x1) / 2.0 - 25.0, bottom - 24.0, 7.0,
              plot.xLabel);
      size_t seriesIndex = 0;
      for (const AlmanacPlotSeries& series : plot.series) {
        const size_t count = std::min(series.x.size(), series.y.size());
        if (count < 2) continue;
        PdfSeriesDash(stream, seriesIndex);
        const double legendSlot =
            (x1 - x0) / std::max<size_t>(1, plot.series.size());
        const double legendX = x0 + seriesIndex * legendSlot;
        const double legendY = top + 7.0;
        stream << "0.70 w " << legendX << " " << legendY << " m "
               << legendX + 28.0 << " " << legendY << " l S\n";
        PdfText(stream, legendX + 33.0, top + 4.0, 7.0, series.label);

        stream << "0.70 w\n";
        bool started = false;
        for (size_t point = 0; point < count; ++point) {
          const double px = x0 + (series.x[point] - plot.xMinimum) /
              (plot.xMaximum - plot.xMinimum) * (x1 - x0);
          const double py = bottom + (series.y[point] - plot.yMinimum) /
              (plot.yMaximum - plot.yMinimum) * (top - bottom);
          if (px < x0 || px > x1 || py < bottom || py > top) {
            if (started) stream << "S\n";
            started = false;
            continue;
          }
          stream << px << " " << py << (started ? " l " : " m ");
          started = true;
        }
        if (started) stream << "S\n";
        ++seriesIndex;
      }
      stream << "[] 0 d\n";
      y = bottom - 35.0;
    }
  }
  if (!page.chart.empty() && y > 150) {
    const double radius = std::min(110.0, (y - 45.0) / 2.0);
    const double cx = width / 2.0, cy = 45.0 + radius;
    const double k = 0.5522847498 * radius;
    stream << "0.45 w " << cx + radius << " " << cy << " m "
           << cx + radius << " " << cy + k << " " << cx + k << " "
           << cy + radius << " " << cx << " " << cy + radius << " c "
           << cx - k << " " << cy + radius << " " << cx - radius << " "
           << cy + k << " " << cx - radius << " " << cy << " c "
           << cx - radius << " " << cy - k << " " << cx - k << " "
           << cy - radius << " " << cx << " " << cy - radius << " c "
           << cx + k << " " << cy - radius << " " << cx + radius << " "
           << cy - k << " " << cx + radius << " " << cy << " c S\n";
    for (const AlmanacChartPoint& point : page.chart) {
      const double r = radius * (90.0 - point.altitude) / 90.0;
      const double x = cx + r * std::sin(Rad(point.azimuth));
      const double py = cy + r * std::cos(Rad(point.azimuth));
      stream << x - 1.5 << " " << py - 1.5 << " 3 3 re f\n";
      PdfText(stream, x + 3.0, py - 2.0, 6.5, point.label);
    }
    PdfText(stream, cx - 2, cy + radius + 5, 7, "N");
  }
  PdfText(stream, left, 24, 7.5, page.section);
  PdfText(stream, width - 90, 24, 7.5,
          wxString::Format("Page %u of %u", pageNumber, totalPages));
  return stream.str();
}
}  // namespace

void AlmanacGenerator::ApplyPreset(AlmanacPreset preset,
                                   AlmanacRequest* request) {
  if (!request) return;
  request->preset = preset;
  request->includeSun = request->includeMoon = request->includeAries = true;
  request->includePlanets = request->includeEvents = true;
  request->includeMoonInformation = request->includeRecommendations = true;
  request->usefulPlanetsOnly = true;
  request->includeIncrementTables = false;
  request->includeCompactReductionTables = false;
  request->includeDirectReductionTables = false;
  request->fullDirectReductionCoverage = false;
  request->includeAltitudeCorrectionTables = false;
  request->includeVisualAids = true;
  request->monthlyStarData = false;
  request->planningIntervalDays = 1;
  request->booklet = false;
  request->signaturePages = 16;
  if (preset == AlmanacPreset::PassageBrief) {
    request->safety = AlmanacSafety::PlanningReference;
    request->selfContained = false;
    request->includeStars = true;
    request->includeStarCharts = false;
    request->includeCorrections = false;
    request->includeInstructions = true;
    request->includeLunar = false;
    request->includeEmergencyGuide = false;
    request->sightForms = 2;
    request->runningFixForms = 1;
    request->noonPolarisForms = 1;
    request->lunarForms = request->watchForms = 0;
  } else if (preset == AlmanacPreset::VoyageAlmanac) {
    request->safety = AlmanacSafety::CalculatorComplete;
    request->selfContained = true;
    request->includeStars = request->includeStarCharts = true;
    request->includeCorrections = request->includeInstructions = true;
    request->includeLunar = request->includeEmergencyGuide = true;
    request->sightForms = 4;
    request->runningFixForms = request->noonPolarisForms = 2;
    request->lunarForms = 2;
    request->watchForms = 1;
  } else if (preset == AlmanacPreset::CalculatorFreeVoyage) {
    request->safety = AlmanacSafety::CalculatorFree;
    request->selfContained = true;
    request->includeStars = request->includeStarCharts = true;
    request->includeCorrections = request->includeInstructions = true;
    request->includeLunar = request->includeEmergencyGuide = true;
    request->includeIncrementTables = true;
    request->includeCompactReductionTables = true;
    request->includeAltitudeCorrectionTables = true;
    request->includeDirectReductionTables = true;
    request->sightForms = 6;
    request->runningFixForms = request->noonPolarisForms = 3;
    request->lunarForms = 3;
    request->watchForms = 2;
  } else if (preset == AlmanacPreset::CelestialNavigator) {
    request->safety = AlmanacSafety::CalculatorComplete;
    request->selfContained = true;
    request->includeStars = request->includeStarCharts = true;
    request->includeCorrections = request->includeInstructions = true;
    request->includeLunar = request->includeEmergencyGuide = true;
    request->usefulPlanetsOnly = false;
    request->sightForms = 8;
    request->runningFixForms = 4;
    request->noonPolarisForms = request->lunarForms = 3;
    request->watchForms = 2;
  } else if (preset == AlmanacPreset::FullGlobalAlmanac) {
    request->coverage = AlmanacCoverage::Global;
    request->safety = AlmanacSafety::CalculatorFree;
    request->selfContained = true;
    request->includeStars = request->includeStarCharts = true;
    request->includeCorrections = request->includeInstructions = true;
    request->includeLunar = request->includeEmergencyGuide = true;
    request->usefulPlanetsOnly = false;
    request->includeIncrementTables = true;
    request->includeCompactReductionTables = true;
    request->includeAltitudeCorrectionTables = true;
    request->includeDirectReductionTables = true;
    request->fullDirectReductionCoverage = true;
    request->monthlyStarData = true;
    request->planningIntervalDays = 30;
    request->sightForms = 10;
    request->runningFixForms = 5;
    request->noonPolarisForms = request->lunarForms = 4;
    request->watchForms = 3;
  }
}

AlmanacDocument AlmanacGenerator::Estimate(const AlmanacRequest& request) {
  AlmanacDocument document;
  const unsigned days = InclusiveDays(request);
  unsigned pages = 2;  // cover and contents
  if (request.preset != AlmanacPreset::PassageBrief) pages += days;
  if (request.preset != AlmanacPreset::PassageBrief && request.includePlanets)
    pages += days;
  const unsigned cadence = request.planningIntervalDays;
  unsigned planningPages = 0;
  if (cadence) {
    planningPages = (days + cadence - 1) / cadence;
    pages += planningPages;
    if (request.includeVisualAids) pages += planningPages;
    if (request.includeVisualAids && request.includeLunar)
      pages += planningPages;
  }
  if (request.includeStars) {
    unsigned epochs = 1;
    if (request.monthlyStarData && days) {
      epochs = 0;
      int previousYear = -1;
      int previousMonth = -1;
      for (unsigned day = 0; day < days; ++day) {
        const wxDateTime date = DayAt(request, day);
        if (date.GetYear() != previousYear ||
            static_cast<int>(date.GetMonth()) != previousMonth) {
          ++epochs;
          previousYear = date.GetYear();
          previousMonth = static_cast<int>(date.GetMonth());
        }
      }
    }
    pages += 3 * epochs;
  }
  if (request.includeInstructions) pages += 3;  // reduction, interpolation, noon
  if (request.includeCorrections) pages += 1;
  if (request.includeLunar) pages += 1;
  if (request.includeEmergencyGuide) pages += 1;
  const unsigned paperPages = AlmanacPaperTables::PageCount(request);
  pages += paperPages;
  pages += request.sightForms + request.runningFixForms +
           request.noonPolarisForms + request.lunarForms + request.watchForms;
  document.pages.resize(pages);
  const unsigned padded = request.booklet ? ((pages + 3) / 4) * 4 : pages;
  document.physicalPdfPages = request.booklet ? padded / 2 : pages;
  document.sheets = request.booklet ? padded / 4
                                    : (request.duplex ? (pages + 1) / 2 : pages);
  size_t estimatedBytes = 1400 + static_cast<size_t>(pages) * 9000;
  if (request.includeIncrementTables)
    estimatedBytes += AlmanacPaperTables::IncrementPageCount() * 36000;
  if (request.includeCompactReductionTables)
    estimatedBytes += AlmanacPaperTables::ReductionPageCount() * 26000;
  if (request.includeAltitudeCorrectionTables)
    estimatedBytes += AlmanacPaperTables::AltitudeCorrectionPageCount() * 16000;
  if (request.includeDirectReductionTables)
    estimatedBytes +=
        AlmanacPaperTables::DirectReductionPageCount(request) * 26000;
  document.estimatedBytes = estimatedBytes;
  document.estimatedSeconds = days * 0.05 + planningPages * 0.06 +
                              paperPages * 0.001;
  if (pages > 5000)
    document.warnings.push_back(wxString::Format(
        "Very large edition: %u logical pages. Check storage, print scope and a small sample before generating the full document.",
        pages));
  return document;
}

bool AlmanacGenerator::Validate(AlmanacRequest* request, wxString* error) {
  if (!request || !request->fromUtc.IsValid() || !request->toUtc.IsValid()) {
    if (error) *error = "Enter a valid UTC date range.";
    return false;
  }
  if (InclusiveDays(*request) == 0 || InclusiveDays(*request) > 366) {
    if (error)
      *error = "Almanac output supports 1 to 366 inclusive days.";
    return false;
  }
  if (request->coverage == AlmanacCoverage::FixedPosition &&
      (request->latitude < -90 || request->latitude > 90 ||
       request->longitude < -180 || request->longitude > 180)) {
    if (error) *error = "The fixed position is outside valid latitude/longitude limits.";
    return false;
  }
  if (request->coverage == AlmanacCoverage::PlannedRoute &&
      request->route.size() < 2) {
    if (error) *error = "Select an OpenCPN route containing at least two waypoints.";
    return false;
  }
  if (request->signaturePages < 4 || request->signaturePages > 64 ||
      request->signaturePages % 4 != 0) {
    if (error) *error = "Booklet signature size must be a multiple of four from 4 to 64 pages.";
    return false;
  }
  if (request->planningIntervalDays > 31) {
    if (error) *error = "Planning-page interval must be 0 to 31 days.";
    return false;
  }
  if (request->fullDirectReductionCoverage)
    request->includeDirectReductionTables = true;
  if (request->safety == AlmanacSafety::CalculatorFree) {
    request->selfContained = true;
    request->includeIncrementTables = true;
    request->includeCompactReductionTables = true;
    request->includeAltitudeCorrectionTables = true;
    request->includeCorrections = true;
    request->includeInstructions = true;
    request->includeEmergencyGuide = true;
    request->includeAries = request->includeSun = request->includeMoon = true;
  } else if (request->selfContained ||
             request->safety == AlmanacSafety::CalculatorComplete) {
    request->selfContained = true;
    request->safety = AlmanacSafety::CalculatorComplete;
    request->includeCorrections = true;
    request->includeInstructions = true;
    request->includeEmergencyGuide = true;
    request->includeAries = request->includeSun = request->includeMoon = true;
  }
  return true;
}

wxString AlmanacGenerator::DependencyManifest(const AlmanacRequest& request) {
  if (request.safety == AlmanacSafety::CalculatorFree &&
      request.includeIncrementTables && request.includeCompactReductionTables &&
      request.includeAltitudeCorrectionTables && request.includeAries &&
      request.includeSun && request.includeMoon && request.includeInstructions &&
      request.includeEmergencyGuide)
    return request.includeDirectReductionTables
        ? "COMPLETE: CALCULATOR-FREE - hourly ephemeris; increments and v/d; altitude corrections; direct Hc/Zn plus compact universal reduction; procedures; plotting forms."
        : "COMPLETE: CALCULATOR-FREE - hourly ephemeris; increments and v/d; altitude corrections; compact universal reduction; procedures; plotting forms.";
  if (request.safety == AlmanacSafety::CalculatorComplete &&
      request.includeInstructions && request.includeCorrections)
    return "COMPLETE: SCIENTIFIC-CALCULATOR - hourly ephemeris; correction formulae; reduction formulae; procedures and forms.";
  return "PLANNING REFERENCE ONLY - retain an independent complete reduction reference.";
}

AlmanacRoutePoint AlmanacGenerator::PositionForDay(
    const AlmanacRequest& request, unsigned dayIndex, unsigned dayCount) {
  if (request.coverage != AlmanacCoverage::PlannedRoute ||
      request.route.size() < 2) {
    AlmanacRoutePoint result;
    if (request.coverage == AlmanacCoverage::LatitudeBand)
      result.latitude = (request.latitudeSouth + request.latitudeNorth) / 2.0;
    else if (request.coverage == AlmanacCoverage::Global)
      result.latitude = result.longitude = 0.0;
    else {
      result.latitude = request.latitude;
      result.longitude = request.longitude;
    }
    return result;
  }
  std::vector<double> lengths;
  double total = 0.0;
  for (size_t i = 1; i < request.route.size(); ++i) {
    lengths.push_back(GreatCircleNm(request.route[i - 1], request.route[i]));
    total += lengths.back();
  }
  (void)dayCount;
  // API 1.18 exposes route geometry but not a reliable route-level departure
  // schedule, so the selected From date is departure and the UI's fallback
  // speed advances the planning position along cumulative route distance.
  double target = std::min(total, dayIndex * 24.0 * request.routeSpeedKnots);
  for (size_t i = 0; i < lengths.size(); ++i) {
    if (target <= lengths[i] || i + 1 == lengths.size())
      return Interpolate(request.route[i], request.route[i + 1],
                         lengths[i] > 0 ? std::min(1.0, target / lengths[i]) : 0.0);
    target -= lengths[i];
  }
  return request.route.back();
}

AlmanacDocument AlmanacGenerator::Build(const AlmanacRequest& input) {
  AlmanacRequest request = input;
  wxString ignored;
  Validate(&request, &ignored);
  AlmanacDocument document;
  document.title = request.voyageName.empty() ? "OpenCPN Voyage Almanac"
                                               : request.voyageName;
  document.generatedUtc = UtcDateTime::FormatIsoUtc(UtcDateTime::Now());
  document.manifest = wxString::Format(
      "Celestial Navigation 2.7; VSOP87D/ELP astronomical engine; DUT1 %+.3f s (%s)",
      request.dut1Seconds, request.dut1Known ? "user supplied" : "assumed");
  if (!request.dut1Known)
    document.warnings.push_back(
        "DUT1 is assumed to be 0.000 s. Obtain a current offline value before relying on sub-second UT1 work.");
  if (request.safety == AlmanacSafety::PlanningReference)
    document.warnings.push_back(
        "Planning reference only: this preset intentionally omits some correction/reference pages.");

  AlmanacPage cover;
  cover.section = "Front matter";
  cover.title = document.title;
  cover.subtitle = wxString::Format("%s to %s UTC",
      UtcDateTime::FormatUtc(request.fromUtc, "%d %b %Y"),
      UtcDateTime::FormatUtc(request.toUtc, "%d %b %Y"));
  cover.paragraphs = {
      "Generated by the OpenCPN Celestial Navigation plugin. This is an independent voyage reference, not an official Nautical Almanac publication.",
      CoverageText(request), document.manifest,
      AlmanacGenerator::DependencyManifest(request),
      request.safety == AlmanacSafety::CalculatorFree
          ? "PAPER-ONLY BACKUP: no computer or calculator is required after printing. Sextant, accurate time, pencil, plotting tools and competent navigation practice are still required."
          : (request.selfContained
             ? "SCIENTIFIC-CALCULATOR BACKUP: includes astronomical quantities, corrections, formulae and forms."
             : "PASSAGE PLANNING REFERENCE: retain a complete sight-reduction reference."),
      "Vessel: ____________________    Primary watch: ____________________",
      "Watch error at departure: __________ s    Watch rate: __________ s/day"};
  for (const wxString& warning : document.warnings)
    cover.paragraphs.push_back("CAUTION: " + warning);
  document.pages.push_back(cover);

  const unsigned days = InclusiveDays(request);
  for (unsigned dayIndex = 0; dayIndex < days; ++dayIndex) {
    const wxDateTime day = DayAt(request, dayIndex);
    const AlmanacRoutePoint position = PositionForDay(request, dayIndex, days);
    if (request.preset != AlmanacPreset::PassageBrief) {
      AlmanacPage ephemeris;
      ephemeris.section = "Daily ephemeris";
      ephemeris.title = UtcDateTime::FormatUtc(day, "%A %d %B %Y");
      ephemeris.subtitle =
          "Hourly geocentric quantities. UTC labels; Earth rotation evaluated using UTC + DUT1.";
      const std::vector<AlmanacTable> universal = UniversalTables(request, day);
      ephemeris.tables.insert(ephemeris.tables.end(), universal.begin(), universal.end());
      document.pages.push_back(ephemeris);
      if (request.includePlanets) {
        AlmanacPage planets;
        planets.section = "Daily ephemeris";
        planets.title = UtcDateTime::FormatUtc(day, "%A %d %B %Y") +
                        " - navigational planets";
        planets.subtitle =
            "Hourly geocentric GHA and declination. Mercury is not a Nautical Almanac navigational planet.";
        const std::vector<AlmanacTable> planetTables =
            PlanetHourlyTables(request, day);
        planets.tables.insert(planets.tables.end(), planetTables.begin(),
                              planetTables.end());
        document.pages.push_back(planets);
      }
    }

    if (!request.planningIntervalDays ||
        dayIndex % request.planningIntervalDays != 0)
      continue;

    AlmanacPage planning;
    planning.section = "Daily planning";
    planning.title = UtcDateTime::FormatUtc(day, "%A %d %B %Y") +
                     " - practical planning";
    planning.subtitle = wxString::Format(
        "Planning position %.4f, %.4f%s", position.latitude,
        position.longitude,
        request.coverage == AlmanacCoverage::PlannedRoute
            ? wxString::Format(" (route corridor +/- %.0f NM)",
                               request.routeCorridorNm)
            : wxString());
    ObserverMotion observer;
    observer.referenceUtc = day;
    observer.latitude = position.latitude;
    observer.longitude = position.longitude;
    if (request.includeEvents) {
      AlmanacTable events;
      events.headings = {"Event", "UTC", "True bearing"};
      const DailyEventsResult daily =
          HorizonEventCalculator::Calculate(day, observer, 2.0);
      for (const HorizonEventResult& event : daily.events)
        events.rows.push_back({HorizonEventCalculator::Name(event.kind),
                               Time(event.utc),
                               wxString::Format("%03.0f deg", event.bearingTrue)});
      planning.tables.push_back(events);
    }
    const wxDateTime planningTime = AtHour(day, 12);
    if (request.includeMoonInformation) {
      const MoonInformation moon = CalculateMoonInformation(
          planningTime, position.latitude, position.longitude);
      planning.paragraphs.push_back(wxString::Format(
          "Moon at 12:00 UTC: %s; %.0f%% illuminated; age %.1f days; %s.",
          moon.phaseName, moon.illuminatedFraction * 100.0, moon.ageDays,
          moon.waxing ? "waxing" : "waning"));
    }
    if (request.includeRecommendations) {
      AlmanacTable recommendations;
      recommendations.headings = {"Suggested body", "Altitude", "Azimuth",
                                   "Reason"};
      const std::vector<RankedBody> ranked = SightRanker::VisibleBodies(
          planningTime, position.latitude, position.longitude, 10, 75, 3.0);
      size_t accepted = 0;
      for (size_t i = 0; i < ranked.size() && accepted < 6; ++i) {
        const CelestialBodyInfo* info = BodyCatalog::Find(ranked[i].state.body);
        if (!info) continue;
        if (ranked[i].state.body.CmpNoCase("Mercury") == 0) continue;
        if ((info->kind == CelestialBodyKind::Sun && !request.includeSun) ||
            (info->kind == CelestialBodyKind::Moon && !request.includeMoon) ||
            (info->kind == CelestialBodyKind::Planet && !request.includePlanets) ||
            (info->kind == CelestialBodyKind::Star && !request.includeStars))
          continue;
        recommendations.rows.push_back(
            {ranked[i].state.body, ShortAngle(ranked[i].state.apparentAltitude),
             ShortAngle(ranked[i].state.azimuthTrue), ranked[i].reason});
        if (request.includeStarCharts) {
          AlmanacChartPoint point;
          point.label = ranked[i].state.body;
          point.azimuth = ranked[i].state.azimuthTrue;
          point.altitude = ranked[i].state.apparentAltitude;
          planning.chart.push_back(point);
        }
        ++accepted;
      }
      planning.tables.push_back(recommendations);
    }
    if (request.includePlanets && request.preset == AlmanacPreset::PassageBrief)
      planning.tables.push_back(PlanetTable(request, day));
    if (request.includeLunar) {
      AlmanacTable lunar;
      lunar.headings = {"Lunar pair at 12h", "Distance", "Rate", "Use"};
      const BodyState moon = CelestialEphemeris::Evaluate(
          "Moon", planningTime, position.latitude, position.longitude);
      const BodyState moonLater = CelestialEphemeris::Evaluate(
          "Moon", UtcDateTime::AddSeconds(planningTime, 600),
          position.latitude, position.longitude);
      const char* targets[] = {"Sun", "Venus", "Mars", "Jupiter", "Saturn",
                               "Aldebaran", "Antares", "Spica", "Regulus"};
      for (const char* name : targets) {
        const BodyState body = CelestialEphemeris::Evaluate(
            name, planningTime, position.latitude, position.longitude);
        const BodyState later = CelestialEphemeris::Evaluate(
            name, UtcDateTime::AddSeconds(planningTime, 600),
            position.latitude, position.longitude);
        if (!body.valid || !later.valid) continue;
        const double distance = Separation(moon, body);
        double change = Separation(moonLater, later) - distance;
        if (change > 180) change -= 360;
        if (change < -180) change += 360;
        const double rate = change * 6.0;
        if (distance > 15 && distance < 120 && std::fabs(rate) > 0.1)
          lunar.rows.push_back(
              {wxString("Moon-") + name, ShortAngle(distance),
               wxString::Format("%+.2f'/min", rate),
               std::fabs(rate) > 0.35 ? "Good sensitivity" : "Moderate"});
        if (lunar.rows.size() == 4) break;
      }
      planning.tables.push_back(lunar);
    }
    document.pages.push_back(planning);

    if (request.includeVisualAids) {
      AlmanacPage visual;
      visual.section = "Visual planning aids";
      visual.title = UtcDateTime::FormatUtc(day, "%A %d %B %Y") +
                     " - expected body altitude";
      visual.subtitle = wxString::Format(
          "At planning position %.4f, %.4f. Curves aid sight selection; use the tabular ephemeris for reduction.",
          position.latitude, position.longitude);
      AlmanacPlot altitudePlot;
      altitudePlot.title = "Apparent altitude through the UTC day";
      altitudePlot.xLabel = "UTC hour";
      altitudePlot.yLabel = "Altitude degrees";
      altitudePlot.xMinimum = 0.0;
      altitudePlot.xMaximum = 24.0;
      altitudePlot.yMinimum = -20.0;
      altitudePlot.yMaximum = 90.0;
      std::vector<wxString> curveBodies;
      if (request.includeSun) curveBodies.push_back("Sun");
      if (request.includeMoon) curveBodies.push_back("Moon");
      if (request.includePlanets) curveBodies.push_back("Venus");
      if (request.includeStars) curveBodies.push_back("Sirius");
      for (const wxString& name : curveBodies) {
        AlmanacPlotSeries series;
        series.label = name;
        for (unsigned sample = 0; sample <= 24; ++sample) {
          const wxDateTime sampleUtc = UtcDateTime::AddSeconds(day, sample * 3600.0);
          const BodyState state = CelestialEphemeris::Evaluate(
              name, sampleUtc, position.latitude, position.longitude);
          if (!state.valid) continue;
          series.x.push_back(sample);
          series.y.push_back(state.apparentAltitude);
        }
        altitudePlot.series.push_back(series);
      }
      visual.plots.push_back(altitudePlot);
      document.pages.push_back(visual);

      if (request.includeLunar) {
        const char* targets[] = {"Sun", "Venus", "Mars", "Jupiter", "Saturn",
                                 "Aldebaran", "Antares", "Spica", "Regulus"};
        wxString target = "Sun";
        double bestRate = 0.0;
        for (const char* candidate : targets) {
          const BodyState moon0 = CelestialEphemeris::Evaluate(
              "Moon", planningTime, position.latitude, position.longitude);
          const BodyState body0 = CelestialEphemeris::Evaluate(
              candidate, planningTime, position.latitude, position.longitude);
          const wxDateTime later = UtcDateTime::AddSeconds(planningTime, 600);
          const BodyState moon1 = CelestialEphemeris::Evaluate(
              "Moon", later, position.latitude, position.longitude);
          const BodyState body1 = CelestialEphemeris::Evaluate(
              candidate, later, position.latitude, position.longitude);
          if (!moon0.valid || !body0.valid || !moon1.valid || !body1.valid)
            continue;
          const double distance = Separation(moon0, body0);
          const double rate = (Separation(moon1, body1) - distance) * 6.0;
          if (distance > 15 && distance < 120 && std::fabs(rate) > bestRate) {
            bestRate = std::fabs(rate);
            target = candidate;
          }
        }
        AlmanacPage lunarVisual;
        lunarVisual.section = "Visual planning aids";
        lunarVisual.title = UtcDateTime::FormatUtc(day, "%A %d %B %Y") +
                            " - lunar-distance opportunity";
        lunarVisual.subtitle = "Planning curves only. Clear each observed lunar distance and use tabular/interpolated ephemeris values for the final solution.";
        AlmanacPlot distancePlot;
        distancePlot.title = "Moon-" + target + " geocentric separation";
        distancePlot.xLabel = "UTC hour";
        distancePlot.yLabel = "Distance degrees";
        distancePlot.xMinimum = 0;
        distancePlot.xMaximum = 24;
        distancePlot.yMinimum = 0;
        distancePlot.yMaximum = 180;
        AlmanacPlot ratePlot;
        ratePlot.title = "Lunar-distance rate (timing sensitivity)";
        ratePlot.xLabel = "UTC hour";
        ratePlot.yLabel = "Arcminutes per minute";
        ratePlot.xMinimum = 0;
        ratePlot.xMaximum = 24;
        ratePlot.yMinimum = -1;
        ratePlot.yMaximum = 1;
        AlmanacPlotSeries distances, rates;
        distances.label = "Distance";
        rates.label = "Rate";
        for (unsigned sample = 0; sample <= 24; ++sample) {
          const wxDateTime when = UtcDateTime::AddSeconds(day, sample * 3600.0);
          const BodyState moon0 = CelestialEphemeris::Evaluate(
              "Moon", when, position.latitude, position.longitude);
          const BodyState body0 = CelestialEphemeris::Evaluate(
              target, when, position.latitude, position.longitude);
          const wxDateTime later = UtcDateTime::AddSeconds(when, 600);
          const BodyState moon1 = CelestialEphemeris::Evaluate(
              "Moon", later, position.latitude, position.longitude);
          const BodyState body1 = CelestialEphemeris::Evaluate(
              target, later, position.latitude, position.longitude);
          if (!moon0.valid || !body0.valid || !moon1.valid || !body1.valid)
            continue;
          const double distance = Separation(moon0, body0);
          distances.x.push_back(sample);
          distances.y.push_back(distance);
          rates.x.push_back(sample);
          rates.y.push_back((Separation(moon1, body1) - distance) * 6.0);
        }
        distancePlot.series.push_back(distances);
        ratePlot.series.push_back(rates);
        lunarVisual.plots.push_back(distancePlot);
        lunarVisual.plots.push_back(ratePlot);
        document.pages.push_back(lunarVisual);
      }
    }
  }

  if (request.includeStars) {
    std::vector<wxDateTime> epochs;
    if (request.monthlyStarData) {
      int previousYear = -1;
      int previousMonth = -1;
      for (unsigned day = 0; day < days; ++day) {
        const wxDateTime date = DayAt(request, day);
        if (date.GetYear() != previousYear ||
            static_cast<int>(date.GetMonth()) != previousMonth) {
          epochs.push_back(date);
          previousYear = date.GetYear();
          previousMonth = static_cast<int>(date.GetMonth());
        }
      }
    } else {
      epochs.push_back(DayAt(request, days / 2));
    }
    const std::vector<wxString>& stars = OfficialNavigationalStars();
    for (const wxDateTime& epoch : epochs) {
     for (unsigned part = 0; part < 3; ++part) {
      AlmanacPage page;
      page.section = "Star data";
      page.title = wxString::Format("57 navigational stars - part %u", part + 1);
      page.subtitle = wxString::Format(
          "SHA and declination epoch %s 00:00 UT1.",
          UtcDateTime::FormatUtc(epoch, "%d %b %Y"));
      AlmanacTable table;
      table.headings = {"No.", "Star", "SHA", "Declination", "Magnitude"};
      const size_t first = part * 19;
      const size_t last = std::min(stars.size(), first + 19);
      for (size_t index = first; index < last; ++index) {
        const BodyState state = CelestialEphemeris::Evaluate(
            stars[index], UtcDateTime::AddSeconds(epoch, request.dut1Seconds),
            0, 0);
        const CelestialBodyInfo* info = BodyCatalog::Find(stars[index]);
        table.rows.push_back({wxString::Format("%u", static_cast<unsigned>(index + 1)),
                              stars[index], Angle(state.sha),
                              Angle(state.declination, true),
                              info ? wxString::Format("%.2f", info->visualMagnitude)
                                   : wxString()});
      }
      page.tables.push_back(table);
      if (part == 2) {
        const BodyState polaris = CelestialEphemeris::Evaluate(
            "Polaris", UtcDateTime::AddSeconds(epoch, request.dut1Seconds), 0, 0);
        page.paragraphs.push_back(wxString::Format(
            "Polaris (supplementary): SHA %s; declination %s. Use the dedicated Polaris correction workflow.",
            Angle(polaris.sha), Angle(polaris.declination, true)));
      }
      document.pages.push_back(page);
     }
    }
  }

  AlmanacPage contents;
  contents.section = "Front matter";
  contents.title = "Contents and generation manifest";
  contents.paragraphs = {document.manifest, CoverageText(request)};
  AlmanacTable toc;
  toc.headings = {"Section", "First page", "Pages"};
  std::map<wxString, std::pair<unsigned, unsigned> > ranges;
  for (size_t i = 0; i < document.pages.size(); ++i) {
    auto& range = ranges[document.pages[i].section];
    if (!range.first) range.first = static_cast<unsigned>(i + 1);
    ++range.second;
  }
  for (const auto& range : ranges)
    toc.rows.push_back({range.first, wxString::Format("%u", range.second.first),
                        wxString::Format("%u", range.second.second)});
  contents.tables.push_back(toc);
  document.pages.insert(document.pages.begin() + 1, contents);

  if (request.includeInstructions) {
    AddReferencePage(&document, "Circle of equal altitude and intercept reduction",
      {"A corrected observed altitude Ho defines a circle centred on the body's geographic position. Every point on that circle sees the body at the same altitude.",
       "For an assumed position calculate Hc and azimuth Zn. Intercept a = 60 (Ho - Hc) nautical miles: plot toward Zn when Ho is greater, away when Ho is smaller. Draw the line of position at right angles to Zn.",
       "Direct formula: sin Hc = sin Lat sin Dec + cos Lat cos Dec cos LHA. LHA = GHA + assumed longitude (east positive); normalize to 0-360 deg.",
       "Azimuth: atan2(sin LHA, cos LHA sin Lat - tan Dec cos Lat), then choose the reciprocal convention which points from observer toward the body. Verify quadrants.",
       "Worked example workflow: record body, UTC and Hs; apply index correction, dip and body corrections to obtain Ho; interpolate GHA/Dec; choose AP; calculate Hc/Zn; plot intercept and repeat with another body."});
  }
  if (request.includeCorrections)
    AddReferencePage(&document, "Altitude corrections",
      {"Ho = Hs + index correction - dip + refraction + semidiameter/parallax corrections appropriate to body and limb.",
       "Dip (minutes) is approximately 1.76 sqrt(height of eye in metres), subtracted for a sea horizon. Abnormal refraction and wave horizon remain observational uncertainties.",
       "Refraction is normally negative and grows rapidly near the horizon. A compact calculator approximation is R = 1.02 / tan(h + 10.3/(h+5.11)) arcminutes, scaled by pressure/1010 and 283/(273+temperature C).",
       "For Sun lower limb add semidiameter after refraction; upper limb subtract it. For the Moon include horizontal parallax projected by cos altitude and use the selected limb. Stars have no useful semidiameter or parallax.",
       "Always retain the signs and limb choice in the sight record. Near-horizon sights deserve larger uncertainty."});
  if (request.includeInstructions) {
    AddReferencePage(&document, "Interpolation, time and plotting",
      {"Interpolate hourly GHA and declination for elapsed UTC minutes and seconds. GHA changes continuously; declination may increase or decrease. The DUT1 statement on the cover records the Earth-rotation assumption.",
       "One second of time corresponds to 15 arcseconds of Greenwich hour angle, or about 0.25 nautical miles at the equator before geometry and observation error are considered.",
       "Two well-separated lines of position give a fix; three or more expose inconsistency. Advance time-separated LOPs by course and distance to a common epoch for a running fix."});
    AddReferencePage(&document, "Noon and Polaris procedures",
      {"Near local apparent noon, bracket the Sun's maximum altitude rather than trusting civil noon. Apply the altitude corrections and combine zenith distance with declination, respecting north/south geometry, to obtain latitude.",
       "Polaris altitude is close to latitude in the northern hemisphere, but apply the time-dependent correction derived from its hour angle. Use the printed hourly GHA/Aries data and the plugin/manual method; do not equate Hs directly to latitude."});
  }
  if (request.includeLunar)
    AddReferencePage(&document, "Lunar distance, watch recovery and sextant checks",
      {"A lunar distance is the angle between a limb of the Moon and a second body. Correct both altitudes and clear the measured distance of refraction, semidiameter and lunar parallax before comparing it with the ephemeris distance.",
       "The Moon moves rapidly against the stars. Solve for the UTC at which calculated and cleared distances agree. Multiple separately timed observations should be fitted jointly with one watch offset (and optionally watch rate), retaining each reading and its uncertainty rather than averaging away time information.",
       "Longitude remains coupled to time and altitude geometry. When the watch offset is unknown, combine lunar time recovery with timed altitude observations; inspect alternate solutions and residuals.",
       "For sextant calibration compare a measured star-star, Moon-star or Moon-planet separation with the computed true angular separation. Repeated pairs across the arc reveal index and scale-dependent error."});
  if (request.includeEmergencyGuide)
    AddReferencePage(&document, "Emergency recovery checklist",
      {"1. Preserve a watch log: displayed time, estimated UTC error, rate, temperature/shock notes and every comparison source.",
       "2. Identify bodies unambiguously; record raw Hs, limb, index correction, eye height, horizon quality, UTC/watch time and uncertainty before reducing anything.",
       "3. Recover latitude first when possible using noon Sun or Polaris. Recover time with a sequence of lunar distances. Then solve longitude with the recovered UTC and altitude sights.",
       "4. Cross-check with independent bodies and reject no sight merely because it is inconvenient: retain residuals and explain exclusions.",
       "5. Treat this document as a calculation reference, not a substitute for training, judgement, a maintained sextant and redundant timekeeping."});

  AlmanacPaperTables::Append(request, &document);

  AddFormPages(&document, "Sight reduction record", request.sightForms,
               {"Body", "UTC/watch", "Hs", "Corrections", "Ho", "Hc", "Zn", "Intercept"});
  AddFormPages(&document, "Running-fix record", request.runningFixForms,
               {"Sight", "UTC", "Body", "LOP", "COG", "SOG", "Advance", "Residual"});
  AddFormPages(&document, "Noon / Polaris record", request.noonPolarisForms,
               {"UTC", "Hs", "IC", "Dip", "Main corr.", "Ho", "Dec/LHA", "Latitude"});
  AddFormPages(&document, "Lunar sequence record", request.lunarForms,
               {"Watch time", "Pair", "Distance", "Moon alt", "Body alt", "Uncertainty", "Residual"});
  AddFormPages(&document, "Watch error and rate log", request.watchForms,
               {"Date", "Displayed", "Reference UTC", "Error", "Source", "Rate", "Notes"});

  // Rebuild contents after all sections have been appended.
  toc.rows.clear();
  ranges.clear();
  for (size_t i = 0; i < document.pages.size(); ++i) {
    if (i == 1) continue;
    auto& range = ranges[document.pages[i].section];
    if (!range.first) range.first = static_cast<unsigned>(i + 1);
    ++range.second;
  }
  for (const auto& range : ranges)
    toc.rows.push_back({range.first, wxString::Format("%u", range.second.first),
                        wxString::Format("%u", range.second.second)});
  document.pages[1].tables[0] = toc;
  const unsigned logical = static_cast<unsigned>(document.pages.size());
  const unsigned padded = request.booklet ? ((logical + 3) / 4) * 4 : logical;
  document.physicalPdfPages = request.booklet ? padded / 2 : logical;
  document.sheets = request.booklet ? padded / 4
                                    : (request.duplex ? (logical + 1) / 2
                                                      : logical);
  document.estimatedBytes = 1400 + document.pages.size() * 2400;
  return document;
}

wxString AlmanacGenerator::PreviewText(const AlmanacDocument& document,
                                       unsigned maximumPages) {
  wxString text;
  const unsigned count = std::min<unsigned>(maximumPages, document.pages.size());
  for (unsigned i = 0; i < count; ++i) {
    const AlmanacPage& page = document.pages[i];
    text += wxString::Format("PAGE %u OF %u\n%s\n%s\n\n", i + 1,
                            static_cast<unsigned>(document.pages.size()),
                            page.title, page.subtitle);
    for (const wxString& paragraph : page.paragraphs) text += paragraph + "\n\n";
    for (const AlmanacTable& table : page.tables) {
      for (const wxString& heading : table.headings) text += heading + " | ";
      text += "\n";
      for (const auto& row : table.rows) {
        for (const wxString& cell : row) text += cell + " | ";
        text += "\n";
      }
      text += "\n";
    }
    text += "\n------------------------------------------------------------\n\n";
  }
  if (document.pages.size() > count)
    text += wxString::Format("Preview limited to the first %u pages.\n", count);
  return text;
}

bool AlmanacPdfWriter::Write(const AlmanacDocument& document,
                             const AlmanacRequest& request,
                             const wxString& filename, wxString* error) {
  if (document.pages.empty()) {
    if (error) *error = "The document contains no pages.";
    return false;
  }
  double width = 595.28, height = 841.89;  // A4
  if (request.paper == AlmanacPaper::Letter) {
    width = 612.0;
    height = 792.0;
  } else if (request.paper == AlmanacPaper::A5) {
    width = 419.53;
    height = 595.28;
  }
  if (request.landscape || request.booklet) std::swap(width, height);

  struct Spread { int left; int right; };
  std::vector<Spread> spreads;
  if (request.booklet) {
    const unsigned total = static_cast<unsigned>(document.pages.size());
    for (unsigned start = 0; start < total;) {
      const unsigned actual = std::min(request.signaturePages, total - start);
      const unsigned padded = ((actual + 3) / 4) * 4;
      unsigned low = 0, high = padded - 1;
      while (low < high) {
        Spread front = {high < actual ? static_cast<int>(start + high) : -1,
                        low < actual ? static_cast<int>(start + low) : -1};
        spreads.push_back(front);
        --high;
        ++low;
        Spread back = {low < actual ? static_cast<int>(start + low) : -1,
                       high < actual ? static_cast<int>(start + high) : -1};
        spreads.push_back(back);
        ++low;
        --high;
      }
      start += actual;
    }
  }

  std::vector<std::string> objects;
  objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
  objects.push_back("");  // Pages, after page object numbers are known.
  objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>");
  std::vector<unsigned> pageObjects;
  const size_t physicalPages = request.booklet ? spreads.size()
                                               : document.pages.size();
  for (size_t i = 0; i < physicalPages; ++i) {
    std::string content;
    if (request.booklet) {
      std::ostringstream combined;
      const double logicalWidth = width / 2.0;
      if (spreads[i].left >= 0) {
        const unsigned index = static_cast<unsigned>(spreads[i].left);
        combined << "q\n" << RenderPage(document.pages[index], logicalWidth,
            height, index + 1, static_cast<unsigned>(document.pages.size()),
            true)
                 << "Q\n";
      }
      if (spreads[i].right >= 0) {
        const unsigned index = static_cast<unsigned>(spreads[i].right);
        combined << "q 1 0 0 1 " << logicalWidth << " 0 cm\n"
                 << RenderPage(document.pages[index], logicalWidth, height,
                    index + 1, static_cast<unsigned>(document.pages.size()),
                    true)
                 << "Q\n";
      }
      combined << "0.35 w " << width / 2.0 << " 10 m " << width / 2.0
               << " " << height - 10 << " l S\n";
      content = combined.str();
    } else {
      content = RenderPage(document.pages[i], width, height,
          static_cast<unsigned>(i + 1),
          static_cast<unsigned>(document.pages.size()), request.compact);
    }
    const unsigned pageNumber = static_cast<unsigned>(objects.size() + 1);
    const unsigned contentNumber = pageNumber + 1;
    pageObjects.push_back(pageNumber);
    std::ostringstream page;
    page << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << width << " "
         << height << "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
         << "/Contents " << contentNumber << " 0 R >>";
    objects.push_back(page.str());
    std::ostringstream stream;
    stream << "<< /Length " << content.size() << " >>\nstream\n" << content
           << "endstream";
    objects.push_back(stream.str());
  }
  std::ostringstream pages;
  pages << "<< /Type /Pages /Count " << pageObjects.size() << " /Kids [";
  for (unsigned number : pageObjects) pages << number << " 0 R ";
  pages << "] >>";
  objects[1] = pages.str();

  wxFileName target(filename);
  if (!target.GetPath().empty() && !target.DirExists() &&
      !wxFileName::Mkdir(target.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
    if (error) *error = "Could not create the output directory.";
    return false;
  }
  const wxString temporary = filename + ".partial";
  std::ofstream out(temporary.ToStdString().c_str(),
                    std::ios::binary | std::ios::trunc);
  if (!out) {
    if (error) *error = "Could not open the temporary PDF for writing.";
    return false;
  }
  out << "%PDF-1.4\n% OpenCPN Voyage Almanac\n";
  std::vector<std::streamoff> offsets(objects.size() + 1, 0);
  for (size_t i = 0; i < objects.size(); ++i) {
    offsets[i + 1] = out.tellp();
    out << i + 1 << " 0 obj\n" << objects[i] << "\nendobj\n";
  }
  const std::streamoff xref = out.tellp();
  out << "xref\n0 " << objects.size() + 1
      << "\n0000000000 65535 f \n";
  for (size_t i = 1; i < offsets.size(); ++i)
    out << std::setw(10) << std::setfill('0') << offsets[i]
        << " 00000 n \n";
  out << "trailer\n<< /Size " << objects.size() + 1
      << " /Root 1 0 R >>\nstartxref\n" << xref << "\n%%EOF\n";
  out.close();
  if (!out) {
    wxRemoveFile(temporary);
    if (error) *error = "Writing the PDF failed; the requested file was not changed.";
    return false;
  }
  if (!wxRenameFile(temporary, filename, true)) {
    wxRemoveFile(temporary);
    if (error) *error = "The completed temporary PDF could not be renamed.";
    return false;
  }
  return true;
}
