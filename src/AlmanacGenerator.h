/******************************************************************************
 * Offline voyage almanac calculation, document model and PDF output.
 ******************************************************************************/

#ifndef CELESTIAL_NAVIGATION_ALMANAC_GENERATOR_H
#define CELESTIAL_NAVIGATION_ALMANAC_GENERATOR_H

#include <wx/datetime.h>
#include <wx/string.h>

#include <vector>

enum class AlmanacPreset {
  PassageBrief,
  VoyageAlmanac,
  CelestialNavigator,
  Custom
};
enum class AlmanacCoverage { PlannedRoute, FixedPosition, LatitudeBand, Global };
enum class AlmanacSafety { PlanningReference, CalculatorComplete };
enum class AlmanacPaper { A4, Letter, A5 };

struct AlmanacRoutePoint {
  double latitude = 0.0;
  double longitude = 0.0;
};

struct AlmanacRequest {
  AlmanacPreset preset = AlmanacPreset::VoyageAlmanac;
  AlmanacCoverage coverage = AlmanacCoverage::FixedPosition;
  AlmanacSafety safety = AlmanacSafety::CalculatorComplete;
  wxDateTime fromUtc;
  wxDateTime toUtc;
  wxString voyageName;
  wxString routeName;
  std::vector<AlmanacRoutePoint> route;
  double latitude = 0.0;
  double longitude = 0.0;
  double latitudeSouth = -60.0;
  double latitudeNorth = 60.0;
  double routeCorridorNm = 150.0;
  double routeSpeedKnots = 6.0;
  double dut1Seconds = 0.0;
  bool dut1Known = false;

  bool includeSun = true;
  bool includeMoon = true;
  bool includeAries = true;
  bool includePlanets = true;
  bool includeStars = true;
  bool usefulPlanetsOnly = true;
  bool includeEvents = true;
  bool includeMoonInformation = true;
  bool includeRecommendations = true;
  bool includeStarCharts = true;
  bool includeCorrections = true;
  bool includeInstructions = true;
  bool includeLunar = true;
  bool includeEmergencyGuide = true;
  bool selfContained = true;

  unsigned sightForms = 4;
  unsigned runningFixForms = 2;
  unsigned noonPolarisForms = 2;
  unsigned lunarForms = 2;
  unsigned watchForms = 1;

  AlmanacPaper paper = AlmanacPaper::A4;
  bool landscape = false;
  bool duplex = true;
  bool monochrome = true;
  bool compact = true;
};

struct AlmanacTable {
  std::vector<wxString> headings;
  std::vector<std::vector<wxString> > rows;
  std::vector<double> relativeWidths;
};

struct AlmanacChartPoint {
  wxString label;
  double azimuth = 0.0;
  double altitude = 0.0;
};

struct AlmanacPage {
  wxString section;
  wxString title;
  wxString subtitle;
  std::vector<wxString> paragraphs;
  std::vector<AlmanacTable> tables;
  std::vector<AlmanacChartPoint> chart;
  bool form = false;
};

struct AlmanacDocument {
  wxString title;
  wxString generatedUtc;
  wxString manifest;
  std::vector<AlmanacPage> pages;
  std::vector<wxString> warnings;
  unsigned sheets = 0;
  size_t estimatedBytes = 0;
};

class AlmanacGenerator {
public:
  static void ApplyPreset(AlmanacPreset preset, AlmanacRequest* request);
  static bool Validate(AlmanacRequest* request, wxString* error);
  static AlmanacDocument Build(const AlmanacRequest& request);
  // Exact structural page count without performing any astronomical work.
  static AlmanacDocument Estimate(const AlmanacRequest& request);
  static AlmanacRoutePoint PositionForDay(const AlmanacRequest& request,
                                           unsigned dayIndex,
                                           unsigned dayCount);
  static wxString PreviewText(const AlmanacDocument& document,
                              unsigned maximumPages = 4);
};

class AlmanacPdfWriter {
public:
  // Writes to a sibling temporary file and replaces the requested file only
  // after a complete PDF has been produced.
  static bool Write(const AlmanacDocument& document,
                    const AlmanacRequest& request, const wxString& filename,
                    wxString* error);
};

#endif
