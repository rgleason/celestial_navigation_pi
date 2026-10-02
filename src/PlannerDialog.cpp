#include "PlatformMessageBox.h"
#include "AndroidFileDialog.h"
#include "AndroidJob.h"
#ifdef __OCPN__ANDROID__
#include "AndroidPlannerCancellation.h"
#endif
#include "PlannerDialog.h"
#include "WaypointPickerDialog.h"

#include "CelestialNavigationDialog.h"
#include "DialogGeometry.h"
#include "NavigationUIUtils.h"
#include "Sight.h"
#include "SkyLabelLayout.h"
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
#include <wx/wrapsizer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#ifdef __OCPN__ANDROID__
#include "AndroidAlmanacCsv.h"
#include <QListWidget>
#include <QScrollBar>
#include <QVBoxLayout>
#endif

namespace {
#ifdef __OCPN__ANDROID__
// The retained Qt build needs explicit ScrollPrepare/Scroll geometry for a
// native list embedded beneath a wx scrolling viewport.
class PlannerListScroll : public QObject {
 public:
  explicit PlannerListScroll(QListWidget* list) : QObject(list), m_list(list) {
    list->viewport()->installEventFilter(this);
  }
 protected:
  bool eventFilter(QObject*, QEvent* event) override {
    auto* bar = m_list->verticalScrollBar();
    if (event->type() == QEvent::ScrollPrepare) {
      auto* prepare = static_cast<QScrollPrepareEvent*>(event);
      prepare->setViewportSize(m_list->viewport()->size());
      prepare->setContentPosRange(QRectF(0, bar->minimum(), 0,
                                        bar->maximum() - bar->minimum()));
      prepare->setContentPos(QPointF(0, bar->value()));
      prepare->accept();
      return true;
    }
    if (event->type() == QEvent::Scroll) {
      bar->setValue(qRound(static_cast<QScrollEvent*>(event)->contentPos().y()));
      event->accept();
      return true;
    }
    return false;
  }
 private:
  QListWidget* m_list;
};

// Height follows the actual viewport and Android font, including rotation.
class PlannerCardDelegate : public QStyledItemDelegate {
 public:
  explicit PlannerCardDelegate(QListWidget* list)
      : QStyledItemDelegate(list), m_list(list) {}
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override {
    const int width = qMax(100, m_list->viewport()->width() - 40);
    const QRect bounds = QFontMetrics(option.font).boundingRect(
        QRect(0, 0, width, 10000), Qt::TextWordWrap,
        index.data(Qt::DisplayRole).toString());
    return QSize(0, qMax(CN_TouchHeight(), bounds.height() + 40));
  }
 private:
  QListWidget* m_list;
};

QListWidget* PlannerCards(wxWindow* parent, wxSizer* root, bool selectable) {
  auto* panel = new wxPanel(parent);
  panel->SetMinSize(wxSize(0, CN_TouchHeight() * 2));
  auto* layout = new QVBoxLayout(panel->GetHandle());
  layout->setContentsMargins(0, 0, 0, 0);
  auto* list = new QListWidget(panel->GetHandle());
  list->setWordWrap(true);
  list->setTextElideMode(Qt::ElideNone);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  // The drag filter feeds the list scroller; no competing gesture grab.
  list->setSelectionMode(selectable ? QAbstractItemView::SingleSelection
                                    : QAbstractItemView::NoSelection);
  list->setItemDelegate(new PlannerCardDelegate(list));
  list->setStyleSheet(QString(
      "QListWidget { font-size: %1pt; background: #f5f8fa; color: #17313e; } "
      "QListWidget::item { padding: 12px; border-bottom: 1px solid #afbdc4; } "
      "QListWidget::item:selected { background: #d1e8f1; color: #102e3b; }")
      .arg(CN_FontPointSize()));
  QPointer<QListWidget> cards(list);
  new CN_AndroidButtonDragFilter(list->viewport(), [cards, selectable](QPoint p) {
    if (!cards || !selectable) return;
    const auto index = cards->indexAt(p);
    if (index.isValid()) cards->setCurrentRow(index.row());
  }, list->viewport());
  new PlannerListScroll(list);
  layout->addWidget(list);
  root->Add(panel, 1, wxEXPAND | wxALL, 8);
  return list;
}
#endif

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
#ifdef __OCPN__ANDROID__
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
      const auto point = event.GetPosition();
      int nearest = -1; double distance = 24.0 * 24.0;
      for (const auto& hit : m_hitBodies) {
        const double dx = point.x-hit.point.x, dy = point.y-hit.point.y;
        if (dx*dx+dy*dy < distance) { distance=dx*dx+dy*dy; nearest=hit.index; }
      }
      if (nearest >= 0 && m_bodyTap) m_bodyTap(m_bodies[nearest].state.body);
      event.Skip();
    });
#endif
    Bind(wxEVT_MOTION, [this](wxMouseEvent& event) {
      const wxPoint cursor = event.GetPosition();
      wxString tooltip;
      for (const auto& hit : m_hitBodies) {
        if (std::abs(cursor.x - hit.point.x) <= 8 &&
            std::abs(cursor.y - hit.point.y) <= 8) {
          const auto& state = m_bodies[hit.index].state;
          tooltip = wxString::Format(_("%s; mag %.1f; Hc %.1f; Zn true %.1f"),
              state.body.c_str(), state.visualMagnitude,
              state.geometricAltitude, state.azimuthTrue);
          break;
        }
      }
      if (tooltip != m_hoverText) {
        m_hoverText = tooltip;
        if (tooltip.empty()) UnsetToolTip(); else SetToolTip(tooltip);
      }
      event.Skip();
    });
  }
  void SetBodyTapCallback(std::function<void(const wxString&)> callback) { m_bodyTap = std::move(callback); }
  void SetBodies(const std::vector<RankedBody>& bodies, bool daylight) {
    m_bodies = bodies;
    m_hitBodies.clear();
    m_hoverText.clear();
    UnsetToolTip();
    m_daylight = daylight;
    Refresh();
  }
  void SetSelectedBody(const wxString& body) { m_selected = body; Refresh(); }
  wxString SelectedBody() const { return m_selected; }
  void SetLabelDensity(int density) { m_labelDensity = density; Refresh(); }
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
  struct PlottedBody { wxPoint point; wxColour colour; size_t index; };

  void DrawBodyLabels(wxDC& dc, const std::vector<PlottedBody>& plotted,
                      const wxRect& bounds, std::vector<wxRect>& labels) {
    m_hitBodies = plotted;
    auto order = SightRanker::SkyLabelPriority(m_bodies);
    const auto selected = std::find_if(order.begin(), order.end(),
        [this](size_t index) { return m_bodies[index].state.body == m_selected; });
    if (selected != order.end()) std::rotate(order.begin(), selected, selected + 1);
    std::vector<wxRect> markers;
    for (const auto& dot : plotted)
      markers.emplace_back(dot.point.x - 5, dot.point.y - 5, 11, 11);
    const unsigned budget = sky_labels::Budget(bounds.GetSize(), m_labelDensity);
    unsigned count = 0;
    for (size_t index : order) {
      const auto dot = std::find_if(plotted.begin(), plotted.end(),
          [index](const PlottedBody& body) { return body.index == index; });
      if (dot == plotted.end()) continue;
      const auto& name = m_bodies[index].state.body;
      const bool selectedBody = name == m_selected;
      if (count >= budget && !selectedBody) continue;
      const wxSize extent = dc.GetTextExtent(name);
      wxRect label;
      if (!sky_labels::Place(dot->point, extent, bounds, labels, markers, &label,
                             selectedBody)) continue;
      wxRect padded = label;
      padded.Inflate(2);
      dc.SetPen(wxPen(dot->colour, 1));
      dc.DrawLine(dot->point, wxPoint(
          std::max(label.x, std::min(dot->point.x, label.GetRight())),
          std::max(label.y, std::min(dot->point.y, label.GetBottom()))));
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.SetBrush(wxBrush(GetBackgroundColour()));
      dc.DrawRectangle(padded);
      dc.SetTextForeground(dot->colour);
      dc.DrawText(name, label.GetPosition());
      labels.push_back(padded);
      ++count;
    }
  }

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
    for (int dec = -90; dec <= 90; dec += 30) {
      const int y = chart.y + chart.height * (90 - dec) / 180;
      dc.DrawLine(chart.x, y, chart.GetRight(), y);
      dc.DrawText(wxString::Format("%d", dec), 4, y - 7);
    }
    dc.SetTextForeground(wxColour(70, 75, 82));
    dc.DrawText("SHA", chart.GetRight() - 25, 2);
    auto xy = [&chart](const PlannerSkyPoint& point) {
      return wxPoint(chart.x + static_cast<int>(chart.width * point.sha / 360.0),
                     chart.y + static_cast<int>(chart.height *
                         (90.0 - point.declination) / 180.0));
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
    std::vector<PlottedBody> plotted;
    for (size_t index : SightRanker::SkyLabelPriority(m_bodies)) {
      const auto& body = m_bodies[index];
      if (body.state.declination < -90.0 || body.state.declination > 90.0)
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
      plotted.push_back({p, colour, index});
    }
    DrawBodyLabels(dc, plotted, chart, labels);
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
    std::vector<PlottedBody> plotted;
    plotted.reserve(m_bodies.size());
    for (size_t index = 0; index < m_bodies.size(); ++index) {
      const auto& body = m_bodies[index];
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
      plotted.push_back({p, colour, index});
    }

    DrawBodyLabels(dc, plotted, GetClientRect(), labels);
  }
  std::vector<RankedBody> m_bodies;
  std::vector<PlannerSkyPoint> m_ecliptic;
  std::vector<PlannerSkyPoint> m_moonPath;
  bool m_daylight = false;
  bool m_equatorial = false;
  bool m_showEcliptic = true;
  bool m_showMoonPath = false;
  wxString m_selected;
  wxString m_hoverText;
  std::function<void(const wxString&)> m_bodyTap;
  int m_labelDensity = 1;
  std::vector<PlottedBody> m_hitBodies;
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
      m_bodySortColumn(-1),
      m_bodySortAscending(false),
      m_lastValidZoneOffset(0.0),
      m_zoneOffsetTextValid(true),
      m_updatingZoneOffset(false) {
  const CelestialNavigationDefaults defaults =
      LoadCelestialNavigationDefaults();
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer* context =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Planning context"));

  m_positionSource = new wxChoice(this, wxID_ANY);
  m_positionSource->Append(_("Manual"));
  m_positionSource->Append(_("Current boat position"));
  m_positionSource->Append(_("Chart cursor"));
  m_positionSource->Append(_("Selected sight DR"));
  m_positionSource->Append(_("Last calculated fix"));
  m_positionSource->Append(_("Waypoint or place..."));
  m_positionSource->SetSelection(1);
  m_latitude = new NavigationAngleCtrl(this, NavigationAngleKind::Latitude, 0.0,
                                       -90.0, 90.0, wxSize(145, -1));
  m_longitude = new NavigationAngleCtrl(this, NavigationAngleKind::Longitude,
                                        0.0, -180.0, 180.0, wxSize(145, -1));
  m_latitude->SetName("PlannerLatitude");
  m_longitude->SetName("PlannerLongitude");
  for (auto* coordinate : {m_latitude, m_longitude})
    coordinate->SetMinSize(wxSize(std::max(145,
        coordinate->GetTextExtent(FormatNavigationAngle(-179.99999,
            NavigationAngleKind::Longitude, true)).x + 24), -1));

  m_timeSource = new wxChoice(this, wxID_ANY);
  m_timeSource->Append(_("Now"));
  m_timeSource->Append(_("Selected sight"));
  m_timeSource->Append(_("Manual date/time"));
  m_timeSource->SetSelection(0);
  m_dateLabel = new wxStaticText(this, wxID_ANY, _("Date (UTC)"));
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
  m_timeLabel = new wxStaticText(this, wxID_ANY, _("Time (UTC)"));
  m_timeContainer = new wxPanel(this);
  wxBoxSizer* timeSizer = new wxBoxSizer(wxVERTICAL);
  m_utcTime = new CelestialTimePicker(m_timeContainer, wxID_ANY);
  m_nauticalTime =
      new wxTextCtrl(m_timeContainer, wxID_ANY, wxEmptyString,
                     wxDefaultPosition, wxSize(145, -1), wxTE_PROCESS_ENTER);
  m_nauticalTime->SetHint(_("HH:MM:SS"));
  timeSizer->Add(m_utcTime, 0, wxEXPAND);
  timeSizer->Add(m_nauticalTime, 0, wxEXPAND);
  m_timeContainer->SetSizer(timeSizer);

  m_inputTimeBasis = new wxChoice(this, wxID_ANY);
  m_inputTimeBasis->Append(_("UTC"));
  m_inputTimeBasis->Append(_("Computer local time"));
  m_inputTimeBasis->Append(_("Ship zone time"));
  m_inputTimeBasis->SetSelection(0);
  m_displayTime = new wxChoice(this, wxID_ANY);
  m_displayTime->Append(_("UTC"));
  m_displayTime->Append(_("Computer local"));
  m_displayTime->Append(_("Local mean time"));
  m_displayTime->Append(_("Fixed offset"));
  m_displayTime->SetSelection(0);
  m_fixedOffset =
      new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(75, -1), wxSP_ARROW_KEYS, -12, 14, 0, 0.5);
  m_fixedOffset->SetDigits(1);

  m_entryFormat = new wxChoice(this, wxID_ANY);
  m_entryFormat->Append(_("Nautical: YYYY-MM-DD, 24-hour"));
  m_entryFormat->Append(_("OpenCPN / platform format"));
  m_entryFormat->SetSelection(0);
  m_entryFormat->SetToolTip(
      _("Nautical format avoids ambiguous dates and AM/PM."));
  m_autoZoneOffset =
      new wxCheckBox(this, wxID_ANY, _("Auto zone from longitude"));
  m_autoZoneOffset->SetValue(true);

  wxWrapSizer* motion = new wxWrapSizer(wxHORIZONTAL);
  m_moving = new wxCheckBox(this, wxID_ANY, _("Time-tagged moving observer"));
  m_moving->SetToolTip(_("The entered position is at the reference time above. Course and speed propagate it to each planning instant."));
  m_course =
      new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(100, -1), wxSP_ARROW_KEYS, 0, 359.9, 0, 0.1);
  m_course->SetDigits(1);
  m_speed =
      new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                           wxSize(100, -1), wxSP_ARROW_KEYS, 0, 100, 0, 0.1);
  m_speed->SetDigits(1);

  m_eyeHeight = new wxSpinCtrlDouble(
      this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
      wxSP_ARROW_KEYS, 0, 100, defaults.eyeHeight, 0.1);
  m_eyeHeight->SetDigits(1);
  wxButton* calculate = new wxButton(this, wxID_ANY, _("Calculate / refresh"));
  // Reflow complete fields and related pairs. A column-count change must not
  // split latitude/longitude or the display-time/zone-offset relationship.
  auto field = [this](const wxString& label, wxWindow* control) {
    auto* pair = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
        wxVERTICAL
#else
        wxHORIZONTAL
#endif
    );
    pair->Add(new wxStaticText(this, wxID_ANY, label), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    pair->Add(control, 0, wxALIGN_CENTER_VERTICAL);
    return pair;
  };
  auto* header = new wxBoxSizer(wxVERTICAL);
  auto* positionRow = new wxWrapSizer(wxHORIZONTAL);
  positionRow->Add(field(_("Position"), m_positionSource), 0, wxRIGHT, 12);
  auto* coordinates = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  coordinates->Add(field(_("Latitude"), m_latitude), 0, wxRIGHT, 12);
  coordinates->Add(field(_("Longitude"), m_longitude));
  positionRow->Add(coordinates);
  header->Add(positionRow, 0, wxEXPAND | wxBOTTOM, 4);

  auto* timeRow = new wxWrapSizer(wxHORIZONTAL);
  timeRow->Add(field(_("Time"), m_timeSource), 0, wxRIGHT, 12);
  auto* dateTime = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  dateTime->Add(m_dateLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  dateTime->Add(m_dateContainer, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
  dateTime->Add(m_timeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  dateTime->Add(m_timeContainer, 0, wxALIGN_CENTER_VERTICAL);
  timeRow->Add(dateTime, 0, wxRIGHT, 12);
  auto* steps = new wxBoxSizer(wxHORIZONTAL);
  for (int hours : {-1, 1}) {
    auto* step = new wxButton(this, wxID_ANY, hours < 0 ? _("-1 h") : _("+1 h"));
    step->SetToolTip(_("Step the whole sky by one UTC hour; advance the entered position when the moving observer is enabled."));
    step->Bind(wxEVT_BUTTON, [this, hours](wxCommandEvent&) { StepPlanningTime(hours); });
    steps->Add(step, 0, wxRIGHT, 4);
  }
  timeRow->Add(steps);
  header->Add(timeRow, 0, wxEXPAND | wxBOTTOM, 4);

  auto* basisRow = new wxWrapSizer(wxHORIZONTAL);
  basisRow->Add(field(_("Enter time as"), m_inputTimeBasis), 0, wxRIGHT, 12);
  auto* displayZone = new wxBoxSizer(
#ifdef __OCPN__ANDROID__
      wxVERTICAL
#else
      wxHORIZONTAL
#endif
  );
  displayZone->Add(field(_("Display event times as"), m_displayTime), 0, wxRIGHT, 12);
  displayZone->Add(field(_("Zone offset (h)"), m_fixedOffset));
  basisRow->Add(displayZone);
  header->Add(basisRow, 0, wxEXPAND | wxBOTTOM, 4);

  auto* formatRow = new wxWrapSizer(wxHORIZONTAL);
  formatRow->Add(field(_("Date/time entry"), m_entryFormat), 0, wxRIGHT, 12);
  formatRow->Add(m_autoZoneOffset, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
  formatRow->Add(field(_("Eye height (m)"), m_eyeHeight), 0, wxRIGHT, 12);
  formatRow->Add(calculate);
  header->Add(formatRow, 0, wxEXPAND);
  motion->Add(m_moving, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
  motion->Add(field(_("COG (true)"), m_course), 0, wxRIGHT, 14);
  motion->Add(field(_("SOG (kn)"), m_speed));
  context->Add(header, 0, wxALL | wxEXPAND, 6);
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
  wxWrapSizer* recommendationControls = new wxWrapSizer(wxHORIZONTAL);
  m_limitRecommendationAltitude =
      new wxCheckBox(bodiesPage, wxID_ANY, _("Limit recommendations by Hc"));
  m_limitRecommendationAltitude->SetValue(true);
  recommendationControls->Add(m_limitRecommendationAltitude, 0,
                              wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
  auto* altitudeLimits = new wxBoxSizer(wxHORIZONTAL);
  altitudeLimits->Add(
      new wxStaticText(bodiesPage, wxID_ANY, _("Minimum")), 0,
      wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  m_recommendationMinAltitude = new wxSpinCtrlDouble(
      bodiesPage, wxID_ANY, "10", wxDefaultPosition, wxSize(80, -1),
      wxSP_ARROW_KEYS, 0.0, 90.0, 10.0, 1.0);
  m_recommendationMinAltitude->SetDigits(1);
  altitudeLimits->Add(m_recommendationMinAltitude, 0,
                              wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
  altitudeLimits->Add(
      new wxStaticText(bodiesPage, wxID_ANY, _("Maximum")), 0,
      wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  m_recommendationMaxAltitude = new wxSpinCtrlDouble(
      bodiesPage, wxID_ANY, "75", wxDefaultPosition, wxSize(80, -1),
      wxSP_ARROW_KEYS, 0.0, 90.0, 75.0, 1.0);
  m_recommendationMaxAltitude->SetDigits(1);
  altitudeLimits->Add(m_recommendationMaxAltitude, 0,
                              wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  altitudeLimits->Add(
      new wxStaticText(bodiesPage, wxID_ANY, CN_UTF8_("°")), 0,
      wxALIGN_CENTER_VERTICAL);
  recommendationControls->Add(altitudeLimits, 0, wxALIGN_CENTER_VERTICAL);
  m_includePolarisRecommendations = new wxCheckBox(bodiesPage, wxID_ANY,
      _("Include Polaris in fixes"));
  m_includePolarisRecommendations->SetToolTip(_("Optional latitude constraint for a fix with other suitable bearings. Polaris remains in the body list and latitude helper."));
  recommendationControls->Add(m_includePolarisRecommendations, 0,
                              wxLEFT | wxALIGN_CENTER_VERTICAL, 12);
  bodiesLeft->Add(recommendationControls, 0,
                  wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 6);
  wxStaticText* recommendationNote = new wxStaticText(
      bodiesPage, wxID_ANY,
      _("All catalogued bodies above the horizon remain listed; these limits "
        "affect recommended pairs and triads only. Three-body fixes are listed first."));
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
  AddColumn(m_bodies, 6, _("Visibility"), 205);
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
  m_lunarOrder->Append(_("Lunar order: cautions, brightness, timing"));
  m_lunarOrder->Append(_("Lunar order: timing sensitivity"));
  m_lunarOrder->SetSelection(0);
  m_lunarOrder->SetToolTip(_("Both lunar planners use the same ordering. Below-horizon pairs come last. Practical order prefers fewer cautions and brighter companions, then timing. Ecliptic latitude is context, not a second timing score."));
  m_lunarCompanions = new wxChoice(recommendationsPage, wxID_ANY);
  m_lunarCompanions->Append(_("Traditional lunar companions"));
  m_lunarCompanions->Append(_("All calculated companions"));
  m_lunarCompanions->SetSelection(0);
  m_lunarCompanions->SetToolTip(_("Sun, Venus, Mars, Jupiter, Saturn and the nine traditional lunar stars. Other bodies remain in All bodies; select All calculated companions to compare them."));
  recommendationSizer->Add(m_lunarCompanions, 0, wxALL, 5);
  recommendationSizer->Add(m_lunarOrder, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  m_lunarCompanions->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshBodies(); });
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
  AddColumn(m_lunarPairs, 4, _("Mag"), 55);
  AddColumn(m_lunarPairs, 5, _("Ecliptic lat"), 95);
  AddColumn(m_lunarPairs, 6, _("Cautions"), 80);
  AddColumn(m_lunarPairs, 7, _("Moon/body Hc, Zn; guidance"), 440);
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
  auto* labelControls = new wxBoxSizer(wxHORIZONTAL);
  labelControls->Add(new wxStaticText(bodiesPage, wxID_ANY, _("Star labels")), 0,
                     wxRIGHT | wxALIGN_CENTER_VERTICAL, 6);
  m_plotLabels = new wxChoice(bodiesPage, wxID_ANY);
  for (const auto& label : {_("Sparse"), _("Balanced"), _("More")})
    m_plotLabels->Append(label);
  m_plotLabels->SetSelection(1);
  m_plotLabels->SetToolTip(_("Show as many names as fit at this density, brightest first. The selected body is labelled first; hover over any dot for its name."));
  labelControls->Add(m_plotLabels);
  plotControls->Add(labelControls, 0, wxEXPAND | wxBOTTOM, 4);
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
#ifdef __OCPN__ANDROID__
  auto bodyTapped = [this](const wxString& name) {
    m_skyPlot->SetSelectedBody(name); m_equatorialPlot->SetSelectedBody(name);
    const auto text = QString::fromUtf8(name.utf8_str());
    for (int row=0; row<m_androidBodies->count(); ++row) {
      if (m_androidBodies->item(row)->data(Qt::UserRole+1).toString() == text) {
        m_androidBodies->setCurrentRow(row); break;
      }
    }
    m_androidSelectedBody->SetLabel(_("Selected: ") + name);
    RefreshSkyPlot();
    celestial_android::LayoutScrolls(this);
  };
  m_skyPlot->SetBodyTapCallback(bodyTapped);
  m_equatorialPlot->SetBodyTapCallback(bodyTapped);
  m_plotLabels->SetToolTip(_("Names fit at the selected density, brightest first. Tap a dot to identify it; the selected body is labelled first."));
#endif
  m_plotNotebook->AddPage(m_skyPlot, _("Local sky"), true);
  m_plotNotebook->AddPage(m_equatorialPlot, _("SHA / Declination"), false);
  m_showMoonPath->SetToolTip(_("Moon motion relative to the stars, on SHA / Declination only. Use -1 h / +1 h to move the entire local sky together."));
  m_plotNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this, spanRow, plotControls, bodiesPage](wxBookCtrlEvent& event) {
    event.Skip();
    const bool equatorial = event.GetSelection() == 1;
    m_showMoonPath->Enable(equatorial);
    m_moonSpan->Enable(equatorial && m_showMoonPath->GetValue());
    spanRow->ShowItems(equatorial);
    plotControls->Layout();
    bodiesPage->Layout();
    bodiesPage->FitInside();
#ifdef __OCPN__ANDROID__
    celestial_android::LayoutScrolls(this);
#endif
  });
  m_showMoonPath->Enable(false);
  m_moonSpan->Enable(false);
  spanRow->ShowItems(false);
  plotSizer->Add(m_plotNotebook, 1, wxALL | wxEXPAND, 8);
  wxStaticText* plotLegend = new wxStaticText(
      bodiesPage, wxID_ANY,
      _("Ecliptic amber; Moon track blue on SHA / Declination only. "
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
#ifdef __OCPN__ANDROID__
                                    // POBsoft (1985-2026): keep the scaled caption on one line.
                                    _("Corrected Ho (degrees)")),
#else
                                    _("Corrected observed altitude Ho")),
#endif
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

#ifdef __OCPN__ANDROID__
  // Keep the context accessible at any orientation instead of reserving a
  // six-column desktop grid above every result page. Retain all controllers.
  root->Hide(buttons); // The Android surface supplies the persistent Close.
  auto* contextPage = new wxPanel(m_notebook);
  root->Detach(context);
  std::vector<wxWindow*> contextChildren;
  for (auto* child : GetChildren())
    if (child != m_notebook && child != contextPage &&
        child->GetId() != wxID_CLOSE)
      contextChildren.push_back(child);
  for (auto* child : contextChildren) child->Reparent(contextPage);
  auto* contextRoot = new wxBoxSizer(wxVERTICAL);
  contextRoot->Add(context, 0, wxEXPAND | wxALL, 8);
  contextPage->SetSizer(contextRoot);
  m_notebook->InsertPage(0, contextPage, _("Context"), true);

  m_events->Hide();
  eventsSizer->Detach(m_events);
  m_androidEvents = new wxStaticText(eventsPage, wxID_ANY, wxEmptyString);
  eventsSizer->Insert(0, m_androidEvents, 0, wxEXPAND | wxALL, 8);

  // Retain desktop controllers and shared models; replace clipped tables.
  for (auto* child : bodiesPage->GetChildren()) child->Hide();
  plotSizer->Detach(plotControls);  // Keep this nested sizer alive across Clear.
  bodiesRoot->Clear(false);
  bodiesRoot->SetOrientation(wxVERTICAL);
  m_bodies->Hide();
  m_combinations->Hide();
  auto add = [](wxWindow* page, wxSizer* sizer, const wxString& text) {
    auto* label = new wxStaticText(page, wxID_ANY, text);
    sizer->Add(label, 0, wxEXPAND | wxALL, 8);
    return label;
  };
  add(bodiesPage, bodiesRoot, _("Sort bodies"));
  auto* sort = new wxChoice(bodiesPage, wxID_ANY);
  for (const auto& name : {_("Body"), _("Hc"), _("Zn true"), _("GHA"),
                           _("Declination"), _("Magnitude"), _("Visibility / cautions"),
                           _("Ecliptic latitude"), _("Reason")}) sort->Append(name);
  sort->Insert(_("Planning order"), 0);
  sort->SetSelection(m_bodySortColumn + 1);
  m_planningMode->Reparent(bodiesPage);
  m_tableBelowHorizon->Reparent(bodiesPage);
  add(bodiesPage, bodiesRoot, _("Planning mode"));
  bodiesRoot->Add(m_planningMode, 0, wxEXPAND | wxALL, 8);
  bodiesRoot->Add(m_tableBelowHorizon, 0, wxEXPAND | wxALL, 8);
  auto* sortRow = new wxBoxSizer(wxHORIZONTAL);
  sortRow->Add(sort, 1, wxEXPAND | wxALL, 8);
  auto* direction = new wxChoice(bodiesPage, wxID_ANY);
  direction->Append(_("Ascending"));
  direction->Append(_("Descending"));
  direction->SetSelection(m_bodySortAscending ? 0 : 1);
  sortRow->Add(direction, 1, wxEXPAND | wxALL, 8);
  bodiesRoot->Add(sortRow, 0, wxEXPAND);
  sort->Bind(wxEVT_CHOICE, [this, sort](wxCommandEvent&) {
    m_bodySortColumn = sort->GetSelection() - 1; RebuildBodyList();
  });
  direction->Bind(wxEVT_CHOICE, [this, direction](wxCommandEvent&) {
    m_bodySortAscending = direction->GetSelection() == 0; RebuildBodyList();
  });
  auto* selectionRow = new wxBoxSizer(wxHORIZONTAL);
  m_androidSelectedBody = new wxStaticText(bodiesPage, wxID_ANY, _("Select a body card"));
  selectionRow->Add(m_androidSelectedBody, 1, wxALIGN_CENTER_VERTICAL | wxALL, 8);
  createSight->SetLabel(_("Create sight"));
  if (auto* native = qobject_cast<QAbstractButton*>(createSight->GetHandle()))
    native->setText(QString::fromUtf8(_("Create sight").utf8_str()));
  selectionRow->Add(createSight, 0, wxALL, 8);
  bodiesRoot->Add(selectionRow, 0, wxEXPAND);
  windows->Reparent(bodiesPage);
  bodiesRoot->Add(windows, 0, wxEXPAND | wxALL, 8);
  m_androidCreateSight = createSight;
  createSight->Enable(false);
  m_androidBodies = PlannerCards(bodiesPage, bodiesRoot, true);
  QObject::connect(m_androidBodies, &QListWidget::currentRowChanged,
                   GetHandle(), [this](int) { UpdateAndroidBodySelection(); });
  bodiesPage->GetHandle()->setProperty("cnNoPageScroll", true);
  bodiesRoot->ShowItems(true);
  auto* recommendationPage = new wxPanel(m_notebook);
  auto* recommendationRoot = new wxBoxSizer(wxVERTICAL);
  for (auto* child : {static_cast<wxWindow*>(m_limitRecommendationAltitude),
                     static_cast<wxWindow*>(m_recommendationMinAltitude),
                     static_cast<wxWindow*>(m_recommendationMaxAltitude),
                     static_cast<wxWindow*>(recommendationNote),
                     static_cast<wxWindow*>(m_plotMagnitude),
                     static_cast<wxWindow*>(m_plotBelowHorizon),
                     static_cast<wxWindow*>(m_showEcliptic),
                     static_cast<wxWindow*>(m_showMoonPath),
                     static_cast<wxWindow*>(m_moonSpan),
                     static_cast<wxWindow*>(m_plotNotebook),
                     static_cast<wxWindow*>(m_lunarOrder),
                     static_cast<wxWindow*>(m_lunarCompanions),
                     static_cast<wxWindow*>(m_includePolarisRecommendations),
                     static_cast<wxWindow*>(m_plotLabels),
                     static_cast<wxWindow*>(plotLegend)})
    child->Reparent(recommendationPage);
  // The magnitude caption belongs to the retained controls' nested sizer.
  for (auto* item : labelControls->GetChildren())
    if (auto* child = item->GetWindow()) child->Reparent(recommendationPage);
  for (auto* item : spanRow->GetChildren())
    if (auto* child = item->GetWindow()) child->Reparent(recommendationPage);
  for (auto* item : magnitudeControls->GetChildren())
    if (auto* child = item->GetWindow()) child->Reparent(recommendationPage);
  recommendationRoot->Add(m_limitRecommendationAltitude, 0, wxEXPAND | wxALL, 8);
  add(recommendationPage, recommendationRoot, _("Minimum recommended Hc (degrees)"));
  recommendationRoot->Add(m_recommendationMinAltitude, 0, wxEXPAND | wxALL, 8);
  add(recommendationPage, recommendationRoot, _("Maximum recommended Hc (degrees)"));
  recommendationRoot->Add(m_recommendationMaxAltitude, 0, wxEXPAND | wxALL, 8);
  recommendationRoot->Add(recommendationNote, 0, wxEXPAND | wxALL, 8);
  recommendationRoot->Add(m_includePolarisRecommendations, 0, wxEXPAND | wxALL, 8);
  recommendationRoot->Add(m_lunarCompanions, 0, wxEXPAND | wxALL, 8);
  recommendationRoot->Add(m_lunarOrder, 0, wxEXPAND | wxALL, 8);
  m_androidCombinations = add(recommendationPage, recommendationRoot, wxEmptyString);
  recommendationRoot->Add(plotControls, 0, wxEXPAND | wxALL, 8);
  m_plotNotebook->SetMinSize(wxSize(0, CN_TouchHeight() * 5));
  recommendationRoot->Add(m_plotNotebook, 0, wxEXPAND | wxALL, 8);
  recommendationRoot->Add(plotLegend, 0, wxEXPAND | wxALL, 8);
  recommendationRoot->ShowItems(true);
  spanRow->ShowItems(false);
  recommendationPage->SetSizer(recommendationRoot);
  m_notebook->InsertPage(3, recommendationPage, _("Recommendations && sky"));

  m_almanac->Hide();
  almanacSizer->Detach(m_almanac);
  almanacSizer->Detach(exportButton);
  almanacSizer->Add(exportButton, 0, wxEXPAND | wxALL, 8);
  m_androidExport = exportButton;
  m_androidAlmanacStatus = add(almanacPage, almanacSizer, wxEmptyString);
  m_androidAlmanac = PlannerCards(almanacPage, almanacSizer, false);
  almanacPage->GetHandle()->setProperty("cnNoPageScroll", true);
  auto* progress = new wxBoxSizer(wxHORIZONTAL);
  m_androidProgress = new wxStaticText(this, wxID_ANY, _("Preparing planner..."));
  m_androidCancel = new wxButton(this, wxID_ANY, _("Cancel calculation"));
  progress->Add(m_androidProgress, 1, wxEXPAND | wxALL, 8);
  progress->Add(m_androidCancel, 0, wxALL, 8);
  root->Insert(0, progress, 0, wxEXPAND);
  m_androidCancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    CancelAndroidCalculation();
  });
  m_androidSolve = solve;
  try {
    m_androidWorker.reset(new celestial_android::PlannerWorker());
  } catch (const std::exception& error) {
    m_androidProgress->SetLabel(_("Cannot start planner worker: ") +
                               wxString::FromUTF8(error.what()));
    m_androidCancel->Enable(false);
  }
  m_androidPoll = new QTimer(GetHandle());
  QObject::connect(m_androidPoll, &QTimer::timeout, GetHandle(), [this] {
    PollAndroidCalculation();
  });
  m_androidPoll->start(80);
#endif
#ifndef __OCPN__ANDROID__
  const int wideContextWidth = 1120;
  Bind(wxEVT_SIZE, [this, bodiesRoot, bodiesPage, wideContextWidth](wxSizeEvent& event) {
    event.Skip();
    if (m_reflowingLayout) return;
    const bool compact = event.GetSize().x < wideContextWidth;
    if (compact == m_compactLayout) return;
    m_reflowingLayout = true;
    bodiesRoot->SetOrientation(compact ? wxVERTICAL : wxHORIZONTAL);
    m_compactLayout = compact;
    Layout();
    bodiesPage->Layout();
    bodiesPage->FitInside();
    m_reflowingLayout = false;
  });
#endif

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
#ifdef __OCPN__ANDROID__
  auto* cursorTimer = new QTimer(GetHandle());
  QObject::connect(cursorTimer, &QTimer::timeout, GetHandle(), [this]() {
    UpdateCursorPosition();
  });
  cursorTimer->start(500);
#else
  m_cursorTimer.Start(500);
#endif
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
#ifdef __OCPN__ANDROID__
    if (m_androidReady) RefreshEvents();
#else
    RefreshEvents();
#endif
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
  m_plotLabels->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshSkyPlot(); });
  m_includePolarisRecommendations->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { RefreshBodies(); });
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
#ifdef __OCPN__ANDROID__
  m_androidRefresh = new QTimer(GetHandle());
  m_androidRefresh->setSingleShot(true);
  QObject::connect(m_androidRefresh, &QTimer::timeout, GetHandle(), [this]() {
    wxTimerEvent event;
    OnRefreshTimer(event);
  });
  // Typed wxQt spin values need native notifications as well as arrow events.
  // The existing owned refresh timer coalesces edits after callbacks return.
  for (auto* control : {m_course, m_speed, m_eyeHeight,
                       m_recommendationMinAltitude,
                       m_recommendationMaxAltitude}) {
    if (auto* spin = qobject_cast<QDoubleSpinBox*>(control->GetHandle()))
      QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                       GetHandle(), [this](double) { ScheduleRefresh(); });
  }
  if (auto* spin = qobject_cast<QDoubleSpinBox*>(m_fixedOffset->GetHandle()))
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        GetHandle(), [this](double value) {
          if (m_updatingZoneOffset) return;
          m_lastValidZoneOffset = value;
          m_zoneOffsetTextValid = true;
          m_autoZoneOffset->SetValue(false);
          ScheduleRefresh();
        });
#endif
  calculate->Bind(wxEVT_BUTTON, &PlannerDialog::RefreshAll, this);
  exportButton->Bind(wxEVT_BUTTON, &PlannerDialog::ExportAlmanac, this);
  exportBodies->Bind(wxEVT_BUTTON, &PlannerDialog::ExportBodyTable, this);
  createSight->Bind(wxEVT_BUTTON, &PlannerDialog::CreateSelectedSight, this);
  solve->Bind(wxEVT_BUTTON, &PlannerDialog::SolveSpecialLatitude, this);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); }, wxID_CLOSE);

  wxFileConfig* config = GetOCPNConfigObject();
  config->SetPath(_T("/PlugIns/CelestialNavigation/Planner"));
  long positionSource = 1, inputTimeBasis = 0, displayTime = 0, entryFormat = 0;
  long planningMode = 0, moonSpan = 0, plotLabels = 1, lunarCompanions = 0;
  bool includePolaris = false;
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
#ifdef __OCPN__ANDROID__
  // Prefer explicit selectors on a new profile, retaining an existing choice.
  if (!config->HasEntry(_T("EntryFormat"))) entryFormat = 1;
#endif
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
  config->Read(_T("PlotLabels"), &plotLabels, 1L);
  config->Read(_T("LunarCompanions"), &lunarCompanions, 0L);
  config->Read(_T("IncludePolarisRecommendations"), &includePolaris, false);
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
  m_plotLabels->SetSelection(std::max(0L, std::min(plotLabels, 2L)));
  m_lunarCompanions->SetSelection(std::max(0L, std::min(lunarCompanions, 1L)));
  m_includePolarisRecommendations->SetValue(includePolaris);
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
#ifdef __OCPN__ANDROID__
  m_androidRefresh->stop();
  m_androidPoll->stop();
  m_androidWorker.reset(); // Cancel and join before widgets/plugin are released.
#endif
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
  config->Write(_T("PlotLabels"), static_cast<long>(m_plotLabels->GetSelection()));
  config->Write(_T("LunarCompanions"), static_cast<long>(m_lunarCompanions->GetSelection()));
  config->Write(_T("IncludePolarisRecommendations"), m_includePolarisRecommendations->GetValue());
  config->Write(_T("WaypointGuid"), m_waypointGuid);
  config->Write(_T("WaypointName"), m_waypointName);
#ifdef __OCPN__ANDROID__
  // POBsoft (1985-2026): Android termination may bypass the host shutdown flush.
  config->Flush();
#endif
}

wxDateTime PlannerDialog::ReadUtc(bool showErrors) {
  wxString date = m_nauticalDate->GetValue(), clock = m_nauticalTime->GetValue();
  if (m_entryFormat->GetSelection() == 1) {
    const auto dateValue = m_utcDate->GetValue(), clockValue = m_utcTime->GetValue();
    if (!dateValue.IsValid() || !clockValue.IsValid()) return wxDateTime();
    date = dateValue.Format("%Y-%m-%d");
#ifdef __OCPN__ANDROID__
    clock = FormatNauticalPlannerTime(clockValue);
#else
    clock = clockValue.Format("%H:%M:%S");
#endif
  }
  wxDateTime utc;
  if (!ParseNauticalPlannerInstant(date, clock,
          static_cast<PlannerTimeBasis>(m_inputTimeBasis->GetSelection()),
          ZoneOffsetHours(), &utc)) {
    if (showErrors)
      CelestialMessageBox(_("Enter a valid date and time. Nautical format is YYYY-MM-DD and HH:MM:SS. A skipped local daylight-saving hour is not a valid computer-local time."),
                   _("Invalid time"), wxOK | wxICON_ERROR, this);
    return wxDateTime();
  }
  const int year = utc.GetYear(wxDateTime::UTC);
  if (year < 1900 || year > 2100) {
    if (showErrors)
      CelestialMessageBox(_("The ordinary offline planner is supported from 1900 through 2100."),
                   _("Time outside supported range"), wxOK | wxICON_ERROR, this);
    return wxDateTime();
  }
  return utc;
}

void PlannerDialog::SetUtcControls(const wxDateTime& utc) {
  if (!utc.IsValid()) return;
#ifdef __OCPN__ANDROID__
  const wxDateTime value = UtcToPlannerFields(
      utc, static_cast<PlannerTimeBasis>(m_inputTimeBasis->GetSelection()),
      ZoneOffsetHours());
  m_utcDate->SetValue(UtcDateTime::CalendarDate(value));
  m_utcTime->SetValue(value);
  m_nauticalDate->ChangeValue(FormatNauticalPlannerDate(value));
  m_nauticalTime->ChangeValue(FormatNauticalPlannerTime(value));
#else
  const auto basis = static_cast<PlannerTimeBasis>(m_inputTimeBasis->GetSelection());
  wxDateTime display = utc;
  if (basis == PlannerTimeBasis::ZoneTime)
    display += wxTimeSpan::Seconds(static_cast<long>(std::lround(ZoneOffsetHours() * 3600)));
  const auto zone = basis == PlannerTimeBasis::ComputerLocal ? wxDateTime::Local : wxDateTime::UTC;
  const auto fields = display.GetTm(zone);
  m_nauticalDate->ChangeValue(display.Format("%Y-%m-%d", zone));
  m_nauticalTime->ChangeValue(display.Format("%H:%M:%S", zone));
  // Native pickers are independent calendar and clock controls. Anchor their
  // time-only value on a stable winter date rather than on a DST transition.
  m_utcDate->SetValue(wxDateTime(fields.mday, fields.mon, fields.year, 12));
  m_utcTime->SetValue(wxDateTime(15, wxDateTime::Jan, 2000,
                                fields.hour, fields.min, fields.sec));
#endif
}

void PlannerDialog::StepPlanningTime(int hours) {
  const ObserverMotion observer = ReadMotion(true);
  if (!observer.referenceUtc.IsValid()) return;
  const wxDateTime next = observer.referenceUtc + wxTimeSpan::Hours(hours);
  long year = 0;
  next.Format("%Y", wxDateTime::UTC).ToLong(&year);
  if (year < 1900 || year > 2100) return;
  m_refreshTimer.Stop();
#ifdef __OCPN__ANDROID__
  m_androidRefresh->stop();
#endif
  if (observer.moving) {
    double latitude = 0, longitude = 0;
    observer.PositionAt(next, &latitude, &longitude);
    m_latitude->SetAngle(latitude);
    m_longitude->SetAngle(longitude);
    m_positionSource->SetSelection(0);
    m_lastPositionSource = 0;
  }
  m_timeSource->SetSelection(2);
  // Resolve any automatic zone change before converting the new UTC fields.
  UpdateAutomaticZoneOffset();
  SetUtcControls(next);
  wxCommandEvent event;
  RefreshAll(event);
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
  const int next = m_entryFormat->GetSelection();
  m_entryFormat->SetSelection(m_lastEntryFormat);
  const wxDateTime utc = ReadUtc(false);
  m_entryFormat->SetSelection(next);
  m_lastEntryFormat = next;
  UpdateEntryFormatControls();
  SetUtcControls(utc);
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
#ifdef __OCPN__ANDROID__
  // AdaptDates replaces the first sizer item with the touch date button.
  // Show that item, not the retained hidden native date picker.
  m_dateContainer->GetSizer()->Show(size_t(0), !nautical);
  if (m_utcDate->GetHandle()->property("cnDateAdapter").toBool())
    m_utcDate->Hide();
#else
  m_utcDate->Show(!nautical);
#endif
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
#ifdef __OCPN__ANDROID__
                          UtcDateTime::FormatInstant(utc, "%Y-%m-%d %H:%M:%S.%l UTC"));
#else
                          UtcDateTime::FormatInstant(utc, "%Y-%m-%d %H:%M:%S UTC"));
#endif
}

void PlannerDialog::ContextPositionEdited(wxCommandEvent&) {
  m_positionSource->SetSelection(0);
  m_lastPositionSource = 0;
  ScheduleRefresh();
}

void PlannerDialog::ContextTimeEdited(wxCommandEvent& event) {
  m_timeSource->SetSelection(2);
  ScheduleRefresh();
#ifdef __OCPN__ANDROID__
  // POBsoft (1985-2026): wxQt's generic hint handler must update its text
  // cache before the deferred refresh reads GetValue().
  event.Skip();
#endif
}

void PlannerDialog::ScheduleRefresh() {
#ifdef __OCPN__ANDROID__
  if (m_androidWorker) m_androidWorker->Cancel();
  if (m_androidReady) ClearCalculatedResults(_("Context changed; refreshing results..."));
  if (m_androidCancel) m_androidCancel->Enable(false);
  if (m_androidProgress) m_androidProgress->SetLabel(_("Waiting for context edits..."));
  m_androidReady = false;
  if (m_androidCreateSight) m_androidCreateSight->Enable(false);
  if (m_androidExport) m_androidExport->Enable(false);
  if (m_androidSolve) m_androidSolve->Enable(false);
  if (m_androidRefresh) m_androidRefresh->start(350);
#else
  m_refreshTimer.StartOnce(350);
#endif
}

void PlannerDialog::OnRefreshTimer(wxTimerEvent&) {
#ifdef __OCPN__ANDROID__
  QElapsedTimer elapsed;
  elapsed.start();
#endif
  UpdateAutomaticZoneOffset();
  const ObserverMotion motion = ReadMotion(false);
  if (!motion.referenceUtc.IsValid()) {
    UpdateResolvedUtc(wxDateTime());
    ClearCalculatedResults(
        _("Results cleared: enter a valid supported date, time and position."));
    return;
  }
  UpdateResolvedUtc(motion.referenceUtc);
#ifdef __OCPN__ANDROID__
  StartAndroidCalculation(motion);
#else
  RefreshEvents();
  RefreshBodies();
  RefreshAlmanac();
  RefreshSpecial();
#endif
  m_status->SetLabel(
      _("Planning context updated; calculations remain fully offline."));
#ifdef __OCPN__ANDROID__
  qDebug() << "Celnav planner context refresh dispatch milliseconds:" << elapsed.elapsed();
#endif
}

ObserverMotion PlannerDialog::ReadMotion(bool showErrors) {
  ObserverMotion motion;
  motion.referenceUtc = ReadUtc(showErrors);
  if (!m_latitude->GetAngle(&motion.latitude) ||
      !m_longitude->GetAngle(&motion.longitude)) {
    if (showErrors)
      CelestialMessageBox(
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
    CelestialMessageBox(_("No OpenCPN waypoints or marks are available."),
                 _("Select waypoint or place"), wxOK | wxICON_INFORMATION,
                 this);
    return false;
  }
  WaypointPickerDialog dialog(this, waypoints, m_waypointGuid);
#ifdef __OCPN__ANDROID__
  if (celestial_android::ModalResult(dialog) != wxID_OK) return false;
#else
  if (dialog.ShowModal() != wxID_OK) return false;
#endif
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
  return UtcDateTime::FormatInstant(utc + wxTimeSpan::Seconds(offset),
                                      "%Y-%m-%d %H:%M:%S") +
         " " + suffix;
}

void PlannerDialog::RefreshAll(wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
  QElapsedTimer elapsed;
  elapsed.start();
#endif
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
#ifdef __OCPN__ANDROID__
  StartAndroidCalculation(motion);
#else
  RefreshEvents();
  RefreshBodies();
  RefreshAlmanac();
  RefreshSpecial();
#endif
#ifdef __OCPN__ANDROID__
  qDebug() << "Celnav planner full refresh dispatch milliseconds:" << elapsed.elapsed();
#endif
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
#ifdef __OCPN__ANDROID__
  if (m_androidWorker) m_androidWorker->Cancel();
  m_androidCancel->Enable(false);
  m_androidReady = false;
  m_androidSolve->Enable(false);
  m_androidEvents->SetLabel(status);
  m_androidBodies->clear();
  m_androidProgress->SetLabel(_("Results unavailable"));
  m_androidSelectedBody->SetLabel(_("No valid body results"));
  m_androidCreateSight->Enable(false);
  m_androidCombinations->SetLabel(wxEmptyString);
  m_androidAlmanac->clear();
  m_androidAlmanacStatus->SetLabel(status);
  m_androidExport->Enable(false);
  m_specialSummary->SetLabel(status);
  celestial_android::LayoutScrolls(this);
#endif
  m_status->SetLabel(status);
}

void PlannerDialog::RefreshEvents() {
  m_events->DeleteAllItems();
#ifdef __OCPN__ANDROID__
  wxString report;
#endif
  const ObserverMotion motion = ReadMotion(false);
#ifdef __OCPN__ANDROID__
  if (!m_androidReady) return;
  const DailyEventsResult& table = m_androidResults.events;
#else
  const DailyEventsResult table = HorizonEventCalculator::Calculate(
      motion.referenceUtc, motion, m_eyeHeight->GetValue());
#endif
  for (const auto& event : table.events) {
#ifndef __OCPN__ANDROID__
    const long row = m_events->InsertItem(
        m_events->GetItemCount(), HorizonEventCalculator::Name(event.kind));
    m_events->SetItem(row, 1,
                      UtcDateTime::FormatInstant(event.utc, "%Y-%m-%d %H:%M:%S"));
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
#endif
#ifdef __OCPN__ANDROID__
    report += HorizonEventCalculator::Name(event.kind) + "\n";
    report += _("UTC: ") + UtcDateTime::FormatInstant(
        event.utc, "%Y-%m-%d %H:%M:%S") + "\n";
    report += _("Display: ") + DisplayTime(event.utc) + "\n";
    report += wxString::Format(_("Bearing true: %.1f degrees\n"), event.bearingTrue);
    report += _("Observer: ") + FormatNavigationAngle(event.observerLatitude,
        NavigationAngleKind::Latitude, true) + ", " +
        FormatNavigationAngle(event.observerLongitude,
        NavigationAngleKind::Longitude, true) + "\n\n";
#endif
  }
#ifdef __OCPN__ANDROID__
  const auto& phases = m_androidResults.phases;
#else
  const auto phases = NextPrincipalMoonPhases(
      motion.referenceUtc, motion.latitude, motion.longitude);
#endif
  for (const auto& phase : phases) {
#ifndef __OCPN__ANDROID__
    const long row =
        m_events->InsertItem(m_events->GetItemCount(), _("Next ") + phase.name);
    m_events->SetItem(row, 1,
                      UtcDateTime::FormatInstant(phase.utc, "%Y-%m-%d %H:%M:%S"));
    m_events->SetItem(row, 2, DisplayTime(phase.utc));
    m_events->SetItem(row, 3, CN_UTF8_("—"));
    m_events->SetItem(row, 4, _("Geocentric phase"));
#endif
#ifdef __OCPN__ANDROID__
    report += _("Next ") + phase.name + "\n";
    report += _("UTC: ") + UtcDateTime::FormatInstant(
        phase.utc, "%Y-%m-%d %H:%M:%S") + "\n";
    report += _("Display: ") + DisplayTime(phase.utc) + "\n";
    // POBsoft (1985-2026): phase times use the shared elongation approximation.
    report += _("Approximate geocentric phase") + "\n\n";
#endif
  }
#ifdef __OCPN__ANDROID__
  const auto& moon = m_androidResults.moon;
  const auto& moonState = m_androidResults.moonState;
#else
  const MoonInformation moon = CalculateMoonInformation(
      motion.referenceUtc, motion.latitude, motion.longitude);
  const BodyState moonState = CelestialEphemeris::Evaluate(
      "Moon", motion.referenceUtc, motion.latitude, motion.longitude);
#endif
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
#ifdef __OCPN__ANDROID__
  m_androidEvents->SetLabel(report.empty() ? _("No events for this context.") : report);
  celestial_android::LayoutScrolls(this);
#endif
}

void PlannerDialog::RefreshBodies() {
  m_combinations->DeleteAllItems();
#ifdef __OCPN__ANDROID__
  wxString combinations;
  if (!m_androidReady) return;
#endif
  m_lunarPairs->DeleteAllItems();
  const ObserverMotion motion = ReadMotion(false);
#ifdef __OCPN__ANDROID__
  m_planningResult = m_androidResults.planning;
#else
  m_planningResult = PlannerRecommendations::Calculate(
      motion.referenceUtc, motion.latitude, motion.longitude);
#endif
  const PlanningMode mode =
      static_cast<PlanningMode>(m_planningMode->GetSelection());
  m_rankedBodies = PlannerRecommendations::Order(
      m_planningResult, mode, m_tableBelowHorizon->GetValue(),
      m_lunarOrder->GetSelection() == 1);
  const bool lunar = mode == PlanningMode::LunarCandidates;
  m_lunarPairs->Show(lunar);
  m_lunarOrder->Show(lunar);
  m_lunarCompanions->Show(lunar);
  m_includePolarisRecommendations->Show(!lunar && mode != PlanningMode::ShowAll);
#ifdef __OCPN__ANDROID__
  m_androidCombinations->Show(mode != PlanningMode::ShowAll);
#else
  m_combinations->Show(!lunar && mode != PlanningMode::ShowAll);
  m_noRecommendations->Show(mode == PlanningMode::ShowAll);
  m_lunarPairs->GetParent()->Layout();
#endif
  wxListItem scoreColumn;
  scoreColumn.SetText(lunar ? _("Cautions") : _("Visibility"));
  m_bodies->SetColumn(6, scoreColumn);
  RebuildBodyList();
  if (lunar) {
#ifdef __OCPN__ANDROID__
    const BodyState& moon = m_androidResults.moonState;
#else
    const BodyState moon = CelestialEphemeris::Evaluate(
        "Moon", motion.referenceUtc, motion.latitude, motion.longitude);
#endif
    for (const auto& body : m_rankedBodies) {
      if (body.state.body == "Moon" ||
          (m_lunarCompanions->GetSelection() == 0 &&
           !PlannerRecommendations::IsTraditionalLunarCompanion(body.state.body))) continue;
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
      m_lunarPairs->SetItem(row, 4, wxString::Format("%.1f", body.state.visualMagnitude));
      m_lunarPairs->SetItem(row, 5,
                            wxString::Format("%+.1f%c", body.eclipticLatitude,
                                             0x00b0));
      m_lunarPairs->SetItem(row, 6,
                            wxString::Format("%d", body.lunarConstraints));
      m_lunarPairs->SetItem(
          row, 7,
          wxString::Format("Moon %.0f%c/%.0f%c; body %.0f%c/%.0f%c; "
                           "illum %.0f%%; %s",
                           moon.geometricAltitude, 0x00b0,
                           moon.azimuthTrue, 0x00b0,
                           body.state.geometricAltitude, 0x00b0,
                           body.state.azimuthTrue, 0x00b0,
                           m_planningResult.moon.illuminatedFraction * 100.0,
                           body.lunarReason.c_str()));
#ifdef __OCPN__ANDROID__
      combinations += wxString::Format(
          _("Moon + %s\nLD: %.2f degrees; rate: %+.1f arcmin/hour; 0.1' time: %s\n"
            "Moon Hc/Zn: %.0f/%.0f degrees; body Hc/Zn: %.0f/%.0f degrees\n"
            "Illumination: %.0f%%; ecliptic latitude: %+.1f degrees; cautions: %d\n%s\n\n"),
          body.state.body.c_str(), body.lunarDistance, body.lunarRateArcminHour,
          (std::isfinite(body.lunarTimingSeconds)
               ? wxString::Format("%.1f s", body.lunarTimingSeconds)
               : CN_UTF8_("—")).c_str(),
          moon.geometricAltitude, moon.azimuthTrue,
          body.state.geometricAltitude, body.state.azimuthTrue,
          m_planningResult.moon.illuminatedFraction * 100.0,
          body.eclipticLatitude, body.lunarConstraints,
          body.lunarReason.c_str());
#endif
    }
  }
  const bool limitAltitude = m_limitRecommendationAltitude->GetValue();
  const double minimumAltitude = m_recommendationMinAltitude->GetValue();
  const double maximumAltitude = m_recommendationMaxAltitude->GetValue();
  const std::vector<RankedBody> candidates =
      SightRanker::RecommendationCandidates(m_rankedBodies, limitAltitude,
                                            minimumAltitude, maximumAltitude,
                                            m_includePolarisRecommendations->GetValue());
  if (limitAltitude && minimumAltitude >= maximumAltitude &&
      (mode == PlanningMode::PracticalFix || mode == PlanningMode::BrightBodies)) {
#ifndef __OCPN__ANDROID__
    m_combinations->InsertItem(
        0, _("Set the minimum recommended Hc below the maximum."));
#endif
#ifdef __OCPN__ANDROID__
    m_androidCombinations->SetLabel(
        _("Set the minimum recommended Hc below the maximum."));
    celestial_android::LayoutScrolls(this);
#endif
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
  triads.insert(triads.end(), pairs.begin(), pairs.end());
  for (const auto& combination : triads) {
    wxString names;
    for (const auto& body : combination.bodies) {
      if (!names.empty()) names += " / ";
      names += body.state.body;
    }
#ifndef __OCPN__ANDROID__
    const long row =
        m_combinations->InsertItem(m_combinations->GetItemCount(), names);
    m_combinations->SetItem(row, 1,
                            wxString::Format("%.0f", combination.score));
    m_combinations->SetItem(row, 2, combination.reason);
#endif
#ifdef __OCPN__ANDROID__
    combinations += names + wxString::Format(_("\nScore: %.0f\n"),
        combination.score) + combination.reason + "\n\n";
#endif
  }
  }
#ifdef __OCPN__ANDROID__
  m_androidCombinations->SetLabel(
      combinations.empty()
          ? (lunar ? _("No lunar candidates for this context.")
                   : _("No pairs or triads meet these recommendation limits."))
          : combinations);
  celestial_android::LayoutScrolls(this);
#endif
#ifndef __OCPN__ANDROID__
  m_notebook->GetPage(1)->Layout();
  if (auto* scroll =
          dynamic_cast<wxScrolledWindow*>(m_notebook->GetPage(1)))
    { scroll->FitInside(); scroll->Scroll(0, 0); }
#endif
  RefreshSkyPlot();
}

void PlannerDialog::RebuildBodyList() {
  m_bodies->DeleteAllItems();
#ifdef __OCPN__ANDROID__
  const QString selectedName = m_androidBodies->currentItem()
      ? m_androidBodies->currentItem()->data(Qt::UserRole + 1).toString()
      : QString();
  m_androidBodies->blockSignals(true);
  m_androidBodies->clear();
#endif
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
                             ? a.lunarConstraints : a.visibilityScore;
                    bv = m_planningMode->GetSelection() == 2
                             ? b.lunarConstraints : b.visibilityScore;
                    break;
                }
                comparison = av < bv ? -1 : av > bv ? 1 : 0;
                if (comparison == 0)
                  comparison = a.state.body.CmpNoCase(b.state.body);
              }
              return ascending ? comparison < 0 : comparison > 0;
            });
  const ObserverMotion motion = ReadMotion(false);
#ifdef __OCPN__ANDROID__
  const auto& sun = m_androidResults.sun;
#else
  const BodyState sun = CelestialEphemeris::Evaluate(
      "Sun", motion.referenceUtc, motion.latitude, motion.longitude);
#endif
  const bool daylight = sun.valid && sun.geometricAltitude >= 0.0;
  for (const size_t index : order) {
    const RankedBody& body = m_rankedBodies[index];
#ifndef __OCPN__ANDROID__
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
    m_bodies->SetItem(row, 6, m_planningMode->GetSelection() == 2
        ? wxString::Format("%d", body.lunarConstraints) : body.visibilityGuidance);
    m_bodies->SetItem(row, 7,
                      wxString::Format("%+.1f%c", body.eclipticLatitude,
                                       0x00b0));
    wxString reason = m_planningMode->GetSelection() == 2 &&
                              body.state.body != "Moon"
                          ? body.lunarReason : body.reason;
#else
    wxString reason = m_planningMode->GetSelection() == 2 &&
                              body.state.body != "Moon"
                          ? body.lunarReason : body.reason;
#endif
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
#ifndef __OCPN__ANDROID__
    m_bodies->SetItem(row, 8, reason);
    if (daylight && body.state.isStar)
      m_bodies->SetItemTextColour(row, wxColour(150, 150, 150));
#endif
#ifdef __OCPN__ANDROID__
    const wxString text = body.state.body + "\n" +
        _("Hc: ") + FormatNavigationAngle(body.state.geometricAltitude) + "\n" +
        wxString::Format(_("Azimuth true: %.2f degrees\n"), body.state.azimuthTrue) +
        _("GHA: ") + FormatNavigationAngle(body.state.gha) + "\n" +
        _("Declination: ") + FormatNavigationAngle(body.state.declination,
            NavigationAngleKind::Latitude) + "\n" +
        wxString::Format(_("Magnitude: %.1f; %s: %.0f\nEcliptic latitude: %+.1f degrees\n"),
            body.state.visualMagnitude,
            m_planningMode->GetSelection() == 2 ? _("cautions") : _("score"),
            m_planningMode->GetSelection() == 2
                ? double(body.lunarConstraints) : body.score,
            body.eclipticLatitude) +
        _("Visibility: ") + body.visibilityGuidance + "\n" +
        _("Handling: ") + body.handlingGuidance + "\n" + reason;
    auto* card = new QListWidgetItem(QString::fromUtf8(text.utf8_str()),
                                     m_androidBodies);
    card->setData(Qt::UserRole, static_cast<int>(index));
    const QString name = QString::fromUtf8(body.state.body.utf8_str());
    card->setData(Qt::UserRole + 1, name);
    if (!selectedName.isEmpty() && selectedName == name)
      m_androidBodies->setCurrentItem(card);
#endif
  }
#ifdef __OCPN__ANDROID__
  m_androidBodies->blockSignals(false);
  UpdateAndroidBodySelection();
  celestial_android::LayoutScrolls(this);
#endif
}

#ifdef __OCPN__ANDROID__
void PlannerDialog::StartAndroidCalculation(const ObserverMotion& motion) {
  if (!m_androidWorker) {
    ClearCalculatedResults(_("Cannot start planning worker. Close and reopen Planner to retry."));
    return;
  }
  ClearCalculatedResults(_("Calculating the current context..."));
  m_androidGeneration = m_androidWorker->Submit(motion, m_eyeHeight->GetValue());
  m_androidCancel->Enable(true);
  m_androidProgress->SetLabel(_("Calculating horizon events..."));
}

void PlannerDialog::CancelAndroidCalculation() {
  m_androidRefresh->stop();
  if (m_androidWorker) m_androidWorker->Cancel();
  ClearCalculatedResults(_("Calculation cancelled. Use Calculate / refresh in Context to try again."));
  m_androidProgress->SetLabel(_("Calculation cancelled"));
  m_androidCancel->Enable(false);
}

void PlannerDialog::PollAndroidCalculation() {
  if (!m_androidWorker || !m_androidCancel->IsEnabled()) return;
  celestial_android::PlannerResults result;
  if (!m_androidWorker->Take(m_androidGeneration, &result)) {
    const wxString stages[] = {_("Preparing calculations..."),
      _("Calculating horizon events..."), _("Finding Moon phases..."),
      _("Ranking celestial bodies..."), _("Generating hourly almanac...")};
    m_androidProgress->SetLabel(stages[std::max(0, std::min(4, m_androidWorker->Stage()))]);
    return;
  }
  m_androidCancel->Enable(false);
  if (!result.error.empty()) {
    ClearCalculatedResults(_("Planning failed: ") + result.error);
    m_androidProgress->SetLabel(_("Planning failed"));
    return;
  }
  m_androidResults = std::move(result);
  m_androidReady = true;
  m_androidSolve->Enable(true);
  RefreshEvents(); RefreshBodies(); RefreshAlmanac(); RefreshSpecial();
  m_androidProgress->SetLabel(_("Results ready"));
  m_status->SetLabel(_("Planning context updated; calculations remain fully offline."));
  qDebug() << "Celnav planner worker milliseconds:" << m_androidResults.elapsedMs;
  celestial_android::LayoutScrolls(this);
}

void PlannerDialog::UpdateAndroidBodySelection() {
  auto* item = m_androidBodies->currentItem();
  m_androidCreateSight->Enable(item != nullptr);
  const wxString selected = item
      ? wxString::FromUTF8(item->data(Qt::UserRole + 1).toString().toUtf8().constData())
      : wxString();
  m_skyPlot->SetSelectedBody(selected);
  m_equatorialPlot->SetSelectedBody(selected);
  m_androidSelectedBody->SetLabel(item
      ? _("Selected: ") + selected
      : _("Select a body card"));
  RefreshSkyPlot();
  celestial_android::LayoutScrolls(this);
}
#endif

void PlannerDialog::RefreshSkyPlot() {
  const ObserverMotion motion = ReadMotion(false);
  if (!motion.referenceUtc.IsValid()) return;
  const double magnitude =
      static_cast<double>(m_plotMagnitude->GetSelection() + 1);
  const double minimumAltitude = m_plotBelowHorizon->GetValue() ? -90.0 : 0.0;
#ifdef __OCPN__ANDROID__
  if (!m_androidReady) return;
  std::vector<RankedBody> plotBodies;
  for (const auto& body : m_androidResults.allBodies)
    if (body.state.geometricAltitude >= minimumAltitude &&
        (!body.state.isStar && !body.state.isPlanet ||
         body.state.visualMagnitude <= magnitude || body.state.body == m_skyPlot->SelectedBody())) plotBodies.push_back(body);
  const auto& sun = m_androidResults.sun;
#else
  std::vector<RankedBody> plotBodies = SightRanker::VisibleBodies(
      motion.referenceUtc, motion.latitude, motion.longitude, minimumAltitude,
      90.0, magnitude);
  const BodyState sun = CelestialEphemeris::Evaluate(
      "Sun", motion.referenceUtc, motion.latitude, motion.longitude);
#endif
  const bool daylight = sun.valid && sun.geometricAltitude >= 0.0;
#ifndef __OCPN__ANDROID__
  const wxString selectedBody = m_skyPlot->SelectedBody();
  if (!selectedBody.empty() && std::none_of(plotBodies.begin(), plotBodies.end(),
          [&selectedBody](const RankedBody& body) { return body.state.body == selectedBody; })) {
    const auto state = CelestialEphemeris::Evaluate(selectedBody, motion.referenceUtc,
                                                   motion.latitude, motion.longitude);
    if (state.valid && state.geometricAltitude >= minimumAltitude)
      plotBodies.push_back(SightRanker::AssessBody(state, sun));
  }
#endif
  m_skyPlot->SetBodies(plotBodies, daylight);
  m_equatorialPlot->SetBodies(plotBodies, daylight);
#ifdef __OCPN__ANDROID__
  const auto& ecliptic = m_androidResults.ecliptic;
  const auto& moonPath = m_androidResults.moonPaths[
      std::max(0, std::min(2, m_moonSpan->GetSelection()))];
#else
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
#endif
  m_skyPlot->SetOverlays(ecliptic, {}, m_showEcliptic->GetValue(), false);
  m_skyPlot->SetLabelDensity(m_plotLabels->GetSelection());
  m_equatorialPlot->SetLabelDensity(m_plotLabels->GetSelection());
  m_moonSpan->Enable(m_plotNotebook->GetSelection() == 1 && m_showMoonPath->GetValue());
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
#ifdef __OCPN__ANDROID__
  m_androidAlmanac->clear();
#endif
  const ObserverMotion motion = ReadMotion(false);
#ifdef __OCPN__ANDROID__
  if (!m_androidReady) return;
  m_almanacRows = m_androidResults.almanac;
#else
  m_almanacRows = BuildAlmanac(
      motion.referenceUtc, 24,
      {"Sun", "Moon", "Venus", "Mars", "Jupiter", "Saturn", "Polaris"}, motion);
#endif
  for (const auto& item : m_almanacRows) {
#ifndef __OCPN__ANDROID__
    const long row = m_almanac->InsertItem(
        m_almanac->GetItemCount(),
        UtcDateTime::FormatInstant(item.utc, "%Y-%m-%d %H:%M"));
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
#endif
#ifdef __OCPN__ANDROID__
    const wxString text = item.body + "\n" + _("UTC: ") +
        UtcDateTime::FormatInstant(item.utc, "%Y-%m-%d %H:%M:%S.%l") + "\n" +
        _("GHA: ") + FormatNavigationAngle(item.gha) + "\n" +
        _("SHA: ") + FormatNavigationAngle(item.sha) + "\n" +
        _("GHA Aries: ") + FormatNavigationAngle(item.ghaAries) + "\n" +
        _("LHA Aries: ") + FormatNavigationAngle(item.lhaAries) + "\n" +
        _("Declination: ") + FormatNavigationAngle(item.declination,
            NavigationAngleKind::Latitude) + "\n" +
        _("Hc: ") + FormatNavigationAngle(item.altitude) + "\n" +
        wxString::Format(_("Azimuth true: %.2f degrees"), item.azimuth);
    new QListWidgetItem(QString::fromUtf8(text.utf8_str()), m_androidAlmanac);
#endif
  }
#ifdef __OCPN__ANDROID__
  m_androidExport->Enable(!m_almanacRows.empty());
  m_androidAlmanacStatus->SetLabel(wxString::Format(
      _("%zu rows. Export CSV retains all shared table values."),
      m_almanacRows.size()));
  celestial_android::LayoutScrolls(this);
#endif
}

void PlannerDialog::RefreshSpecial() {
#ifdef __OCPN__ANDROID__
  if (!m_androidReady) return;
#endif
  const ObserverMotion motion = ReadMotion(false);
  if (m_specialBody->GetSelection() == 1) {
#ifdef __OCPN__ANDROID__
    const auto& polaris = m_androidResults.polaris;
#else
    const BodyState polaris = CelestialEphemeris::Evaluate(
        "Polaris", motion.referenceUtc, motion.latitude, motion.longitude);
#endif
    if (!polaris.valid || polaris.geometricAltitude < 0.0) {
      m_specialSummary->SetLabel(
          _("Polaris is below the horizon and is not observable from the "
            "selected position and time."));
#ifdef __OCPN__ANDROID__
      celestial_android::LayoutScrolls(this);
#endif
      return;
    }
    m_specialSummary->SetLabel(wxString::Format(
        _("Polaris is observable: predicted Hc %s, Zn true %.2f%c.\n"
          "Enter corrected Ho above to estimate latitude."),
        FormatNavigationAngle(polaris.geometricAltitude).c_str(),
        polaris.azimuthTrue, 0x00b0));
#ifdef __OCPN__ANDROID__
    celestial_android::LayoutScrolls(this);
#endif
    return;
  }
#ifdef __OCPN__ANDROID__
  const auto& events = m_androidResults.noonEvents;
#else
  const DailyEventsResult events =
      HorizonEventCalculator::Calculate(motion.referenceUtc, motion);
#endif
  for (const auto& event : events.events) {
    if (event.kind == HorizonEventKind::UpperTransit) {
      m_specialSummary->SetLabel(wxString::Format(
          _("Local apparent noon (solar LHA 0%c): %s UTC at observer "
            "position %s, %s.\n"
            "Enter corrected Ho above to estimate latitude; longitude comes "
            "primarily from noon timing."),
          0x00b0,
          UtcDateTime::FormatInstant(event.utc, "%Y-%m-%d %H:%M:%S").c_str(),
          FormatNavigationAngle(event.observerLatitude,
                                NavigationAngleKind::Latitude, true)
              .c_str(),
          FormatNavigationAngle(event.observerLongitude,
                                NavigationAngleKind::Longitude, true)
              .c_str()));
      break;
    }
  }
#ifdef __OCPN__ANDROID__
  celestial_android::LayoutScrolls(this);
#endif
}

void PlannerDialog::ExportAlmanac(wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
  if (!m_androidReady || !ReadMotion(false).referenceUtc.IsValid() || m_almanacRows.empty()) {
    ClearCalculatedResults(_("Enter a valid Context before exporting."));
    return;
  }
#endif
  CelestialFileDialog dialog(this, _("Export offline celestial almanac"),
                      wxEmptyString, "celestial-almanac.csv",
                      _("CSV files (*.csv)|*.csv"),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK) return;
  wxFFile file(dialog.GetPath(), "wb");
#ifdef __OCPN__ANDROID__
  const wxString csv = AndroidAlmanacCsv(m_almanacRows);
  if (!file.IsOpened() || !file.Write(csv))
#else
  if (!file.IsOpened() || !file.Write(AlmanacToCsv(m_almanacRows)))
#endif
    CelestialMessageBox(_("Could not write the selected file."), _("Export failed"),
                 wxOK | wxICON_ERROR, this);
}

void PlannerDialog::ExportBodyTable(wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
  if (!m_androidReady || !ReadMotion(false).referenceUtc.IsValid()) return;
#endif
  CelestialFileDialog dialog(this, _("Export sight planning table"), wxEmptyString,
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
                 "Representative magnitude,Planning preference,Lunar cautions,Ecliptic latitude "
                 "deg,LD deg,Signed LD rate arcmin/h,Seconds per 0.1 arcmin,"
                 "Moon illumination %,Visibility,Handling,Explanation\n";
  #ifdef __OCPN__ANDROID__
  const long rowCount = m_androidBodies->count();
  #else
  const long rowCount = m_bodies->GetItemCount();
  #endif
  for (long row = 0; row < rowCount; ++row) {
  #ifdef __OCPN__ANDROID__
    const long index = m_androidBodies->item(row)->data(Qt::UserRole).toInt();
  #else
    const long index = m_bodies->GetItemData(row);
  #endif
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
           "," + field(body.visibilityGuidance) + "," + field(body.handlingGuidance) +
           "," + field(m_planningMode->GetSelection() == 2
                            ? body.lunarReason : body.reason) + "\n";
  }
  wxFFile file(dialog.GetPath(), "wb");
  if (!file.IsOpened() || !file.Write(csv))
    CelestialMessageBox(_("Could not write the selected file."), _("Export failed"),
                        wxOK | wxICON_ERROR, this);
}

void PlannerDialog::FindLunarWindows(wxCommandEvent&) {
#ifdef __OCPN__ANDROID__
  const long index = m_androidBodies->currentItem()
      ? m_androidBodies->currentItem()->data(Qt::UserRole).toInt() : -1;
#else
  const long selected = m_bodies->GetNextItem(
      -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  const long index = selected < 0 ? -1 : m_bodies->GetItemData(selected);
#endif
  if (index < 0 || size_t(index) >= m_rankedBodies.size() ||
      m_rankedBodies[index].state.body == "Moon") {
    CelestialMessageBox(_("Select a companion body in All bodies first."),
                        _("Lunar observing windows"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  const auto motion = ReadMotion(true);
  if (!motion.referenceUtc.IsValid()) return;
  const auto body = m_rankedBodies[index].state.body;
#ifdef __OCPN__ANDROID__
  std::vector<LunarObservingWindow> windows;
  wxString error;
  if (!celestial_android::RunJob(this, _("Finding lunar observing windows"),
      [&](celestial_android::JobState& job) {
        celestial_android::PlannerCancellationScope scope(job.cancel);
        try {
          windows = PlannerRecommendations::ObservingWindows(body, motion);
        } catch (const celestial_android::PlannerCancelled&) {
          throw celestial_android::JobCancelled();
        }
      }, &error)) {
    if (!error.empty()) CelestialMessageBox(error, _("Lunar observing windows"),
                                             wxOK | wxICON_ERROR, this);
    return;
  }
#else
  const auto windows = [&]() {
    wxBusyCursor busy;
    return PlannerRecommendations::ObservingWindows(body, motion);
  }();
#endif
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
#ifdef __OCPN__ANDROID__
  if (!m_androidReady || !ReadMotion(false).referenceUtc.IsValid()) {
    ClearCalculatedResults(_("Enter a valid Context before creating a sight."));
    return;
  }
  const long bodyIndex = m_androidBodies->currentItem()
      ? m_androidBodies->currentItem()->data(Qt::UserRole).toInt() : -1;
#else
  const long selected =
      m_bodies->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  const long bodyIndex = selected < 0 ? -1 : m_bodies->GetItemData(selected);
#endif
  if (bodyIndex < 0 ||
      static_cast<size_t>(bodyIndex) >= m_rankedBodies.size()) {
    CelestialMessageBox(_("Select a body first."), _("Sight Planner"),
                 wxOK | wxICON_INFORMATION, this);
    return;
  }
  m_parent->CreatePlannedSight(m_rankedBodies[bodyIndex].state.body,
                               ReadUtc(false), m_latitude->GetAngleOr(0.0),
                               m_longitude->GetAngleOr(0.0));
}

void PlannerDialog::SolveSpecialLatitude(wxCommandEvent&) {
  ObserverMotion motion = ReadMotion(false);
#ifdef __OCPN__ANDROID__
  if (!m_androidReady || !motion.referenceUtc.IsValid()) {
    ClearCalculatedResults(_("Enter a valid Context before solving latitude."));
    return;
  }
#endif
  wxString body = m_specialBody->GetSelection() == 0 ? "Sun" : "Polaris";
  wxDateTime time = motion.referenceUtc;
  if (body == "Sun") {
#ifdef __OCPN__ANDROID__
    const auto& events = m_androidResults.noonEvents.events;
#else
    const auto events = HorizonEventCalculator::Calculate(time, motion).events;
#endif
    for (const auto& event : events)
      if (event.kind == HorizonEventKind::UpperTransit) time = event.utc;
  } else {
    const BodyState planned = CelestialEphemeris::Evaluate(
        body, time, motion.latitude, motion.longitude);
    if (!planned.valid || planned.geometricAltitude < 0.0) {
      CelestialMessageBox(
          _("Polaris is below the horizon and is not observable from the "
            "selected position and time."),
          _("Polaris not observable"), wxOK | wxICON_INFORMATION, this);
      return;
    }
  }
  double observedAltitude = 0.0;
  if (!m_specialAltitude->GetAngle(&observedAltitude)) {
    CelestialMessageBox(_("Enter a valid corrected observed altitude."),
                 _("Invalid altitude"), wxOK | wxICON_ERROR, this);
    return;
  }
  m_specialAltitude->Normalize();
  const double latitude = SolveLatitudeFromAltitude(
      body, time, motion.longitude, observedAltitude, motion.latitude);
  if (!std::isfinite(latitude)) {
    m_specialSummary->SetLabel(_("No converged latitude solution. Check Ho, time, longitude and approximate latitude."));
#ifdef __OCPN__ANDROID__
    celestial_android::LayoutScrolls(this);
#endif
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
      UtcDateTime::FormatInstant(time, "%Y-%m-%d %H:%M:%S").c_str(),
      state.azimuthTrue, 0x00b0));
#ifdef __OCPN__ANDROID__
  celestial_android::LayoutScrolls(this);
#endif
}
