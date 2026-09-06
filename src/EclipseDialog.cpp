#include "EclipseDialog.h"

#include "AtomicXmlFile.h"
#include "NavigationUIUtils.h"
#include "Utf8Translation.h"

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
#include <chrono>
#include <cmath>
#include <future>
#include <iomanip>
#include <sstream>

namespace {

using celestial_navigation::EclipseDataKind;
using celestial_navigation::InstalledDataState;

enum OptionalDataAction {
  ID_DOWNLOAD_PCK = wxID_HIGHEST + 410,
  ID_IMPORT_PCK,
  ID_DOWNLOAD_LOLA,
  ID_IMPORT_LOLA
};

enum VerificationPurpose {
  VERIFY_NONE = 0,
  VERIFY_INSTALLED = 1,
  VERIFY_DOWNLOAD = 2,
  VERIFY_LOCAL_IMPORT = 3
};

wxString MiB(std::uint64_t bytes) {
  return wxString::Format("%.1f MiB", bytes / (1024.0 * 1024.0));
}

wxString OptionalStateLabel(InstalledDataState state) {
  if (state == InstalledDataState::Verified) return _("Installed and verified");
  if (state == InstalledDataState::VerificationRequired)
    return _("Installed; verification required");
  return _("Not installed");
}

class OptionalLunarDataDialog : public wxDialog {
public:
  OptionalLunarDataDialog(wxWindow* parent, InstalledDataState pck_state,
                          InstalledDataState lola_state)
      : wxDialog(parent, wxID_ANY,
                 CN_UTF8_("Advanced Eclipse Data — Optional Lunar Refinement"),
                 wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
    wxStaticText* introduction = new wxStaticText(
        this, wxID_ANY,
        _("These files are optional. They are not required for ordinary "
          "celestial navigation or standard eclipse calculations. They "
          "refine lunar contact timing by modelling the Moon's orientation "
          "and irregular surface."));
    introduction->Wrap(690);
    root->Add(introduction, 0, wxEXPAND | wxALL, 12);

    AddDataSection(
        root, CN_UTF8_("Lunar orientation — 12.3 MiB"),
        _("Provides the Moon's physical orientation relative to the DE440 "
          "ephemeris. It is required when LOLA limb refinement is used."),
        pck_state, ID_DOWNLOAD_PCK, ID_IMPORT_PCK);
    AddDataSection(
        root, CN_UTF8_("LOLA lunar limb — 506 MiB"),
        _("Adds high-resolution lunar topography so mountains and valleys "
          "can refine eclipse contact times. It requires the lunar-"
          "orientation file above."),
        lola_state, ID_DOWNLOAD_LOLA, ID_IMPORT_LOLA);

    wxBoxSizer* footer = new wxBoxSizer(wxHORIZONTAL);
    footer->AddStretchSpacer();
    wxButton* close = new wxButton(this, wxID_CANCEL, _("Close"));
    footer->Add(close, 0);
    root->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    SetSizerAndFit(root);
    SetMinSize(wxSize(700, GetSize().GetHeight()));
    CentreOnParent();
  }

private:
  void AddDataSection(wxBoxSizer* root, const wxString& title,
                      const wxString& description, InstalledDataState state,
                      int download_id, int import_id) {
    wxStaticBoxSizer* box = new wxStaticBoxSizer(wxVERTICAL, this, title);
    wxStaticText* explanation = new wxStaticText(this, wxID_ANY, description);
    explanation->Wrap(660);
    box->Add(explanation, 0, wxEXPAND | wxALL, 7);
    wxBoxSizer* actions = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* status = new wxStaticText(
        this, wxID_ANY, _("Status: ") + OptionalStateLabel(state));
    actions->Add(status, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    wxButton* download =
        new wxButton(this, download_id, CN_UTF8_("Download and install…"));
    download->Enable(state != InstalledDataState::Verified);
    wxButton* import =
        new wxButton(this, import_id, CN_UTF8_("Import local file…"));
    actions->Add(download, 0, wxRIGHT, 5);
    actions->Add(import, 0);
    box->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 7);
    root->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    download->Bind(wxEVT_BUTTON, [this, download_id](wxCommandEvent&) {
      EndModal(download_id);
    });
    import->Bind(wxEVT_BUTTON,
                 [this, import_id](wxCommandEvent&) { EndModal(import_id); });
  }
};

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
  return FormatNavigationAngle(point.latitude_deg,
                               NavigationAngleKind::Latitude, true) +
         ", " +
         FormatNavigationAngle(point.longitude_deg,
                               NavigationAngleKind::Longitude, true);
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
      m_local_button(NULL),
      m_import_de(NULL),
      m_download_de(NULL),
      m_optional_data(NULL),
      m_cancel_install(NULL),
      m_install_index(0),
      m_source_index(0),
      m_download_kind(-1),
      m_download_handle(0),
      m_cancel_requested(false),
      m_verification_timer(this),
      m_verifying(false),
      m_verification_purpose(VERIFY_NONE),
      m_verification_kind(EclipseDataKind::De440s),
      m_installed_check_index(0) {
  m_invalid_data[0] = m_invalid_data[1] = m_invalid_data[2] = false;
  BuildInterface();
  UpdateDataStatus();
  StartInstalledDataCheck();
  wxCommandEvent initial_position;
  OnBoatPosition(initial_position);
}

EclipseDialog::~EclipseDialog() {
  m_verification_timer.Stop();
  Unbind(wxEVT_TIMER, &EclipseDialog::OnVerificationTimer, this,
         m_verification_timer.GetId());
  Disconnect(wxID_ANY, wxEVT_DOWNLOAD_EVENT,
             wxEventHandler(EclipseDialog::OnDownloadEvent), NULL, this);
  if (m_download_handle) {
    OCPN_cancelDownloadFileBackground(m_download_handle);
    m_download_handle = 0;
  }
  if (m_verification_future.valid()) m_verification_future.wait();
  if (!m_download_temp.empty() && wxFileExists(m_download_temp))
    wxRemoveFile(m_download_temp);
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
  wxStaticBoxSizer* data =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Eclipse astronomy data"));
  m_data_status = new wxStaticText(this, wxID_ANY, _("Checking data..."));
  data->Add(m_data_status, 0, wxEXPAND | wxALL, 5);
  wxStaticText* data_note = new wxStaticText(
      this, wxID_ANY,
      _("DE440s is required only for the eclipse planner. Lunar-orientation "
        "and LOLA terrain files are optional refinements."));
  data_note->Wrap(880);
  data->Add(data_note, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  wxBoxSizer* imports = new wxBoxSizer(wxHORIZONTAL);
  m_download_de = new wxButton(
      this, wxID_ANY, CN_UTF8_("Download and install DE440s… (31.2 MiB)"));
  m_import_de = new wxButton(this, wxID_ANY, CN_UTF8_("Import DE440s…"));
  m_optional_data =
      new wxButton(this, wxID_ANY, CN_UTF8_("Optional lunar data…"));
  m_cancel_install =
      new wxButton(this, wxID_ANY, _("Cancel data installation"));
  m_cancel_install->Hide();
  imports->Add(m_download_de, 0, wxRIGHT, 5);
  imports->Add(m_import_de, 0, wxRIGHT, 5);
  imports->Add(m_optional_data, 0);
  data->Add(imports, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  data->Add(m_cancel_install, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
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
  m_latitude = new wxTextCtrl(
      this, wxID_ANY,
      FormatNavigationAngle(0.0, NavigationAngleKind::Latitude, true),
      wxDefaultPosition, wxSize(150, -1));
  coordinates->Add(m_latitude, 0, wxRIGHT, 8);
  coordinates->Add(new wxStaticText(this, wxID_ANY, _("Longitude")), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
  m_longitude = new wxTextCtrl(
      this, wxID_ANY,
      FormatNavigationAngle(0.0, NavigationAngleKind::Longitude, true),
      wxDefaultPosition, wxSize(160, -1));
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
      CN_UTF8_("DE440 coverage: 1850–2150. Planner intentionally limits "
               "searches to 2100. ΔT is modelled unless a reference event "
               "supplies it."));
  note->Wrap(800);
  footer->Add(note, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  wxButton* close = new wxButton(this, wxID_CLOSE, _("Close"));
  footer->Add(close, 0);
  root->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  SetSizer(root);
  SetMinSize(wxSize(760, 600));

  m_import_de->Bind(wxEVT_BUTTON, &EclipseDialog::OnImportDe440, this);
  m_download_de->Bind(wxEVT_BUTTON, &EclipseDialog::OnDownloadDe440, this);
  m_optional_data->Bind(wxEVT_BUTTON, &EclipseDialog::OnOptionalData, this);
  m_cancel_install->Bind(wxEVT_BUTTON, &EclipseDialog::OnCancelInstall, this);
  Connect(wxID_ANY, wxEVT_DOWNLOAD_EVENT,
          wxEventHandler(EclipseDialog::OnDownloadEvent), NULL, this);
  Bind(wxEVT_TIMER, &EclipseDialog::OnVerificationTimer, this,
       m_verification_timer.GetId());
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
  const EclipseDataKind kinds[] = {EclipseDataKind::De440s,
                                   EclipseDataKind::LunarOrientation,
                                   EclipseDataKind::LolaLimb};
  wxString labels[3];
  for (int index = 0; index < 3; ++index) {
    const InstalledDataState state = celestial_navigation::InspectInstalledData(
        kinds[index], DataPath(kinds[index]));
    if (m_invalid_data[index])
      labels[index] = _("invalid");
    else if (state == InstalledDataState::Verified)
      labels[index] = _("verified");
    else if (state == InstalledDataState::VerificationRequired)
      labels[index] = _("checking");
    else
      labels[index] = _("not installed");
  }
  m_data_status->SetLabel(wxString::Format(
      "DE440s: %s   |   Lunar orientation: %s   |   LOLA limb: %s",
      labels[0].c_str(), labels[1].c_str(), labels[2].c_str()));
  const bool de_ok = DataVerified(EclipseDataKind::De440s);
  const bool pck_ok = DataVerified(EclipseDataKind::LunarOrientation);
  const bool lola_ok = DataVerified(EclipseDataKind::LolaLimb);
  m_use_lola->Enable(pck_ok && lola_ok);
  if (!(pck_ok && lola_ok)) m_use_lola->SetValue(false);
  m_plot_button->Enable(de_ok);
  m_local_button->Enable(de_ok);
  if (!de_ok) m_engine_ready = false;
  SetInstallationControls(m_download_kind >= 0 || m_verifying);
  Layout();
}

wxString EclipseDialog::DataPath(EclipseDataKind kind) const {
  if (kind == EclipseDataKind::LunarOrientation) return PckPath();
  if (kind == EclipseDataKind::LolaLimb) return LolaPath();
  return De440Path();
}

bool EclipseDialog::DataVerified(EclipseDataKind kind) const {
  const int index = static_cast<int>(kind);
  return !m_invalid_data[index] &&
         celestial_navigation::InspectInstalledData(kind, DataPath(kind)) ==
             InstalledDataState::Verified;
}

void EclipseDialog::SetInstallationControls(bool busy) {
  if (!m_download_de) return;
  m_download_de->Enable(!busy && !DataVerified(EclipseDataKind::De440s));
  m_import_de->Enable(!busy);
  m_optional_data->Enable(!busy);
  const bool cancellable = busy && m_verification_purpose != VERIFY_INSTALLED;
  m_cancel_install->Show(cancellable);
  m_cancel_install->Enable(cancellable && !m_cancel_requested);
  Layout();
}

void EclipseDialog::SelectAndImport(EclipseDataKind kind) {
  if (m_download_kind >= 0 || m_verifying) return;
  wxString title;
  if (kind == EclipseDataKind::De440s)
    title = _("Select official de440s.bsp");
  else if (kind == EclipseDataKind::LunarOrientation)
    title = _("Select moon_pa_de440_200625.bpc");
  else
    title = _("Select converted LOLA principal-axes limb pack");
  wxFileDialog dialog(this, title, wxEmptyString, wxEmptyString,
                      _("All files (*.*)|*.*"),
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dialog.ShowModal() != wxID_OK) return;
  if (!wxFileName::Mkdir(DataDirectory(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) &&
      !wxFileName::DirExists(DataDirectory())) {
    wxMessageBox(_("Unable to create the private eclipse-data directory."),
                 _("Import failed"), wxOK | wxICON_ERROR, this);
    return;
  }
  const std::vector<EclipseDataKind> plan(1, kind);
  if (!EnsureInstallationSpace(plan)) return;
  m_cancel_requested = false;
  m_download_kind = static_cast<int>(kind);
  m_download_temp.clear();
  BeginVerification(kind, dialog.GetPath(), VERIFY_LOCAL_IMPORT);
}

void EclipseDialog::OnImportDe440(wxCommandEvent&) {
  SelectAndImport(EclipseDataKind::De440s);
}

void EclipseDialog::OnDownloadDe440(wxCommandEvent&) {
  BeginInstall(EclipseDataKind::De440s);
}

void EclipseDialog::OnOptionalData(wxCommandEvent&) {
  OptionalLunarDataDialog dialog(
      this,
      celestial_navigation::InspectInstalledData(
          EclipseDataKind::LunarOrientation, PckPath()),
      celestial_navigation::InspectInstalledData(EclipseDataKind::LolaLimb,
                                                 LolaPath()));
  const int action = dialog.ShowModal();
  if (action == ID_DOWNLOAD_PCK)
    BeginInstall(EclipseDataKind::LunarOrientation);
  else if (action == ID_IMPORT_PCK)
    SelectAndImport(EclipseDataKind::LunarOrientation);
  else if (action == ID_DOWNLOAD_LOLA)
    BeginInstall(EclipseDataKind::LolaLimb);
  else if (action == ID_IMPORT_LOLA)
    SelectAndImport(EclipseDataKind::LolaLimb);
}

bool EclipseDialog::EnsureInstallationSpace(
    const std::vector<EclipseDataKind>& plan) {
  wxDiskspaceSize_t free_space;
  if (!wxGetDiskSpace(DataDirectory(), NULL, &free_space)) return true;
#if wxUSE_LONGLONG
  const double available = free_space.ToDouble();
#else
  const double available = static_cast<double>(free_space);
#endif
  const std::uint64_t required =
      celestial_navigation::RequiredWorkingSpaceBytes(plan);
  if (available >= static_cast<double>(required)) return true;
  wxMessageBox(
      wxString::Format(
          _("This verified installation needs approximately %s of free "
            "working space, but only %s is available."),
          MiB(required).c_str(),
          MiB(static_cast<std::uint64_t>(std::max(0.0, available))).c_str()),
      _("Not enough disk space"), wxOK | wxICON_ERROR, this);
  return false;
}

void EclipseDialog::BeginInstall(EclipseDataKind requested) {
  if (m_download_kind >= 0 || m_verifying) return;
  if (DataVerified(requested)) {
    wxMessageBox(_("That astronomy data file is already installed and "
                   "verified."),
                 _("Astronomy data"), wxOK | wxICON_INFORMATION, this);
    return;
  }
  const bool pck_ok = DataVerified(EclipseDataKind::LunarOrientation);
  std::vector<EclipseDataKind> plan =
      celestial_navigation::BuildEclipseDataInstallPlan(requested, pck_ok);
  if (requested == EclipseDataKind::LolaLimb && !pck_ok) {
    const int answer = wxMessageBox(
        wxString::Format(
            _("LOLA refinement also requires the 12.3 MiB lunar-orientation "
              "file. Download and verify both files?\n\nTotal download: "
              "%s\nTemporary working-space requirement: approximately "
              "%s"),
            MiB(celestial_navigation::DownloadBytes(plan)).c_str(),
            MiB(celestial_navigation::RequiredWorkingSpaceBytes(plan)).c_str()),
        _("Install optional lunar data"),
        wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this);
    if (answer != wxYES) return;
  }
  if (!wxFileName::Mkdir(DataDirectory(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) &&
      !wxFileName::DirExists(DataDirectory())) {
    wxMessageBox(_("Unable to create the private astronomy-data directory."),
                 _("Installation failed"), wxOK | wxICON_ERROR, this);
    return;
  }
  if (!EnsureInstallationSpace(plan)) return;
  m_install_queue = plan;
  m_install_index = 0;
  m_source_index = 0;
  m_cancel_requested = false;
  m_download_kind = static_cast<int>(m_install_queue.front());
  SetInstallationControls(true);
  StartNextDownload();
}

void EclipseDialog::StartNextDownload() {
  if (m_cancel_requested) {
    FinishInstallation(false, _("Astronomy-data installation was cancelled."));
    return;
  }
  while (m_install_index < m_install_queue.size() &&
         DataVerified(m_install_queue[m_install_index]))
    ++m_install_index;
  if (m_install_index >= m_install_queue.size()) {
    FinishInstallation(
        true, _("The requested astronomy data was downloaded, verified and "
                "installed."));
    return;
  }
  const EclipseDataKind kind = m_install_queue[m_install_index];
  m_download_kind = static_cast<int>(kind);
  m_download_sources =
      celestial_navigation::GetEclipseDataFileSpec(kind).sources;
  m_source_index = 0;
  TryCurrentSource();
}

void EclipseDialog::TryCurrentSource() {
  if (m_cancel_requested) {
    FinishInstallation(false, _("Astronomy-data installation was cancelled."));
    return;
  }
  const EclipseDataKind kind = static_cast<EclipseDataKind>(m_download_kind);
  const celestial_navigation::EclipseDataFileSpec& spec =
      celestial_navigation::GetEclipseDataFileSpec(kind);
  if (m_source_index >= m_download_sources.size()) {
    FinishInstallation(
        false,
        wxString::Format(
            _("None of the trusted download sources supplied a valid %s "
              "file. You can still use Import local file with a verified "
              "copy."),
            wxString::FromUTF8(spec.display_name).c_str()));
    return;
  }
  m_download_temp = DataPath(kind) + ".download";
  if (wxFileExists(m_download_temp)) wxRemoveFile(m_download_temp);
  celestial_navigation::ForgetVerifiedDataFile(m_download_temp);
  m_data_status->SetLabel(
      wxString::Format(CN_UTF8_("Downloading %s: source %lu of %lu…"),
                       wxString::FromUTF8(spec.display_name).c_str(),
                       static_cast<unsigned long>(m_source_index + 1),
                       static_cast<unsigned long>(m_download_sources.size())));
  const OCPN_DLStatus status = OCPN_downloadFileBackground(
      wxString::FromUTF8(m_download_sources[m_source_index].c_str()),
      m_download_temp, this, &m_download_handle);
  if (status != OCPN_DL_STARTED && status != OCPN_DL_NO_ERROR) {
    m_download_handle = 0;
    ++m_source_index;
    TryCurrentSource();
  }
}

void EclipseDialog::OnDownloadEvent(wxEvent& raw) {
  OCPN_downloadEvent& event = static_cast<OCPN_downloadEvent&>(raw);
  if (m_download_kind < 0) return;
  if (event.getDLEventCondition() == OCPN_DL_EVENT_TYPE_PROGRESS) {
    const long total = event.getTotal();
    const int percent =
        total > 0 ? static_cast<int>(100.0 * event.getTransferred() / total)
                  : 0;
    const EclipseDataKind kind = static_cast<EclipseDataKind>(m_download_kind);
    m_data_status->SetLabel(wxString::Format(
        CN_UTF8_("Downloading %s: %d%% (source %lu of %lu)…"),
        wxString::FromUTF8(
            celestial_navigation::GetEclipseDataFileSpec(kind).display_name)
            .c_str(),
        percent, static_cast<unsigned long>(m_source_index + 1),
        static_cast<unsigned long>(m_download_sources.size())));
    return;
  }
  if (event.getDLEventCondition() != OCPN_DL_EVENT_TYPE_END) return;
  m_download_handle = 0;
  if (m_cancel_requested) {
    if (wxFileExists(m_download_temp)) wxRemoveFile(m_download_temp);
    FinishInstallation(false, _("Astronomy-data installation was cancelled."));
    return;
  }
  if (event.getDLEventStatus() != OCPN_DL_NO_ERROR) {
    if (wxFileExists(m_download_temp)) wxRemoveFile(m_download_temp);
    ++m_source_index;
    TryCurrentSource();
    return;
  }
  BeginVerification(static_cast<EclipseDataKind>(m_download_kind),
                    m_download_temp, VERIFY_DOWNLOAD);
}

void EclipseDialog::BeginVerification(EclipseDataKind kind,
                                      const wxString& path, int purpose) {
  m_verifying = true;
  m_verification_kind = kind;
  m_verification_path = path;
  m_verification_purpose = purpose;
  SetInstallationControls(true);
  const std::string native_path = path.ToStdString();
  m_data_status->SetLabel(wxString::Format(
      CN_UTF8_("Verifying %s size, SHA-256 and file structure…"),
      wxString::FromUTF8(
          celestial_navigation::GetEclipseDataFileSpec(kind).display_name)
          .c_str()));
  m_verification_future = std::async(std::launch::async, [kind, native_path]() {
    const eclipse::DataPackStatus status =
        celestial_navigation::VerifyEclipseDataFile(kind, native_path);
    VerificationResult result;
    result.valid = status.valid;
    result.error = status.error;
    return result;
  });
  m_verification_timer.Start(100);
}

void EclipseDialog::OnVerificationTimer(wxTimerEvent&) {
  if (!m_verifying || !m_verification_future.valid()) return;
  if (m_verification_future.wait_for(std::chrono::milliseconds(0)) !=
      std::future_status::ready)
    return;
  const VerificationResult result = m_verification_future.get();
  m_verification_timer.Stop();
  m_verifying = false;
  if (m_verification_purpose == VERIFY_INSTALLED) {
    const int index = static_cast<int>(m_verification_kind);
    m_invalid_data[index] = !result.valid;
    if (result.valid) {
      wxString record_error;
      if (!celestial_navigation::RecordVerifiedDataFile(
              m_verification_kind, m_verification_path, &record_error))
        m_invalid_data[index] = true;
    } else {
      celestial_navigation::ForgetVerifiedDataFile(m_verification_path);
    }
    m_verification_purpose = VERIFY_NONE;
    ++m_installed_check_index;
    StartNextInstalledDataCheck();
    return;
  }
  FinishDownloadedVerification(result.valid,
                               wxString::FromUTF8(result.error.c_str()));
}

void EclipseDialog::FinishDownloadedVerification(bool valid,
                                                 const wxString& error) {
  const int purpose = m_verification_purpose;
  const EclipseDataKind kind = m_verification_kind;
  const wxString source = m_verification_path;
  m_verification_purpose = VERIFY_NONE;
  if (m_cancel_requested) {
    if (purpose == VERIFY_DOWNLOAD && wxFileExists(source))
      wxRemoveFile(source);
    FinishInstallation(false, _("Astronomy-data installation was cancelled."));
    return;
  }
  if (!valid) {
    if (purpose == VERIFY_DOWNLOAD) {
      if (wxFileExists(source)) wxRemoveFile(source);
      ++m_source_index;
      TryCurrentSource();
    } else {
      FinishInstallation(
          false,
          error.empty() ? _("The selected file failed verification.") : error);
    }
    return;
  }

  const wxString destination = DataPath(kind);
  wxString install_error;
  bool installed = false;
  if (wxFileName(source).GetFullPath() == wxFileName(destination).GetFullPath())
    installed = true;
  else
    installed = celestial_navigation::CopyFileAtomically(source, destination,
                                                         &install_error);
  if (purpose == VERIFY_DOWNLOAD && wxFileExists(source)) wxRemoveFile(source);
  if (!installed || !celestial_navigation::RecordVerifiedDataFile(
                        kind, destination, &install_error)) {
    FinishInstallation(
        false, install_error.empty()
                   ? _("The verified file could not be installed atomically.")
                   : install_error);
    return;
  }
  m_invalid_data[static_cast<int>(kind)] = false;
  if (kind == EclipseDataKind::De440s) m_engine_ready = false;
  if (purpose == VERIFY_DOWNLOAD) {
    ++m_install_index;
    StartNextDownload();
  } else {
    FinishInstallation(
        true,
        _("The selected astronomy data file was verified and installed."));
  }
}

void EclipseDialog::OnCancelInstall(wxCommandEvent&) {
  if (m_download_kind < 0 && !m_verifying) return;
  m_cancel_requested = true;
  m_cancel_install->Disable();
  m_data_status->SetLabel(
      CN_UTF8_("Cancelling safely; a verification already in progress may "
               "finish first…"));
  if (m_download_handle) OCPN_cancelDownloadFileBackground(m_download_handle);
}

void EclipseDialog::FinishInstallation(bool success, const wxString& message) {
  m_download_handle = 0;
  m_download_kind = -1;
  m_download_sources.clear();
  m_install_queue.clear();
  m_install_index = 0;
  m_source_index = 0;
  m_cancel_requested = false;
  m_download_temp.clear();
  UpdateDataStatus();
  SetInstallationControls(false);
  if (!message.empty())
    wxMessageBox(
        message,
        success ? _("Astronomy data ready") : _("Astronomy-data installation"),
        wxOK | (success ? wxICON_INFORMATION : wxICON_WARNING), this);
}

void EclipseDialog::StartInstalledDataCheck() {
  m_installed_check_queue.clear();
  const EclipseDataKind kinds[] = {EclipseDataKind::De440s,
                                   EclipseDataKind::LunarOrientation,
                                   EclipseDataKind::LolaLimb};
  for (int index = 0; index < 3; ++index) {
    if (celestial_navigation::InspectInstalledData(kinds[index],
                                                   DataPath(kinds[index])) ==
        InstalledDataState::VerificationRequired)
      m_installed_check_queue.push_back(kinds[index]);
  }
  m_installed_check_index = 0;
  StartNextInstalledDataCheck();
}

void EclipseDialog::StartNextInstalledDataCheck() {
  if (m_installed_check_index >= m_installed_check_queue.size()) {
    m_verification_purpose = VERIFY_NONE;
    UpdateDataStatus();
    return;
  }
  const EclipseDataKind kind = m_installed_check_queue[m_installed_check_index];
  BeginVerification(kind, DataPath(kind), VERIFY_INSTALLED);
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
  m_latitude->SetValue(
      FormatNavigationAngle(latitude, NavigationAngleKind::Latitude, true));
  m_longitude->SetValue(
      FormatNavigationAngle(longitude, NavigationAngleKind::Longitude, true));
}

bool EclipseDialog::Observer(eclipse::GeoPoint* observer) const {
  if (!observer) return false;
  double latitude = 0.0, longitude = 0.0;
  if (!ParseNavigationAngle(m_latitude->GetValue(),
                            NavigationAngleKind::Latitude, -90.0, 90.0,
                            &latitude) ||
      !ParseNavigationAngle(m_longitude->GetValue(),
                            NavigationAngleKind::Longitude, -180.0, 180.0,
                            &longitude))
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
  m_latitude->ChangeValue(FormatNavigationAngle(
      observer.latitude_deg, NavigationAngleKind::Latitude, true));
  m_longitude->ChangeValue(FormatNavigationAngle(
      observer.longitude_deg, NavigationAngleKind::Longitude, true));
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
  text << "Observer: " << FormatLocation(observer) << "\n";
  for (int index = 0; index < 5; ++index) {
    if (!times[index]->valid) continue;
    text << wxString::Format(
        "%-3s  %s   Sun alt %s  az %7.3f%c\n", names[index],
        FormatDateTime(times[index]->tt_jd, event.delta_t_seconds, false),
        FormatNavigationAngle(times[index]->sun_altitude_deg).c_str(),
        times[index]->sun_azimuth_deg, 0x00b0);
  }
  text << wxString::Format(
      "\nType: %s   magnitude %.5f   obscuration %.3f%%\n",
      wxString::FromUTF8(eclipse::EclipseTypeName(contacts.type)),
      contacts.magnitude, contacts.obscuration * 100.0);
  if (contacts.c2.valid && contacts.c3.valid)
    text << wxString::Format("Central duration: %.2f seconds\n",
                             contacts.central_duration_seconds);
  text << wxString::Format(CN_UTF8_("ΔT model: %.2f seconds"),
                           event.delta_t_seconds);
  if (contacts.limb_adjusted)
    text << CN_UTF8_("   |   C1–C4 refined from LOLA terrain");
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
  m_latitude->SetValue(
      FormatNavigationAngle(25.505, NavigationAngleKind::Latitude, true));
  m_longitude->SetValue(
      FormatNavigationAngle(33.183333, NavigationAngleKind::Longitude, true));
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
