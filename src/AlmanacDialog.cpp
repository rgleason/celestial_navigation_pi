#include "AlmanacDialog.h"

#include "CelestialNavigationDialog.h"
#include "UtcDateTime.h"
#include "celestial_navigation_pi.h"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wfstream.h>

namespace {
void AddLabelled(wxSizer* sizer, wxWindow* parent, const wxString& label,
                 wxWindow* control) {
  wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText* text = new wxStaticText(parent, wxID_ANY, label,
                                        wxDefaultPosition, wxSize(155, -1));
  row->Add(text, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  row->Add(control, 1, wxEXPAND);
  sizer->Add(row, 0, wxEXPAND | wxBOTTOM, 6);
}

wxSpinCtrlDouble* Coordinate(wxWindow* parent, double minimum, double maximum,
                             double value) {
  wxSpinCtrlDouble* result = new wxSpinCtrlDouble(
      parent, wxID_ANY, wxString::Format("%.4f", value), wxDefaultPosition,
      wxDefaultSize, wxSP_ARROW_KEYS, minimum, maximum, value, 0.01);
  result->SetDigits(4);
  return result;
}

wxCheckBox* Check(wxWindow* parent, wxSizer* sizer, const wxString& label) {
  wxCheckBox* result = new wxCheckBox(parent, wxID_ANY, label);
  sizer->Add(result, 0, wxBOTTOM, 5);
  return result;
}
}  // namespace

AlmanacDialog::AlmanacDialog(CelestialNavigationDialog* parent,
                             const wxString& preferredRouteGuid)
    : wxDialog(parent, wxID_ANY, _("Generate Voyage Almanac"),
               wxDefaultPosition, wxSize(1080, 760),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_parent(parent),
      m_preferredRouteGuid(preferredRouteGuid),
      m_applyingPreset(false) {
  BuildInterface();
  LoadRoutes();

  const wxDateTime now = UtcDateTime::Now();
  // A date picker represents calendar fields in the computer's local zone.
  // Copy the UTC fields rather than converting the instant, which could move
  // the selected date at either side of midnight or in a non-UTC timezone.
  m_from->SetValue(UtcDateTime::CopyFields(now));
  m_to->SetValue(UtcDateTime::CopyFields(now) + wxTimeSpan::Days(14));
  double lat = 0.0, lon = 0.0;
  if (parent && parent->GetPlugin() &&
      parent->GetPlugin()->GetBoatPosition(&lat, &lon)) {
    m_latitude->SetValue(lat);
    m_longitude->SetValue(lon);
  }
  m_preset->SetSelection(1);
  ApplyPresetSelection();
  if (!m_preferredRouteGuid.empty()) {
    for (size_t index = 0; index < m_routeGuids.size(); ++index) {
      if (m_routeGuids[index] != m_preferredRouteGuid) continue;
      m_coverage->SetSelection(
          static_cast<int>(AlmanacCoverage::PlannedRoute));
      m_route->SetSelection(static_cast<int>(index));
      m_voyageName->ChangeValue(
          wxString::Format(_("%s - fallback almanac"),
                           m_route->GetString(static_cast<unsigned>(index))));
      break;
    }
  }
  UpdateCoverageControls();
  UpdateSummary();
  CentreOnParent();
}

void AlmanacDialog::BuildInterface() {
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer* body = new wxBoxSizer(wxHORIZONTAL);
  m_notebook = new wxNotebook(this, wxID_ANY);

  wxScrolledWindow* setup = new wxScrolledWindow(m_notebook);
  setup->SetScrollRate(0, 10);
  wxBoxSizer* setupSizer = new wxBoxSizer(wxVERTICAL);
  m_preset = new wxChoice(setup, wxID_ANY);
  m_preset->Append(_("Passage Brief"));
  m_preset->Append(_("Voyage Almanac"));
  m_preset->Append(_("Calculator-free Voyage"));
  m_preset->Append(_("Celestial Navigator"));
  m_preset->Append(_("Full Global Annual"));
  m_preset->Append(_("Custom"));
  AddLabelled(setupSizer, setup, _("Preset"), m_preset);
  m_voyageName = new wxTextCtrl(setup, wxID_ANY, _("OpenCPN Voyage Almanac"));
  AddLabelled(setupSizer, setup, _("Document title"), m_voyageName);
  m_from = new wxDatePickerCtrl(setup, wxID_ANY);
  m_to = new wxDatePickerCtrl(setup, wxID_ANY);
  AddLabelled(setupSizer, setup, _("From (UTC date)"), m_from);
  AddLabelled(setupSizer, setup, _("To (UTC date)"), m_to);
  m_coverage = new wxChoice(setup, wxID_ANY);
  m_coverage->Append(_("Planned OpenCPN route"));
  m_coverage->Append(_("Fixed position"));
  m_coverage->Append(_("Latitude band"));
  m_coverage->Append(_("Global"));
  m_coverage->SetSelection(1);
  AddLabelled(setupSizer, setup, _("Planning coverage"), m_coverage);
  m_route = new wxChoice(setup, wxID_ANY);
  AddLabelled(setupSizer, setup, _("Saved OpenCPN route"), m_route);
  m_corridor = Coordinate(setup, 0, 2000, 150);
  m_corridor->SetDigits(0);
  AddLabelled(setupSizer, setup, _("Route corridor (NM)"), m_corridor);
  m_speed = Coordinate(setup, 0.1, 100, 6);
  m_speed->SetDigits(1);
  AddLabelled(setupSizer, setup, _("Fallback speed (kn)"), m_speed);
  m_latitude = Coordinate(setup, -90, 90, 0);
  m_longitude = Coordinate(setup, -180, 180, 0);
  AddLabelled(setupSizer, setup, _("Latitude"), m_latitude);
  AddLabelled(setupSizer, setup, _("Longitude"), m_longitude);
  m_latSouth = Coordinate(setup, -90, 90, -60);
  m_latNorth = Coordinate(setup, -90, 90, 60);
  AddLabelled(setupSizer, setup, _("Latitude-band south"), m_latSouth);
  AddLabelled(setupSizer, setup, _("Latitude-band north"), m_latNorth);
  m_dut1Known = new wxCheckBox(setup, wxID_ANY,
      _("Use the following current DUT1 (UT1 - UTC) value"));
  setupSizer->Add(m_dut1Known, 0, wxTOP | wxBOTTOM, 5);
  m_dut1 = Coordinate(setup, -0.9, 0.9, 0.0);
  m_dut1->SetDigits(3);
  AddLabelled(setupSizer, setup, _("DUT1 (seconds)"), m_dut1);
  setupSizer->Add(new wxStaticText(setup, wxID_ANY,
      _("No network access is used. Leave unchecked to document the explicit 0.000 s assumption.")),
      0, wxTOP | wxBOTTOM, 6);
  setup->SetSizer(setupSizer);
  m_notebook->AddPage(setup, _("Voyage && coverage"));

  wxScrolledWindow* content = new wxScrolledWindow(m_notebook);
  content->SetScrollRate(0, 10);
  wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);
  m_safety = new wxChoice(content, wxID_ANY);
  m_safety->Append(_("Planning reference"));
  m_safety->Append(_("Self-contained with scientific calculator"));
  m_safety->Append(_("Self-contained calculator-free paper backup"));
  m_safety->SetSelection(1);
  AddLabelled(contentSizer, content, _("Safety level"), m_safety);
  m_selfContained = Check(content, contentSizer,
      _("Enforce all dependencies for the selected safety level"));
  wxStaticBoxSizer* ephemeris =
      new wxStaticBoxSizer(wxVERTICAL, content, _("Daily ephemeris"));
  m_aries = Check(content, ephemeris, _("Aries"));
  m_sun = Check(content, ephemeris, _("Sun"));
  m_moon = Check(content, ephemeris, _("Moon"));
  m_planets = Check(content, ephemeris, _("Venus, Mars, Jupiter and Saturn"));
  m_stars = Check(content, ephemeris, _("Navigational stars"));
  m_usefulPlanets = Check(content, ephemeris,
      _("Prefer useful/observable planets in planning lists"));
  contentSizer->Add(ephemeris, 0, wxEXPAND | wxTOP, 8);
  wxStaticBoxSizer* planning =
      new wxStaticBoxSizer(wxVERTICAL, content, _("Planning information"));
  m_events = Check(content, planning, _("Twilight, rise, set and transits"));
  m_moonInfo = Check(content, planning, _("Moon phase and illumination"));
  m_recommendations = Check(content, planning, _("Suggested sights and geometry"));
  m_charts = Check(content, planning, _("Compact sky plots"));
  m_visualAids = Check(content, planning,
      _("Planning and correction graphs (tables remain authoritative)"));
  m_planningInterval = new wxChoice(content, wxID_ANY);
  m_planningInterval->Append(_("None"));
  m_planningInterval->Append(_("Every day"));
  m_planningInterval->Append(_("Every 2 days"));
  m_planningInterval->Append(_("Every 7 days"));
  m_planningInterval->Append(_("Every 14 days"));
  m_planningInterval->Append(_("Every 30 days"));
  m_planningInterval->SetSelection(1);
  AddLabelled(planning, content, _("Planning-page cadence"), m_planningInterval);
  contentSizer->Add(planning, 0, wxEXPAND | wxTOP, 8);
  wxStaticBoxSizer* references =
      new wxStaticBoxSizer(wxVERTICAL, content, _("Working reference"));
  m_corrections = Check(content, references, _("Altitude corrections and formulae"));
  m_instructions = Check(content, references, _("Sight reduction and plotting instructions"));
  m_lunar = Check(content, references, _("Lunar-distance and watch-recovery reference"));
  m_emergency = Check(content, references, _("Emergency recovery checklist"));
  m_incrementTables = Check(content, references,
      _("Minute/second increments and v/d tables (60 pages)"));
  m_reductionTables = Check(content, references,
      _("Compact universal Ageton reduction tables (46 pages)"));
  m_directTables = Check(content, references,
      _("Precomputed direct Hc/Zn tables for selected coverage"));
  m_fullDirectTables = Check(content, references,
      _("Direct tables for every declination (very large)"));
  m_altitudeTables = Check(content, references,
      _("Calculator-free altitude correction tables (6 pages)"));
  contentSizer->Add(references, 0, wxEXPAND | wxTOP, 8);
  content->SetSizer(contentSizer);
  m_notebook->AddPage(content, _("Content"));

  wxScrolledWindow* forms = new wxScrolledWindow(m_notebook);
  forms->SetScrollRate(0, 10);
  wxBoxSizer* formSizer = new wxBoxSizer(wxVERTICAL);
  auto formControl = [forms, formSizer](const wxString& label) {
    wxSpinCtrl* control = new wxSpinCtrl(forms, wxID_ANY, "", wxDefaultPosition,
                                        wxDefaultSize, wxSP_ARROW_KEYS, 0, 50, 0);
    AddLabelled(formSizer, forms, label, control);
    return control;
  };
  m_sightForms = formControl(_("Sight-reduction forms"));
  m_runningForms = formControl(_("Running-fix forms"));
  m_noonForms = formControl(_("Noon / Polaris forms"));
  m_lunarForms = formControl(_("Lunar-sequence forms"));
  m_watchForms = formControl(_("Watch error/rate forms"));
  m_paper = new wxChoice(forms, wxID_ANY);
  m_paper->Append("A4");
  m_paper->Append(_("US Letter"));
  m_paper->Append("A5");
  m_paper->SetSelection(0);
  AddLabelled(formSizer, forms, _("Paper"), m_paper);
  m_landscape = Check(forms, formSizer, _("Landscape"));
  m_duplex = Check(forms, formSizer, _("Duplex-friendly page count"));
  m_monochrome = Check(forms, formSizer, _("Monochrome friendly"));
  m_compact = Check(forms, formSizer, _("Compact layout"));
  m_booklet = Check(forms, formSizer,
      _("Booklet imposition (two logical pages per PDF page)"));
  m_signaturePages = new wxSpinCtrl(forms, wxID_ANY, "16", wxDefaultPosition,
      wxDefaultSize, wxSP_ARROW_KEYS, 4, 64, 16);
  m_signaturePages->SetIncrement(4);
  AddLabelled(formSizer, forms, _("Booklet signature pages"), m_signaturePages);
  m_output = new wxFilePickerCtrl(
      forms, wxID_ANY, wxEmptyString, _("Choose output PDF"),
      _("PDF files (*.pdf)|*.pdf"), wxDefaultPosition, wxDefaultSize,
      wxFLP_SAVE | wxFLP_OVERWRITE_PROMPT | wxFLP_USE_TEXTCTRL);
  AddLabelled(formSizer, forms, _("Output PDF"), m_output);
  forms->SetSizer(formSizer);
  m_notebook->AddPage(forms, _("Forms && output"));

  body->Add(m_notebook, 1, wxEXPAND | wxALL, 8);
  wxStaticBoxSizer* summaryBox =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Output summary"));
  m_summaryPanel = new wxScrolledWindow(
      this, wxID_ANY, wxDefaultPosition, wxSize(300, 220),
      wxVSCROLL | wxBORDER_NONE);
  m_summaryPanel->SetScrollRate(0, 10);
  wxBoxSizer* summaryContent = new wxBoxSizer(wxVERTICAL);
  m_summary = new wxStaticText(m_summaryPanel, wxID_ANY, "",
                               wxDefaultPosition, wxSize(275, -1));
  m_summary->Wrap(275);
  summaryContent->Add(m_summary, 0, wxEXPAND | wxALL, 8);
  m_warning = new wxStaticText(m_summaryPanel, wxID_ANY, "",
                               wxDefaultPosition, wxSize(275, -1));
  m_warning->SetForegroundColour(wxColour(160, 80, 0));
  m_warning->Wrap(275);
  summaryContent->Add(m_warning, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  m_summaryPanel->SetSizer(summaryContent);
  summaryBox->Add(m_summaryPanel, 1, wxEXPAND | wxALL, 2);
  wxButton* preview = new wxButton(this, wxID_ANY, _("Preview first pages"));
  summaryBox->Add(preview, 0, wxEXPAND | wxALL, 8);
  body->Add(summaryBox, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 8);
  root->Add(body, 1, wxEXPAND);

  wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
  buttons->AddStretchSpacer();
  wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
  wxButton* generate = new wxButton(this, wxID_OK, _("Generate PDF"));
  buttons->Add(cancel, 0, wxALL, 8);
  buttons->Add(generate, 0, wxALL, 8);
  root->Add(buttons, 0, wxEXPAND);
  SetSizer(root);

  Bind(wxEVT_CHOICE, &AlmanacDialog::OnChanged, this);
  Bind(wxEVT_CHECKBOX, &AlmanacDialog::OnChanged, this);
  Bind(wxEVT_SPINCTRL, &AlmanacDialog::OnChanged, this);
  Bind(wxEVT_SPINCTRLDOUBLE, &AlmanacDialog::OnChanged, this);
  m_from->Bind(wxEVT_DATE_CHANGED, &AlmanacDialog::OnDateChanged, this);
  m_to->Bind(wxEVT_DATE_CHANGED, &AlmanacDialog::OnDateChanged, this);
  m_preset->Bind(wxEVT_CHOICE, &AlmanacDialog::OnPreset, this);
  m_coverage->Bind(wxEVT_CHOICE, &AlmanacDialog::OnCoverage, this);
  m_route->Bind(wxEVT_CHOICE, &AlmanacDialog::OnRoute, this);
  m_voyageName->Bind(wxEVT_TEXT, &AlmanacDialog::OnChanged, this);
  preview->Bind(wxEVT_BUTTON, &AlmanacDialog::OnPreview, this);
  generate->Bind(wxEVT_BUTTON, &AlmanacDialog::OnGenerate, this);
}

void AlmanacDialog::SelectIntegrationPage(int page) {
  if (!m_notebook || page < 0 || page >= static_cast<int>(m_notebook->GetPageCount()))
    return;
  m_notebook->SetSelection(static_cast<size_t>(page));
}

void AlmanacDialog::LoadRoutes() {
  m_route->Clear();
  m_routeGuids.clear();
#ifndef UNIT_TESTS
  const wxArrayString guids = GetRouteGUIDArray();
  for (const wxString& guid : guids) {
    std::unique_ptr<PlugIn_Route_Ex> route = GetRouteEx_Plugin(guid);
    if (!route) continue;
    m_route->Append(route->m_NameString.empty() ? guid : route->m_NameString);
    m_routeGuids.push_back(guid);
  }
#endif
  if (!m_route->IsEmpty()) m_route->SetSelection(0);
}

void AlmanacDialog::UpdateCoverageControls() {
  const AlmanacCoverage coverage = static_cast<AlmanacCoverage>(
      std::max(0, m_coverage->GetSelection()));
  const bool route = coverage == AlmanacCoverage::PlannedRoute;
  const bool fixed = coverage == AlmanacCoverage::FixedPosition;
  const bool band = coverage == AlmanacCoverage::LatitudeBand;
  m_route->Enable(route && !m_route->IsEmpty());
  m_corridor->Enable(route);
  m_speed->Enable(route);
  m_latitude->Enable(fixed);
  m_longitude->Enable(fixed);
  m_latSouth->Enable(band);
  m_latNorth->Enable(band);
}

void AlmanacDialog::ApplyPresetSelection() {
  if (m_preset->GetSelection() == 5) return;
  m_applyingPreset = true;
  AlmanacRequest request;
  AlmanacGenerator::ApplyPreset(
      static_cast<AlmanacPreset>(std::max(0, m_preset->GetSelection())),
      &request);
  m_safety->SetSelection(static_cast<int>(request.safety));
  m_selfContained->SetValue(request.selfContained);
  m_sun->SetValue(request.includeSun);
  m_moon->SetValue(request.includeMoon);
  m_aries->SetValue(request.includeAries);
  m_planets->SetValue(request.includePlanets);
  m_stars->SetValue(request.includeStars);
  m_usefulPlanets->SetValue(request.usefulPlanetsOnly);
  m_events->SetValue(request.includeEvents);
  m_moonInfo->SetValue(request.includeMoonInformation);
  m_recommendations->SetValue(request.includeRecommendations);
  m_charts->SetValue(request.includeStarCharts);
  m_corrections->SetValue(request.includeCorrections);
  m_instructions->SetValue(request.includeInstructions);
  m_lunar->SetValue(request.includeLunar);
  m_emergency->SetValue(request.includeEmergencyGuide);
  m_incrementTables->SetValue(request.includeIncrementTables);
  m_reductionTables->SetValue(request.includeCompactReductionTables);
  m_directTables->SetValue(request.includeDirectReductionTables);
  m_fullDirectTables->SetValue(request.fullDirectReductionCoverage);
  m_altitudeTables->SetValue(request.includeAltitudeCorrectionTables);
  m_visualAids->SetValue(request.includeVisualAids);
  const unsigned intervals[] = {0, 1, 2, 7, 14, 30};
  for (unsigned i = 0; i < 6; ++i)
    if (request.planningIntervalDays == intervals[i])
      m_planningInterval->SetSelection(i);
  m_sightForms->SetValue(request.sightForms);
  m_runningForms->SetValue(request.runningFixForms);
  m_noonForms->SetValue(request.noonPolarisForms);
  m_lunarForms->SetValue(request.lunarForms);
  m_watchForms->SetValue(request.watchForms);
  m_duplex->SetValue(request.duplex);
  m_monochrome->SetValue(request.monochrome);
  m_compact->SetValue(request.compact);
  m_booklet->SetValue(request.booklet);
  m_signaturePages->SetValue(request.signaturePages);
  if (request.preset == AlmanacPreset::FullGlobalAlmanac) {
    const int year = m_from->GetValue().GetYear();
    m_from->SetValue(wxDateTime(1, wxDateTime::Jan, year));
    m_to->SetValue(wxDateTime(31, wxDateTime::Dec, year));
    m_coverage->SetSelection(static_cast<int>(AlmanacCoverage::Global));
    UpdateCoverageControls();
  }
  m_applyingPreset = false;
}

AlmanacRequest AlmanacDialog::ReadRequest(wxString* error,
                                          bool includeRoute) const {
  AlmanacRequest request;
  request.preset = static_cast<AlmanacPreset>(std::max(0, m_preset->GetSelection()));
  request.voyageName = m_voyageName->GetValue();
  request.fromUtc = UtcDateTime::CopyFields(m_from->GetValue());
  request.toUtc = UtcDateTime::CopyFields(m_to->GetValue());
  request.coverage = static_cast<AlmanacCoverage>(std::max(0, m_coverage->GetSelection()));
  request.routeName = m_route->GetStringSelection();
  request.latitude = m_latitude->GetValue();
  request.longitude = m_longitude->GetValue();
  request.latitudeSouth = m_latSouth->GetValue();
  request.latitudeNorth = m_latNorth->GetValue();
  request.routeCorridorNm = m_corridor->GetValue();
  request.routeSpeedKnots = m_speed->GetValue();
  request.dut1Known = m_dut1Known->GetValue();
  request.dut1Seconds = request.dut1Known ? m_dut1->GetValue() : 0.0;
  request.safety = static_cast<AlmanacSafety>(
      std::max(0, m_safety->GetSelection()));
  request.selfContained = m_selfContained->GetValue();
  request.includeSun = m_sun->GetValue();
  request.includeMoon = m_moon->GetValue();
  request.includeAries = m_aries->GetValue();
  request.includePlanets = m_planets->GetValue();
  request.includeStars = m_stars->GetValue();
  request.usefulPlanetsOnly = m_usefulPlanets->GetValue();
  request.includeEvents = m_events->GetValue();
  request.includeMoonInformation = m_moonInfo->GetValue();
  request.includeRecommendations = m_recommendations->GetValue();
  request.includeStarCharts = m_charts->GetValue();
  request.includeCorrections = m_corrections->GetValue();
  request.includeInstructions = m_instructions->GetValue();
  request.includeLunar = m_lunar->GetValue();
  request.includeEmergencyGuide = m_emergency->GetValue();
  request.includeIncrementTables = m_incrementTables->GetValue();
  request.includeCompactReductionTables = m_reductionTables->GetValue();
  request.includeDirectReductionTables = m_directTables->GetValue();
  request.fullDirectReductionCoverage = m_fullDirectTables->GetValue();
  request.includeAltitudeCorrectionTables = m_altitudeTables->GetValue();
  request.includeVisualAids = m_visualAids->GetValue();
  const unsigned intervals[] = {0, 1, 2, 7, 14, 30};
  request.planningIntervalDays = intervals[std::min(5,
      std::max(0, m_planningInterval->GetSelection()))];
  request.monthlyStarData = request.preset == AlmanacPreset::FullGlobalAlmanac;
  request.sightForms = m_sightForms->GetValue();
  request.runningFixForms = m_runningForms->GetValue();
  request.noonPolarisForms = m_noonForms->GetValue();
  request.lunarForms = m_lunarForms->GetValue();
  request.watchForms = m_watchForms->GetValue();
  request.paper = static_cast<AlmanacPaper>(std::max(0, m_paper->GetSelection()));
  request.landscape = m_landscape->GetValue();
  request.duplex = m_duplex->GetValue();
  request.monochrome = m_monochrome->GetValue();
  request.compact = m_compact->GetValue();
  request.booklet = m_booklet->GetValue();
  request.signaturePages = m_signaturePages->GetValue();
#ifndef UNIT_TESTS
  if (includeRoute && request.coverage == AlmanacCoverage::PlannedRoute &&
      m_route->GetSelection() >= 0 &&
      static_cast<size_t>(m_route->GetSelection()) < m_routeGuids.size()) {
    std::unique_ptr<PlugIn_Route_Ex> route =
        GetRouteEx_Plugin(m_routeGuids[m_route->GetSelection()]);
    if (route && route->pWaypointList) {
      Plugin_WaypointExList::compatibility_iterator node =
          route->pWaypointList->GetFirst();
      while (node) {
        PlugIn_Waypoint_Ex* waypoint = node->GetData();
        if (waypoint) {
          AlmanacRoutePoint point;
          point.latitude = waypoint->m_lat;
          point.longitude = waypoint->m_lon;
          request.route.push_back(point);
        }
        node = node->GetNext();
      }
    }
  }
#else
  (void)includeRoute;
#endif
  AlmanacGenerator::Validate(&request, error);
  return request;
}

void AlmanacDialog::UpdateSummary() {
  wxString error;
  AlmanacRequest request = ReadRequest(&error);
  if (!error.empty()) {
    m_summary->SetLabel(_("The output cannot yet be estimated."));
    m_warning->SetLabel(error);
    m_summaryPanel->Layout();
    m_summaryPanel->FitInside();
    return;
  }
  wxBusyCursor busy;
  const AlmanacDocument document = AlmanacGenerator::Estimate(request);
  const double mb = static_cast<double>(document.estimatedBytes) / (1024.0 * 1024.0);
  m_summary->SetLabel(wxString::Format(
      _("%u days\n%u logical pages\n%u PDF pages\n%u printed sheets%s\nEstimated PDF: %.2f MB\nEstimated generation: %s\n\n%s\n\nIncludes:\n%s%s%s%s%s%s\nPlanning may be route-filtered; ephemeris and paper reduction data remain universal."),
      static_cast<unsigned>((UtcDateTime::ToInstant(request.toUtc) -
                             UtcDateTime::ToInstant(request.fromUtc)).GetDays() + 1),
      static_cast<unsigned>(document.pages.size()), document.physicalPdfPages,
      document.sheets,
      request.duplex ? _(" (duplex)") : "", mb,
      document.estimatedSeconds < 90
          ? wxString::Format(_("about %.0f seconds"), document.estimatedSeconds)
          : wxString::Format(_("about %.1f minutes"), document.estimatedSeconds / 60.0),
      AlmanacGenerator::DependencyManifest(request),
      request.preset == AlmanacPreset::PassageBrief ? _("Daily planning\n") : _("Hourly daily ephemeris\n"),
      request.includeEvents ? _("Rise/set and twilight\n") : "",
      request.includeRecommendations ? _("Suggested sights\n") : "",
      request.includeCorrections ? _("Corrections and formulae\n") : "",
      request.includeLunar ? _("Lunar reference/opportunities\n") : "",
      request.includeCompactReductionTables ? _("Calculator-free paper tables\n") : ""));
  m_summary->Wrap(275);
  m_warning->SetLabel(document.warnings.empty() ? wxString() : document.warnings.front());
  m_warning->Wrap(275);
  m_summaryPanel->Layout();
  m_summaryPanel->FitInside();
  Layout();
}

void AlmanacDialog::OnChanged(wxCommandEvent& event) {
  if (!m_applyingPreset && event.GetEventObject() == m_safety &&
      m_safety->GetSelection() ==
          static_cast<int>(AlmanacSafety::CalculatorFree)) {
    m_selfContained->SetValue(true);
    m_aries->SetValue(true);
    m_sun->SetValue(true);
    m_moon->SetValue(true);
    m_corrections->SetValue(true);
    m_instructions->SetValue(true);
    m_emergency->SetValue(true);
    m_incrementTables->SetValue(true);
    m_reductionTables->SetValue(true);
    m_altitudeTables->SetValue(true);
  }
  if (!m_applyingPreset && event.GetEventObject() != m_preset &&
      m_preset->GetSelection() != 5)
    m_preset->SetSelection(5);
  event.Skip();
  UpdateSummary();
}

void AlmanacDialog::OnDateChanged(wxDateEvent& event) {
  event.Skip();
  UpdateSummary();
}

void AlmanacDialog::OnPreset(wxCommandEvent& event) {
  ApplyPresetSelection();
  event.Skip();
  UpdateSummary();
}

void AlmanacDialog::OnCoverage(wxCommandEvent& event) {
  UpdateCoverageControls();
  event.Skip();
  UpdateSummary();
}

void AlmanacDialog::OnRoute(wxCommandEvent& event) {
  if (m_route->GetSelection() >= 0) {
    m_coverage->SetSelection(
        static_cast<int>(AlmanacCoverage::PlannedRoute));
    UpdateCoverageControls();
  }
  event.Skip();
  UpdateSummary();
}

void AlmanacDialog::OnPreview(wxCommandEvent&) {
  wxString error;
  AlmanacRequest request = ReadRequest(&error);
  if (!error.empty()) {
    wxMessageBox(error, _("Generate Almanac"), wxOK | wxICON_WARNING, this);
    return;
  }
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  wxDialog preview(this, wxID_ANY, _("Voyage Almanac - first pages"),
                   wxDefaultPosition, wxSize(820, 680),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
  wxTextCtrl* text = new wxTextCtrl(
      &preview, wxID_ANY, AlmanacGenerator::PreviewText(document, 4),
      wxDefaultPosition, wxDefaultSize,
      wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);
  text->SetFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE));
  sizer->Add(text, 1, wxEXPAND | wxALL, 8);
  sizer->Add(preview.CreateButtonSizer(wxOK), 0, wxEXPAND | wxALL, 8);
  preview.SetSizer(sizer);
  preview.ShowModal();
}

void AlmanacDialog::OnGenerate(wxCommandEvent&) {
  wxString error;
  AlmanacRequest request = ReadRequest(&error);
  if (!error.empty()) {
    wxMessageBox(error, _("Generate Almanac"), wxOK | wxICON_WARNING, this);
    return;
  }
  wxString output = m_output->GetPath();
  if (output.empty()) {
    wxMessageBox(_("Choose an output PDF first."), _("Generate Almanac"),
                 wxOK | wxICON_WARNING, this);
    return;
  }
  if (!output.Lower().EndsWith(".pdf")) output += ".pdf";
  wxBusyCursor busy;
  const AlmanacDocument document = AlmanacGenerator::Build(request);
  if (!AlmanacPdfWriter::Write(document, request, output, &error)) {
    wxMessageBox(error, _("Generate Almanac"), wxOK | wxICON_ERROR, this);
    return;
  }
  wxMessageBox(wxString::Format(
                   _("Created %u-page voyage almanac:\n%s"),
                   static_cast<unsigned>(document.pages.size()), output),
               _("Generate Almanac"), wxOK | wxICON_INFORMATION, this);
}
