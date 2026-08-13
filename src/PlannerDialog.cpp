#include "PlannerDialog.h"

#include "CelestialNavigationDialog.h"
#include "Sight.h"
#include "UtcDateTime.h"
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
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timectrl.h>

#include <algorithm>
#include <cmath>

namespace {
void AddColumn(wxListCtrl* list, int column, const wxString& title,
               int width = wxLIST_AUTOSIZE_USEHEADER) {
  list->InsertColumn(column, title);
  list->SetColumnWidth(column, width);
}

class SkyPlotPanelImpl : public wxPanel {
public:
  explicit SkyPlotPanelImpl(wxWindow* parent)
      : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(260, 260)) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &SkyPlotPanelImpl::OnPaint, this);
  }
  void SetBodies(const std::vector<RankedBody>& bodies) {
    m_bodies = bodies;
    Refresh();
  }

private:
  void OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    const wxSize size = GetClientSize();
    const wxPoint center(size.x / 2, size.y / 2);
    const int radius = std::max(10, std::min(size.x, size.y) / 2 - 22);
    dc.SetPen(wxPen(wxColour(100, 100, 100)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawCircle(center, radius);
    dc.DrawCircle(center, radius / 2);
    dc.DrawLine(center.x, center.y - radius, center.x,
                center.y + radius);
    dc.DrawLine(center.x - radius, center.y, center.x + radius, center.y);
    dc.DrawText("N", center.x - 5, center.y - radius - 20);
    dc.DrawText("E", center.x + radius + 4, center.y - 8);
    dc.DrawText("S", center.x - 5, center.y + radius + 2);
    dc.DrawText("W", center.x - radius - 18, center.y - 8);
    std::vector<wxRect> labels;
    size_t rank = 0;
    for (const auto& body : m_bodies) {
      const double radial =
          radius * (90.0 - body.state.geometricAltitude) / 90.0;
      const double angle = body.state.azimuthTrue * 3.141592653589793 / 180.0;
      const wxPoint p(center.x + static_cast<int>(radial * std::sin(angle)),
                      center.y - static_cast<int>(radial * std::cos(angle)));
      dc.SetBrush(wxBrush(body.state.body == "Sun" ? wxColour(240, 180, 0)
                                                    : wxColour(40, 100, 210)));
      dc.DrawCircle(p, 3);
      if (rank++ >= 14) continue;
      const wxSize extent = dc.GetTextExtent(body.state.body);
      int labelX = p.x + 5;
      if (labelX + extent.x > size.x - 3) labelX = p.x - extent.x - 5;
      int labelY = std::max(2, std::min(p.y - extent.y / 2,
                                       size.y - extent.y - 2));
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
        dc.DrawText(body.state.body, label.GetPosition());
        labels.push_back(padded);
      }
    }
  }
  std::vector<RankedBody> m_bodies;
};
}  // namespace

class SkyPlotPanel : public SkyPlotPanelImpl {
public:
  explicit SkyPlotPanel(wxWindow* parent) : SkyPlotPanelImpl(parent) {}
  using SkyPlotPanelImpl::SetBodies;
};

void PlannerDialog::SelectPageForIntegration(unsigned page) {
  if (m_notebook && page < m_notebook->GetPageCount()) {
    m_notebook->SetSelection(page);
    Layout();
    Refresh();
    Update();
  }
}

PlannerDialog::PlannerDialog(CelestialNavigationDialog* parent)
    : wxDialog(parent, wxID_ANY, _("Sun, Moon and Sight Planner"),
               wxDefaultPosition, wxSize(1120, 720),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_parent(parent) {
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
  m_positionSource->SetSelection(1);
  grid->Add(m_positionSource, 0, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Latitude")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_latitude = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                    wxSize(125, -1), wxSP_ARROW_KEYS, -90, 90,
                                    0, 0.0001);
  m_latitude->SetDigits(4);
  grid->Add(m_latitude);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Longitude")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_longitude = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                     wxSize(125, -1), wxSP_ARROW_KEYS, -180,
                                     180, 0, 0.0001);
  m_longitude->SetDigits(4);
  grid->Add(m_longitude);

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
  m_utcDate = new wxDatePickerCtrl(this, wxID_ANY);
  grid->Add(m_utcDate, 0, wxEXPAND);
  m_timeLabel = new wxStaticText(this, wxID_ANY, _("Time (UTC)"));
  grid->Add(m_timeLabel, 0, wxALIGN_CENTER_VERTICAL);
  m_utcTime = new wxTimePickerCtrl(this, wxID_ANY);
  grid->Add(m_utcTime, 0, wxEXPAND);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Enter time as")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_inputTimeBasis = new wxChoice(this, wxID_ANY);
  m_inputTimeBasis->Append(_("UTC"));
  m_inputTimeBasis->Append(_("Computer local time"));
  m_inputTimeBasis->SetSelection(0);
  grid->Add(m_inputTimeBasis, 0, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, _("Display event times as")), 0,
            wxALIGN_CENTER_VERTICAL);
  wxBoxSizer* displaySizer = new wxBoxSizer(wxHORIZONTAL);
  m_displayTime = new wxChoice(this, wxID_ANY);
  m_displayTime->Append(_("UTC"));
  m_displayTime->Append(_("Computer local"));
  m_displayTime->Append(_("Local mean time"));
  m_displayTime->Append(_("Fixed offset"));
  m_displayTime->SetSelection(0);
  displaySizer->Add(m_displayTime, 1, wxRIGHT, 4);
  m_fixedOffset = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                       wxSize(75, -1), wxSP_ARROW_KEYS, -12,
                                       14, 0, 0.5);
  m_fixedOffset->SetDigits(1);
  displaySizer->Add(m_fixedOffset);
  grid->Add(displaySizer, 1, wxEXPAND);
  grid->AddSpacer(1);
  grid->AddSpacer(1);

  wxBoxSizer* motion = new wxBoxSizer(wxHORIZONTAL);
  m_moving = new wxCheckBox(this, wxID_ANY, _("Time-tagged moving observer"));
  motion->Add(m_moving, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
  motion->Add(new wxStaticText(this, wxID_ANY,
                               _("Reference time is the entry above")),
              0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
  motion->Add(new wxStaticText(this, wxID_ANY, _("COG (true)")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_course = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                  wxSize(100, -1), wxSP_ARROW_KEYS, 0, 359.9, 0,
                                  0.1);
  m_course->SetDigits(1);
  motion->Add(m_course, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
  motion->Add(new wxStaticText(this, wxID_ANY, _("SOG (kn)")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  m_speed = new wxSpinCtrlDouble(this, wxID_ANY, "0", wxDefaultPosition,
                                 wxSize(100, -1), wxSP_ARROW_KEYS, 0, 100, 0,
                                 0.1);
  m_speed->SetDigits(1);
  motion->Add(m_speed, 0, wxALIGN_CENTER_VERTICAL);

  grid->Add(new wxStaticText(this, wxID_ANY, _("Eye height (m)")), 0,
            wxALIGN_CENTER_VERTICAL);
  m_eyeHeight = new wxSpinCtrlDouble(this, wxID_ANY, "2", wxDefaultPosition,
                                     wxDefaultSize, wxSP_ARROW_KEYS, 0, 100,
                                     2, 0.1);
  m_eyeHeight->SetDigits(1);
  grid->Add(m_eyeHeight);
  wxButton* calculate = new wxButton(this, wxID_ANY, _("Calculate / refresh"));
  grid->Add(calculate, 0, wxEXPAND);
  grid->AddSpacer(1);
  grid->AddSpacer(1);
  grid->AddSpacer(1);
  context->Add(grid, 1, wxALL | wxEXPAND, 6);
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
  AddColumn(m_events, 4, _("Observer position"), 190);
  eventsSizer->Add(m_events, 1, wxALL | wxEXPAND, 5);
  m_moonSummary = new wxStaticText(eventsPage, wxID_ANY, wxEmptyString);
  eventsSizer->Add(m_moonSummary, 0, wxALL | wxEXPAND, 6);
  eventsPage->SetSizer(eventsSizer);
  m_notebook->AddPage(eventsPage, _("Events"), true);

  wxPanel* bodiesPage = new wxPanel(m_notebook);
  wxBoxSizer* bodiesRoot = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer* bodiesLeft = new wxBoxSizer(wxVERTICAL);
  m_bodies = new wxListCtrl(bodiesPage, wxID_ANY, wxDefaultPosition,
                            wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL |
                                               wxLC_HRULES);
  AddColumn(m_bodies, 0, _("Body"), 115);
  AddColumn(m_bodies, 1, _("Hc"), 75);
  AddColumn(m_bodies, 2, _("Zn true"), 80);
  AddColumn(m_bodies, 3, _("GHA"), 75);
  AddColumn(m_bodies, 4, _("Dec"), 75);
  AddColumn(m_bodies, 5, _("Mag"), 55);
  AddColumn(m_bodies, 6, _("Score"), 60);
  AddColumn(m_bodies, 7, _("Why"), 260);
  bodiesLeft->Add(m_bodies, 1, wxALL | wxEXPAND, 5);
  wxButton* createSight =
      new wxButton(bodiesPage, wxID_ANY, _("Create selected sight..."));
  bodiesLeft->Add(createSight, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  m_combinations = new wxListCtrl(bodiesPage, wxID_ANY, wxDefaultPosition,
                                  wxSize(-1, 115), wxLC_REPORT | wxLC_HRULES);
  AddColumn(m_combinations, 0, _("Recommended pair / triad"), 300);
  AddColumn(m_combinations, 1, _("Score"), 70);
  AddColumn(m_combinations, 2, _("Geometry"), 270);
  bodiesLeft->Add(m_combinations, 0, wxALL | wxEXPAND, 5);
  bodiesRoot->Add(bodiesLeft, 1, wxEXPAND);
  m_skyPlot = new SkyPlotPanel(bodiesPage);
  bodiesRoot->Add(m_skyPlot, 0, wxALL | wxEXPAND, 8);
  bodiesPage->SetSizer(bodiesRoot);
  m_notebook->AddPage(bodiesPage, _("Bodies && Best Sights"), false);

  wxPanel* almanacPage = new wxPanel(m_notebook);
  wxBoxSizer* almanacSizer = new wxBoxSizer(wxVERTICAL);
  wxStaticText* almanacNote = new wxStaticText(
      almanacPage, wxID_ANY,
      _("Hourly Sun, Moon, planet and Polaris almanac (24 hours from the selected UTC)."));
  almanacSizer->Add(almanacNote, 0, wxALL, 6);
  m_almanac = new wxListCtrl(almanacPage, wxID_ANY, wxDefaultPosition,
                             wxDefaultSize, wxLC_REPORT | wxLC_HRULES);
  AddColumn(m_almanac, 0, _("UTC"), 155);
  AddColumn(m_almanac, 1, _("Body"), 90);
  AddColumn(m_almanac, 2, _("GHA"), 90);
  AddColumn(m_almanac, 3, _("SHA"), 90);
  AddColumn(m_almanac, 4, _("Declination"), 100);
  AddColumn(m_almanac, 5, _("Hc"), 90);
  AddColumn(m_almanac, 6, _("Zn true"), 90);
  almanacSizer->Add(m_almanac, 1, wxALL | wxEXPAND, 5);
  wxButton* exportButton = new wxButton(almanacPage, wxID_ANY, _("Export CSV..."));
  almanacSizer->Add(exportButton, 0, wxALL, 5);
  almanacPage->SetSizer(almanacSizer);
  m_notebook->AddPage(almanacPage, _("Almanac"), false);

  wxPanel* specialPage = new wxPanel(m_notebook);
  wxBoxSizer* specialSizer = new wxBoxSizer(wxVERTICAL);
  specialSizer->Add(new wxStaticText(
                        specialPage, wxID_ANY,
                        _("Noon and Polaris helpers use the same ephemeris and the planning position/time above.")),
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
  m_specialAltitude = new wxSpinCtrlDouble(
      specialPage, wxID_ANY, "45", wxDefaultPosition, wxDefaultSize,
      wxSP_ARROW_KEYS, -10, 90, 45, 0.01);
  m_specialAltitude->SetDigits(2);
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

  m_positionSource->Bind(wxEVT_CHOICE,
                         [this](wxCommandEvent&) { ApplyPositionSource(); });
  m_timeSource->Bind(wxEVT_CHOICE,
                     [this](wxCommandEvent&) { ApplyTimeSource(); });
  m_inputTimeBasis->Bind(wxEVT_CHOICE,
                         &PlannerDialog::ChangeInputTimeBasis, this);
  m_displayTime->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
    RefreshEvents();
  });
  m_fixedOffset->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) {
    RefreshEvents();
  });
  calculate->Bind(wxEVT_BUTTON, &PlannerDialog::RefreshAll, this);
  exportButton->Bind(wxEVT_BUTTON, &PlannerDialog::ExportAlmanac, this);
  createSight->Bind(wxEVT_BUTTON, &PlannerDialog::CreateSelectedSight, this);
  solve->Bind(wxEVT_BUTTON, &PlannerDialog::SolveSpecialLatitude, this);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); }, wxID_CLOSE);

  wxFileConfig* config = GetOCPNConfigObject();
  config->SetPath(_T("/PlugIns/CelestialNavigation/Planner"));
  long positionSource = 1, inputTimeBasis = 0, displayTime = 0;
  bool moving = false;
  double latitude = 0.0, longitude = 0.0, course = 0.0, speed = 0.0,
         eyeHeight = 2.0, fixedOffset = 0.0;
  config->Read(_T("PositionSource"), &positionSource, 1L);
  config->Read(_T("InputTimeBasis"), &inputTimeBasis, 0L);
  config->Read(_T("DisplayTime"), &displayTime, 0L);
  config->Read(_T("Latitude"), &latitude, 0.0);
  config->Read(_T("Longitude"), &longitude, 0.0);
  config->Read(_T("Moving"), &moving, false);
  config->Read(_T("CourseTrue"), &course, 0.0);
  config->Read(_T("SpeedKnots"), &speed, 0.0);
  config->Read(_T("EyeHeight"), &eyeHeight, 2.0);
  config->Read(_T("FixedOffset"), &fixedOffset, 0.0);
  m_positionSource->SetSelection(
      std::max(0L, std::min(positionSource, 4L)));
  m_inputTimeBasis->SetSelection(
      std::max(0L, std::min(inputTimeBasis, 1L)));
  m_lastInputTimeBasis = m_inputTimeBasis->GetSelection();
  UpdateInputTimeLabels();
  m_displayTime->SetSelection(std::max(0L, std::min(displayTime, 3L)));
  m_latitude->SetValue(latitude);
  m_longitude->SetValue(longitude);
  m_moving->SetValue(moving);
  m_course->SetValue(course);
  m_speed->SetValue(speed);
  m_eyeHeight->SetValue(eyeHeight);
  m_fixedOffset->SetValue(fixedOffset);
  SetUtcControls(wxDateTime::UNow());
  ApplyPositionSource();
  ApplyTimeSource();
  wxCommandEvent dummy;
  RefreshAll(dummy);
  CentreOnParent();
}

PlannerDialog::~PlannerDialog() {
  wxFileConfig* config = GetOCPNConfigObject();
  config->SetPath(_T("/PlugIns/CelestialNavigation/Planner"));
  config->Write(_T("PositionSource"),
                static_cast<long>(m_positionSource->GetSelection()));
  config->Write(_T("InputTimeBasis"),
                static_cast<long>(m_inputTimeBasis->GetSelection()));
  config->Write(_T("DisplayTime"),
                static_cast<long>(m_displayTime->GetSelection()));
  config->Write(_T("Latitude"), m_latitude->GetValue());
  config->Write(_T("Longitude"), m_longitude->GetValue());
  config->Write(_T("Moving"), m_moving->GetValue());
  config->Write(_T("CourseTrue"), m_course->GetValue());
  config->Write(_T("SpeedKnots"), m_speed->GetValue());
  config->Write(_T("EyeHeight"), m_eyeHeight->GetValue());
  config->Write(_T("FixedOffset"), m_fixedOffset->GetValue());
}

wxDateTime PlannerDialog::ReadUtc(bool showErrors) {
  const wxDateTime date = m_utcDate->GetValue();
  const wxDateTime time = m_utcTime->GetValue();
  if (!date.IsValid() || !time.IsValid()) {
    if (showErrors)
      wxMessageBox(_("Select a valid UTC date and time."), _("Invalid time"),
                   wxOK | wxICON_ERROR, this);
    return wxDateTime();
  }
  wxDateTime entered(date.GetDay(), date.GetMonth(), date.GetYear(),
                     time.GetHour(), time.GetMinute(), time.GetSecond());
  wxDateTime utc = m_inputTimeBasis->GetSelection() == 1
                       ? entered
                       : UtcDateTime::ToInstant(entered);
  if (utc.GetYear() < 1900 || utc.GetYear() > 2100) {
    if (showErrors)
      wxMessageBox(_("The ordinary offline planner is supported from 1900 through 2100."),
                   _("Time outside supported range"), wxOK | wxICON_ERROR,
                   this);
    return wxDateTime();
  }
  return utc;
}

void PlannerDialog::SetUtcControls(const wxDateTime& utc) {
  if (!utc.IsValid()) return;
  wxDateTime value = m_inputTimeBasis->GetSelection() == 1
                         ? utc
                         : UtcDateTime::CopyFields(utc.ToUTC());
  m_utcDate->SetValue(value);
  m_utcTime->SetValue(value);
}

void PlannerDialog::ChangeInputTimeBasis(wxCommandEvent&) {
  const int next = m_inputTimeBasis->GetSelection();
  m_inputTimeBasis->SetSelection(m_lastInputTimeBasis);
  const wxDateTime utc = ReadUtc(false);
  m_inputTimeBasis->SetSelection(next);
  m_lastInputTimeBasis = next;
  UpdateInputTimeLabels();
  SetUtcControls(utc);
  wxCommandEvent refresh;
  RefreshAll(refresh);
}

void PlannerDialog::UpdateInputTimeLabels() {
  const bool local = m_inputTimeBasis->GetSelection() == 1;
  m_dateLabel->SetLabel(local ? _("Date (local)") : _("Date (UTC)"));
  m_timeLabel->SetLabel(local ? _("Time (local)") : _("Time (UTC)"));
}

ObserverMotion PlannerDialog::ReadMotion(bool showErrors) {
  ObserverMotion motion;
  motion.referenceUtc = ReadUtc(showErrors);
  motion.latitude = m_latitude->GetValue();
  motion.longitude = m_longitude->GetValue();
  motion.courseTrue = m_course->GetValue();
  motion.speedKnots = m_speed->GetValue();
  motion.moving = m_moving->GetValue();
  return motion;
}

void PlannerDialog::ApplyPositionSource() {
  double lat = m_latitude->GetValue(), lon = m_longitude->GetValue();
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
  }
  if (available) {
    m_latitude->SetValue(lat);
    m_longitude->SetValue(lon);
    m_status->SetLabel(_("Position source applied; calculations remain fully offline."));
  } else {
    m_positionSource->SetSelection(0);
    m_status->SetLabel(_("Requested position is unavailable; retained manual position."));
  }
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
    offset = static_cast<long>(std::lround(m_longitude->GetValue() * 240.0));
    suffix = "LMT";
  } else if (m_displayTime->GetSelection() == 3) {
    offset = static_cast<long>(std::lround(m_fixedOffset->GetValue() * 3600.0));
    suffix = wxString::Format("UTC%+.1f", m_fixedOffset->GetValue());
  }
  return (utc + wxTimeSpan::Seconds(offset))
             .Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC) +
         " " + suffix;
}

void PlannerDialog::RefreshAll(wxCommandEvent&) {
  ApplyPositionSource();
  ApplyTimeSource();
  if (!ReadUtc(true).IsValid()) return;
  RefreshEvents();
  RefreshBodies();
  RefreshAlmanac();
  RefreshSpecial();
}

void PlannerDialog::RefreshEvents() {
  m_events->DeleteAllItems();
  const ObserverMotion motion = ReadMotion(false);
  const DailyEventsResult table = HorizonEventCalculator::Calculate(
      motion.referenceUtc, motion, m_eyeHeight->GetValue());
  for (const auto& event : table.events) {
    const long row = m_events->InsertItem(
        m_events->GetItemCount(), HorizonEventCalculator::Name(event.kind));
    m_events->SetItem(
        row, 1, event.utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC));
    m_events->SetItem(row, 2, DisplayTime(event.utc));
    m_events->SetItem(row, 3,
                      wxString::Format("%.1f%c", event.bearingTrue, 0x00b0));
    m_events->SetItem(row, 4,
                      wxString::Format("%.4f, %.4f", event.observerLatitude,
                                       event.observerLongitude));
  }
  for (const auto& phase : NextPrincipalMoonPhases(
           motion.referenceUtc, motion.latitude, motion.longitude)) {
    const long row = m_events->InsertItem(m_events->GetItemCount(),
                                          _("Next ") + phase.name);
    m_events->SetItem(
        row, 1, phase.utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC));
    m_events->SetItem(row, 2, DisplayTime(phase.utc));
    m_events->SetItem(row, 3, _("—"));
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
      _("Moon: %s, %.1f%% illuminated, %s, age %.1f days; altitude %.1f%c, azimuth %.1f%c, Sun separation %.1f%c.%s"),
      moon.phaseName.c_str(), moon.illuminatedFraction * 100.0,
      direction.c_str(), moon.ageDays, moonState.geometricAltitude, 0x00b0,
      moonState.azimuthTrue, 0x00b0, moon.elongationDegrees, 0x00b0,
      polar.c_str()));
}

void PlannerDialog::RefreshBodies() {
  m_bodies->DeleteAllItems();
  m_combinations->DeleteAllItems();
  const ObserverMotion motion = ReadMotion(false);
  m_rankedBodies = SightRanker::VisibleBodies(
      motion.referenceUtc, motion.latitude, motion.longitude);
  for (const auto& body : m_rankedBodies) {
    const long row = m_bodies->InsertItem(m_bodies->GetItemCount(),
                                         body.state.body);
    m_bodies->SetItem(row, 1,
                      wxString::Format("%.2f%c", body.state.geometricAltitude,
                                       0x00b0));
    m_bodies->SetItem(row, 2,
                      wxString::Format("%.1f%c", body.state.azimuthTrue,
                                       0x00b0));
    m_bodies->SetItem(row, 3, wxString::Format("%.2f", body.state.gha));
    m_bodies->SetItem(row, 4,
                      wxString::Format("%+.2f", body.state.declination));
    m_bodies->SetItem(row, 5,
                      wxString::Format("%.1f", body.state.visualMagnitude));
    m_bodies->SetItem(row, 6, wxString::Format("%.0f", body.score));
    m_bodies->SetItem(row, 7, body.reason);
  }
  auto pairs = SightRanker::BestCombinations(m_rankedBodies, 2, 5);
  auto triads = SightRanker::BestCombinations(m_rankedBodies, 3, 5);
  pairs.insert(pairs.end(), triads.begin(), triads.end());
  for (const auto& combination : pairs) {
    wxString names;
    for (const auto& body : combination.bodies) {
      if (!names.empty()) names += " / ";
      names += body.state.body;
    }
    const long row = m_combinations->InsertItem(m_combinations->GetItemCount(), names);
    m_combinations->SetItem(row, 1, wxString::Format("%.0f", combination.score));
    m_combinations->SetItem(row, 2, combination.reason);
  }
  m_skyPlot->SetBodies(m_rankedBodies);
}

void PlannerDialog::RefreshAlmanac() {
  m_almanac->DeleteAllItems();
  const ObserverMotion motion = ReadMotion(false);
  m_almanacRows = BuildAlmanac(motion.referenceUtc, 24,
                               {"Sun", "Moon", "Venus", "Mars", "Jupiter",
                                "Saturn", "Polaris"},
                               motion);
  for (const auto& item : m_almanacRows) {
    const long row = m_almanac->InsertItem(
        m_almanac->GetItemCount(),
        item.utc.Format("%m-%d %H:%M", wxDateTime::UTC));
    m_almanac->SetItem(row, 1, item.body);
    m_almanac->SetItem(row, 2, wxString::Format("%.3f", item.gha));
    m_almanac->SetItem(row, 3, wxString::Format("%.3f", item.sha));
    m_almanac->SetItem(row, 4, wxString::Format("%+.3f", item.declination));
    m_almanac->SetItem(row, 5, wxString::Format("%.2f", item.altitude));
    m_almanac->SetItem(row, 6, wxString::Format("%.1f", item.azimuth));
  }
}

void PlannerDialog::RefreshSpecial() {
  const ObserverMotion motion = ReadMotion(false);
  const DailyEventsResult events =
      HorizonEventCalculator::Calculate(motion.referenceUtc, motion);
  for (const auto& event : events.events) {
    if (event.kind == HorizonEventKind::UpperTransit) {
      m_specialSummary->SetLabel(wxString::Format(
          _("Local apparent noon: %s UTC at observer position %.4f, %.4f.\n"
            "Enter corrected Ho above to estimate latitude; longitude comes primarily from noon timing."),
          event.utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC).c_str(),
          event.observerLatitude, event.observerLongitude));
      break;
    }
  }
}

void PlannerDialog::ExportAlmanac(wxCommandEvent&) {
  wxFileDialog dialog(this, _("Export offline celestial almanac"), wxEmptyString,
                      "celestial-almanac.csv", _("CSV files (*.csv)|*.csv"),
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK) return;
  wxFFile file(dialog.GetPath(), "wb");
  if (!file.IsOpened() || !file.Write(AlmanacToCsv(m_almanacRows)))
    wxMessageBox(_("Could not write the selected file."), _("Export failed"),
                 wxOK | wxICON_ERROR, this);
}

void PlannerDialog::CreateSelectedSight(wxCommandEvent&) {
  const long selected = m_bodies->GetNextItem(-1, wxLIST_NEXT_ALL,
                                               wxLIST_STATE_SELECTED);
  if (selected < 0 || static_cast<size_t>(selected) >= m_rankedBodies.size()) {
    wxMessageBox(_("Select a body first."), _("Sight Planner"),
                 wxOK | wxICON_INFORMATION, this);
    return;
  }
  m_parent->CreatePlannedSight(m_rankedBodies[selected].state.body,
                               ReadUtc(false), m_latitude->GetValue(),
                               m_longitude->GetValue());
}

void PlannerDialog::SolveSpecialLatitude(wxCommandEvent&) {
  ObserverMotion motion = ReadMotion(false);
  wxString body = m_specialBody->GetSelection() == 0 ? "Sun" : "Polaris";
  wxDateTime time = motion.referenceUtc;
  if (body == "Sun") {
    const auto events = HorizonEventCalculator::Calculate(time, motion).events;
    for (const auto& event : events)
      if (event.kind == HorizonEventKind::UpperTransit) time = event.utc;
  }
  const double latitude = SolveLatitudeFromAltitude(
      body, time, motion.longitude, m_specialAltitude->GetValue(),
      motion.latitude);
  const BodyState state = CelestialEphemeris::Evaluate(
      body, time, latitude, motion.longitude);
  m_specialSummary->SetLabel(wxString::Format(
      _("%s solution: latitude %.5f%c at %s UTC. Calculated azimuth %.2f%c.\n"
        "Treat this as a workflow aid: uncertainty still depends on Ho, time, horizon and DR errors."),
      body.c_str(), latitude, 0x00b0,
      time.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC).c_str(),
      state.azimuthTrue, 0x00b0));
}
