#include "EclipseDialog.h"

#include "celestial_navigation_pi.h"

#include "eclipse/data_pack.h"
#include "eclipse/lunar_limb.h"
#include "eclipse/pck.h"
#include "eclipse/time.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {

wxString FormatDateTime(double tt_jd, double delta_t_seconds,
                        bool include_date) {
  const eclipse::CalendarDateTime value =
      eclipse::JulianDateToCalendar(tt_jd - delta_t_seconds / 86400.0);
  if (include_date)
    return wxString::Format("%04d-%02d-%02d %02d:%02d:%05.2f UT1", value.year,
                            value.month, value.day, value.hour, value.minute,
                            value.second);
  return wxString::Format("%02d:%02d:%05.2f UT1", value.hour, value.minute,
                          value.second);
}

wxString FormatLocation(const eclipse::GeoPoint& point) {
  return wxString::Format("%.3f%c, %.3f%c", std::fabs(point.latitude_deg),
                          point.latitude_deg >= 0.0 ? 'N' : 'S',
                          std::fabs(point.longitude_deg),
                          point.longitude_deg >= 0.0 ? 'E' : 'W');
}

wxString FormatOverlayTime(double tt_jd, double delta_t_seconds) {
  const eclipse::CalendarDateTime value =
      eclipse::JulianDateToCalendar(tt_jd - delta_t_seconds / 86400.0);
  return wxString::Format("%02d:%02d UT1", value.hour, value.minute);
}

void DrawPolyline(piDC* dc, PlugIn_ViewPort* viewport,
                  const std::vector<eclipse::GeoPoint>& points) {
  for (std::size_t index = 1; index < points.size(); ++index) {
    if (std::fabs(eclipse::NormalizeLongitude(
            points[index].longitude_deg - points[index - 1].longitude_deg)) >
        30.0)
      continue;
    wxPoint first;
    wxPoint second;
    GetCanvasPixLL(viewport, &first, points[index - 1].latitude_deg,
                   points[index - 1].longitude_deg);
    GetCanvasPixLL(viewport, &second, points[index].latitude_deg,
                   points[index].longitude_deg);
    dc->DrawLine(first.x, first.y, second.x, second.y);
  }
}

}  // namespace

EclipseDialog::EclipseDialog(wxWindow* parent, celestial_navigation_pi* plugin)
    : wxDialog(parent, wxID_ANY, _("Offline Solar Eclipse Planner"),
               wxDefaultPosition, wxSize(960, 720),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plugin(plugin),
      m_engine_ready(false),
      m_plotted_delta_t(0.0),
      m_data_status(NULL),
      m_start_year(NULL),
      m_end_year(NULL),
      m_event_list(NULL),
      m_plot_path(NULL),
      m_plot_contours(NULL),
      m_latitude(NULL),
      m_longitude(NULL),
      m_use_lola(NULL),
      m_local_results(NULL),
      m_plot_button(NULL),
      m_local_button(NULL) {
  BuildInterface();
  UpdateDataStatus();
  wxCommandEvent initial_position;
  OnBoatPosition(initial_position);
}

wxString EclipseDialog::DataDirectory() const {
  return celestial_navigation_pi::StandardPath() + "eclipse" +
         wxFileName::GetPathSeparator();
}

wxString EclipseDialog::De440Path() const {
  return DataDirectory() + "de440s.bsp";
}

wxString EclipseDialog::PckPath() const {
  return DataDirectory() + "moon_pa_de440_200625.bpc";
}

wxString EclipseDialog::LolaPath() const {
  return DataDirectory() + "lola64-pa.bin";
}

void EclipseDialog::BuildInterface() {
  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer* data = new wxStaticBoxSizer(
      wxVERTICAL, this, _("Offline data (no network access)"));
  m_data_status = new wxStaticText(this, wxID_ANY, _("Checking data..."));
  data->Add(m_data_status, 0, wxEXPAND | wxALL, 5);
  wxBoxSizer* imports = new wxBoxSizer(wxHORIZONTAL);
  wxButton* import_de = new wxButton(this, wxID_ANY, _("Import DE440s..."));
  wxButton* import_pck =
      new wxButton(this, wxID_ANY, _("Import lunar orientation..."));
  wxButton* import_lola =
      new wxButton(this, wxID_ANY, _("Import LOLA pack..."));
  imports->Add(import_de, 0, wxRIGHT, 5);
  imports->Add(import_pck, 0, wxRIGHT, 5);
  imports->Add(import_lola, 0);
  data->Add(imports, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  root->Add(data, 0, wxEXPAND | wxALL, 6);

  wxBoxSizer* search = new wxBoxSizer(wxHORIZONTAL);
  search->Add(new wxStaticText(this, wxID_ANY, _("Search years")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  const int year = wxDateTime::Now().GetYear();
  m_start_year = new wxSpinCtrl(this, wxID_ANY);
  m_start_year->SetRange(1850, 2100);
  m_start_year->SetValue(std::max(1850, std::min(2100, year)));
  m_end_year = new wxSpinCtrl(this, wxID_ANY);
  m_end_year->SetRange(1850, 2100);
  m_end_year->SetValue(std::max(1850, std::min(2100, year + 10)));
  wxButton* find = new wxButton(this, wxID_ANY, _("Find eclipses"));
  search->Add(m_start_year, 0, wxRIGHT, 4);
  search->Add(new wxStaticText(this, wxID_ANY, _("to")), 0,
              wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  search->Add(m_end_year, 0, wxRIGHT, 8);
  search->Add(find, 0);
  root->Add(search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  m_event_list =
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 180),
                     wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
  m_event_list->InsertColumn(0, _("Date and greatest eclipse"));
  m_event_list->InsertColumn(1, _("Type"));
  m_event_list->InsertColumn(2, _("Greatest position"));
  m_event_list->InsertColumn(3, _("Magnitude"));
  root->Add(m_event_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

  wxBoxSizer* plot = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer* plot_options = new wxBoxSizer(wxHORIZONTAL);
  m_plot_path = new wxCheckBox(
      this, wxID_ANY, _("Central line and totality/annularity limits"));
  m_plot_path->SetValue(true);
  m_plot_contours =
      new wxCheckBox(this, wxID_ANY, _("Partial magnitude contours"));
  m_plot_contours->SetValue(true);
  m_plot_button = new wxButton(this, wxID_ANY, _("Plot selected"));
  wxButton* clear = new wxButton(this, wxID_ANY, _("Clear plot"));
  plot_options->Add(m_plot_path, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 14);
  plot_options->Add(m_plot_contours, 0, wxALIGN_CENTER_VERTICAL);
  plot->Add(plot_options, 0, wxEXPAND | wxBOTTOM, 5);
  wxBoxSizer* plot_buttons = new wxBoxSizer(wxHORIZONTAL);
  plot_buttons->AddStretchSpacer();
  plot_buttons->Add(m_plot_button, 0, wxRIGHT, 5);
  plot_buttons->Add(clear, 0);
  plot->Add(plot_buttons, 0, wxEXPAND);
  root->Add(plot, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  wxStaticBoxSizer* local =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Local circumstances"));
  wxBoxSizer* position = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer* coordinates = new wxBoxSizer(wxHORIZONTAL);
  coordinates->Add(new wxStaticText(this, wxID_ANY, _("Latitude")), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  m_latitude = new wxTextCtrl(this, wxID_ANY, "0.000000", wxDefaultPosition,
                              wxSize(105, -1));
  coordinates->Add(m_latitude, 0, wxRIGHT, 8);
  coordinates->Add(new wxStaticText(this, wxID_ANY, _("Longitude")), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  m_longitude = new wxTextCtrl(this, wxID_ANY, "0.000000", wxDefaultPosition,
                               wxSize(105, -1));
  coordinates->Add(m_longitude, 0, wxRIGHT, 8);
  wxButton* boat = new wxButton(this, wxID_ANY, _("Use boat position"));
  coordinates->Add(boat, 0);
  position->Add(coordinates, 0, wxEXPAND | wxBOTTOM, 5);
  wxBoxSizer* local_actions = new wxBoxSizer(wxHORIZONTAL);
  m_use_lola =
      new wxCheckBox(this, wxID_ANY, _("Refine contacts with LOLA limb"));
  local_actions->Add(m_use_lola, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_local_button = new wxButton(this, wxID_ANY, _("Calculate"));
  local_actions->AddStretchSpacer();
  local_actions->Add(m_local_button, 0);
  position->Add(local_actions, 0, wxEXPAND);
  local->Add(position, 0, wxEXPAND | wxALL, 5);
  m_local_results = new wxTextCtrl(
      this, wxID_ANY,
      _("Select an eclipse and calculate local contacts. Times are UT1; "
        "future UTC can differ because leap seconds are not predictable."),
      wxDefaultPosition, wxSize(-1, 180),
      wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  wxFont result_font = m_local_results->GetFont();
  result_font.SetFamily(wxFONTFAMILY_TELETYPE);
  m_local_results->SetFont(result_font);
  local->Add(m_local_results, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  root->Add(local, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

  wxBoxSizer* footer = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText* note = new wxStaticText(
      this, wxID_ANY,
      _("DE440 coverage: 1850–2150. Planner intentionally limits searches "
        "to 2100. ΔT is modelled unless a reference event supplies it."));
  note->Wrap(800);
  footer->Add(note, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  wxButton* close = new wxButton(this, wxID_CLOSE, _("Close"));
  footer->Add(close, 0);
  root->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  SetSizer(root);
  SetMinSize(wxSize(760, 600));

  import_de->Bind(wxEVT_BUTTON, &EclipseDialog::OnImportDe440, this);
  import_pck->Bind(wxEVT_BUTTON, &EclipseDialog::OnImportPck, this);
  import_lola->Bind(wxEVT_BUTTON, &EclipseDialog::OnImportLola, this);
  find->Bind(wxEVT_BUTTON, &EclipseDialog::OnFind, this);
  m_event_list->Bind(wxEVT_LIST_ITEM_SELECTED, &EclipseDialog::OnSelection,
                     this);
  m_plot_button->Bind(wxEVT_BUTTON, &EclipseDialog::OnPlot, this);
  clear->Bind(wxEVT_BUTTON, &EclipseDialog::OnClear, this);
  boat->Bind(wxEVT_BUTTON, &EclipseDialog::OnBoatPosition, this);
  m_local_button->Bind(wxEVT_BUTTON, &EclipseDialog::OnLocal, this);
  close->Bind(wxEVT_BUTTON, &EclipseDialog::OnCloseButton, this);
  Bind(wxEVT_CLOSE_WINDOW, &EclipseDialog::OnWindowClose, this);
}

bool EclipseDialog::OpenEngine(bool report_error) {
  if (m_engine_ready) return true;
  std::string error;
  const eclipse::DataPackStatus status =
      eclipse::VerifyDe440s(De440Path().ToStdString());
  if (status.valid &&
      m_engine.OpenEphemeris(De440Path().ToStdString(), &error)) {
    m_engine_ready = true;
    return true;
  }
  if (report_error) {
    wxMessageBox(status.error.empty()
                     ? wxString::FromUTF8(error.c_str())
                     : wxString::FromUTF8(status.error.c_str()),
                 _("Eclipse data unavailable"), wxOK | wxICON_ERROR, this);
  }
  return false;
}

void EclipseDialog::UpdateDataStatus() {
  const eclipse::DataPackStatus de =
      eclipse::VerifyDe440s(De440Path().ToStdString());
  const bool pck_ok =
      eclipse::VerifyLunarOrientationPck(PckPath().ToStdString()).valid;
  const bool lola_ok = eclipse::VerifyLola64Pa(LolaPath().ToStdString()).valid;
  m_data_status->SetLabel(wxString::Format(
      "DE440s: %s   |   Lunar orientation: %s   |   LOLA limb: %s",
      de.valid ? _("verified") : _("not installed"),
      pck_ok ? _("installed") : _("not installed"),
      lola_ok ? _("installed") : _("not installed")));
  m_use_lola->Enable(pck_ok && lola_ok);
  if (!(pck_ok && lola_ok)) m_use_lola->SetValue(false);
  m_plot_button->Enable(de.valid);
  m_local_button->Enable(de.valid);
  if (!de.valid) m_engine_ready = false;
  Layout();
}

bool EclipseDialog::ImportFile(const wxString& title,
                               const wxString& destination, int kind) {
  wxFileDialog dialog(this, title, wxEmptyString, wxEmptyString,
                      _("All files (*.*)|*.*"),
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dialog.ShowModal() != wxID_OK) return false;
  std::string error;
  bool valid = false;
  if (kind == 0) {
    const eclipse::DataPackStatus status =
        eclipse::VerifyDe440s(dialog.GetPath().ToStdString());
    valid = status.valid;
    error = status.error;
  } else if (kind == 1) {
    const eclipse::DataPackStatus status =
        eclipse::VerifyLunarOrientationPck(dialog.GetPath().ToStdString());
    valid = status.valid;
    error = status.error;
  } else {
    const eclipse::DataPackStatus status =
        eclipse::VerifyLola64Pa(dialog.GetPath().ToStdString());
    valid = status.valid;
    error = status.error;
  }
  if (!valid) {
    wxMessageBox(wxString::FromUTF8(error.c_str()), _("Invalid eclipse data"),
                 wxOK | wxICON_ERROR, this);
    return false;
  }
  if (!wxFileName::Mkdir(DataDirectory(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) &&
      !wxFileName::DirExists(DataDirectory())) {
    wxMessageBox(_("Unable to create the private eclipse-data directory."),
                 _("Import failed"), wxOK | wxICON_ERROR, this);
    return false;
  }
  if (!wxCopyFile(dialog.GetPath(), destination, true)) {
    wxMessageBox(_("Unable to copy the selected data file."),
                 _("Import failed"), wxOK | wxICON_ERROR, this);
    return false;
  }
  return true;
}

void EclipseDialog::OnImportDe440(wxCommandEvent&) {
  if (ImportFile(_("Select official de440s.bsp"), De440Path(), 0)) {
    m_engine_ready = false;
    UpdateDataStatus();
  }
}

void EclipseDialog::OnImportPck(wxCommandEvent&) {
  if (ImportFile(_("Select moon_pa_de440_200625.bpc"), PckPath(), 1))
    UpdateDataStatus();
}

void EclipseDialog::OnImportLola(wxCommandEvent&) {
  if (ImportFile(_("Select converted LOLA principal-axes limb pack"),
                 LolaPath(), 2))
    UpdateDataStatus();
}

void EclipseDialog::OnFind(wxCommandEvent&) {
  if (!OpenEngine(true)) return;
  if (m_end_year->GetValue() < m_start_year->GetValue()) {
    wxMessageBox(_("The ending year must not precede the starting year."),
                 _("Invalid search"), wxOK | wxICON_WARNING, this);
    return;
  }
  eclipse::CalendarDateTime start;
  eclipse::CalendarDateTime end;
  start.year = m_start_year->GetValue();
  end.year = m_end_year->GetValue() + 1;
  double start_jd = 0.0, end_jd = 0.0;
  std::string error;
  if (!eclipse::CalendarToJulianDate(start, &start_jd, &error) ||
      !eclipse::CalendarToJulianDate(end, &end_jd, &error))
    return;
  wxBusyCursor busy;
  if (!m_engine.FindEvents(start_jd, end_jd, &m_events, &error)) {
    wxMessageBox(wxString::FromUTF8(error.c_str()), _("Eclipse search failed"),
                 wxOK | wxICON_ERROR, this);
    return;
  }
  m_event_list->DeleteAllItems();
  for (std::size_t index = 0; index < m_events.size(); ++index) {
    const eclipse::EclipseEvent& event = m_events[index];
    const long item = m_event_list->InsertItem(
        static_cast<long>(index),
        FormatDateTime(event.maximum_tt_jd, event.delta_t_seconds, true));
    m_event_list->SetItem(
        item, 1, wxString::FromUTF8(eclipse::EclipseTypeName(event.type)));
    m_event_list->SetItem(item, 2, FormatLocation(event.greatest_position));
    m_event_list->SetItem(item, 3, wxString::Format("%.4f", event.magnitude));
    m_event_list->SetItemData(item, static_cast<long>(index));
  }
  for (int column = 0; column < 4; ++column)
    m_event_list->SetColumnWidth(column, wxLIST_AUTOSIZE_USEHEADER);
  if (!m_events.empty()) {
    m_event_list->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    m_event_list->EnsureVisible(0);
  }
}

void EclipseDialog::OnSelection(wxListEvent&) {}

bool EclipseDialog::SelectedEvent(eclipse::EclipseEvent* event) const {
  const long selected =
      m_event_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (selected < 0 || !event) return false;
  const long index = m_event_list->GetItemData(selected);
  if (index < 0 || static_cast<std::size_t>(index) >= m_events.size())
    return false;
  *event = m_events[static_cast<std::size_t>(index)];
  return true;
}

void EclipseDialog::OnPlot(wxCommandEvent&) {
  eclipse::EclipseEvent event;
  if (!SelectedEvent(&event)) {
    wxMessageBox(_("Select an eclipse first."), _("Eclipse planner"),
                 wxOK | wxICON_INFORMATION, this);
    return;
  }
  std::string error;
  wxBusyCursor busy;
  m_path.clear();
  m_contours.clear();
  m_plotted_delta_t = event.delta_t_seconds;
  if (m_plot_path->GetValue() && event.type != eclipse::kPartialEclipse &&
      !m_engine.BuildCentralPath(event, 120.0, &m_path, &error)) {
    wxMessageBox(wxString::FromUTF8(error.c_str()),
                 _("Path calculation failed"), wxOK | wxICON_ERROR, this);
    return;
  }
  if (m_plot_contours->GetValue()) {
    const double values[] = {0.2, 0.4, 0.6, 0.8, 0.9};
    if (!m_engine.BuildMagnitudeContours(
            event, std::vector<double>(values, values + 5), 2.0, 300.0,
            &m_contours, &error)) {
      wxMessageBox(wxString::FromUTF8(error.c_str()),
                   _("Contour calculation failed"), wxOK | wxICON_ERROR, this);
      return;
    }
  }
  RequestRefresh(GetOCPNCanvasWindow());
}

void EclipseDialog::OnClear(wxCommandEvent&) {
  m_path.clear();
  m_contours.clear();
  RequestRefresh(GetOCPNCanvasWindow());
}

void EclipseDialog::OnBoatPosition(wxCommandEvent&) {
  double latitude = 0.0, longitude = 0.0;
  if (!m_plugin->GetBoatPosition(&latitude, &longitude)) {
    m_local_results->SetValue(
        _("No valid boat position has been received yet; enter latitude and "
          "longitude manually."));
    return;
  }
  m_latitude->SetValue(wxString::Format("%.6f", latitude));
  m_longitude->SetValue(wxString::Format("%.6f", longitude));
}

bool EclipseDialog::Observer(eclipse::GeoPoint* observer) const {
  if (!observer) return false;
  double latitude = 0.0, longitude = 0.0;
  if (!m_latitude->GetValue().ToDouble(&latitude) ||
      !m_longitude->GetValue().ToDouble(&longitude) || latitude < -90.0 ||
      latitude > 90.0 || longitude < -180.0 || longitude > 180.0)
    return false;
  *observer = eclipse::GeoPoint(latitude, longitude);
  return true;
}

void EclipseDialog::OnLocal(wxCommandEvent&) {
  eclipse::EclipseEvent event;
  eclipse::GeoPoint observer;
  if (!SelectedEvent(&event) || !Observer(&observer)) {
    wxMessageBox(_("Select an eclipse and enter a valid latitude/longitude."),
                 _("Local circumstances"), wxOK | wxICON_WARNING, this);
    return;
  }
  std::string error;
  eclipse::LocalContacts contacts;
  wxBusyCursor busy;
  if (!m_engine.SolveLocalContacts(event, observer, 0.0, &contacts, &error)) {
    wxMessageBox(wxString::FromUTF8(error.c_str()),
                 _("Local calculation failed"), wxOK | wxICON_ERROR, this);
    return;
  }
  if (m_use_lola->GetValue() && contacts.c1.valid) {
    eclipse::PckKernel pck;
    eclipse::LunarLimbGrid lola;
    if (!pck.Open(PckPath().ToStdString(), &error) ||
        !lola.Open(LolaPath().ToStdString(), &error) ||
        !m_engine.RefineContactsWithLola(event, observer, 0.0, pck, lola,
                                         &contacts, &error)) {
      wxMessageBox(wxString::FromUTF8(error.c_str()),
                   _("LOLA refinement failed"), wxOK | wxICON_ERROR, this);
      return;
    }
  }
  if (!contacts.c1.valid) {
    m_local_results->SetValue(
        _("This eclipse is not visible from the entered position."));
    return;
  }
  const eclipse::ContactTime* times[] = {&contacts.c1, &contacts.c2,
                                         &contacts.maximum, &contacts.c3,
                                         &contacts.c4};
  const char* names[] = {"C1", "C2", "MAX", "C3", "C4"};
  wxString text;
  text << wxString::Format("Observer: %.5f, %.5f\n", observer.latitude_deg,
                           observer.longitude_deg);
  for (int index = 0; index < 5; ++index) {
    if (!times[index]->valid) continue;
    text << wxString::Format(
        "%-3s  %s   Sun alt %7.3f%c  az %7.3f%c\n", names[index],
        FormatDateTime(times[index]->tt_jd, event.delta_t_seconds, false),
        times[index]->sun_altitude_deg, 0x00b0, times[index]->sun_azimuth_deg,
        0x00b0);
  }
  text << wxString::Format(
      "\nType: %s   magnitude %.5f   obscuration %.3f%%\n",
      wxString::FromUTF8(eclipse::EclipseTypeName(contacts.type)),
      contacts.magnitude, contacts.obscuration * 100.0);
  if (contacts.c2.valid && contacts.c3.valid)
    text << wxString::Format("Central duration: %.2f seconds\n",
                             contacts.central_duration_seconds);
  text << wxString::Format("ΔT model: %.2f seconds", event.delta_t_seconds);
  if (contacts.limb_adjusted)
    text << _("   |   C1–C4 refined from LOLA terrain");
  m_local_results->SetValue(text);
}

void EclipseDialog::OnCloseButton(wxCommandEvent&) { Hide(); }

void EclipseDialog::RunIntegrationScenario2027() {
  m_start_year->SetValue(2027);
  m_end_year->SetValue(2027);
  wxCommandEvent command;
  OnFind(command);
  for (std::size_t index = 0; index < m_events.size(); ++index) {
    const eclipse::CalendarDateTime date =
        eclipse::JulianDateToCalendar(m_events[index].maximum_tt_jd);
    if (date.month != 8) continue;
    m_event_list->SetItemState(static_cast<long>(index), wxLIST_STATE_SELECTED,
                               wxLIST_STATE_SELECTED);
    m_event_list->EnsureVisible(static_cast<long>(index));
    break;
  }
  OnPlot(command);
  m_latitude->SetValue("25.505000");
  m_longitude->SetValue("33.183333");
  m_use_lola->SetValue(m_use_lola->IsEnabled());
  OnLocal(command);
  wxLogMessage(
      "CELESTIAL_ECLIPSE_INTEGRATION_TEST: 2027 scenario complete; "
      "path=%zu contours=%zu LOLA=%d",
      m_path.size(), m_contours.size(),
      static_cast<int>(m_use_lola->GetValue()));
}

void EclipseDialog::OnWindowClose(wxCloseEvent& event) {
  // This dialog is owned for the lifetime of the main celestial-navigation
  // window. Hiding it keeps that owner pointer valid when the window-manager
  // close control is used.
  Hide();
  event.Veto();
}

bool EclipseDialog::Render(piDC* dc, PlugIn_ViewPort* viewport) {
  if (!dc || !viewport || !HasPlot()) return false;
  if (!m_path.empty()) {
    std::vector<eclipse::GeoPoint> north, centre, south;
    north.reserve(m_path.size());
    centre.reserve(m_path.size());
    south.reserve(m_path.size());
    for (std::size_t index = 0; index < m_path.size(); ++index) {
      north.push_back(m_path[index].northern_limit);
      centre.push_back(m_path[index].central_line);
      south.push_back(m_path[index].southern_limit);
    }
    dc->SetPen(wxPen(wxColour(190, 20, 35), 3));
    DrawPolyline(dc, viewport, north);
    DrawPolyline(dc, viewport, south);
    dc->SetPen(wxPen(wxColour(30, 60, 200), 2));
    DrawPolyline(dc, viewport, centre);

    // BuildCentralPath uses two-minute samples. Label every tenth minute so
    // the chart is useful for passage planning without becoming cluttered.
    dc->SetFont(
        wxFont(9, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    dc->SetBrush(wxBrush(wxColour(255, 255, 255)));
    for (std::size_t index = 0; index < m_path.size(); index += 5) {
      wxPoint point;
      GetCanvasPixLL(viewport, &point, m_path[index].central_line.latitude_deg,
                     m_path[index].central_line.longitude_deg);
      dc->SetPen(wxPen(wxColour(30, 60, 200), 1));
      dc->DrawCircle(point, 3);
      const wxString label =
          FormatOverlayTime(m_path[index].tt_jd, m_plotted_delta_t);
      dc->SetTextForeground(wxColour(15, 15, 15));
      dc->DrawText(label, point.x + 6, point.y - 7);
    }
  }
  const wxColour colours[] = {wxColour(246, 174, 45), wxColour(238, 142, 34),
                              wxColour(226, 103, 26), wxColour(205, 64, 25),
                              wxColour(155, 35, 45)};
  for (std::size_t contour = 0; contour < m_contours.size(); ++contour) {
    dc->SetPen(wxPen(colours[std::min<std::size_t>(contour, 4)], 2));
    bool labelled = false;
    wxCoord canvas_width = 0, canvas_height = 0;
    dc->GetSize(&canvas_width, &canvas_height);
    for (std::size_t segment = 0; segment < m_contours[contour].segments.size();
         ++segment) {
      const eclipse::ContourSegment& item =
          m_contours[contour].segments[segment];
      if (std::fabs(eclipse::NormalizeLongitude(
              item.second.longitude_deg - item.first.longitude_deg)) > 10.0)
        continue;
      wxPoint first, second;
      GetCanvasPixLL(viewport, &first, item.first.latitude_deg,
                     item.first.longitude_deg);
      GetCanvasPixLL(viewport, &second, item.second.latitude_deg,
                     item.second.longitude_deg);
      dc->DrawLine(first.x, first.y, second.x, second.y);
      const wxPoint midpoint((first.x + second.x) / 2,
                             (first.y + second.y) / 2);
      if (!labelled && midpoint.x >= 0 && midpoint.y >= 0 &&
          midpoint.x < canvas_width && midpoint.y < canvas_height) {
        dc->SetFont(wxFont(9, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                           wxFONTWEIGHT_BOLD));
        dc->SetTextForeground(colours[std::min<std::size_t>(contour, 4)]);
        dc->DrawText(wxString::Format("%.1f", m_contours[contour].magnitude),
                     midpoint.x + 3, midpoint.y + 3);
        labelled = true;
      }
    }
  }
  return true;
}
