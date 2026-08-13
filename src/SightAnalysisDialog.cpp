#include "SightAnalysisDialog.h"

#include "CelestialNavigationDialog.h"
#include "NavigationAlgorithms.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include "celestial_navigation_pi.h"

#include <wx/checkbox.h>
#include <wx/dcbuffer.h>
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include <algorithm>
#include <cmath>

class ResidualPlotPanel : public wxPanel {
public:
  explicit ResidualPlotPanel(wxWindow* parent)
      : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 150)) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &ResidualPlotPanel::OnPaint, this);
  }
  void SetAnalysis(const SequenceStatistics& analysis) {
    m_analysis = analysis;
    Refresh();
  }

private:
  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    const wxSize size = GetClientSize();
    const int left = 48, right = size.x - 12, top = 12, bottom = size.y - 28;
    dc.SetPen(wxPen(wxColour(110, 110, 110)));
    dc.DrawLine(left, top, left, bottom);
    dc.DrawLine(left, bottom, right, bottom);
    dc.DrawText(_("UTC sequence"), std::max(left, right - 95), bottom + 5);
    if (!m_analysis.valid || m_analysis.residuals.empty()) return;
    wxDateTime first = m_analysis.residuals.front().utc;
    wxDateTime last = first;
    double extent = 2.0;
    for (const auto& residual : m_analysis.residuals) {
      if (residual.utc.IsEarlierThan(first)) first = residual.utc;
      if (residual.utc.IsLaterThan(last)) last = residual.utc;
      extent = std::max(extent, std::abs(residual.interceptMinutes));
    }
    extent *= 1.15;
    const double seconds = std::max(
        1.0, static_cast<double>((last - first).GetMilliseconds().GetValue()) /
                 1000.0);
    auto point = [&](const FixResidual& residual) {
      const double x =
          static_cast<double>((residual.utc - first)
                                  .GetMilliseconds()
                                  .GetValue()) /
          1000.0 / seconds;
      const double y = residual.interceptMinutes / extent;
      return wxPoint(left + static_cast<int>(x * (right - left)),
                     (top + bottom) / 2 -
                         static_cast<int>(y * (bottom - top) / 2));
    };
    const int zero = (top + bottom) / 2;
    dc.SetPen(wxPen(wxColour(160, 160, 160), 1, wxPENSTYLE_DOT));
    dc.DrawLine(left, zero, right, zero);
    dc.DrawText(wxString::Format("%+.1f'", extent), 2, top - 3);
    dc.DrawText(wxString::Format("%+.1f'", -extent), 2, bottom - 10);
    wxPoint previous;
    bool havePrevious = false;
    for (const auto& residual : m_analysis.residuals) {
      const wxPoint p = point(residual);
      if (havePrevious) {
        dc.SetPen(wxPen(wxColour(80, 120, 180)));
        dc.DrawLine(previous, p);
      }
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(residual.outlier ? wxColour(210, 55, 40)
                                           : wxColour(25, 120, 200)));
      dc.DrawCircle(p, residual.outlier ? 5 : 4);
      previous = p;
      havePrevious = true;
    }
  }
  SequenceStatistics m_analysis;
};

SightAnalysisDialog::SightAnalysisDialog(CelestialNavigationDialog* parent)
    : wxDialog(parent, wxID_ANY, _("Sight Sequence Analyzer"), wxDefaultPosition,
               wxSize(820, 540),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_parent(parent) {
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxStaticText* introduction = new wxStaticText(
      this, wxID_ANY,
      _("Compare visible altitude sights with a known/DR track to reveal scatter, outliers, trend and personal bias."));
  introduction->Wrap(780);
  root->Add(introduction, 0, wxALL | wxEXPAND, 8);

  wxBoxSizer* options = new wxBoxSizer(wxHORIZONTAL);
  m_onlySelectedBody =
      new wxCheckBox(this, wxID_ANY, _("Only the selected sight's body"));
  options->Add(m_onlySelectedBody, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
  m_moving = new wxCheckBox(this, wxID_ANY, _("Moving observer"));
  options->Add(m_moving, 0, wxALIGN_CENTER_VERTICAL);
  root->Add(options, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

  wxBoxSizer* position = new wxBoxSizer(wxHORIZONTAL);
  position->Add(new wxStaticText(this, wxID_ANY, _("Track latitude")), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_latitude = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                    wxSize(125, -1), wxSP_ARROW_KEYS, -90, 90, 0,
                                    0.0001);
  m_latitude->SetDigits(4);
  position->Add(m_latitude, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 18);
  position->Add(new wxStaticText(this, wxID_ANY, _("Track longitude")), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_longitude = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                     wxSize(125, -1), wxSP_ARROW_KEYS, -180, 180,
                                     0, 0.0001);
  m_longitude->SetDigits(4);
  position->Add(m_longitude, 0, wxALIGN_CENTER_VERTICAL);
  root->Add(position, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

  wxBoxSizer* motion = new wxBoxSizer(wxHORIZONTAL);
  motion->Add(new wxStaticText(this, wxID_ANY, _("COG true")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_course = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                  wxSize(110, -1), wxSP_ARROW_KEYS, 0, 359.9, 0,
                                  0.1);
  m_course->SetDigits(1);
  motion->Add(m_course, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 18);
  motion->Add(new wxStaticText(this, wxID_ANY, _("SOG kn")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_speed = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                 wxSize(110, -1), wxSP_ARROW_KEYS, 0, 100, 0,
                                 0.1);
  m_speed->SetDigits(1);
  motion->Add(m_speed, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 18);
  wxButton* analyze = new wxButton(this, wxID_ANY, _("Analyze visible sights"));
  motion->Add(analyze, 0, wxALIGN_CENTER_VERTICAL);
  root->Add(motion, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
  m_summary = new wxStaticText(this, wxID_ANY, wxEmptyString);
  root->Add(m_summary, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
  m_plot = new ResidualPlotPanel(this);
  root->Add(m_plot, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
  m_results = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_HRULES);
  m_results->InsertColumn(0, _("UTC"), wxLIST_FORMAT_LEFT, 160);
  m_results->InsertColumn(1, _("Body"), wxLIST_FORMAT_LEFT, 110);
  m_results->InsertColumn(2, _("Hc"), wxLIST_FORMAT_LEFT, 85);
  m_results->InsertColumn(3, _("Ho-Hc"), wxLIST_FORMAT_LEFT, 90);
  m_results->InsertColumn(4, _("Assessment"), wxLIST_FORMAT_LEFT, 220);
  root->Add(m_results, 1, wxALL | wxEXPAND, 8);
  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  buttons->AddButton(new wxButton(this, wxID_CLOSE));
  buttons->Realize();
  root->Add(buttons, 0, wxALL | wxEXPAND, 6);
  SetSizer(root);

  double lat = 0.0, lon = 0.0;
  if (m_parent->GetPlugin()->GetBoatPosition(&lat, &lon)) {
    m_latitude->SetValue(lat);
    m_longitude->SetValue(lon);
  } else if (const Sight* selected = m_parent->GetSelectedSight()) {
    m_latitude->SetValue(selected->m_DRLat);
    m_longitude->SetValue(selected->m_DRLon);
  }
  const BoatNavigationSnapshot boat =
      m_parent->GetPlugin()->GetBoatNavigationSnapshot();
  if (boat.valid) {
    m_course->SetValue(boat.cogTrue);
    m_speed->SetValue(boat.sogKnots);
  }
  analyze->Bind(wxEVT_BUTTON, &SightAnalysisDialog::Analyze, this);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); }, wxID_CLOSE);
  wxCommandEvent dummy;
  Analyze(dummy);
  CentreOnParent();
}

void SightAnalysisDialog::Analyze(wxCommandEvent&) {
  std::vector<FixObservation> observations;
  const Sight* selected = m_parent->GetSelectedSight();
  for (const Sight& sight : m_parent->m_Sights) {
    if (!sight.IsVisible() || !sight.IsCalculated() ||
        sight.m_Type != Sight::ALTITUDE)
      continue;
    if (m_onlySelectedBody->GetValue() && selected &&
        sight.m_Body != selected->m_Body)
      continue;
    FixObservation observation;
    observation.label = sight.m_Body;
    observation.body = sight.m_Body;
    observation.utc = UtcDateTime::ToInstant(sight.m_DateTime);
    observation.observedAltitude = sight.m_ObservedAltitude;
    observation.uncertaintyMinutes =
        std::max(0.1, sight.m_MeasurementCertainty);
    observations.push_back(observation);
  }
  m_results->DeleteAllItems();
  if (observations.size() < 2) {
    m_summary->SetLabel(
        _("At least two visible, calculated altitude sights are required."));
    return;
  }
  ObserverMotion motion;
  motion.referenceUtc = observations.front().utc;
  motion.latitude = m_latitude->GetValue();
  motion.longitude = m_longitude->GetValue();
  motion.courseTrue = m_course->GetValue();
  motion.speedKnots = m_speed->GetValue();
  motion.moving = m_moving->GetValue();
  const SequenceStatistics analysis = SightSequenceAnalyzer::Analyze(
      observations, motion, motion.latitude, motion.longitude);
  if (!analysis.valid) {
    m_summary->SetLabel(_("The sequence could not be analyzed."));
    return;
  }
  m_plot->SetAnalysis(analysis);
  m_summary->SetLabel(wxString::Format(
      _("%u sights  |  mean %+.2f'  SD %.2f'  |  median/personal bias %+.2f'  MAD %.2f'  |  trend %+.2f' per hour\n"
        "Bias is reported, not silently applied. Review flagged sights before choosing any correction."),
      analysis.count, analysis.meanMinutes, analysis.standardDeviationMinutes,
      analysis.personalBiasMinutes, analysis.madMinutes,
      analysis.trendMinutesPerHour));
  for (const auto& residual : analysis.residuals) {
    const long row = m_results->InsertItem(
        m_results->GetItemCount(),
        residual.utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC));
    m_results->SetItem(row, 1, residual.body);
    m_results->SetItem(row, 2,
                       wxString::Format("%.3f%c", residual.calculatedAltitude,
                                        0x00b0));
    m_results->SetItem(row, 3,
                       wxString::Format("%+.2f'", residual.interceptMinutes));
    m_results->SetItem(row, 4,
                       residual.outlier ? _("Possible outlier — inspect")
                                        : _("Within robust sequence spread"));
    if (residual.outlier)
      m_results->SetItemBackgroundColour(row, wxColour(255, 225, 210));
  }
}
