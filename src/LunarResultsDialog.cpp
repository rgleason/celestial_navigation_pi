#include "LunarResultsDialog.h"
#include "LunarPresentation.h"
#include "PlatformMessageBox.h"
#include "DialogGeometry.h"
#include "Sight.h"
#include "SightDialog.h"
#include "CelestialNavigationDialog.h"
#include "NavigationUIUtils.h"
#include "UtcDateTime.h"
#include "WxDpiCompat.h"
#ifdef __OCPN__ANDROID__
#include "AndroidSurface.h"
#endif

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <algorithm>
#include <cmath>

namespace {
class ReportLabel : public wxStaticText {
public:
  ReportLabel(wxWindow* parent, const wxString& text)
      : wxStaticText(parent, wxID_ANY, text), m_original(text) {}
  void SetLabel(const wxString& value) override {
    if (!m_wrapping) m_original = value;
    wxStaticText::SetLabel(value);
  }
  void Reflow(int width) {
    m_wrapping = true;
    wxStaticText::SetLabel(m_original);
    Wrap(width);
    m_wrapping = false;
  }

private:
  wxString m_original;
  bool m_wrapping = false;
};
void ReflowLabels(wxWindow* window) {
  for (auto* child : window->GetChildren()) {
    if (auto* label = dynamic_cast<ReportLabel*>(child))
      if (label->GetSize().x >= 80) label->Reflow(label->GetSize().x - 2);
    ReflowLabels(child);
  }
}
bool HasSavedDr(const Sight& sight) {
  return std::isfinite(sight.m_DRLat) && std::isfinite(sight.m_DRLon) &&
         std::fabs(sight.m_DRLat) <= 90 && std::fabs(sight.m_DRLon) <= 180 &&
         (sight.m_DRLat != 0 || sight.m_DRLon != 0);
}
wxString Angle(double value,
               NavigationAngleKind kind = NavigationAngleKind::Generic) {
  return std::isfinite(value) ? FormatNavigationAngle(value, kind)
                              : _("Unavailable");
}
wxString TimeSigma(const lunar_distance::TimeCandidate& candidate) {
  return candidate.uncertainty_available &&
                 std::isfinite(candidate.time_uncertainty_seconds)
             ? wxString::Format("%.1f s", candidate.time_uncertainty_seconds)
             : _("Indeterminate");
}
wxString PositionSigma(const lunar_distance::TimeCandidate& candidate) {
  return candidate.uncertainty_available &&
                 std::isfinite(candidate.position_uncertainty_nm)
             ? wxString::Format("%.2f NM", candidate.position_uncertainty_nm)
             : _("Indeterminate");
}
wxStaticText* Label(wxWindow* parent, const wxString& text, bool bold = false) {
  auto* label = new ReportLabel(parent, text);
  if (bold) label->SetFont(label->GetFont().Bold());
  label->SetMinSize(wxSize(0, -1));
  return label;
}
wxScrolledWindow* ScrollPage(wxWindow* parent) {
  auto* page = new wxScrolledWindow(parent);
  page->SetScrollRate(0, 10);
  page->SetMinSize(wxSize(0, 0));
#ifdef __OCPN__ANDROID__
  CN_EnableAndroidScrolling(page);
#endif
  return page;
}
void FitColumns(wxListCtrl* list) {
  const int padding = celestial_ui::FromDIP(list, 24);
  for (int column = 0; column < list->GetColumnCount(); ++column) {
    wxListItem header;
    header.SetMask(wxLIST_MASK_TEXT);
    list->GetColumn(column, header);
    int width = list->GetTextExtent(header.GetText()).x;
    for (long row = 0; row < list->GetItemCount(); ++row)
      width = std::max(width,
                       list->GetTextExtent(list->GetItemText(row, column)).x);
    list->SetColumnWidth(column, width + padding);
  }
}
void RenderTables(wxScrolledWindow* page, const wxString& heading,
                  const std::vector<lunar_presentation::Table>& tables) {
  page->Freeze();
  if (auto* old = page->GetSizer()) old->Clear(true);
  auto* layout = page->GetSizer();
  if (!layout) {
    layout = new wxBoxSizer(wxVERTICAL);
    page->SetSizer(layout);
  }
  layout->Add(Label(page, heading, true), 0, wxALL | wxEXPAND, 10);
  for (const auto& table : tables) {
    layout->Add(Label(page, table.title, true), 0, wxALL | wxEXPAND, 10);
#ifdef __OCPN__ANDROID__
    for (const auto& row : table.rows) {
      auto* card = new wxPanel(page);
      auto* body = new wxBoxSizer(wxVERTICAL);
      for (std::size_t column = 0; column < row.size(); ++column)
        body->Add(Label(card, table.columns[column] + ": " + row[column],
                        column == 0),
                  0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
      card->SetSizer(body);
      layout->Add(card, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
    }
#else
    auto* list = new wxListCtrl(page, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxLC_REPORT);
    for (std::size_t column = 0; column < table.columns.size(); ++column)
      list->InsertColumn(column, table.columns[column]);
    for (std::size_t row = 0; row < table.rows.size(); ++row) {
      list->InsertItem(
          row, table.rows[row].empty() ? wxString() : table.rows[row][0]);
      for (std::size_t column = 1; column < table.rows[row].size(); ++column)
        list->SetItem(row, column, table.rows[row][column]);
    }
    FitColumns(list);
    const int rows =
        std::max(1, std::min(16, static_cast<int>(table.rows.size())));
    list->SetMinSize(wxSize(0, (rows + 1) * (list->GetCharHeight() + 8) + 24));
    layout->Add(list, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
#endif
    if (!table.note.empty())
      layout->Add(Label(page, table.note), 0, wxALL | wxEXPAND, 10);
  }
  page->Layout();
  page->FitInside();
  page->Thaw();
}
}  // namespace

LunarResultsDialog::LunarResultsDialog(wxWindow* parent, Sight& sight)
    : wxDialog(parent, wxID_ANY, _("Lunar results"), wxDefaultPosition,
               wxSize(1080, 820), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_sight(sight) {
  auto* root = new wxBoxSizer(wxVERTICAL);
  m_pages = new wxNotebook(this, wxID_ANY);
  m_pages->SetName("LunarPages");
  m_resultsPage = ScrollPage(m_pages);
  auto* results = new wxBoxSizer(wxVERTICAL);
  m_summary = Label(m_resultsPage, wxEmptyString, true);
  m_summary->SetName("LunarSelectedSummary");
  results->Add(m_summary, 0, wxALL | wxEXPAND, 10);
  m_status = Label(m_resultsPage, wxEmptyString);
  results->Add(m_status, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
#ifdef __OCPN__ANDROID__
  m_androidCandidates = new wxPanel(m_resultsPage);
  m_androidCandidates->SetSizer(new wxBoxSizer(wxVERTICAL));
  results->Add(m_androidCandidates, 0, wxEXPAND | wxALL, 6);
#else
  m_candidates = new wxListCtrl(m_resultsPage, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
  m_candidates->SetName("LunarCandidates");
  for (const auto& title :
       {_("Solution"), _("Derived UTC"), _("Correction"), _("UTC uncertainty")})
    m_candidates->InsertColumn(m_candidates->GetColumnCount(), title);
  m_candidates->SetMinSize(wxSize(0, 5 * (GetCharHeight() + 8) + 24));
  results->Add(m_candidates, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  m_candidates->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
    if (!m_updating) SelectCandidate(event.GetIndex());
  });
#endif
  auto* reference = new wxPanel(m_resultsPage);
  auto* referenceLayout = new wxBoxSizer(wxVERTICAL);
  referenceLayout->Add(
      Label(reference, _("Accuracy check at entered UTC"), true), 0,
      wxALL | wxEXPAND, 6);
  referenceLayout->Add(
      Label(reference,
            lunar_presentation::ResultUtc(m_sight.m_CorrectedDateTime) +
                _(" UTC | Existing correction: ") +
                wxString::Format("%+.1f s", UtcDateTime::SecondsBetween(
                                                m_sight.m_CorrectedDateTime,
                                                m_sight.m_DateTime))),
      0, wxALL | wxEXPAND, 6);
  m_dr = Label(
      reference,
      HasSavedDr(m_sight)
          ? _("Reference initially from saved DR: ") +
                Angle(m_sight.m_DRLat, NavigationAngleKind::Latitude) + ", " +
                Angle(m_sight.m_DRLon, NavigationAngleKind::Longitude)
          : _("No saved DR. Enter a reference position to check the sight."));
  referenceLayout->Add(m_dr, 0, wxALL | wxEXPAND, 6);
  auto* coordinates = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  m_knownLatitude = new NavigationAngleCtrl(
      reference, NavigationAngleKind::Latitude, m_sight.m_DRLat, -90, 90);
  m_knownLongitude = new NavigationAngleCtrl(
      reference, NavigationAngleKind::Longitude, m_sight.m_DRLon, -180, 180);
  m_knownLatitude->SetName("LunarKnownLatitude");
  m_knownLongitude->SetName("LunarKnownLongitude");
  if (!HasSavedDr(m_sight)) {
    m_knownLatitude->ChangeValue("");
    m_knownLongitude->ChangeValue("");
  }
  for (const auto& field :
       {std::make_pair(_("Reference latitude"), m_knownLatitude),
        std::make_pair(_("Reference longitude"), m_knownLongitude)}) {
    auto* column = new wxBoxSizer(wxVERTICAL);
    column->Add(Label(reference, field.first), 0, wxBOTTOM | wxEXPAND, 4);
    column->Add(field.second, 0, wxEXPAND);
    coordinates->Add(column, 1, wxALL | wxEXPAND, 6);
  }
  referenceLayout->Add(coordinates, 0, wxEXPAND);
  auto* distances = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  m_trueDistance = Label(reference, wxEmptyString, true);
  m_clearedDistance = Label(reference, wxEmptyString, true);
  m_lunarError = Label(reference, wxEmptyString, true);
  m_trueDistance->SetName("LunarTrueDistance");
  m_clearedDistance->SetName("LunarClearedDistance");
  m_lunarError->SetName("LunarDistanceCheck");
  for (const auto& field :
       {std::make_pair(_("True LD"), m_trueDistance),
        std::make_pair(_("Cleared LD (model corrected)"), m_clearedDistance),
        std::make_pair(_("Lunar error (cleared - true)"), m_lunarError)}) {
    auto* column = new wxBoxSizer(wxVERTICAL);
    column->Add(Label(reference, field.first), 0, wxBOTTOM | wxEXPAND, 4);
    column->Add(field.second, 0, wxEXPAND);
    distances->Add(column, 1, wxALL | wxEXPAND, 6);
  }
  referenceLayout->Add(distances, 0, wxEXPAND);
  m_referenceStatus = Label(reference, wxEmptyString);
  referenceLayout->Add(m_referenceStatus, 0, wxALL | wxEXPAND, 6);
  reference->SetSizer(referenceLayout);
  results->Add(reference, 0, wxALL | wxEXPAND, 6);
  results->Add(
      Label(m_resultsPage, _("Positions from the measured altitudes"), true), 0,
      wxALL | wxEXPAND, 10);
#ifdef __OCPN__ANDROID__
  m_androidPositions = new wxPanel(m_resultsPage);
  m_androidPositions->SetSizer(new wxBoxSizer(wxVERTICAL));
  results->Add(m_androidPositions, 0, wxALL | wxEXPAND, 6);
#else
  m_positions = new wxListCtrl(m_resultsPage, wxID_ANY, wxDefaultPosition,
                               wxDefaultSize, wxLC_REPORT);
  m_positions->SetName("LunarPositions");
  for (const auto& title :
       {_("Time basis / branch"), _("Latitude"), _("Longitude"),
        _("From saved DR"), _("Horizontal RMS")})
    m_positions->InsertColumn(m_positions->GetColumnCount(), title);
  m_positions->SetMinSize(wxSize(0, 5 * (GetCharHeight() + 8) + 24));
  results->Add(m_positions, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);
#endif
  m_geometry = Label(m_resultsPage, wxEmptyString);
  results->Add(m_geometry, 0, wxALL | wxEXPAND, 10);
  m_resultsPage->SetSizer(results);
  m_pages->AddPage(m_resultsPage, _("Results"), true);

  auto* calculations = new wxPanel(m_pages);
#ifdef __OCPN__ANDROID__
  calculations->GetHandle()->setProperty("cnNoPageScroll", true);
#endif
  auto* calculationsLayout = new wxBoxSizer(wxVERTICAL);
  m_calculationPages = new wxNotebook(calculations, wxID_ANY);
  m_calculationPages->SetName("LunarCalculationPages");
  m_overview = ScrollPage(m_calculationPages);
  m_almanac = ScrollPage(m_calculationPages);
  m_search = ScrollPage(m_calculationPages);
  auto* trail = new wxPanel(m_calculationPages);
#ifdef __OCPN__ANDROID__
  trail->GetHandle()->setProperty("cnNoPageScroll", true);
#endif
  auto* trailLayout = new wxBoxSizer(wxVERTICAL);
  m_details = new wxTextCtrl(
      trail, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
      wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);
  m_details->SetName("LunarDetailedTrail");
  m_details->SetFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
  trailLayout->Add(m_details, 1, wxALL | wxEXPAND, 6);
  trail->SetSizer(trailLayout);
  m_calculationPages->AddPage(m_overview, _("Overview"));
  m_calculationPages->AddPage(m_almanac, _("Almanac and corrections"));
  m_calculationPages->AddPage(m_search, _("UTC search"));
  m_calculationPages->AddPage(trail, _("Detailed trail"));
#ifdef __OCPN__ANDROID__
  auto* section = new wxChoice(calculations, wxID_ANY);
  section->SetName("LunarCalculationSection");
  for (std::size_t page = 0; page < m_calculationPages->GetPageCount(); ++page)
    section->Append(m_calculationPages->GetPageText(page));
  section->SetSelection(0);
  section->Bind(wxEVT_CHOICE, [this, section](wxCommandEvent&) {
    m_calculationPages->SetSelection(section->GetSelection());
    Relayout();
  });
  celestial_android::PrepareBook(m_calculationPages);
  calculationsLayout->Add(section, 0, wxALL | wxEXPAND, 6);
#endif
  calculationsLayout->Add(m_calculationPages, 1, wxEXPAND);
  calculations->SetSizer(calculationsLayout);
  m_pages->AddPage(calculations, _("Calculations"));
  root->Add(m_pages, 1, wxALL | wxEXPAND, 6);

  auto* buttons = new wxBoxSizer(wxHORIZONTAL);
  auto* copy = new wxButton(this, wxID_ANY, _("Copy report"));
  m_applyOffset = new wxButton(this, wxID_ANY, _("Save lunar solution"));
  auto* close = new wxButton(this, wxID_CLOSE, _("Close"));
  buttons->AddStretchSpacer();
  buttons->Add(copy, 0, wxRIGHT, 8);
  buttons->Add(m_applyOffset, 0, wxRIGHT, 8);
  buttons->Add(close, 0);
  root->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    if (wxTheClipboard->Open()) {
      wxTheClipboard->SetData(new wxTextDataObject(m_details->GetValue()));
      wxTheClipboard->Close();
    }
  });
  m_applyOffset->Bind(wxEVT_BUTTON,
                      &LunarResultsDialog::ApplySelectedWatchOffset, this);
  Bind(
      wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); },
      wxID_CLOSE);
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { EndModal(wxID_CLOSE); });
  SetEscapeId(wxID_CLOSE);
  SetSizer(root);
  SetMinSize(wxSize(760, 500));
  // Prepare the fixed-UTC position once. Editing the reference coordinates
  // changes only the independent accuracy check, never the fitted positions.
  if (m_sight.LunarEphemeris())
    m_enteredPositions = lunar_distance::PositionAtTime(
        m_sight.LunarObservation(), m_sight.LunarEphemeris(), 0);
  UpdateResults();
  for (auto* field : {m_knownLatitude, m_knownLongitude})
    field->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
      m_referenceEdited = true;
      UpdateReferenceCheck();
      Relayout();
    });
  dialog_geometry::Restore(this, "LunarResults299", wxSize(1080, 820));
  Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
    event.Skip();
    Relayout();
  });
  Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& event) {
    event.Skip();
    wxWeakRef<LunarResultsDialog> weak(this);
    CallAfter([weak]() {
      if (weak) weak->Relayout();
    });
  });
#ifdef __OCPN__ANDROID__
  close->Hide();
  celestial_android::Decorate(this, GetTitle(),
                              [this]() { EndModal(wxID_CLOSE); });
#endif
  Relayout();
}

LunarResultsDialog::~LunarResultsDialog() {
  dialog_geometry::Save(this, "LunarResults299");
}

void LunarResultsDialog::Relayout() {
  m_summary->SetLabel(m_summaryText);
  Layout();
  for (auto* page : {m_resultsPage, m_overview, m_almanac, m_search}) {
    page->Layout();
    ReflowLabels(page);
    page->Layout();
    page->FitInside();
  }
#ifdef __OCPN__ANDROID__
  celestial_android::LayoutScrolls(this);
#endif
  Layout();
}

void LunarResultsDialog::UpdateResults() {
  m_updating = true;
#ifndef __OCPN__ANDROID__
  m_candidates->DeleteAllItems();
  for (std::size_t index = 0; index < m_sight.m_LunarCandidates.size();
       ++index) {
    const auto& candidate = m_sight.m_LunarCandidates[index];
    const long row =
        m_candidates->InsertItem(index, lunar_presentation::SolutionId(index));
    m_candidates->SetItem(
        row, 1,
        lunar_presentation::ResultUtc(UtcDateTime::AddSeconds(
            m_sight.m_CorrectedDateTime, candidate.offset_seconds)));
    m_candidates->SetItem(
        row, 2, wxString::Format("%+.1f s", candidate.offset_seconds));
    m_candidates->SetItem(row, 3, TimeSigma(candidate));
  }
  FitColumns(m_candidates);
  m_candidates->SetMinSize(
      wxSize(0, (std::max(1, std::min(4, m_candidates->GetItemCount())) + 1) *
                        (GetCharHeight() + 8) +
                    16));
#endif
  m_updating = false;
  m_selected = -1;
  SelectCandidate(
      m_sight.SelectLunarCandidate(m_sight.m_LunarSelectedCandidate));
  UpdateReferenceCheck();
}

void LunarResultsDialog::SelectCandidate(long index) {
  if (m_updating || (index == m_selected && index >= 0)) return;
  m_updating = true;
  m_selected = index >= 0 && static_cast<std::size_t>(index) <
                                 m_sight.m_LunarCandidates.size()
                   ? index
                   : -1;
  if (m_selected >= 0) m_sight.UpdateLunarReport(m_selected);
  m_applyOffset->Enable(m_sight.m_LunarSolutionValid && m_selected >= 0);
  if (m_selected >= 0) {
    const auto& candidate = m_sight.m_LunarCandidates[m_selected];
    m_summaryText =
        _("Solution ") + lunar_presentation::SolutionId(m_selected) +
        (HasSavedDr(m_sight) && m_selected == m_sight.SelectLunarCandidate()
             ? _(" (nearest saved DR)")
             : wxString()) +
        " | " +
        lunar_presentation::ResultUtc(UtcDateTime::AddSeconds(
            m_sight.m_CorrectedDateTime, candidate.offset_seconds)) +
        wxString::Format(_(" UTC\nAdditional correction: %+.1f s | UTC "
                           "uncertainty (1-sigma): %s"),
                         candidate.offset_seconds, TimeSigma(candidate));
    m_status->SetLabel(
        m_sight.m_LunarCandidates.size() > 1
            ? wxString::Format(_("%zu possible UTC solutions. Use independent "
                                 "evidence to choose a branch."),
                               m_sight.m_LunarCandidates.size())
            : _("One UTC solution in the selected search interval."));
  } else {
    m_summaryText = _("No derived UTC solution");
    m_status->SetLabel(m_sight.m_LunarSolutionError);
  }
  if (m_sight.m_LunarDut1Fallback)
    m_status->SetLabel(m_status->GetLabel() +
                       _(" Earth-rotation data unavailable for part of this "
                         "calculation; reduced accuracy."));
#ifndef __OCPN__ANDROID__
  if (m_selected >= 0)
    m_candidates->SetItemState(m_selected, wxLIST_STATE_SELECTED,
                               wxLIST_STATE_SELECTED);
#endif
  UpdatePositions();
  UpdateCalculations();
  m_updating = false;
#ifdef __OCPN__ANDROID__
  RefreshAndroidCards();
#endif
  Relayout();
}

void LunarResultsDialog::UpdateReferenceCheck() {
  m_trueDistance->SetLabel(_("Unavailable"));
  m_clearedDistance->SetLabel(_("Unavailable"));
  m_lunarError->SetLabel(_("Unavailable"));
  m_checkReport = _("ACCURACY CHECK AT ENTERED UTC\n") +
                  UtcDateTime::FormatUtc(m_sight.m_CorrectedDateTime,
                                         "%Y-%m-%d %H:%M:%S.%l UTC\n");
  lunar_distance::EphemerisSample sample;
  std::string error;
  const auto ephemeris = m_sight.LunarEphemeris();
  if (ephemeris && ephemeris(0, &sample, &error))
    m_trueDistance->SetLabel(Angle(sample.predicted_distance_deg));
  double latitude = NAN, longitude = NAN;
  wxString status;
  if (!m_knownLatitude->GetAngle(&latitude) ||
      !m_knownLongitude->GetAngle(&longitude)) {
    status = _("Enter a valid reference latitude and longitude.");
  } else {
    const auto check = lunar_distance::CheckDistanceAtPosition(
        m_sight.LunarObservation(), ephemeris, {latitude, longitude});
    if (check.valid) {
      m_trueDistance->SetLabel(Angle(check.true_distance_deg));
      m_clearedDistance->SetLabel(Angle(check.cleared_distance_deg));
      m_lunarError->SetLabel(
          wxString::Format("%+.3f arcmin", check.lunar_error_arcmin));
      m_checkReport += wxString::Format(
          _("Reference position: %s, %s\nTrue LD: %s\nCleared LD (model "
            "corrected): %s\nLunar error (cleared - true): %+.6f arcmin\n"),
          Angle(latitude, NavigationAngleKind::Latitude),
          Angle(longitude, NavigationAngleKind::Longitude),
          Angle(check.true_distance_deg), Angle(check.cleared_distance_deg),
          check.lunar_error_arcmin);
      status =
          _("Accuracy assumes independently known UTC and position. Positive "
            "error means the observed LD is too large.");
    } else
      status = _("Accuracy check unavailable: ") +
               wxString::FromUTF8(check.error.c_str());
  }
  if (m_referenceEdited) status = _("Reference edited. ") + status;
  m_referenceStatus->SetLabel(status);
  m_checkReport += status + "\n";
  m_details->ChangeValue(ReportText());
}

void LunarResultsDialog::UpdatePositions() {
#ifndef __OCPN__ANDROID__
  m_positions->DeleteAllItems();
  auto append =
      [&](const wxString& basis,
          const std::vector<lunar_distance::GeographicPoint>& positions,
          const wxString& sigma) {
        for (std::size_t index = 0; index < positions.size(); ++index) {
          const auto& position = positions[index];
          const long row = m_positions->InsertItem(
              m_positions->GetItemCount(),
              basis + wxString::Format(" / %zu", index + 1));
          m_positions->SetItem(
              row, 1,
              Angle(position.latitude_deg, NavigationAngleKind::Latitude));
          m_positions->SetItem(
              row, 2,
              Angle(position.longitude_deg, NavigationAngleKind::Longitude));
          m_positions->SetItem(
              row, 3,
              HasSavedDr(m_sight)
                  ? wxString::Format(
                        "%.1f NM",
                        lunar_distance::GreatCircleDistanceNm(
                            {m_sight.m_DRLat, m_sight.m_DRLon}, position))
                  : _("Unavailable"));
          m_positions->SetItem(row, 4, sigma);
        }
      };
  if (m_enteredPositions.valid)
    append(_("Entered ") +
               lunar_presentation::ResultUtc(m_sight.m_CorrectedDateTime)
                   .AfterFirst(' '),
           m_enteredPositions.candidates, _("Not estimated"));
  else
    m_positions->InsertItem(0, _("Entered UTC: no altitude intersection"));
  if (m_selected >= 0) {
    const auto& candidate = m_sight.m_LunarCandidates[m_selected];
    append(_("Derived ") + lunar_presentation::SolutionId(m_selected) + " " +
               lunar_presentation::ResultUtc(
                   UtcDateTime::AddSeconds(m_sight.m_CorrectedDateTime,
                                           candidate.offset_seconds))
                   .AfterFirst(' '),
           m_sight.m_LunarPositionResult.candidates, PositionSigma(candidate));
  }
  if (m_positions->GetItemCount() == 0)
    m_positions->InsertItem(0, _("No altitude intersection"));
  FitColumns(m_positions);
  m_positions->SetMinSize(
      wxSize(0, (std::max(1, std::min(4, m_positions->GetItemCount())) + 1) *
                        (GetCharHeight() + 8) +
                    16));
#endif
  m_geometry->SetLabel(wxEmptyString);
  const auto& geometries = m_sight.m_LunarPositionResult.geometry;
  std::size_t index = m_sight.m_LunarSelectedPosition >= 0
                          ? m_sight.m_LunarSelectedPosition
                          : 0;
  if (m_selected >= 0 && index < geometries.size()) {
    const auto& geometry = geometries[index];
    const wxString assessment = geometry.effective_crossing_angle_deg < 15
                                    ? _("Very weak position geometry")
                                : geometry.effective_crossing_angle_deg < 30
                                    ? _("Weak position geometry")
                                : geometry.effective_crossing_angle_deg < 60
                                    ? _("Moderate position geometry")
                                    : _("Good position geometry");
    m_geometry->SetLabel(wxString::Format(
        _("Solution %s: Moon Zn %.1f%c | %s Zn %.1f%c | Azimuth separation "
          "%.1f%c | %s"),
        lunar_presentation::SolutionId(m_selected), geometry.moon_azimuth_deg,
        0x00B0, m_sight.m_Body, geometry.body_azimuth_deg, 0x00B0,
        geometry.azimuth_separation_deg, 0x00B0, assessment));
  }
}

void LunarResultsDialog::UpdateCalculations() {
  const auto report = lunar_presentation::BuildReport(m_sight, m_selected);
  RenderTables(m_overview, report.heading, report.overview);
  RenderTables(m_almanac, report.heading, report.almanac);
  RenderTables(m_search, report.heading, report.search);
  m_details->ChangeValue(ReportText());
}

wxString LunarResultsDialog::ReportText() const {
  wxString text = _("RESULTS SOLUTION IDS\n");
  for (std::size_t index = 0; index < m_sight.m_LunarCandidates.size();
       ++index) {
    const auto& candidate = m_sight.m_LunarCandidates[index];
    text +=
        lunar_presentation::SolutionId(index) + " | " +
        UtcDateTime::FormatUtc(
            UtcDateTime::AddSeconds(m_sight.m_CorrectedDateTime,
                                    candidate.offset_seconds),
            "%Y-%m-%d %H:%M:%S.%l UTC") +
        wxString::Format(" | %+.6f s", candidate.offset_seconds) +
        (static_cast<long>(index) == m_selected ? _(" | selected\n") : "\n");
  }
  return text + "\n" + m_sight.m_CalcStr + "\n" + m_checkReport;
}

#ifdef __OCPN__ANDROID__
void LunarResultsDialog::RefreshAndroidCards() {
  if (m_androidRefreshPending) return;
  m_androidRefreshPending = true;
  wxWeakRef<LunarResultsDialog> weak(this);
  QTimer::singleShot(0, GetHandle(), [weak]() {
    if (!weak) return;
    auto* self = weak.get();
    self->m_androidRefreshPending = false;
    auto* candidates = self->m_androidCandidates->GetSizer();
    candidates->Clear(true);
    for (std::size_t index = 0; index < self->m_sight.m_LunarCandidates.size();
         ++index) {
      const auto& candidate = self->m_sight.m_LunarCandidates[index];
      const wxString caption =
          (static_cast<long>(index) == self->m_selected
               ? _("Selected solution ")
               : _("Solution ")) +
          lunar_presentation::SolutionId(index) + "\n" +
          lunar_presentation::ResultUtc(UtcDateTime::AddSeconds(
              self->m_sight.m_CorrectedDateTime, candidate.offset_seconds)) +
          " UTC\n" +
          wxString::Format(_("Correction: %+.1f s\nUncertainty (1-sigma): %s"),
                           candidate.offset_seconds, TimeSigma(candidate));
      auto* card = new wxButton(self->m_androidCandidates, wxID_ANY, caption);
      card->Bind(wxEVT_BUTTON, [weak, index](wxCommandEvent&) {
        if (!weak) return;
        QTimer::singleShot(0, weak->GetHandle(), [weak, index]() {
          if (weak) weak->SelectCandidate(index);
        });
      });
      candidates->Add(card, 0, wxALL | wxEXPAND, 6);
      CN_StyleAndroidControls(card);
      card->SetMinSize(wxSize(
          0, QFontMetrics(card->GetHandle()->font()).lineSpacing() * 4 + 32));
      if (static_cast<long>(index) == self->m_selected)
        card->GetHandle()->setStyleSheet(
            card->GetHandle()->styleSheet() +
            "QPushButton { background: #d1e8f1; color: #102e3b; }");
    }
    auto* positions = self->m_androidPositions->GetSizer();
    positions->Clear(true);
    auto append =
        [&](const wxString& basis,
            const std::vector<lunar_distance::GeographicPoint>& points,
            const wxString& sigma) {
          for (std::size_t index = 0; index < points.size(); ++index) {
            const auto& point = points[index];
            const wxString caption =
                basis +
                wxString::Format(
                    _(" / branch %zu\nLatitude: %s\nLongitude: %s\nFrom saved "
                      "DR: %s\nHorizontal RMS: %s"),
                    index + 1,
                    Angle(point.latitude_deg, NavigationAngleKind::Latitude),
                    Angle(point.longitude_deg, NavigationAngleKind::Longitude),
                    HasSavedDr(self->m_sight)
                        ? wxString::Format(
                              "%.1f NM", lunar_distance::GreatCircleDistanceNm(
                                             {self->m_sight.m_DRLat,
                                              self->m_sight.m_DRLon},
                                             point))
                        : _("Unavailable"),
                    sigma);
            positions->Add(Label(self->m_androidPositions, caption), 0,
                           wxALL | wxEXPAND, 10);
          }
        };
    if (self->m_enteredPositions.valid)
      append(_("At entered UTC ") + lunar_presentation::ResultUtc(
                                        self->m_sight.m_CorrectedDateTime),
             self->m_enteredPositions.candidates, _("Not estimated"));
    else
      positions->Add(
          Label(self->m_androidPositions,
                _("Entered UTC: no altitude intersection. ") +
                    wxString::FromUTF8(self->m_enteredPositions.error.c_str())),
          0, wxALL | wxEXPAND, 10);
    if (self->m_selected >= 0) {
      const auto& candidate = self->m_sight.m_LunarCandidates[self->m_selected];
      append(
          _("At derived UTC ") +
              lunar_presentation::SolutionId(self->m_selected) + " " +
              lunar_presentation::ResultUtc(UtcDateTime::AddSeconds(
                  self->m_sight.m_CorrectedDateTime, candidate.offset_seconds)),
          self->m_sight.m_LunarPositionResult.candidates,
          PositionSigma(candidate));
    }
    CN_StyleAndroidControls(self->m_androidPositions);
    self->Relayout();
  });
}
#endif

void LunarResultsDialog::ApplySelectedWatchOffset(wxCommandEvent&) {
  const long selected = m_selected;
  if (selected < 0 ||
      static_cast<std::size_t>(selected) >= m_sight.m_LunarCandidates.size()) {
    CelestialMessageBox(_("Select a UTC candidate first."), _("Lunar distance"),
                        wxOK | wxICON_INFORMATION, this);
    return;
  }
  SightDialog* sightDialog = dynamic_cast<SightDialog*>(GetParent());
  CelestialNavigationDialog* mainDialog =
      sightDialog
          ? dynamic_cast<CelestialNavigationDialog*>(sightDialog->GetParent())
          : nullptr;
  if (!sightDialog || !mainDialog) return;
  const auto& candidate = m_sight.m_LunarCandidates[selected];
  LunarSolutionRecord record;
  record.reference_time =
      UtcDateTime::FormatUtc(m_sight.m_DateTime, "%Y-%m-%d %H:%M:%S");
  record.method = m_sight.m_LunarSeparateTimes
                      ? "WGS84 time-tagged UTC/position"
                      : "WGS84 simultaneous UTC/position";
  record.base_correction_seconds = mainDialog->GetClockCorrection();
  record.additional_correction_seconds = candidate.offset_seconds;
  record.time_sigma_seconds = candidate.time_uncertainty_seconds;
  record.inputs.push_back(LunarInputSnapshot(m_sight));
  record.report = _("Selected solution: ") +
                  lunar_presentation::SolutionId(selected) + "\n" +
                  m_details->GetValue();
  if (mainDialog->SaveLunarSolution(record)) m_applyOffset->Enable(false);
}
