#include "PlannerDialog.h"
#include "WaypointPickerDialog.h"

#include "CelestialNavigationDialog.h"
#include "DialogGeometry.h"
#include "NavigationUIUtils.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include "Utf8Translation.h"
#include "celestial_navigation_pi.h"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dcbuffer.h>
#include <wx/datectrl.h>
#include <wx/filedlg.h>
#include <wx/fileconf.h>
#include <wx/ffile.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/srchctrl.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timectrl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
void AddColumn(wxListCtrl* list, int column, const wxString& title,
               int width = wxLIST_AUTOSIZE_USEHEADER) {
  list->InsertColumn(column, title);
  list->SetColumnWidth(column, width < 0 ? width
      : std::max(width, list->GetTextExtent(title).x + 24));
}

class SkyPlotPanelImpl : public wxPanel {
public:
  explicit SkyPlotPanelImpl(wxWindow* parent, bool equatorial = false)
      : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(330, 290)),
        m_equatorial(equatorial) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &SkyPlotPanelImpl::OnPaint, this);
  }
  void SetBodies(const std::vector<RankedBody>& bodies, bool daylight) {
    m_bodies = bodies;
    m_daylight = daylight;
    Refresh();
  }
  void SetSelectedBody(const wxString& body) { m_selected = body; Refresh(); }
  void SetOverlays(const std::vector<PlannerSkyPoint>& ecliptic,
                   const std::vector<PlannerSkyPoint>& moonPath,
                   bool showEcliptic, bool showMoonPath) {
    m_ecliptic = ecliptic;
    m_moonPath = moonPath;
    m_showEcliptic = showEcliptic;
    m_showMoonPath = showMoonPath;
    Refresh();
  }

private:
  wxPoint LocalPoint(const PlannerSkyPoint& point, const wxPoint& center,
                     int radius) const {
    const double radial = radius *
                          (90.0 - std::max(0.0, std::min(90.0, point.altitude))) /
                          90.0;
    const double angle = point.azimuth * 3.141592653589793 / 180.0;
    return wxPoint(center.x + static_cast<int>(radial * std::sin(angle)),
                   center.y - static_cast<int>(radial * std::cos(angle)));
  }

  void DrawTrackTime(wxDC& dc, const PlannerSkyPoint& sample,
                     const wxPoint& point, const wxRect& bounds,
                     const wxColour& colour, std::vector<wxRect>& labels) {
    const wxString text = sample.utc.Format("%H:%MZ", wxDateTime::UTC);
    const wxSize extent = dc.GetTextExtent(text);
    for (int row : {-1, 1, -2, 2, -3, 3, -4, 4}) {
      for (int side : {1, -1}) {
        wxRect label(
            std::max(bounds.x, std::min(point.x +
                (side > 0 ? 8 : -extent.x - 8), bounds.GetRight() - extent.x)),
            std::max(bounds.y, std::min(point.y + row * (extent.y + 3),
                                       bounds.GetBottom() - extent.y)),
            extent.x, extent.y);
        wxRect padded = label;
        padded.Inflate(2);
        if (std::any_of(labels.begin(), labels.end(),
            [&](const wxRect& used) { return used.Intersects(padded); })) continue;
        dc.SetPen(wxPen(colour, 1));
        dc.DrawLine(point, wxPoint(label.x + extent.x / 2, label.y + extent.y / 2));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(GetBackgroundColour()));
        dc.DrawRectangle(padded);
        dc.SetTextForeground(colour);
        dc.DrawText(text, label.GetPosition());
        labels.push_back(padded);
        return;
      }
    }
  }

  void DrawLocalTrack(wxDC& dc, const std::vector<PlannerSkyPoint>& track,
                      const wxPoint& center, int radius,
                      const wxColour& colour, bool markTimes,
                      std::vector<wxRect>& labels) {
    dc.SetPen(wxPen(colour, 2));
    wxPoint previous;
    bool previousVisible = false;
    for (size_t i = 0; i < track.size(); ++i) {
      const bool visible = track[i].altitude >= 0.0;
      const wxPoint point = LocalPoint(track[i], center, radius);
      if (visible && previousVisible) dc.DrawLine(previous, point);
      if (visible && markTimes &&
          (i == 0 || i == track.size() / 2 || i + 1 == track.size())) {
        dc.SetBrush(wxBrush(colour));
        dc.DrawCircle(point, i == track.size() / 2 ? 5 : 3);
        DrawTrackTime(dc, track[i], point, GetClientRect(), colour, labels);
        dc.SetPen(wxPen(colour, 2));
      }
      previous = point;
      previousVisible = visible;
    }
  }

  void PaintEquatorial(wxDC& dc, const wxSize& size) {
    std::vector<wxRect> labels;
    const wxRect chart(42, 20, std::max(30, size.x - 57),
                       std::max(30, size.y - 52));
    dc.SetPen(wxPen(wxColour(215, 220, 225)));
    for (int sha = 0; sha <= 360; sha += 60) {
      const int x = chart.x + chart.width * sha / 360;
      dc.DrawLine(x, chart.y, x, chart.GetBottom());
      dc.DrawText(wxString::Format("%d", sha), x - 9, chart.GetBottom() + 5);
    }
    for (int dec = -60; dec <= 60; dec += 30) {
      const int y = chart.y + chart.height * (60 - dec) / 120;
      dc.DrawLine(chart.x, y, chart.GetRight(), y);
      dc.DrawText(wxString::Format("%d", dec), 4, y - 7);
    }
    dc.SetTextForeground(wxColour(70, 75, 82));
    dc.DrawText("SHA", chart.GetRight() - 25, 2);
    auto xy = [&chart](const PlannerSkyPoint& point) {
      return wxPoint(chart.x + static_cast<int>(chart.width * point.sha / 360.0),
                     chart.y + static_cast<int>(chart.height *
                         (60.0 - point.declination) / 120.0));
    };
    auto drawTrack = [&](const std::vector<PlannerSkyPoint>& track,
                         const wxColour& colour, bool markTimes) {
      dc.SetPen(wxPen(colour, 2));
      for (size_t i = 1; i < track.size(); ++i) {
        const wxPoint a = xy(track[i - 1]), b = xy(track[i]);
        if (std::abs(a.x - b.x) < chart.width / 2) dc.DrawLine(a, b);
      }
      if (markTimes && !track.empty()) {
        dc.SetBrush(wxBrush(colour));
        for (size_t i : {size_t(0), track.size() / 2, track.size() - 1}) {
          const wxPoint p = xy(track[i]);
          dc.DrawCircle(p, i == track.size() / 2 ? 5 : 3);
          DrawTrackTime(dc, track[i], p, chart, colour, labels);
          dc.SetPen(wxPen(colour, 2));
          dc.SetBrush(wxBrush(colour));
        }
      }
    };
    if (m_showEcliptic)
      drawTrack(m_ecliptic, wxColour(200, 120, 20), false);
    if (m_showMoonPath)
      drawTrack(m_moonPath, wxColour(38, 100, 210), true);
    unsigned drawnLabels = 0;
    for (size_t index : SightRanker::SkyLabelPriority(m_bodies)) {
      const auto& body = m_bodies[index];
      if (body.state.declination < -60.0 || body.state.declination > 60.0)
        continue;
      const PlannerSkyPoint position{body.state.utc, 0, 0, body.state.sha,
                                      body.state.declination};
      const wxPoint p = xy(position);
      const wxColour colour = body.state.body == "Moon"
                                  ? wxColour(38, 100, 210)
                                  : body.state.body == "Sun"
                                        ? wxColour(220, 160, 0)
                                        : body.state.isPlanet
                                              ? wxColour(205, 55, 45)
                                              : wxColour(45, 45, 45);
      dc.SetPen(wxPen(colour));
      dc.SetBrush(wxBrush(colour));
      dc.DrawCircle(p, 3);
      if (body.state.body == m_selected) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(p, 7);
      }
      if (drawnLabels >= 10 && body.state.body != m_selected) continue;
      const wxSize extent = dc.GetTextExtent(body.state.body);
      wxRect label(std::min(p.x + 5, chart.GetRight() - extent.x),
                   std::max(chart.y, std::min(p.y - extent.y / 2,
                                              chart.GetBottom() - extent.y)),
                   extent.x, extent.y);
      bool overlap = false;
      for (const auto& used : labels)
        if (used.Intersects(label)) { overlap = true; break; }
      if (!overlap) {
        dc.SetTextForeground(colour);
        dc.DrawText(body.state.body, label.GetPosition());
        label.Inflate(2, 2);
        labels.push_back(label);
        ++drawnLabels;
      }
    }
  }

  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    const wxSize size = GetClientSize();
    if (m_equatorial) {
      PaintEquatorial(dc, size);
      return;
    }
    const wxPoint center(size.x / 2, size.y / 2);
    const int radius = std::max(10, std::min(size.x, size.y) / 2 - 22);
    dc.SetPen(wxPen(wxColour(100, 100, 100)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawCircle(center, radius);
    dc.DrawCircle(center, radius / 2);
    dc.DrawLine(center.x, center.y - radius, center.x, center.y + radius);
    dc.DrawLine(center.x - radius, center.y, center.x + radius, center.y);
    dc.DrawText("N", center.x - 5, center.y - radius - 20);
    dc.DrawText("E", center.x + radius + 4, center.y - 8);
    dc.DrawText("S", center.x - 5, center.y + radius + 2);
    dc.DrawText("W", center.x - radius - 18, center.y - 8);
    std::vector<wxRect> labels;
    if (m_showEcliptic)
      DrawLocalTrack(dc, m_ecliptic, center, radius, wxColour(200, 120, 20),
                     false, labels);
    if (m_showMoonPath)
      DrawLocalTrack(dc, m_moonPath, center, radius, wxColour(38, 100, 210),
                     true, labels);
    struct PlottedBody {
      wxPoint point;
      wxColour colour;
    };
    std::vector<PlottedBody> plotted;
    plotted.reserve(m_bodies.size());
    for (const auto& body : m_bodies) {
      const bool belowHorizon = body.state.geometricAltitude < 0.0;
      const double plotAltitude =
          std::max(0.0, std::min(90.0, body.state.geometricAltitude));
      const double radial = radius * (90.0 - plotAltitude) / 90.0;
      const double angle = body.state.azimuthTrue * 3.141592653589793 / 180.0;
      const wxPoint p(center.x + static_cast<int>(radial * std::sin(angle)),
                      center.y - static_cast<int>(radial * std::cos(angle)));
      wxColour colour(45, 45, 45);
      if (body.state.body == "Sun")
        colour = wxColour(240, 180, 0);
      else if (body.state.body == "Moon")
        colour = wxColour(40, 100, 210);
      else if (body.state.isPlanet)
        colour = wxColour(205, 55, 45);
      else if (m_daylight)
        colour = wxColour(150, 150, 150);
      dc.SetPen(wxPen(colour));
      dc.SetBrush(belowHorizon ? *wxTRANSPARENT_BRUSH : wxBrush(colour));
      dc.DrawCircle(p, belowHorizon ? 4 : 3);
      if (body.state.body == m_selected) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(p, 7);
      }
      plotted.push_back({p, colour});
    }

    // Label the brightest bodies first, while reserving a fair share of the
    // available space for each compass quadrant. Collision suppression still
    // has final authority on compact displays.
    std::array<unsigned, 4> quadrantLabels = {{0, 0, 0, 0}};
    const unsigned labelsPerQuadrant = 2;
    for (const size_t index : SightRanker::SkyLabelPriority(m_bodies)) {
      const RankedBody& body = m_bodies[index];
      const wxPoint& p = plotted[index].point;
      double azimuth = std::fmod(body.state.azimuthTrue, 360.0);
      if (azimuth < 0.0) azimuth += 360.0;
      const unsigned quadrant =
          static_cast<unsigned>((azimuth + 45.0) / 90.0) % 4;
      if (quadrantLabels[quadrant] >= labelsPerQuadrant) continue;
      const wxSize extent = dc.GetTextExtent(body.state.body);
      int labelX = p.x + 5;
      if (labelX + extent.x > size.x - 3) labelX = p.x - extent.x - 5;
      labelX = std::max(2, std::min(labelX, size.x - extent.x - 2));
      int labelY =
          std::max(2, std::min(p.y - extent.y / 2, size.y - extent.y - 2));
      wxRect label(labelX, labelY, extent.x, extent.y);
      wxRect padded = label;
      padded.Inflate(2, 1);
      bool overlaps = false;
      for (const wxRect& used : labels)
        if (used.Intersects(padded)) {
          overlaps = true;
          break;
        }
      if (!overlaps) {
        dc.SetTextForeground(plotted[index].colour);
        dc.DrawText(body.state.body, label.GetPosition());
        labels.push_back(padded);
        ++quadrantLabels[quadrant];
      }
    }
  }
  std::vector<RankedBody> m_bodies;
  std::vector<PlannerSkyPoint> m_ecliptic;
  std::vector<PlannerSkyPoint> m_moonPath;
  bool m_daylight = false;
  bool m_equatorial = false;
  bool m_showEcliptic = true;
  bool m_showMoonPath = false;
  wxString m_selected;
};

}  // namespace

class SkyPlotPanel : public SkyPlotPanelImpl {
public:
  explicit SkyPlotPanel(wxWindow* parent, bool equatorial = false)
      : SkyPlotPanelImpl(parent, equatorial) {}
  using SkyPlotPanelImpl::SetBodies;
  using SkyPlotPanelImpl::SetOverlays;
  using SkyPlotPanelImpl::SetSelectedBody;
};

void PlannerDialog::SelectPageForIntegration(unsigned page) {
  if (m_notebook && page < m_notebook->GetPageCount()) {
    m_notebook->SetSelection(page);
    m_notebook->GetPage(page)->Layout();
    m_notebook->Layout();
    Layout();
    Refresh();
    Update();
  }
}

PlannerDialog::PlannerDialog(CelestialNavigationDialog* parent)
    : wxDialog(parent, wxID_ANY, _("Sun, Moon and Sight Planner"),
               wxDefaultPosition, wxSize(1370, 820),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_parent(parent),
      m_bodySortColumn(6),
      m_bodySortAscending(false),
      m_lastValidZoneOffset(0.0),
      m_zoneOffsetTextValid(true),
      m_updatingZoneOffset(false) {
  const CelestialNavigationDefaults defaults =
      LoadCelestialNavigationDefaults();
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer* context =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Planning context"));
  wxFlexGridSizer* grid = new wxFlexGridSizer(0, 6, 4, 6);
  grid->AddGrowableCol(5);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Position")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_positionSource = new wxChoice(this, wxID_ANY);
  m_positionSource->Append(_("Manual"));
  m_positionSource->Append(_("Current boat position"));
  m_positionSource->Append(_("Chart cursor"));
  m_positionSource->Append(_("Selected sight DR"));
  m_positionSource->Append(_("Last calculated fix"));
  m_positionSource->Append(_("Waypoint or place..."));
  m_positionSource->SetSelection(1);
  grid->Add(m_positionSource, 0, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Latitude")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_latitude = new NavigationAngleCtrl(this, NavigationAngleKind::Latitude, 0.0,
                                       -90.0, 90.0, wxSize(145, -1));
  grid->Add(m_latitude);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Longitude")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_longitude = new NavigationAngleCtrl(this, NavigationAngleKind::Longitude,
                                        0.0, -180.0, 180.0, wxSize(145, -1));
  grid->Add(m_longitude);
  for (auto* coordinate : {m_latitude, m_longitude})
    coordinate->SetMinSize(wxSize(std::max(145,
        coordinate->GetTextExtent(FormatNavigationAngle(-179.99999,
            NavigationAngleKind::Longitude, true)).x + 24), -1));

  grid->Add(new wxStaticText(this, wxID_ANY, _("Time")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_timeSource = new wxChoice(this, wxID_ANY);
  m_timeSource->Append(_("Now"));
  m_timeSource->Append(_("Selected sight"));
  m_timeSource->Append(_("Manual date/time"));
  m_timeSource->SetSelection(0);
  grid->Add(m_timeSource, 0, wxEXPAND);
  m_dateLabel = new wxStaticText(this, wxID_ANY, _("Date (UTC)"));
  grid->Add(m_dateLabel, 0, wxALIGN_CENTER_VERTICAL);
  m_dateContainer = new wxPanel(this);
  wxBoxSizer* dateSizer = new wxBoxSizer(wxVERTICAL);
  m_utcDate = new wxDatePickerCtrl(m_dateContainer, wxID_ANY);
  m_nauticalDate =
      new wxTextCtrl(m_dateContainer, wxID_ANY, wxEmptyString,
                     wxDefaultPosition, wxSize(145, -1), wxTE_PROCESS_ENTER);
  m_nauticalDate->SetHint(_("YYYY-MM-DD"));
  dateSizer->Add(m_utcDate, 0, wxEXPAND);
  dateSizer->Add(m_nauticalDate, 0, wxEXPAND);
  m_dateContainer->SetSizer(dateSizer);
  grid->Add(m_dateContainer, 0, wxEXPAND);
  m_timeLabel = new wxStaticText(this, wxID_ANY, _("Time (UTC)"));
  grid->Add(m_timeLabel, 0, wxALIGN_CENTER_VERTICAL);
  m_timeContainer = new wxPanel(this);
  wxBoxSizer* timeSizer = new wxBoxSizer(wxVERTICAL);
  m_utcTime = new wxTimePickerCtrl(m_timeContainer, wxID_ANY);
  m_nauticalTime =
      new wxTextCtrl(m_timeContainer, wxID_ANY, wxEmptyString,
                     wxDefaultPosition, wxSize(145, -1), wxTE_PROCESS_ENTER);
  m_nauticalTime->SetHint(_("HH:MM:SS"));
  timeSizer->Add(m_utcTime, 0, wxEXPAND);
  timeSizer->Add(m_nauticalTime, 0, wxEXPAND);
  m_timeContainer->SetSizer(timeSizer);
  grid->Add(m_timeContainer, 0, wxEXPAND);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Enter time as")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_inputTimeBasis = new wxChoice(this, wxID_ANY);
  m_inputTimeBasis->Append(_("UTC"));
  m_inputTimeBasis->Append(_("Computer local time"));
  m_inputTimeBasis->Append(_("Ship zone time"));
  m_inputTimeBasis->SetSelection(0);
  grid->Add(m_inputTimeBasis, 0, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Display event times as")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_displayTime = new wxChoice(this, wxID_ANY);
  m_displayTime->Append(_("UTC"));
  m_displayTime->Append(_("Computer local"));
  m_displayTime->Append(_("Local mean time"));
  m_displayTime->Append(_("Fixed offset"));
  m_displayTime->SetSelection(0);
  grid->Add(m_displayTime, 0, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Zone offset (h)")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_fixedOffset =
      new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(75, -1), wxSP_ARROW_KEYS, -12, 14, 0, 0.5);
  m_fixedOffset->SetDigits(1);
  grid->Add(m_fixedOffset, 0, wxEXPAND);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Date/time entry")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_entryFormat = new wxChoice(this, wxID_ANY);
  m_entryFormat->Append(_("Nautical: YYYY-MM-DD, 24-hour"));
  m_entryFormat->Append(_("OpenCPN / platform format"));
  m_entryFormat->SetSelection(0);
  m_entryFormat->SetToolTip(
      _("Nautical format avoids ambiguous dates and AM/PM."));
  grid->Add(m_entryFormat, 0, wxEXPAND);
  grid->AddSpacer(1);
  grid->AddSpacer(1);
  m_autoZoneOffset =
      new wxCheckBox(this, wxID_ANY, _("Auto zone from longitude"));
  m_autoZoneOffset->SetValue(true);
  grid->Add(m_autoZoneOffset, 0, wxALIGN_CENTER_VERTICAL);
  grid->AddSpacer(1);

  wxBoxSizer* motion = new wxBoxSizer(wxHORIZONTAL);
  m_moving = new wxCheckBox(this, wxID_ANY, _("Time-tagged moving observer"));
  m_moving->SetToolTip(_("The entered position is at the reference time above. Course and speed propagate it to each planning instant."));
  motion->Add(m_moving, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
  motion->Add(new wxStaticText(this, wxID_ANY, _("COG (true)")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_course =
      new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(100, -1), wxSP_ARROW_KEYS, 0, 359.9, 0, 0.1);
  m_course->SetDigits(1);
  motion->Add(m_course, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
  motion->Add(new wxStaticText(this, wxID_ANY, _("SOG (kn)")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_speed =
      new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(100, -1), wxSP_ARROW_KEYS, 0, 100, 0, 0.1);
  m_speed->SetDigits(1);
  motion->Add(m_speed, 0, wxALIGN_CENTER_VERTICAL);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Eye height (m)")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_eyeHeight = new wxSpinCtrlDouble(
      this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
      wxSP_ARROW_KEYS, 0, 100, defaults.eyeHeight, 0.1);
  m_eyeHeight->SetDigits(1);
  grid->Add(m_eyeHeight);
  wxButton* calculate = new wxButton(this, wxID_ANY, _("Calculate / refresh"));
  grid->Add(calculate, 0, wxEXPAND);
  grid->AddSpacer(1);
  grid->AddSpacer(1);
  grid->AddSpacer(1);
  context->Add(grid, 1, wxALL | wxEXPAND, 6);
  m_resolvedUtc = new wxStaticText(
      this, wxID_ANY, _("Resolved UTC: waiting for a valid date and time"));
  wxFont resolvedFont = m_resolvedUtc->GetFont();
  resolvedFont.SetWeight(wxFONTWEIGHT_BOLD);
  m_resolvedUtc->SetFont(resolvedFont);
  m_resolvedUtc->SetToolTip(
      _("This is the single UTC instant used by Events, Bodies, Almanac and "
        "Noon/Polaris calculations."));
  context->Add(m_resolvedUtc, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
  context->Add(motion, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
  m_status = new wxStaticText(this, wxID_ANY,
                              _("All calculations use bundled offline data."));
  context->Add(m_status, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
  root->Add(context, 0, wxALL | wxEXPAND, 6);

  m_notebook = new wxNotebook(this, wxID_ANY);

  wxPanel* eventsPage = new wxPanel(m_notebook);
  wxBoxSizer* eventsSizer = new wxBoxSizer(wxVERTICAL);
  m_events = new wxListCtrl(eventsPage, wxID_ANY, wxDefaultPosition,
                            wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
  AddColumn(m_events, 0, _("Event"), 170);
  AddColumn(m_events, 1, _("UTC"), 190);
  AddColumn(m_events, 2, _("Selected display time"), 235);
  AddColumn(m_events, 3, _("Bearing true"), 105);
  AddColumn(m_events, 4, _("Observer position"), 300);
  eventsSizer->Add(m_events, 1, wxALL | wxEXPAND, 5);
  m_moonSummary = new wxStaticText(eventsPage, wxID_ANY, wxEmptyString);
  eventsSizer->Add(m_moonSummary, 0, wxALL | wxEXPAND, 6);
  eventsPage->SetSizer(eventsSizer);
  m_notebook->AddPage(eventsPage, _("Events"), true);

  wxScrolledWindow* bodiesPage = new wxScrolledWindow(m_notebook);
  bodiesPage->SetScrollRate(10, 10);
  wxBoxSizer* bodiesRoot = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer* bodiesLeft = new wxBoxSizer(wxVERTICAL);
  bodiesLeft->SetMinSize(wxSize(380, 390));
  wxBoxSizer* modeControls = new wxBoxSizer(wxHORIZONTAL);
  modeControls->Add(new wxStaticText(bodiesPage, wxID_ANY, _("Planning mode")),
                    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  m_planningMode = new wxChoice(bodiesPage, wxID_ANY);
  for (const wxString& name : {_("Practical Fix"), _("Bright Bodies"),
                               _("Lunar Candidates"), _("Show All")})
    m_planningMode->Append(name);
  m_planningMode->SetSelection(0);
  modeControls->Add(m_planningMode, 0, wxRIGHT, 14);
  m_tableBelowHorizon =
      new wxCheckBox(bodiesPage, wxID_ANY, _("Include below-horizon bodies"));
  modeControls->Add(m_tableBelowHorizon, 0, wxALIGN_CENTER_VERTICAL);
  bodiesLeft->Add(modeControls, 0, wxALL | wxEXPAND, 6);
  wxBoxSizer* recommendationControls = new wxBoxSizer(wxHORIZONTAL);
  m_limitRecommendationAltitude =
      new wxCheckBox(bodiesPage, wxID_ANY, _("Limit recommendations by Hc"));
  m_limitRecommendationAltitude->SetValue(true);
  recommendationControls->Add(m_limitRecommendationAltitude, 0,
                              wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
  recommendationControls->Add(
      new wxStaticText(bodiesPage, wxID_ANY, _("Minimum")), 0,
      wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  m_recommendationMinAltitude = new wxSpinCtrlDouble(
      bodiesPage, wxID_ANY, "10", wxDefaultPosition, wxSize(80, -1),
      wxSP_ARROW_KEYS, 0.0, 90.0, 10.0, 1.0);
  m_recommendationMinAltitude->SetDigits(1);
  recommendationControls->Add(m_recommendationMinAltitude, 0,
                              wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
  recommendationControls->Add(
      new wxStaticText(bodiesPage, wxID_ANY, _("Maximum")), 0,
      wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  m_recommendationMaxAltitude = new wxSpinCtrlDouble(
      bodiesPage, wxID_ANY, "75", wxDefaultPosition, wxSize(80, -1),
      wxSP_ARROW_KEYS, 0.0, 90.0, 75.0, 1.0);
  m_recommendationMaxAltitude->SetDigits(1);
  recommendationControls->Add(m_recommendationMaxAltitude, 0,
                              wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  recommendationControls->Add(
      new wxStaticText(bodiesPage, wxID_ANY, CN_UTF8_("°")), 0,
      wxALIGN_CENTER_VERTICAL);
  bodiesLeft->Add(recommendationControls, 0,
                  wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 6);
  wxStaticText* recommendationNote = new wxStaticText(
      bodiesPage, wxID_ANY,
      _("All catalogued bodies above the horizon remain listed; these limits "
        "affect recommended pairs and triads only."));
  recommendationNote->Wrap(650);
  bodiesLeft->Add(recommendationNote, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
                  6);
  m_resultsNotebook = new wxNotebook(bodiesPage, wxID_ANY, wxDefaultPosition,
                                      wxSize(560, 320));
  m_resultsNotebook->SetMinSize(wxSize(400, 290));
  wxPanel* allBodiesPage = new wxPanel(m_resultsNotebook);
  wxBoxSizer* allBodiesSizer = new wxBoxSizer(wxVERTICAL);
  m_bodies =
      new wxListCtrl(allBodiesPage, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                     wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
  AddColumn(m_bodies, 0, _("Body"), 115);
  AddColumn(m_bodies, 1, _("Hc"), 115);
  AddColumn(m_bodies, 2, _("Zn true"), 80);
  AddColumn(m_bodies, 3, _("GHA"), 115);
  AddColumn(m_bodies, 4, _("Dec"), 125);
  AddColumn(m_bodies, 5, _("Mag"), 55);
  AddColumn(m_bodies, 6, _("Observability"), 110);
  AddColumn(m_bodies, 7, _("Ecliptic lat"), 100);
  AddColumn(m_bodies, 8, _("Why"), 290);
  m_bodies->SetMinSize(wxSize(400, 180));
  allBodiesSizer->Add(m_bodies, 1, wxALL | wxEXPAND, 5);
  wxButton* createSight =
      new wxButton(allBodiesPage, wxID_ANY, _("Create selected sight..."));
  wxButton* exportBodies =
      new wxButton(allBodiesPage, wxID_ANY,
                   _("Export planning table CSV..."));
  wxBoxSizer* bodyActions = new wxBoxSizer(wxHORIZONTAL);
  bodyActions->Add(createSight, 0, wxRIGHT, 8);
  bodyActions->Add(exportBodies, 0);
  allBodiesSizer->Add(bodyActions, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  auto* windows = new wxButton(allBodiesPage, wxID_ANY,
                               _("Lunar windows for selected body..."));
  windows->Bind(wxEVT_BUTTON, &PlannerDialog::FindLunarWindows, this);
  allBodiesSizer->Add(windows, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  allBodiesPage->SetSizer(allBodiesSizer);
  m_resultsNotebook->AddPage(allBodiesPage, _("All bodies"), true);
  wxPanel* recommendationsPage = new wxPanel(m_resultsNotebook);
  wxBoxSizer* recommendationSizer = new wxBoxSizer(wxVERTICAL);
  m_lunarOrder = new wxChoice(recommendationsPage, wxID_ANY);
  m_lunarOrder->Append(_("Lunar order: fewest cautions, then timing"));
  m_lunarOrder->Append(_("Lunar order: timing sensitivity"));
  m_lunarOrder->SetSelection(0);
  m_lunarOrder->SetToolTip(_("Both lunar planners use the same ordering. Below-horizon pairs come last. Ecliptic latitude is shown for context, not used as a second timing score."));
  recommendationSizer->Add(m_lunarOrder, 0, wxALL, 5);
  m_lunarOrder->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
    m_bodySortColumn = -1;
    RefreshBodies();
  });
  m_combinations = new wxListCtrl(recommendationsPage, wxID_ANY,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxLC_REPORT | wxLC_HRULES);
  AddColumn(m_combinations, 0, _("Recommended pair / triad"), 300);
  AddColumn(m_combinations, 1, _("Score"), 70);
  AddColumn(m_combinations, 2, _("Geometry"), 270);
  m_combinations->SetMinSize(wxSize(400, 180));
  recommendationSizer->Add(m_combinations, 1, wxALL | wxEXPAND, 5);
  m_lunarPairs = new wxListCtrl(recommendationsPage, wxID_ANY,
                                wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_HRULES);
  AddColumn(m_lunarPairs, 0, _("Moon + body"), 155);
  AddColumn(m_lunarPairs, 1, _("LD"), 85);
  AddColumn(m_lunarPairs, 2, _("Rate '/h"), 90);
  AddColumn(m_lunarPairs, 3, _("0.1' time"), 95);
  AddColumn(m_lunarPairs, 4, _("Ecliptic lat"), 95);
  AddColumn(m_lunarPairs, 5, _("Cautions"), 80);
  AddColumn(m_lunarPairs, 6, _("Moon/body Hc, Zn; guidance"), 440);
  m_lunarPairs->SetMinSize(wxSize(400, 180));
  recommendationSizer->Add(m_lunarPairs, 1, wxALL | wxEXPAND, 5);
  m_lunarPairs->Hide();
  m_noRecommendations = new wxStaticText(
      recommendationsPage, wxID_ANY,
      _("Show All lists every calculated body without recommendation ranking."));
  recommendationSizer->Add(m_noRecommendations, 0, wxALL, 8);
  m_noRecommendations->Hide();
  recommendationsPage->SetSizer(recommendationSizer);
  m_resultsNotebook->AddPage(recommendationsPage, _("Recommendations"), false);
  bodiesLeft->Add(m_resultsNotebook, 1, wxALL | wxEXPAND, 5);
  bodiesRoot->Add(bodiesLeft, 1, wxEXPAND);
  wxBoxSizer* plotSizer = new wxBoxSizer(wxVERTICAL);
  plotSizer->SetMinSize(wxSize(340, 390));
  wxBoxSizer* plotControls = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer* magnitudeControls = new wxBoxSizer(wxHORIZONTAL);
  magnitudeControls->Add(
      new wxStaticText(bodiesPage, wxID_ANY, _("Sky plot magnitude")), 0,
      wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  m_plotMagnitude = new wxChoice(bodiesPage, wxID_ANY);
  m_plotMagnitude->Append(_("1 or brighter"));
  m_plotMagnitude->Append(_("2 or brighter"));
  m_plotMagnitude->Append(_("3 or brighter"));
  m_plotMagnitude->SetSelection(2);
  magnitudeControls->Add(m_plotMagnitude, 0);
  plotControls->Add(magnitudeControls, 0, wxEXPAND | wxBOTTOM, 4);
  m_plotBelowHorizon =
      new wxCheckBox(bodiesPage, wxID_ANY, _("Show below horizon"));
  plotControls->Add(m_plotBelowHorizon, 0, wxTOP, 3);
  m_showEcliptic = new wxCheckBox(bodiesPage, wxID_ANY, _("Show ecliptic"));
  m_showEcliptic->SetValue(true);
  m_showMoonPath = new wxCheckBox(bodiesPage, wxID_ANY, _("Show Moon path"));
  wxBoxSizer* overlayRow = new wxBoxSizer(wxHORIZONTAL);
  overlayRow->Add(m_showEcliptic, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 9);
  overlayRow->Add(m_showMoonPath, 0, wxALIGN_CENTER_VERTICAL);
  plotControls->Add(overlayRow, 0, wxTOP, 4);
  wxBoxSizer* spanRow = new wxBoxSizer(wxHORIZONTAL);
  spanRow->Add(new wxStaticText(bodiesPage, wxID_ANY,
                                CN_UTF8_("Moon path ±")),
               0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_moonSpan = new wxChoice(bodiesPage, wxID_ANY);
  m_moonSpan->Append(_("3 hours"));
  m_moonSpan->Append(_("6 hours"));
  m_moonSpan->Append(_("12 hours"));
  m_moonSpan->SetSelection(0);
  spanRow->Add(m_moonSpan);
  plotControls->Add(spanRow, 0, wxTOP, 3);
  plotSizer->Add(plotControls, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
  m_plotNotebook = new wxNotebook(bodiesPage, wxID_ANY, wxDefaultPosition,
                                   wxSize(340, 350));
  m_plotNotebook->SetMinSize(wxSize(320, 260));
  m_skyPlot = new SkyPlotPanel(m_plotNotebook);
  m_equatorialPlot = new SkyPlotPanel(m_plotNotebook, true);
  m_plotNotebook->AddPage(m_skyPlot, _("Local sky"), true);
  m_plotNotebook->AddPage(m_equatorialPlot, _("SHA / Declination"), false);
  plotSizer->Add(m_plotNotebook, 1, wxALL | wxEXPAND, 8);
  wxStaticText* plotLegend = new wxStaticText(
      bodiesPage, wxID_ANY,
      _("Ecliptic amber; Moon track blue (ends and selected time marked). "
        "Sun yellow; planets red; hollow = below horizon."));
  plotLegend->Wrap(340);
  plotSizer->Add(plotLegend, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
  bodiesRoot->Add(plotSizer, 0, wxEXPAND);
  bodiesPage->SetSizer(bodiesRoot);
  bodiesPage->FitInside();
  bodiesPage->Layout();
  m_notebook->AddPage(bodiesPage, _("Bodies && Best Sights"), false);

  wxPanel* almanacPage = new wxPanel(m_notebook);
  wxBoxSizer* almanacSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticText* almanacNote =
      new wxStaticText(almanacPage, wxID_ANY,
                       _("Hourly Sun, Moon, planet and Polaris almanac (24 "
                         "hours from the selected UTC)."));
  almanacSizer->Add(almanacNote, 0, wxALL, 6);
  m_almanac = new wxListCtrl(almanacPage, wxID_ANY, wxDefaultPosition,
                             wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
  AddColumn(m_almanac, 0, _("UTC"), 155);
  AddColumn(m_almanac, 1, _("Body"), 90);
  AddColumn(m_almanac, 2, _("GHA"), 120);
  AddColumn(m_almanac, 3, _("SHA"), 120);
  AddColumn(m_almanac, 4, _("GHA Aries"), 120);
  AddColumn(m_almanac, 5, _("LHA Aries"), 120);
  AddColumn(m_almanac, 6, _("Declination"), 130);
  AddColumn(m_almanac, 7, _("Hc"), 120);
  AddColumn(m_almanac, 8, _("Zn true"), 90);
  almanacSizer->Add(m_almanac, 1, wxALL | wxEXPAND, 5);
  wxButton* exportButton =
      new wxButton(almanacPage, wxID_ANY, _("Export CSV..."));
  almanacSizer->Add(exportButton, 0, wxALL, 5);
  almanacPage->SetSizer(almanacSizer);
  m_notebook->AddPage(almanacPage, _("Almanac"), false);

  wxPanel* specialPage = new wxPanel(m_notebook);
  wxBoxSizer* specialSizer = new wxBoxSizer(wxVERTICAL);
  specialSizer->Add(
      new wxStaticText(specialPage, wxID_ANY,
                       _("Noon and Polaris helpers use the same ephemeris and "
                         "the planning position/time above.")),
      0, wxALL, 8);
  wxFlexGridSizer* specialGrid = new wxFlexGridSizer(0, 2, 6, 8);
  specialGrid->Add(new wxStaticText(specialPage, wxID_ANY, _("Workflow")), 0,
                   wxALIGN_CENTER_VERTICAL);
  m_specialBody = new wxChoice(specialPage, wxID_ANY);
  m_specialBody->Append(_("Sun at local apparent noon"));
  m_specialBody->Append(_("Polaris latitude"));
  m_specialBody->SetSelection(0);
  specialGrid->Add(m_specialBody);
  specialGrid->Add(new wxStaticText(specialPage, wxID_ANY,
                                    _("Corrected observed altitude Ho")),
                   0, wxALIGN_CENTER_VERTICAL);
  m_specialAltitude = new NavigationAngleCtrl(
      specialPage, NavigationAngleKind::Generic, 45.0, -10.0, 90.0);
  specialGrid->Add(m_specialAltitude);
  wxButton* solve = new wxButton(specialPage, wxID_ANY, _("Solve latitude"));
  specialGrid->Add(solve);
  specialSizer->Add(specialGrid, 0, wxALL, 8);
  m_specialSummary = new wxStaticText(specialPage, wxID_ANY, wxEmptyString);
  specialSizer->Add(m_specialSummary, 0, wxALL | wxEXPAND, 8);
  specialPage->SetSizer(specialSizer);
  m_notebook->AddPage(specialPage, _("Noon && Polaris"), false);

  root->Add(m_notebook, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 6);
  wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
  buttons->AddButton(new wxButton(this, wxID_CLOSE));
  buttons->Realize();
  root->Add(buttons, 0, wxALL | wxEXPAND, 6);
  SetSizer(root);

  const int wideContextWidth = std::max(1120, grid->CalcMin().x + 24);
  Bind(wxEVT_SIZE, [this, grid, bodiesRoot, bodiesPage, wideContextWidth](wxSizeEvent& event) {
    event.Skip();
    if (m_reflowingLayout) return;
    const bool compact = event.GetSize().x < wideContextWidth;
    if (compact == m_compactLayout) return;
    m_reflowingLayout = true;
    grid->RemoveGrowableCol(m_compactLayout ? 3 : 5);
    grid->SetCols(compact ? 4 : 6);
    grid->AddGrowableCol(compact ? 3 : 5);
    bodiesRoot->SetOrientation(compact ? wxVERTICAL : wxHORIZONTAL);
    m_compactLayout = compact;
    Layout();
    bodiesPage->Layout();
    bodiesPage->FitInside();
    m_reflowingLayout = false;
  });

  // On GTK, notebook pages which were hidden while their list controls were
  // populated can retain the page's original full-size child allocation.
  // Relayout the newly selected page explicitly so its controls cannot cover
  // the recommendation row or the sky-plot pane.
  m_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED,
                   [this](wxBookCtrlEvent& event) {
                     event.Skip();
                     if (wxWindow* page = m_notebook->GetCurrentPage())
                       page->Layout();
                     m_notebook->Layout();
                     Layout();
                     if (event.GetSelection() == 1)
                       CallAfter([this] {
                         if (auto* scroll = dynamic_cast<wxScrolledWindow*>(
                                 m_notebook->GetPage(1)))
                           scroll->Scroll(0, 0);
                       });
                   });

  m_positionSource->Bind(wxEVT_CHOICE, &PlannerDialog::ChangePositionSource,
                         this);
  m_cursorTimer.SetOwner(this);
  Bind(wxEVT_TIMER, &PlannerDialog::OnCursorTimer, this, m_cursorTimer.GetId());
  m_cursorTimer.Start(500);
  m_timeSource->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
    ApplyTimeSource();
    ScheduleRefresh();
  });
  m_inputTimeBasis->Bind(wxEVT_CHOICE, &PlannerDialog::ChangeInputTimeBasis,
                         this);
  m_entryFormat->Bind(wxEVT_CHOICE, &PlannerDialog::ChangeEntryFormat, this);
  m_latitude->Bind(wxEVT_TEXT, &PlannerDialog::ContextPositionEdited, this);
  m_longitude->Bind(wxEVT_TEXT, &PlannerDialog::ContextPositionEdited, this);
  m_utcDate->Bind(wxEVT_DATE_CHANGED, [this](wxDateEvent&) {
    wxCommandEvent event;
    ContextTimeEdited(event);
  });
  m_utcTime->Bind(wxEVT_TIME_CHANGED, [this](wxDateEvent&) {
    wxCommandEvent event;
    ContextTimeEdited(event);
  });
  m_nauticalDate->Bind(wxEVT_TEXT, &PlannerDialog::ContextTimeEdited, this);
  m_nauticalTime->Bind(wxEVT_TEXT, &PlannerDialog::ContextTimeEdited, this);
  m_displayTime->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
    UpdateZoneOffsetControls();
    RefreshEvents();
  });
  m_fixedOffset->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) {
    if (m_updatingZoneOffset) return;
    m_lastValidZoneOffset = m_fixedOffset->GetValue();
    m_zoneOffsetTextValid = true;
    m_autoZoneOffset->SetValue(false);
    ScheduleRefresh();
  });
  m_fixedOffset->Bind(wxEVT_TEXT, [this](wxCommandEvent& event) {
    if (m_updatingZoneOffset) return;
    double value = 0.0;
    m_zoneOffsetTextValid =
        event.GetString().ToDouble(&value) && value >= -12.0 && value <= 14.0;
    if (m_zoneOffsetTextValid) {
      m_lastValidZoneOffset = value;
      m_autoZoneOffset->SetValue(false);
      ScheduleRefresh();
    }
  });
  m_fixedOffset->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
    if (!m_zoneOffsetTextValid) {
      m_updatingZoneOffset = true;
      m_fixedOffset->SetValue(m_lastValidZoneOffset);
      m_updatingZoneOffset = false;
      m_zoneOffsetTextValid = true;
    }
    event.Skip();
  });
  m_autoZoneOffset->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
    UpdateAutomaticZoneOffset();
    ScheduleRefresh();
  });
  m_moving->Bind(wxEVT_CHECKBOX,
                 [this](wxCommandEvent&) { ScheduleRefresh(); });
  m_course->Bind(wxEVT_SPINCTRLDOUBLE,
                 [this](wxSpinDoubleEvent&) { ScheduleRefresh(); });
  m_speed->Bind(wxEVT_SPINCTRLDOUBLE,
                [this](wxSpinDoubleEvent&) { ScheduleRefresh(); });
  m_eyeHeight->Bind(wxEVT_SPINCTRLDOUBLE,
                    [this](wxSpinDoubleEvent&) { ScheduleRefresh(); });
  m_limitRecommendationAltitude->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
    const bool enabled = m_limitRecommendationAltitude->GetValue();
    m_recommendationMinAltitude->Enable(enabled);
    m_recommendationMaxAltitude->Enable(enabled);
    ScheduleRefresh();
  });
  m_recommendationMinAltitude->Bind(
      wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { ScheduleRefresh(); });
  m_recommendationMaxAltitude->Bind(
      wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) { ScheduleRefresh(); });
  m_planningMode->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
    m_bodySortColumn = -1;
    m_resultsNotebook->SetSelection(m_planningMode->GetSelection() == 2 ? 1 : 0);
    RefreshBodies();
  });
  m_tableBelowHorizon->Bind(wxEVT_CHECKBOX,
                             [this](wxCommandEvent&) { RefreshBodies(); });
  m_bodies->Bind(wxEVT_LIST_COL_CLICK, &PlannerDialog::SortBodies, this);
  m_bodies->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
    const long index = m_bodies->GetItemData(event.GetIndex());
    if (index >= 0 && size_t(index) < m_rankedBodies.size()) {
      m_skyPlot->SetSelectedBody(m_rankedBodies[index].state.body);
      m_equatorialPlot->SetSelectedBody(m_rankedBodies[index].state.body);
    }
  });
  m_plotMagnitude->Bind(wxEVT_CHOICE,
                        [this](wxCommandEvent&) { RefreshSkyPlot(); });
  m_plotBelowHorizon->Bind(wxEVT_CHECKBOX,
                           [this](wxCommandEvent&) { RefreshSkyPlot(); });
  m_showEcliptic->Bind(wxEVT_CHECKBOX,
                       [this](wxCommandEvent&) { RefreshSkyPlot(); });
  m_showMoonPath->Bind(wxEVT_CHECKBOX,
                       [this](wxCommandEvent&) { RefreshSkyPlot(); });
  m_moonSpan->Bind(wxEVT_CHOICE,
                   [this](wxCommandEvent&) { RefreshSkyPlot(); });
  m_specialBody->Bind(wxEVT_CHOICE,
                      [this](wxCommandEvent&) { RefreshSpecial(); });
  m_refreshTimer.SetOwner(this);
  Bind(wxEVT_TIMER, &PlannerDialog::OnRefreshTimer, this,
       m_refreshTimer.GetId());
  calculate->Bind(wxEVT_BUTTON, &PlannerDialog::RefreshAll, this);
  exportButton->Bind(wxEVT_BUTTON, &PlannerDialog::ExportAlmanac, this);
  exportBodies->Bind(wxEVT_BUTTON, &PlannerDialog::ExportBodyTable, this);
  createSight->Bind(wxEVT_BUTTON, &PlannerDialog::CreateSelectedSight, this);
  solve->Bind(wxEVT_BUTTON, &PlannerDialog::SolveSpecialLatitude, this);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); }, wxID_CLOSE);

  wxFileConfig* config = GetOCPNConfigObject();
  config->SetPath(_T("/PlugIns/CelestialNavigation/Planner"));
  long positionSource = 1, inputTimeBasis = 0, displayTime = 0, entryFormat = 0;
  long planningMode = 0, moonSpan = 0;
  bool moving = false, autoZoneOffset = true,
       limitRecommendationAltitude = true, tableBelow = false,
       showEcliptic = true, showMoonPath = false;
  double latitude = 0.0, longitude = 0.0, course = 0.0, speed = 0.0,
         eyeHeight = defaults.eyeHeight, fixedOffset = 0.0,
         recommendationMinAltitude = 10.0, recommendationMaxAltitude = 75.0;
  config->Read(_T("PositionSource"), &positionSource, 1L);
  config->Read(_T("InputTimeBasis"), &inputTimeBasis, 0L);
  config->Read(_T("DisplayTime"), &displayTime, 0L);
  config->Read(_T("EntryFormat"), &entryFormat, 0L);
  config->Read(_T("Latitude"), &latitude, 0.0);
  config->Read(_T("Longitude"), &longitude, 0.0);
  config->Read(_T("Moving"), &moving, false);
  config->Read(_T("CourseTrue"), &course, 0.0);
  config->Read(_T("SpeedKnots"), &speed, 0.0);
  config->Read(_T("FixedOffset"), &fixedOffset, 0.0);
  config->Read(_T("AutoZoneOffset"), &autoZoneOffset, true);
  config->Read(_T("LimitRecommendationAltitude"), &limitRecommendationAltitude,
               true);
  config->Read(_T("RecommendationMinAltitude"), &recommendationMinAltitude,
               10.0);
  config->Read(_T("RecommendationMaxAltitude"), &recommendationMaxAltitude,
               75.0);
  config->Read(_T("PlanningMode"), &planningMode, 0L);
  config->Read(_T("IncludeBelowHorizonBodies"), &tableBelow, false);
  config->Read(_T("ShowEcliptic"), &showEcliptic, true);
  config->Read(_T("ShowMoonPath"), &showMoonPath, false);
  config->Read(_T("MoonPathSpan"), &moonSpan, 0L);
  config->Read(_T("WaypointGuid"), &m_waypointGuid, wxEmptyString);
  config->Read(_T("WaypointName"), &m_waypointName, wxEmptyString);
  m_positionSource->SetSelection(std::max(0L, std::min(positionSource, 5L)));
  m_lastPositionSource = m_positionSource->GetSelection();
  m_inputTimeBasis->SetSelection(std::max(0L, std::min(inputTimeBasis, 2L)));
  m_lastInputTimeBasis = m_inputTimeBasis->GetSelection();
  m_entryFormat->SetSelection(std::max(0L, std::min(entryFormat, 1L)));
  m_lastEntryFormat = m_entryFormat->GetSelection();
  UpdateEntryFormatControls();
  UpdateInputTimeLabels();
  m_displayTime->SetSelection(std::max(0L, std::min(displayTime, 3L)));
  m_latitude->SetAngle(latitude);
  m_longitude->SetAngle(longitude);
  m_moving->SetValue(moving);
  m_course->SetValue(course);
  m_speed->SetValue(speed);
  m_eyeHeight->SetValue(eyeHeight);
  m_fixedOffset->SetValue(fixedOffset);
  m_lastValidZoneOffset = fixedOffset;
  m_zoneOffsetTextValid = true;
  m_autoZoneOffset->SetValue(autoZoneOffset);
  recommendationMinAltitude =
      std::max(0.0, std::min(90.0, recommendationMinAltitude));
  recommendationMaxAltitude =
      std::max(0.0, std::min(90.0, recommendationMaxAltitude));
  if (recommendationMinAltitude >= recommendationMaxAltitude) {
    recommendationMinAltitude = 10.0;
    recommendationMaxAltitude = 75.0;
  }
  m_limitRecommendationAltitude->SetValue(limitRecommendationAltitude);
  m_recommendationMinAltitude->SetValue(recommendationMinAltitude);
  m_recommendationMaxAltitude->SetValue(recommendationMaxAltitude);
  m_recommendationMinAltitude->Enable(limitRecommendationAltitude);
  m_recommendationMaxAltitude->Enable(limitRecommendationAltitude);
  m_planningMode->SetSelection(std::max(0L, std::min(planningMode, 3L)));
  m_resultsNotebook->SetSelection(m_planningMode->GetSelection() == 2 ? 1 : 0);
  m_tableBelowHorizon->SetValue(tableBelow);
  m_showEcliptic->SetValue(showEcliptic);
  m_showMoonPath->SetValue(showMoonPath);
  m_moonSpan->SetSelection(std::max(0L, std::min(moonSpan, 2L)));
  UpdateAutomaticZoneOffset();
  UpdateZoneOffsetControls();
  SetUtcControls(wxDateTime::UNow());
  ApplyPositionSource();
  m_lastPositionSource = m_positionSource->GetSelection();
  ApplyTimeSource();
  wxCommandEvent dummy;
  RefreshAll(dummy);
  SetMinSize(wxSize(760, 560));
  dialog_geometry::Restore(this, _T("Planner"), wxSize(1370, 820));
  for (size_t page = 0; page < m_notebook->GetPageCount(); ++page)
    m_notebook->GetPage(page)->Layout();
  m_notebook->Layout();
  Layout();
}

PlannerDialog::~PlannerDialog() {
  dialog_geometry::Save(this, _T("Planner"));
  m_cursorTimer.Stop();
  m_refreshTimer.Stop();
  Unbind(wxEVT_TIMER, &PlannerDialog::OnCursorTimer, this,
         m_cursorTimer.GetId());
  Unbind(wxEVT_TIMER, &PlannerDialog::OnRefreshTimer, this,
         m_refreshTimer.GetId());
  wxFileConfig* config = GetOCPNConfigObject();
  config->SetPath(_T("/PlugIns/CelestialNavigation/Planner"));
  config->Write(_T("PositionSource"),
                static_cast<long>(m_positionSource->GetSelection()));
  config->Write(_T("InputTimeBasis"),
                static_cast<long>(m_inputTimeBasis->GetSelection()));
  config->Write(_T("DisplayTime"),
                static_cast<long>(m_displayTime->GetSelection()));
  config->Write(_T("EntryFormat"),
                static_cast<long>(m_entryFormat->GetSelection()));
  config->Write(_T("Latitude"), m_latitude->GetAngleOr(0.0));
  config->Write(_T("Longitude"), m_longitude->GetAngleOr(0.0));
  config->Write(_T("Moving"), m_moving->GetValue());
  config->Write(_T("CourseTrue"), m_course->GetValue());
  config->Write(_T("SpeedKnots"), m_speed->GetValue());
  config->Write(_T("FixedOffset"), ZoneOffsetHours());
  config->Write(_T("AutoZoneOffset"), m_autoZoneOffset->GetValue());
  config->Write(_T("LimitRecommendationAltitude"),
                m_limitRecommendationAltitude->GetValue());
  config->Write(_T("RecommendationMinAltitude"),
                m_recommendationMinAltitude->GetValue());
  config->Write(_T("RecommendationMaxAltitude"),
                m_recommendationMaxAltitude->GetValue());
  config->Write(_T("PlanningMode"),
                static_cast<long>(m_planningMode->GetSelection()));
  config->Write(_T("IncludeBelowHorizonBodies"),
                m_tableBelowHorizon->GetValue());
  config->Write(_T("ShowEcliptic"), m_showEcliptic->GetValue());
  config->Write(_T("ShowMoonPath"), m_showMoonPath->GetValue());
  config->Write(_T("MoonPathSpan"),
                static_cast<long>(m_moonSpan->GetSelection()));
  config->Write(_T("WaypointGuid"), m_waypointGuid);
  config->Write(_T("WaypointName"), m_waypointName);
}

wxDateTime PlannerDialog::ReadUtc(bool showErrors) {
  const wxDateTime entered =
      ReadEntryFields(m_entryFormat->GetSelection(), showErrors);
  if (!entered.IsValid()) return wxDateTime();
  const wxDateTime utc = PlannerFieldsToUtc(
      entered, static_cast<PlannerTimeBasis>(m_inputTimeBasis->GetSelection()),
      ZoneOffsetHours());
  if (utc.GetYear() < 1900 || utc.GetYear() > 2100) {
    if (showErrors)
      wxMessageBox(_("The ordinary offline planner is supported from 1900 "
                     "through 2100."),
                   _("Time outside supported range"), wxOK | wxICON_ERROR,
                   this);
    return wxDateTime();
  }
  return utc;
}

wxDateTime PlannerDialog::ReadEntryFields(int format, bool showErrors) {
  wxDateTime entered;
  if (format == 0) {
    if (!ParseNauticalPlannerDateTime(m_nauticalDate->GetValue(),
                                      m_nauticalTime->GetValue(), &entered)) {
      if (showErrors)
        wxMessageBox(
            _("Enter date as YYYY-MM-DD and 24-hour time as HH:MM:SS."),
            _("Invalid time"), wxOK | wxICON_ERROR, this);
      return wxDateTime();
    }
    return entered;
  }
  const wxDateTime date = m_utcDate->GetValue();
  const wxDateTime time = m_utcTime->GetValue();
  if (date.IsValid() && time.IsValid())
    entered = wxDateTime(date.GetDay(), date.GetMonth(), date.GetYear(),
                         time.GetHour(), time.GetMinute(), time.GetSecond());
  if (!entered.IsValid() && showErrors)
    wxMessageBox(_("Select a valid date and time."), _("Invalid time"),
                 wxOK | wxICON_ERROR, this);
  return entered;
}

void PlannerDialog::SetUtcControls(const wxDateTime& utc) {
  if (!utc.IsValid()) return;
  const wxDateTime value = UtcToPlannerFields(
      utc, static_cast<PlannerTimeBasis>(m_inputTimeBasis->GetSelection()),
      ZoneOffsetHours());
  m_utcDate->SetValue(value);
  m_utcTime->SetValue(value);
  m_nauticalDate->ChangeValue(FormatNauticalPlannerDate(value));
  m_nauticalTime->ChangeValue(FormatNauticalPlannerTime(value));
}

void PlannerDialog::ChangeInputTimeBasis(wxCommandEvent&) {
  const int next = m_inputTimeBasis->GetSelection();
  m_inputTimeBasis->SetSelection(m_lastInputTimeBasis);
  const wxDateTime utc = ReadUtc(false);
  m_inputTimeBasis->SetSelection(next);
  if (next == static_cast<int>(PlannerTimeBasis::ZoneTime))
    UpdateAutomaticZoneOffset();
  m_lastInputTimeBasis = next;
  UpdateInputTimeLabels();
  UpdateZoneOffsetControls();
  SetUtcControls(utc);
  wxCommandEvent refresh;
  RefreshAll(refresh);
}

void PlannerDialog::ChangeEntryFormat(wxCommandEvent&) {
  const wxDateTime fields = ReadEntryFields(m_lastEntryFormat, false);
  m_lastEntryFormat = m_entryFormat->GetSelection();
  UpdateEntryFormatControls();
  if (fields.IsValid()) {
    m_utcDate->SetValue(fields);
    m_utcTime->SetValue(fields);
    m_nauticalDate->ChangeValue(FormatNauticalPlannerDate(fields));
    m_nauticalTime->ChangeValue(FormatNauticalPlannerTime(fields));
  }
}

void PlannerDialog::UpdateInputTimeLabels() {
  const int basis = m_inputTimeBasis->GetSelection();
  const wxString suffix =
      basis == static_cast<int>(PlannerTimeBasis::ComputerLocal) ? _("local")
      : basis == static_cast<int>(PlannerTimeBasis::ZoneTime)    ? _("zone")
                                                                 : _("UTC");
  m_dateLabel->SetLabel(wxString::Format(_("Date (%s)"), suffix.c_str()));
  m_timeLabel->SetLabel(wxString::Format(_("Time (%s)"), suffix.c_str()));
}

void PlannerDialog::UpdateEntryFormatControls() {
  const bool nautical = m_entryFormat->GetSelection() == 0;
  m_nauticalDate->Show(nautical);
  m_nauticalTime->Show(nautical);
  m_utcDate->Show(!nautical);
  m_utcTime->Show(!nautical);
  m_dateContainer->Layout();
  m_timeContainer->Layout();
  Layout();
}

void PlannerDialog::UpdateAutomaticZoneOffset() {
  if (!m_autoZoneOffset->GetValue()) return;
  double longitude = 0.0;
  if (m_longitude->GetAngle(&longitude)) {
    m_lastValidZoneOffset = SuggestedZoneOffsetHours(longitude);
    m_updatingZoneOffset = true;
    m_fixedOffset->SetValue(m_lastValidZoneOffset);
    m_updatingZoneOffset = false;
    m_zoneOffsetTextValid = true;
  }
}

void PlannerDialog::UpdateZoneOffsetControls() {
  const bool relevant = m_inputTimeBasis->GetSelection() ==
                            static_cast<int>(PlannerTimeBasis::ZoneTime) ||
                        m_displayTime->GetSelection() == 3;
  m_fixedOffset->Enable(relevant);
  m_autoZoneOffset->Enable(relevant);
  const wxString explanation =
      relevant ? _("Used for ship-zone entry or fixed-offset display.")
               : _("Not used by the selected input or display time basis.");
  m_fixedOffset->SetToolTip(explanation);
  m_autoZoneOffset->SetToolTip(explanation);
}

double PlannerDialog::ZoneOffsetHours() const { return m_lastValidZoneOffset; }

void PlannerDialog::UpdateResolvedUtc(const wxDateTime& utc) {
  if (!utc.IsValid()) {
    m_resolvedUtc->SetLabel(
        CN_UTF8_("Resolved UTC: invalid — calculations have not been updated"));
    return;
  }
  m_resolvedUtc->SetLabel(_("Resolved UTC: ") +
                          utc.Format("%Y-%m-%d %H:%M:%S UTC", wxDateTime::UTC));
}

void PlannerDialog::ContextPositionEdited(wxCommandEvent&) {
  m_positionSource->SetSelection(0);
  m_lastPositionSource = 0;
  ScheduleRefresh();
}

void PlannerDialog::ContextTimeEdited(wxCommandEvent&) {
  m_timeSource->SetSelection(2);
  ScheduleRefresh();
}

void PlannerDialog::ScheduleRefresh() { m_refreshTimer.StartOnce(350); }

void PlannerDialog::OnRefreshTimer(wxTimerEvent&) {
  UpdateAutomaticZoneOffset();
  const ObserverMotion motion = ReadMotion(false);
  if (!motion.referenceUtc.IsValid()) {
    UpdateResolvedUtc(wxDateTime());
    ClearCalculatedResults(
        _("Results cleared: enter a valid supported date, time and position."));
    return;
  }
  UpdateResolvedUtc(motion.referenceUtc);
  RefreshEvents();
  RefreshBodies();
  RefreshAlmanac();
  RefreshSpecial();
  m_status->SetLabel(
      _("Planning context updated; calculations remain fully offline."));
}

ObserverMotion PlannerDialog::ReadMotion(bool showErrors) {
  ObserverMotion motion;
  motion.referenceUtc = ReadUtc(showErrors);
  if (!m_latitude->GetAngle(&motion.latitude) ||
      !m_longitude->GetAngle(&motion.longitude)) {
    if (showErrors)
      wxMessageBox(
          _("Enter a valid latitude and longitude. Decimal degrees, degrees "
            "and minutes, and degrees/minutes/seconds are accepted."),
          _("Invalid position"), wxOK | wxICON_ERROR, this);
    motion.referenceUtc = wxDateTime();
    return motion;
  }
  motion.courseTrue = m_course->GetValue();
  motion.speedKnots = m_speed->GetValue();
  motion.moving = m_moving->GetValue();
  return motion;
}

void PlannerDialog::ApplyPositionSource() {
  double lat = m_latitude->GetAngleOr(0.0);
  double lon = m_longitude->GetAngleOr(0.0);
  bool available = true;
  const int source = m_positionSource->GetSelection();
  if (source == 1) {
    available = m_parent->GetPlugin()->GetBoatPosition(&lat, &lon);
    const BoatNavigationSnapshot boat =
        m_parent->GetPlugin()->GetBoatNavigationSnapshot();
    if (boat.valid) {
      m_course->SetValue(boat.cogTrue);
      m_speed->SetValue(boat.sogKnots);
    }
  } else if (source == 2) {
    available = m_parent->GetPlugin()->GetCursorPosition(&lat, &lon);
  } else if (source == 3) {
    const Sight* sight = m_parent->GetSelectedSight();
    available = sight != nullptr;
    if (sight) {
      lat = sight->m_DRLat;
      lon = sight->m_DRLon;
    }
  } else if (source == 4) {
    available = m_parent->GetLastFix(&lat, &lon);
  } else if (source == 5) {
    WaypointPosition waypoint;
    available = ResolveSelectedWaypoint(&waypoint);
    if (available) {
      lat = waypoint.latitude;
      lon = waypoint.longitude;
      m_waypointName = waypoint.name;
    }
  }
  if (available) {
    m_latitude->SetAngle(lat);
    m_longitude->SetAngle(lon);
    if (source == 5) {
      const wxString name =
          m_waypointName.empty() ? _("Unnamed waypoint") : m_waypointName;
      m_status->SetLabel(
          wxString::Format(_("Waypoint/place \"%s\" applied; calculations "
                             "remain fully offline."),
                           name.c_str()));
    } else {
      m_status->SetLabel(
          _("Position source applied; calculations remain fully offline."));
    }
  } else {
    m_positionSource->SetSelection(0);
    m_status->SetLabel(source == 5 ? _("The selected waypoint/place is "
                                       "unavailable; retained manual position.")
                                   : _("Requested position is unavailable; "
                                       "retained manual position."));
  }
}

void PlannerDialog::ChangePositionSource(wxCommandEvent&) {
  if (m_positionSource->GetSelection() == 5 && !ChooseWaypoint()) {
    m_positionSource->SetSelection(m_lastPositionSource);
    return;
  }
  ApplyPositionSourceAndRefresh();
}

void PlannerDialog::ApplyPositionSourceAndRefresh() {
  ApplyPositionSource();
  m_lastPositionSource = m_positionSource->GetSelection();
  wxCommandEvent refresh;
  RefreshAll(refresh);
}

bool PlannerDialog::UpdateCursorPosition() {
  if (m_positionSource->GetSelection() != 2) return false;
  double latitude = 0.0;
  double longitude = 0.0;
  if (!m_parent->GetPlugin()->GetCursorPosition(&latitude, &longitude))
    return false;
  if (std::fabs(m_latitude->GetAngleOr(latitude) - latitude) < 0.0001 &&
      std::fabs(m_longitude->GetAngleOr(longitude) - longitude) < 0.0001)
    return false;
  m_latitude->SetAngle(latitude);
  m_longitude->SetAngle(longitude);
  wxCommandEvent refresh;
  RefreshAll(refresh);
  m_status->SetLabel(
      _("Chart cursor position updated; calculations remain fully offline."));
  return true;
}

void PlannerDialog::OnCursorTimer(wxTimerEvent&) { UpdateCursorPosition(); }

#ifdef CELESTIAL_PLANNER_INTEGRATION_TEST
void PlannerDialog::ScheduleWaypointIntegration(const wxString& name) {
  m_waypointIntegrationName = name;
  m_waypointIntegrationAttempts = 0;
  m_waypointIntegrationTimer.SetOwner(this);
  Bind(wxEVT_TIMER, &PlannerDialog::OnWaypointIntegrationTimer, this,
       m_waypointIntegrationTimer.GetId());
  m_waypointIntegrationTimer.Start(500);
}

void PlannerDialog::OnWaypointIntegrationTimer(wxTimerEvent&) {
  ++m_waypointIntegrationAttempts;
  const bool passed = SelectWaypointForIntegration(m_waypointIntegrationName);
  if (!passed && m_waypointIntegrationAttempts < 10) return;
  m_waypointIntegrationTimer.Stop();
  Unbind(wxEVT_TIMER, &PlannerDialog::OnWaypointIntegrationTimer, this,
         m_waypointIntegrationTimer.GetId());
  wxLogMessage("Celestial waypoint integration result: %s",
               passed ? "PASS" : "FAIL");
}

bool PlannerDialog::SelectWaypointForIntegration(const wxString& name) {
  m_positionSource->SetSelection(0);
  m_latitude->SetAngle(0.0);
  m_longitude->SetAngle(0.0);
  ApplyPositionSourceAndRefresh();

  wxString previousSunset;
  for (long row = 0; row < m_events->GetItemCount(); ++row) {
    if (m_events->GetItemText(row) == _("Sunset"))
      previousSunset = m_events->GetItemText(row, 1);
  }

  const std::vector<WaypointPosition> waypoints = LoadWaypoints();
  const WaypointPosition* selected = NULL;
  for (size_t index = 0; index < waypoints.size(); ++index) {
    if (waypoints[index].name.Lower().Find(name.Lower()) != wxNOT_FOUND) {
      selected = &waypoints[index];
      break;
    }
  }
  if (!selected) {
    return false;
  }

  m_waypointGuid = selected->guid;
  m_waypointName = selected->name;
  m_positionSource->SetSelection(5);
  ApplyPositionSourceAndRefresh();

  wxString currentSunset;
  for (long row = 0; row < m_events->GetItemCount(); ++row) {
    if (m_events->GetItemText(row) == _("Sunset"))
      currentSunset = m_events->GetItemText(row, 1);
  }
  const bool coordinatesApplied =
      std::fabs(m_latitude->GetAngleOr(0.0) - selected->latitude) < 0.0001 &&
      std::fabs(m_longitude->GetAngleOr(0.0) - selected->longitude) < 0.0001;
  const bool eventsRefreshed = !previousSunset.empty() &&
                               !currentSunset.empty() &&
                               previousSunset != currentSunset;
  wxLogMessage(
      "Celestial waypoint integration: name=%s lat=%.5f lon=%.5f "
      "sunset_before=%s sunset_after=%s coordinates=%d refreshed=%d",
      selected->name, m_latitude->GetAngleOr(0.0), m_longitude->GetAngleOr(0.0),
      previousSunset, currentSunset, coordinatesApplied, eventsRefreshed);

  m_parent->GetPlugin()->SetCursorLatLon(10.0, 20.0);
  m_positionSource->SetSelection(2);
  UpdateCursorPosition();
  wxString cursorSunset;
  for (long row = 0; row < m_events->GetItemCount(); ++row) {
    if (m_events->GetItemText(row) == _("Sunset"))
      cursorSunset = m_events->GetItemText(row, 1);
  }
  const bool cursorApplied =
      std::fabs(m_latitude->GetAngleOr(0.0) - 10.0) < 0.0001 &&
      std::fabs(m_longitude->GetAngleOr(0.0) - 20.0) < 0.0001;
  const bool cursorEventsRefreshed = !currentSunset.empty() &&
                                     !cursorSunset.empty() &&
                                     currentSunset != cursorSunset;
  wxLogMessage(
      "Celestial chart cursor integration: lat=%.5f lon=%.5f "
      "sunset_before=%s sunset_after=%s coordinates=%d refreshed=%d",
      m_latitude->GetAngleOr(0.0), m_longitude->GetAngleOr(0.0), currentSunset,
      cursorSunset, cursorApplied, cursorEventsRefreshed);
  return coordinatesApplied && eventsRefreshed && cursorApplied &&
         cursorEventsRefreshed;
}
#endif

bool PlannerDialog::ChooseWaypoint() {
  const std::vector<WaypointPosition> waypoints = LoadWaypoints();
  if (waypoints.empty()) {
    wxMessageBox(_("No OpenCPN waypoints or marks are available."),
                 _("Select waypoint or place"), wxOK | wxICON_INFORMATION,
                 this);
    return false;
  }
  WaypointPickerDialog dialog(this, waypoints, m_waypointGuid);
  if (dialog.ShowModal() != wxID_OK) return false;
  const WaypointPosition* waypoint = dialog.GetSelectedWaypoint();
  if (!waypoint) return false;
  m_waypointGuid = waypoint->guid;
  m_waypointName = waypoint->name;
  return true;
}

std::vector<WaypointPosition> PlannerDialog::LoadWaypoints() const {
  return LoadOpenCpnWaypoints();
}

bool PlannerDialog::ResolveSelectedWaypoint(WaypointPosition* waypoint) const {
  if (!waypoint || m_waypointGuid.empty()) return false;
#ifndef UNIT_TESTS
  PlugIn_Waypoint current;
  if (!GetSingleWaypoint(m_waypointGuid, &current)) return false;
  WaypointPosition candidate;
  candidate.guid = m_waypointGuid;
  candidate.name = current.m_MarkName;
  candidate.latitude = current.m_lat;
  candidate.longitude = current.m_lon;
  if (!WaypointPositionSource::IsUsable(candidate)) return false;
  *waypoint = candidate;
  return true;
#endif
  return false;
}

void PlannerDialog::ApplyTimeSource() {
  if (m_timeSource->GetSelection() == 0)
    SetUtcControls(wxDateTime::UNow());
  else if (m_timeSource->GetSelection() == 1) {
    const Sight* sight = m_parent->GetSelectedSight();
    if (sight)
      SetUtcControls(UtcDateTime::ToInstant(sight->m_DateTime));
    else {
      m_timeSource->SetSelection(2);
      m_status->SetLabel(_("No sight is selected; retained manual UTC."));
    }
  }
}

wxString PlannerDialog::DisplayTime(const wxDateTime& utc) const {
  if (m_displayTime->GetSelection() == 1)
    return utc.Format("%Y-%m-%d %H:%M:%S %Z", wxDateTime::Local);
  long offset = 0;
  wxString suffix = "UTC";
  if (m_displayTime->GetSelection() == 2) {
    offset =
        static_cast<long>(std::lround(m_longitude->GetAngleOr(0.0) * 240.0));
    suffix = "LMT";
  } else if (m_displayTime->GetSelection() == 3) {
    offset = static_cast<long>(std::lround(ZoneOffsetHours() * 3600.0));
    suffix = wxString::Format("UTC%+.1f", ZoneOffsetHours());
  }
  return (utc + wxTimeSpan::Seconds(offset))
             .Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC) +
         " " + suffix;
}

void PlannerDialog::RefreshAll(wxCommandEvent&) {
  ApplyPositionSource();
  UpdateAutomaticZoneOffset();
  ApplyTimeSource();
  const ObserverMotion motion = ReadMotion(true);
  if (!motion.referenceUtc.IsValid()) {
    UpdateResolvedUtc(wxDateTime());
    ClearCalculatedResults(
        _("Results cleared: enter a valid supported date, time and position."));
    return;
  }
  UpdateResolvedUtc(motion.referenceUtc);
  m_latitude->Normalize();
  m_longitude->Normalize();
  RefreshEvents();
  RefreshBodies();
  RefreshAlmanac();
  RefreshSpecial();
}

void PlannerDialog::ClearCalculatedResults(const wxString& status) {
  m_events->DeleteAllItems();
  m_moonSummary->SetLabel(wxEmptyString);
  m_rankedBodies.clear();
  m_bodies->DeleteAllItems();
  m_combinations->DeleteAllItems();
  m_lunarPairs->DeleteAllItems();
  m_skyPlot->SetBodies({}, false);
  m_equatorialPlot->SetBodies({}, false);
  m_almanacRows.clear();
  m_almanac->DeleteAllItems();
  m_specialSummary->SetLabel(wxEmptyString);
  m_status->SetLabel(status);
}

void PlannerDialog::RefreshEvents() {
  m_events->DeleteAllItems();
  const ObserverMotion motion = ReadMotion(false);
  const DailyEventsResult table = HorizonEventCalculator::Calculate(
      motion.referenceUtc, motion, m_eyeHeight->GetValue());
  for (const auto& event : table.events) {
    const long row = m_events->InsertItem(
        m_events->GetItemCount(), HorizonEventCalculator::Name(event.kind));
    m_events->SetItem(row, 1,
                      event.utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC));
    m_events->SetItem(row, 2, DisplayTime(event.utc));
    m_events->SetItem(row, 3,
                      wxString::Format("%.1f%c", event.bearingTrue, 0x00b0));
    m_events->SetItem(
        row, 4,
        FormatNavigationAngle(event.observerLatitude,
                              NavigationAngleKind::Latitude, true) +
            ", " +
            FormatNavigationAngle(event.observerLongitude,
                                  NavigationAngleKind::Longitude, true));
  }
  for (const auto& phase : NextPrincipalMoonPhases(
           motion.referenceUtc, motion.latitude, motion.longitude)) {
    const long row =
        m_events->InsertItem(m_events->GetItemCount(), _("Next ") + phase.name);
    m_events->SetItem(row, 1,
                      phase.utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC));
    m_events->SetItem(row, 2, DisplayTime(phase.utc));
    m_events->SetItem(row, 3, CN_UTF8_("—"));
    m_events->SetItem(row, 4, _("Geocentric phase"));
  }
  const MoonInformation moon = CalculateMoonInformation(
      motion.referenceUtc, motion.latitude, motion.longitude);
  const BodyState moonState = CelestialEphemeris::Evaluate(
      "Moon", motion.referenceUtc, motion.latitude, motion.longitude);
  wxString polar;
  if (table.sunAlwaysAbove) polar += _(" Sun above the horizon all day.");
  if (table.sunAlwaysBelow) polar += _(" Sun below the horizon all day.");
  if (table.moonAlwaysAbove) polar += _(" Moon above the horizon all day.");
  if (table.moonAlwaysBelow) polar += _(" Moon below the horizon all day.");
  const wxString direction = moon.waxing ? _("waxing") : _("waning");
  m_moonSummary->SetLabel(wxString::Format(
      _("Moon: %s, %.1f%% illuminated, %s, age %.1f days; altitude %s, azimuth "
        "%.1f%c, Sun separation %s.%s"),
      moon.phaseName.c_str(), moon.illuminatedFraction * 100.0,
      direction.c_str(), moon.ageDays,
      FormatNavigationAngle(moonState.geometricAltitude).c_str(),
      moonState.azimuthTrue, 0x00b0,
      FormatNavigationAngle(moon.elongationDegrees).c_str(), polar.c_str()));
}

void PlannerDialog::RefreshBodies() {
  m_combinations->DeleteAllItems();
  m_lunarPairs->DeleteAllItems();
  const ObserverMotion motion = ReadMotion(false);
  m_planningResult = PlannerRecommendations::Calculate(
      motion.referenceUtc, motion.latitude, motion.longitude);
  const PlanningMode mode =
      static_cast<PlanningMode>(m_planningMode->GetSelection());
  m_rankedBodies = PlannerRecommendations::Order(
      m_planningResult, mode, m_tableBelowHorizon->GetValue(),
      m_lunarOrder->GetSelection() == 1);
  const bool lunar = mode == PlanningMode::LunarCandidates;
  m_lunarPairs->Show(lunar);
  m_lunarOrder->Show(lunar);
  m_combinations->Show(!lunar && mode != PlanningMode::ShowAll);
  m_noRecommendations->Show(mode == PlanningMode::ShowAll);
  m_lunarPairs->GetParent()->Layout();
  wxListItem scoreColumn;
  scoreColumn.SetText(lunar ? _("Cautions") : _("Observability"));
  m_bodies->SetColumn(6, scoreColumn);
  RebuildBodyList();
  if (lunar) {
    const BodyState moon = CelestialEphemeris::Evaluate(
        "Moon", motion.referenceUtc, motion.latitude, motion.longitude);
    for (const auto& body : m_rankedBodies) {
      if (body.state.body == "Moon") continue;
      const long row = m_lunarPairs->InsertItem(
          m_lunarPairs->GetItemCount(), _("Moon + ") + body.state.body);
      m_lunarPairs->SetItem(row, 1,
                            wxString::Format("%.2f%c", body.lunarDistance,
                                             0x00b0));
      m_lunarPairs->SetItem(
          row, 2, wxString::Format("%+.1f", body.lunarRateArcminHour));
      m_lunarPairs->SetItem(
          row, 3, std::isfinite(body.lunarTimingSeconds)
                      ? wxString::Format("%.1f s", body.lunarTimingSeconds)
                      : CN_UTF8_("—"));
      m_lunarPairs->SetItem(row, 4,
                            wxString::Format("%+.1f%c", body.eclipticLatitude,
                                             0x00b0));
      m_lunarPairs->SetItem(row, 5,
                            wxString::Format("%d", body.lunarConstraints));
      m_lunarPairs->SetItem(
          row, 6,
          wxString::Format("Moon %.0f%c/%.0f%c; body %.0f%c/%.0f%c; "
                           "illum %.0f%%; %s",
                           moon.geometricAltitude, 0x00b0,
                           moon.azimuthTrue, 0x00b0,
                           body.state.geometricAltitude, 0x00b0,
                           body.state.azimuthTrue, 0x00b0,
                           m_planningResult.moon.illuminatedFraction * 100.0,
                           body.lunarReason.c_str()));
    }
  }
  const bool limitAltitude = m_limitRecommendationAltitude->GetValue();
  const double minimumAltitude = m_recommendationMinAltitude->GetValue();
  const double maximumAltitude = m_recommendationMaxAltitude->GetValue();
  const std::vector<RankedBody> candidates =
      SightRanker::RecommendationCandidates(m_rankedBodies, limitAltitude,
                                            minimumAltitude, maximumAltitude);
  if (limitAltitude && minimumAltitude >= maximumAltitude) {
    m_combinations->InsertItem(
        0, _("Set the minimum recommended Hc below the maximum."));
    RefreshSkyPlot();
    return;
  }
  if (mode == PlanningMode::PracticalFix ||
      mode == PlanningMode::BrightBodies) {
  std::vector<RankedBody> recommendationBodies = candidates;
  if (mode == PlanningMode::BrightBodies) {
    for (auto& body : recommendationBodies) {
      const double brightness = std::max(0.0, std::min(
          1.0, (3.0 - body.state.visualMagnitude) / 5.0));
      body.score = 75.0 * brightness + 0.25 * body.score;
    }
  }
  auto pairs = SightRanker::BestCombinations(recommendationBodies, 2, 5);
  auto triads = SightRanker::BestCombinations(recommendationBodies, 3, 5);
  pairs.insert(pairs.end(), triads.begin(), triads.end());
  for (const auto& combination : pairs) {
    wxString names;
    for (const auto& body : combination.bodies) {
      if (!names.empty()) names += " / ";
      names += body.state.body;
    }
    const long row =
        m_combinations->InsertItem(m_combinations->GetItemCount(), names);
    m_combinations->SetItem(row, 1,
                            wxString::Format("%.0f", combination.score));
    m_combinations->SetItem(row, 2, combination.reason);
  }
  }
  m_notebook->GetPage(1)->Layout();
  if (auto* scroll =
          dynamic_cast<wxScrolledWindow*>(m_notebook->GetPage(1)))
    { scroll->FitInside(); scroll->Scroll(0, 0); }
  RefreshSkyPlot();
}

void PlannerDialog::RebuildBodyList() {
  m_bodies->DeleteAllItems();
  std::vector<size_t> order;
  for (size_t index = 0; index < m_rankedBodies.size(); ++index)
    order.push_back(index);
  const int column = m_bodySortColumn;
  const bool ascending = m_bodySortAscending;
  if (column >= 0) std::sort(order.begin(), order.end(),
            [this, column, ascending](size_t left, size_t right) {
              const RankedBody& a = m_rankedBodies[left];
              const RankedBody& b = m_rankedBodies[right];
              int comparison = 0;
              if (column == 0)
                comparison = a.state.body.CmpNoCase(b.state.body);
              else if (column == 8)
                comparison = a.reason.CmpNoCase(b.reason);
              else {
                double av = 0.0, bv = 0.0;
                switch (column) {
                  case 1:
                    av = a.state.geometricAltitude;
                    bv = b.state.geometricAltitude;
                    break;
                  case 2:
                    av = a.state.azimuthTrue;
                    bv = b.state.azimuthTrue;
                    break;
                  case 3:
                    av = a.state.gha;
                    bv = b.state.gha;
                    break;
                  case 4:
                    av = a.state.declination;
                    bv = b.state.declination;
                    break;
                  case 5:
                    av = a.state.visualMagnitude;
                    bv = b.state.visualMagnitude;
                    break;
                  case 7:
                    av = a.eclipticLatitude;
                    bv = b.eclipticLatitude;
                    break;
                  default:
                    av = m_planningMode->GetSelection() == 2
                             ? a.lunarConstraints : a.score;
                    bv = m_planningMode->GetSelection() == 2
                             ? b.lunarConstraints : b.score;
                    break;
                }
                comparison = av < bv ? -1 : av > bv ? 1 : 0;
                if (comparison == 0)
                  comparison = a.state.body.CmpNoCase(b.state.body);
              }
              return ascending ? comparison < 0 : comparison > 0;
            });
  const ObserverMotion motion = ReadMotion(false);
  const BodyState sun = CelestialEphemeris::Evaluate(
      "Sun", motion.referenceUtc, motion.latitude, motion.longitude);
  const bool daylight = sun.valid && sun.geometricAltitude >= 0.0;
  for (const size_t index : order) {
    const RankedBody& body = m_rankedBodies[index];
    const long row =
        m_bodies->InsertItem(m_bodies->GetItemCount(), body.state.body);
    m_bodies->SetItemData(row, static_cast<long>(index));
    m_bodies->SetItem(row, 1,
                      FormatNavigationAngle(body.state.geometricAltitude));
    m_bodies->SetItem(
        row, 2, wxString::Format("%.1f%c", body.state.azimuthTrue, 0x00b0));
    m_bodies->SetItem(row, 3, FormatNavigationAngle(body.state.gha));
    m_bodies->SetItem(row, 4,
                      FormatNavigationAngle(body.state.declination,
                                            NavigationAngleKind::Latitude));
    m_bodies->SetItem(row, 5,
                      wxString::Format("%.1f", body.state.visualMagnitude));
    m_bodies->SetItem(row, 6,
                      wxString::Format("%.0f",
                          m_planningMode->GetSelection() == 2
                              ? double(body.lunarConstraints) : body.score));
    m_bodies->SetItem(row, 7,
                      wxString::Format("%+.1f%c", body.eclipticLatitude,
                                       0x00b0));
    wxString reason = m_planningMode->GetSelection() == 2 &&
                              body.state.body != "Moon"
                          ? body.lunarReason : body.reason;
    if (m_limitRecommendationAltitude->GetValue()) {
      if (body.state.geometricAltitude <
          m_recommendationMinAltitude->GetValue())
        reason += _("; below preferred Hc");
      else if (body.state.geometricAltitude >
               m_recommendationMaxAltitude->GetValue())
        reason += _("; above preferred Hc");
    }
    if (daylight && body.state.isStar) reason += _("; star in daylight");
    if (body.state.geometricAltitude < 0.0)
      reason += _("; below horizon");
    m_bodies->SetItem(row, 8, reason);
    if (daylight && body.state.isStar)
      m_bodies->SetItemTextColour(row, wxColour(150, 150, 150));
  }
}

void PlannerDialog::RefreshSkyPlot() {
  const ObserverMotion motion = ReadMotion(false);
  if (!motion.referenceUtc.IsValid()) return;
  const double magnitude =
      static_cast<double>(m_plotMagnitude->GetSelection() + 1);
  const double minimumAltitude = m_plotBelowHorizon->GetValue() ? -90.0 : 0.0;
  const std::vector<RankedBody> plotBodies = SightRanker::VisibleBodies(
      motion.referenceUtc, motion.latitude, motion.longitude, minimumAltitude,
      90.0, magnitude);
  const BodyState sun = CelestialEphemeris::Evaluate(
      "Sun", motion.referenceUtc, motion.latitude, motion.longitude);
  const bool daylight = sun.valid && sun.geometricAltitude >= 0.0;
  m_skyPlot->SetBodies(plotBodies, daylight);
  m_equatorialPlot->SetBodies(plotBodies, daylight);
  const auto ecliptic = m_showEcliptic->GetValue()
                            ? PlannerRecommendations::Ecliptic(
                                  motion.referenceUtc, motion.latitude,
                                  motion.longitude)
                            : std::vector<PlannerSkyPoint>{};
  const int spans[] = {3, 6, 12};
  const auto moonPath = m_showMoonPath->GetValue()
                            ? PlannerRecommendations::MoonPath(
                                  motion, spans[std::max(0, std::min(2,
                                      m_moonSpan->GetSelection()))])
                            : std::vector<PlannerSkyPoint>{};
  m_skyPlot->SetOverlays(ecliptic, moonPath, m_showEcliptic->GetValue(),
                         m_showMoonPath->GetValue());
  m_equatorialPlot->SetOverlays(ecliptic, moonPath,
                                m_showEcliptic->GetValue(),
                                m_showMoonPath->GetValue());
}

void PlannerDialog::SortBodies(wxListEvent& event) {
  if (m_bodySortColumn == event.GetColumn())
    m_bodySortAscending = !m_bodySortAscending;
  else {
    m_bodySortColumn = event.GetColumn();
    m_bodySortAscending = true;
  }
  RebuildBodyList();
}

void PlannerDialog::RefreshAlmanac() {
  m_almanac->DeleteAllItems();
  const ObserverMotion motion = ReadMotion(false);
  m_almanacRows = BuildAlmanac(
      motion.referenceUtc, 24,
      {"Sun", "Moon", "Venus", "Mars", "Jupiter", "Saturn", "Polaris"}, motion);
  for (const auto& item : m_almanacRows) {
    const long row = m_almanac->InsertItem(
        m_almanac->GetItemCount(),
        item.utc.Format("%Y-%m-%d %H:%M", wxDateTime::UTC));
    m_almanac->SetItem(row, 1, item.body);
    m_almanac->SetItem(row, 2, FormatNavigationAngle(item.gha));
    m_almanac->SetItem(row, 3, FormatNavigationAngle(item.sha));
    m_almanac->SetItem(row, 4, FormatNavigationAngle(item.ghaAries));
    m_almanac->SetItem(row, 5, FormatNavigationAngle(item.lhaAries));
    m_almanac->SetItem(
        row, 6,
        FormatNavigationAngle(item.declination, NavigationAngleKind::Latitude));
    m_almanac->SetItem(row, 7, FormatNavigationAngle(item.altitude));
    m_almanac->SetItem(row, 8, wxString::Format("%.1f", item.azimuth));
  }
}

void PlannerDialog::RefreshSpecial() {
  const ObserverMotion motion = ReadMotion(false);
  if (m_specialBody->GetSelection() == 1) {
    const BodyState polaris = CelestialEphemeris::Evaluate(
        "Polaris", motion.referenceUtc, motion.latitude, motion.longitude);
    if (!polaris.valid || polaris.geometricAltitude < 0.0) {
      m_specialSummary->SetLabel(
          _("Polaris is below the horizon and is not observable from the "
            "selected position and time."));
      return;
    }
    m_specialSummary->SetLabel(wxString::Format(
        _("Polaris is observable: predicted Hc %s, Zn true %.2f%c.\n"
          "Enter corrected Ho above to estimate latitude."),
        FormatNavigationAngle(polaris.geometricAltitude).c_str(),
        polaris.azimuthTrue, 0x00b0));
    return;
  }
  const DailyEventsResult events =
      HorizonEventCalculator::Calculate(motion.referenceUtc, motion);
  for (const auto& event : events.events) {
    if (event.kind == HorizonEventKind::UpperTransit) {
      m_specialSummary->SetLabel(wxString::Format(
          _("Local apparent noon (solar LHA 0%c): %s UTC at observer "
            "position %s, %s.\n"
            "Enter corrected Ho above to estimate latitude; longitude comes "
            "primarily from noon timing."),
          0x00b0,
          event.utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC).c_str(),
          FormatNavigationAngle(event.observerLatitude,
                                NavigationAngleKind::Latitude, true)
              .c_str(),
          FormatNavigationAngle(event.observerLongitude,
                                NavigationAngleKind::Longitude, true)
              .c_str()));
      break;
    }
  }
}

void PlannerDialog::ExportAlmanac(wxCommandEvent&) {
  wxFileDialog dialog(this, _("Export offline celestial almanac"),
                      wxEmptyString, "celestial-almanac.csv",
                      _("CSV files (*.csv)|*.csv"),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK) return;
  wxFFile file(dialog.GetPath(), "wb");
  if (!file.IsOpened() || !file.Write(AlmanacToCsv(m_almanacRows)))
    wxMessageBox(_("Could not write the selected file."), _("Export failed"),
                 wxOK | wxICON_ERROR, this);
}

void PlannerDialog::ExportBodyTable(wxCommandEvent&) {
  wxFileDialog dialog(this, _("Export sight planning table"), wxEmptyString,
                      "celestial-sight-planning.csv",
                      _("CSV files (*.csv)|*.csv"),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK) return;
  auto field = [](wxString value) {
    value.Replace("\"", "\"\"");
    return "\"" + value + "\"";
  };
  const ObserverMotion motion = ReadMotion(false);
  wxString csv = "UTC,Mode,Body,Hc deg,Zn true deg,GHA deg,Dec deg,"
                 "Magnitude,Observability,Lunar cautions,Ecliptic latitude "
                 "deg,LD deg,Signed LD rate arcmin/h,Seconds per 0.1 arcmin,"
                 "Moon illumination %,Explanation\n";
  for (long row = 0; row < m_bodies->GetItemCount(); ++row) {
    const long index = m_bodies->GetItemData(row);
    if (index < 0 || static_cast<size_t>(index) >= m_rankedBodies.size())
      continue;
    const RankedBody& body = m_rankedBodies[index];
    csv += field(motion.referenceUtc.Format("%Y-%m-%dT%H:%M:%SZ",
                                                 wxDateTime::UTC)) + "," +
           field(m_planningMode->GetStringSelection()) + "," +
           field(body.state.body) + "," +
           wxString::Format("%.5f,%.5f,%.5f,%.5f,%.2f,%.1f,%.1f,%.3f,"
                            "%.5f,%.3f,",
                            body.state.geometricAltitude, body.state.azimuthTrue,
                            body.state.gha, body.state.declination,
                            body.state.visualMagnitude, body.score,
                            double(body.lunarConstraints), body.eclipticLatitude,
                            body.lunarDistance, body.lunarRateArcminHour) +
           (std::isfinite(body.lunarTimingSeconds)
                ? wxString::Format("%.2f", body.lunarTimingSeconds)
                : wxString()) + "," +
           wxString::Format("%.1f", m_planningResult.moon.illuminatedFraction *
                                          100.0) +
           "," + field(m_planningMode->GetSelection() == 2
                            ? body.lunarReason : body.reason) + "\n";
  }
  wxFFile file(dialog.GetPath(), "wb");
  if (!file.IsOpened() || !file.Write(csv))
    wxMessageBox(_("Could not write the selected file."), _("Export failed"),
                 wxOK | wxICON_ERROR, this);
}

void PlannerDialog::FindLunarWindows(wxCommandEvent&) {
  const long selected = m_bodies->GetNextItem(
      -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  const long index = selected < 0 ? -1 : m_bodies->GetItemData(selected);
  if (index < 0 || size_t(index) >= m_rankedBodies.size() ||
      m_rankedBodies[index].state.body == "Moon") {
    wxMessageBox(_("Select a companion body in All bodies first."),
                 _("Lunar observing windows"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  const auto motion = ReadMotion(true);
  if (!motion.referenceUtc.IsValid()) return;
  const auto body = m_rankedBodies[index].state.body;
  const auto windows = [&]() {
    wxBusyCursor busy;
    return PlannerRecommendations::ObservingWindows(body, motion);
  }();
  wxString text = _("Next 24 hours from the planning instant, sampled every 10 minutes.\n"
      "Times are UTC; boundaries and best time are approximate sampled values.\n"
      "Both altitudes 10-75 degrees; LD 20-100 degrees; rate at least 10 arcmin/hour;\n"
      "companion magnitude <= 2.5; Sun <= -6 degrees for companions fainter than -2.\n"
      "These preferences do not establish visibility: check glare, weather and horizon.\n\n");
  text += motion.moving ? _("Uses entered course and speed.\n\n")
                        : _("Uses a stationary observer.\n\n");
  if (windows.empty()) text += _("No sampled interval meets all planning limits.\n"
      "Try another body or planning date; the full candidate table remains available.");
  for (const auto& window : windows) {
    text += wxString::Format(
        _("Moon + %s\n%s to %s UTC\nBest sampled timing: %s UTC\n"
          "LD %.2f deg; rate %+.1f arcmin/hour; 0.1' time %.1f s\n"
          "Moon Hc %.1f deg; body Hc %.1f deg\n\n"),
        body.c_str(), window.startUtc.Format("%Y-%m-%d %H:%M", wxDateTime::UTC),
        window.endUtc.Format("%Y-%m-%d %H:%M", wxDateTime::UTC),
        window.bestUtc.Format("%Y-%m-%d %H:%M", wxDateTime::UTC),
        window.best.lunarDistance, window.best.lunarRateArcminHour,
        window.best.lunarTimingSeconds, window.moonAltitude,
        window.best.state.geometricAltitude);
  }
  wxDialog dialog(this, wxID_ANY, _("Lunar observing windows"), wxDefaultPosition,
                  wxSize(760, 460), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
  auto* sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(new wxTextCtrl(&dialog, wxID_ANY, text, wxDefaultPosition,
      wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY), 1, wxALL | wxEXPAND, 8);
  sizer->Add(dialog.CreateButtonSizer(wxOK), 0, wxALL | wxALIGN_RIGHT, 8);
  dialog.SetSizer(sizer);
  dialog.ShowModal();
}

void PlannerDialog::CreateSelectedSight(wxCommandEvent&) {
  const long selected =
      m_bodies->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  const long bodyIndex = selected < 0 ? -1 : m_bodies->GetItemData(selected);
  if (bodyIndex < 0 ||
      static_cast<size_t>(bodyIndex) >= m_rankedBodies.size()) {
    wxMessageBox(_("Select a body first."), _("Sight Planner"),
                 wxOK | wxICON_INFORMATION, this);
    return;
  }
  m_parent->CreatePlannedSight(m_rankedBodies[bodyIndex].state.body,
                               ReadUtc(false), m_latitude->GetAngleOr(0.0),
                               m_longitude->GetAngleOr(0.0));
}

void PlannerDialog::SolveSpecialLatitude(wxCommandEvent&) {
  ObserverMotion motion = ReadMotion(false);
  wxString body = m_specialBody->GetSelection() == 0 ? "Sun" : "Polaris";
  wxDateTime time = motion.referenceUtc;
  if (body == "Sun") {
    const auto events = HorizonEventCalculator::Calculate(time, motion).events;
    for (const auto& event : events)
      if (event.kind == HorizonEventKind::UpperTransit) time = event.utc;
  } else {
    const BodyState planned = CelestialEphemeris::Evaluate(
        body, time, motion.latitude, motion.longitude);
    if (!planned.valid || planned.geometricAltitude < 0.0) {
      wxMessageBox(
          _("Polaris is below the horizon and is not observable from the "
            "selected position and time."),
          _("Polaris not observable"), wxOK | wxICON_INFORMATION, this);
      return;
    }
  }
  double observedAltitude = 0.0;
  if (!m_specialAltitude->GetAngle(&observedAltitude)) {
    wxMessageBox(_("Enter a valid corrected observed altitude."),
                 _("Invalid altitude"), wxOK | wxICON_ERROR, this);
    return;
  }
  m_specialAltitude->Normalize();
  const double latitude = SolveLatitudeFromAltitude(
      body, time, motion.longitude, observedAltitude, motion.latitude);
  if (!std::isfinite(latitude)) {
    m_specialSummary->SetLabel(_("No converged latitude solution. Check Ho, time, longitude and approximate latitude."));
    return;
  }
  const BodyState state =
      CelestialEphemeris::Evaluate(body, time, latitude, motion.longitude);
  m_specialSummary->SetLabel(wxString::Format(
      _("%s solution: latitude %s at %s UTC. Zn true (azimuth) %.2f%c.\n"
        "Treat this as a workflow aid: uncertainty still depends on Ho, time, "
        "horizon and DR errors."),
      body.c_str(),
      FormatNavigationAngle(latitude, NavigationAngleKind::Latitude, true)
          .c_str(),
      time.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC).c_str(),
      state.azimuthTrue, 0x00b0));
}
