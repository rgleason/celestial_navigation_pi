// Celestial Navigation Lab 0.1: an offline, standalone comparison front end.
// The calculations below call the same Sight and DE440s provider as the plugin.
#include "NavigationEphemerisProvider.h"
#include "Sight.h"
#include "UtcDateTime.h"
#include "mock_plugin_api.h"

#include <wx/app.h>
#include <wx/fileconf.h>
#include <wx/filedlg.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

namespace {

constexpr int kRows = 6;
const wxString kNames[kRows] = {"GHA (deg)", "Declination (deg)",
                                "Ho (deg)",  "Hc (deg)",
                                "Zn (deg)",  "Intercept (NM)"};

struct Result {
  bool ok = false;
  bool used_de = false;
  double value[kRows] = {};
  wxString detail;
  wxString error;
};

bool ReadNumber(wxTextCtrl* field, double* number) {
  return field->GetValue().ToDouble(number) && std::isfinite(*number);
}

wxString Number(double x) { return wxString::Format("%.7f", x); }

wxDateTime ParseUtc(const wxString& date, const wxString& clock) {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0;
  double seconds = 0.0;
  if (std::sscanf(date.ToStdString().c_str(), "%d-%d-%d", &year, &month,
                  &day) != 3 ||
      std::sscanf(clock.ToStdString().c_str(), "%d:%d:%lf", &hour, &minute,
                  &seconds) != 3 ||
      year < 1600 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || seconds < 0.0 ||
      seconds >= 60.0)
    return wxDateTime();
  const int whole = static_cast<int>(seconds);
  const int millis = static_cast<int>(std::lround((seconds - whole) * 1000));
  if (millis >= 1000) return wxDateTime();
  wxDateTime utc(day, static_cast<wxDateTime::Month>(month - 1), year, hour,
                 minute, whole, millis);
  return utc.IsValid() ? utc : wxDateTime();
}

class LabFrame final : public wxFrame {
public:
  LabFrame()
      : wxFrame(nullptr, wxID_ANY, "Celestial Navigation Lab 0.1",
                wxDefaultPosition, wxSize(1050, 830)) {
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition,
                                        wxDefaultSize, wxHSCROLL | wxVSCROLL);
    scroll->SetScrollRate(0, 12);
    auto* content = new wxBoxSizer(wxVERTICAL);

    auto* data = new wxStaticBoxSizer(wxVERTICAL, scroll, "Offline data");
    auto* data_row = new wxBoxSizer(wxHORIZONTAL);
    kernel_ = new wxTextCtrl(scroll, wxID_ANY);
    data_row->Add(new wxStaticText(scroll, wxID_ANY, "DE440s file"), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    data_row->Add(kernel_, 1, wxRIGHT, 8);
    auto* browse = new wxButton(scroll, wxID_ANY, "Choose…");
    data_row->Add(browse);
    data->Add(data_row, 0, wxEXPAND | wxALL, 6);
    status_ = new wxStaticText(
        scroll, wxID_ANY,
        "DE440s is optional. Analytical calculations work without it.");
    data->Add(status_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 7);
    content->Add(data, 0, wxEXPAND | wxALL, 8);

    auto* input = new wxStaticBoxSizer(wxVERTICAL, scroll, "Single test");
    auto* grid = new wxFlexGridSizer(4, 8, 9);
    grid->AddGrowableCol(1, 1);
    grid->AddGrowableCol(3, 1);
    type_ = Choice(grid, scroll, "Test type",
                   {"Altitude sight", "Ephemeris / look angle"});
    body_ = Choice(grid, scroll, "Body", {"Sun", "Moon", "Mercury", "Venus"});
    mode_ = Choice(grid, scroll, "Primary mode",
                   {"Automatic (2.9)", "DE440s required", "Analytical"});
    date_ = Field(grid, scroll, "UTC date (YYYY-MM-DD)", "2024-06-13");
    time_ = Field(grid, scroll, "UTC time (hh:mm:ss.sss)", "19:26:00.000");
    lat_ = Field(grid, scroll, "Latitude (+N, deg)", "41.35");
    lon_ = Field(grid, scroll, "Longitude (+E, deg)", "-71.48");
    hs_ = Field(grid, scroll, "Sextant altitude (deg)", "30.0");
    limb_ = Choice(grid, scroll, "Limb", {"Lower", "Centre", "Upper"});
    eye_ = Field(grid, scroll, "Eye height (m)", "2.5");
    index_ = Field(grid, scroll, "Index correction (arcmin)", "0.0");
    pressure_ = Field(grid, scroll, "Pressure (hPa)", "1013.0");
    temperature_ = Field(grid, scroll, "Temperature (°C)", "15.0");
    input->Add(grid, 0, wxEXPAND | wxALL, 8);
    auto* source_row = new wxBoxSizer(wxHORIZONTAL);
    source_row->Add(
        new wxStaticText(scroll, wxID_ANY, "Reference source / page"), 0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    source_ = new wxTextCtrl(scroll, wxID_ANY);
    source_row->Add(source_, 1);
    input->Add(source_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    content->Add(input, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* calculate = new wxButton(scroll, wxID_ANY, "Calculate");
    auto* save = new wxButton(scroll, wxID_ANY, "Save test…");
    auto* open = new wxButton(scroll, wxID_ANY, "Open test…");
    auto* export_button = new wxButton(scroll, wxID_ANY, "Export results…");
    actions->Add(calculate, 0, wxRIGHT, 8);
    actions->Add(save, 0, wxRIGHT, 8);
    actions->Add(open, 0, wxRIGHT, 8);
    actions->Add(export_button);
    content->Add(actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* results_box =
        new wxStaticBoxSizer(wxVERTICAL, scroll, "Calculation results");
    note_ = new wxStaticText(scroll, wxID_ANY,
                             "Reference cells are editable. Enter decimal "
                             "degrees; intercept is NM.");
    results_box->Add(note_, 0, wxALL, 6);
    results_ = new wxGrid(scroll, wxID_ANY);
    results_->CreateGrid(kRows, 5);
    const wxString headings[] = {"Reference", "Analytical", "DE440s",
                                 "Analytical − ref", "DE440s − ref"};
    for (int c = 0; c < 5; ++c) results_->SetColLabelValue(c, headings[c]);
    for (int r = 0; r < kRows; ++r) {
      results_->SetRowLabelValue(r, kNames[r]);
      for (int c = 1; c < 5; ++c) results_->SetReadOnly(r, c);
    }
    results_->SetRowLabelSize(175);
    results_->SetDefaultColSize(140);
    results_box->Add(results_, 0, wxEXPAND | wxALL, 6);
    primary_ = new wxStaticText(scroll, wxID_ANY, "No calculation yet.");
    results_box->Add(primary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    details_ =
        new wxTextCtrl(scroll, wxID_ANY, wxEmptyString, wxDefaultPosition,
                       wxSize(-1, 175), wxTE_MULTILINE | wxTE_READONLY);
    results_box->Add(details_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    content->Add(results_box, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    scroll->SetSizer(content);
    root->Add(scroll, 1, wxEXPAND);
    SetSizer(root);
    SetMinSize(wxSize(940, 620));

    browse->Bind(wxEVT_BUTTON, &LabFrame::ChooseKernel, this);
    calculate->Bind(wxEVT_BUTTON, &LabFrame::Calculate, this);
    save->Bind(wxEVT_BUTTON, &LabFrame::Save, this);
    open->Bind(wxEVT_BUTTON, &LabFrame::Open, this);
    export_button->Bind(wxEVT_BUTTON, &LabFrame::Export, this);
    kernel_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { CheckKernel(); });

    const wxString resources = wxStandardPaths::Get().GetResourcesDir();
    wxString data_dir = resources;
    if (!wxFileExists(data_dir + "/data/vsop87d.txt"))
      data_dir = wxString::FromUTF8(CELNAV_LAB_SOURCE_DIR);
    SetTestPluginDataRoot(data_dir);
    SetTestPrivateDataPath(wxStandardPaths::Get().GetUserDataDir());
    const wxString local_kernel = data_dir + "/eclipse/data/de440s.bsp";
    wxString selected_kernel;
    if (wxGetEnv("CELNAV_LAB_DE440_PATH", &selected_kernel) &&
        !selected_kernel.empty())
      kernel_->SetValue(selected_kernel);
    else if (wxFileExists(local_kernel))
      kernel_->SetValue(local_kernel);
    CheckKernel();
  }

  bool SmokeTest() {
    wxCommandEvent event(wxEVT_BUTTON);
    Calculate(event);
    if (!analytical_.ok) return false;
    if (!kernel_->GetValue().empty() && !de_.ok) return false;
    const bool verified_de = de_.ok;
    const double de_hc = de_.value[3];
    const wxString selected_kernel = kernel_->GetValue();
    kernel_->SetValue(wxEmptyString);
    mode_->SetSelection(1);
    Calculate(event);
    const bool explicit_failure =
        !de_.ok && analytical_.ok &&
        primary_->GetLabel().StartsWith("Primary result unavailable");
    mode_->SetSelection(0);
    Calculate(event);
    const bool auto_fallback =
        primary_->GetLabel().Contains("analytical fallback");
    kernel_->SetValue(selected_kernel);
    std::printf(
        "Celestial Navigation Lab smoke: analytical Hc=%.7f; "
        "DE440s Hc=%s; forced-DE failure=%d; automatic fallback=%d\n",
        analytical_.value[3],
        verified_de ? Number(de_hc).ToStdString().c_str() : "unavailable",
        explicit_failure, auto_fallback);
    return explicit_failure && auto_fallback;
  }

private:
  static wxTextCtrl* Field(wxFlexGridSizer* grid, wxWindow* parent,
                           const wxString& label, const wxString& value) {
    grid->Add(new wxStaticText(parent, wxID_ANY, label), 0,
              wxALIGN_CENTER_VERTICAL);
    auto* field = new wxTextCtrl(parent, wxID_ANY, value);
    grid->Add(field, 1, wxEXPAND);
    return field;
  }

  static wxChoice* Choice(wxFlexGridSizer* grid, wxWindow* parent,
                          const wxString& label,
                          std::initializer_list<wxString> options) {
    grid->Add(new wxStaticText(parent, wxID_ANY, label), 0,
              wxALIGN_CENTER_VERTICAL);
    auto* choice = new wxChoice(parent, wxID_ANY);
    for (const auto& option : options) choice->Append(option);
    choice->SetSelection(0);
    grid->Add(choice, 1, wxEXPAND);
    return choice;
  }

  void ChooseKernel(wxCommandEvent&) {
    wxFileDialog dialog(this, "Choose DE440s.bsp", wxEmptyString, wxEmptyString,
                        "SPK kernel (*.bsp)|*.bsp|All files|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) kernel_->SetValue(dialog.GetPath());
  }

  void CheckKernel() {
    const wxString path = kernel_->GetValue();
    wxSetEnv("CELNAV_LAB_DE440_PATH", path);
    if (path.empty()) {
      status_->SetLabel(
          "No DE440s selected; analytical mode remains available.");
      return;
    }
    celestial_navigation::De440NavigationSample sample;
    std::string why;
    const wxDateTime epoch = ParseUtc("2024-01-01", "00:00:00.000");
    if (celestial_navigation::TryDe440NavigationSample("Sun", epoch, &sample,
                                                       &why))
      status_->SetLabel(
          "DE440s verified locally. Coverage: 1850–2150; UTC "
          "conversion in this app requires 1972 or later.");
    else
      status_->SetLabel("DE440s unavailable: " + wxString::FromUTF8(why));
    Layout();
  }

  Result Run(bool allow_de, const wxDateTime& utc, double lat, double lon,
             double hs, double eye, double index, double pressure,
             double temperature) {
    Result output;
    const wxString body = body_->GetStringSelection();
    const Sight::BodyLimb limb = limb_->GetSelection() == 0   ? Sight::LOWER
                                 : limb_->GetSelection() == 2 ? Sight::UPPER
                                                              : Sight::CENTER;
    if (allow_de) {
      celestial_navigation::De440NavigationSample sample;
      std::string reason;
      if (!celestial_navigation::TryDe440NavigationSample(body, utc, &sample,
                                                          &reason)) {
        output.error = wxString::FromUTF8(reason);
        return output;
      }
    }
    Sight sight(Sight::ALTITUDE, body, limb, utc, hs, 0.0, 1.0);
    sight.m_AllowDe440 = allow_de;
    sight.m_CorrectedDateTime = utc;
    sight.m_DRLat = lat;
    sight.m_DRLon = lon;
    sight.m_EyeHeight = eye;
    sight.m_IndexError = index;
    sight.m_Pressure = pressure;
    sight.m_Temperature = temperature;
    sight.m_ObservedAltitude = std::numeric_limits<double>::quiet_NaN();
    const bool altitude_sight = type_->GetSelection() == 0;
    if (altitude_sight) sight.RecomputeAltitude();
    double dec = 0, gp_lon = 0, aries = 0, range = 0, distance = 0;
    sight.BodyLocation(utc, &dec, &gp_lon, &aries, &range, &distance, false,
                       allow_de, std::numeric_limits<double>::quiet_NaN(),
                       &output.used_de);
    double hc = 0, zn = 0;
    sight.CalculateAtDR(&hc, &zn);
    const double gha = std::fmod(360.0 - gp_lon, 360.0);
    output.value[0] = gha;
    output.value[1] = dec;
    output.value[2] = altitude_sight ? sight.m_ObservedAltitude
                                     : std::numeric_limits<double>::quiet_NaN();
    output.value[3] = hc;
    output.value[4] = zn;
    output.value[5] = altitude_sight ? (sight.m_ObservedAltitude - hc) * 60.0
                                     : std::numeric_limits<double>::quiet_NaN();
    output.detail =
        altitude_sight
            ? sight.m_CalcStr
            : "GHA, declination and look angle from the selected body model. "
              "No sextant corrections were applied.";
    output.ok = true;
    for (int r = 0; r < kRows; ++r)
      if ((altitude_sight || (r != 2 && r != 5)) &&
          !std::isfinite(output.value[r]))
        output.ok = false;
    if (!output.ok) output.error = "Calculation produced a non-finite value";
    if (allow_de && !output.used_de) {
      output.ok = false;
      output.error = "DE440s was requested but the sight used analytical data";
    }
    return output;
  }

  void Calculate(wxCommandEvent&) {
    const wxDateTime utc = ParseUtc(date_->GetValue(), time_->GetValue());
    double lat, lon, hs, eye, index, pressure, temperature;
    const bool altitude_sight = type_->GetSelection() == 0;
    if (!utc.IsValid() || !ReadNumber(lat_, &lat) || !ReadNumber(lon_, &lon) ||
        std::abs(lat) > 90 || std::abs(lon) > 180) {
      wxMessageBox("Check the UTC, coordinates and sight inputs.",
                   "Invalid test", wxOK | wxICON_WARNING, this);
      return;
    }
    if (!ReadNumber(hs_, &hs) || !ReadNumber(eye_, &eye) ||
        !ReadNumber(index_, &index) || !ReadNumber(pressure_, &pressure) ||
        !ReadNumber(temperature_, &temperature) ||
        (altitude_sight && (hs < -10 || hs > 90 || eye < 0 || pressure <= 0 ||
                            temperature <= -100))) {
      wxMessageBox("Check the sight and weather inputs.", "Invalid test",
                   wxOK | wxICON_WARNING, this);
      return;
    }
    analytical_ =
        Run(false, utc, lat, lon, hs, eye, index, pressure, temperature);
    de_ = Run(true, utc, lat, lon, hs, eye, index, pressure, temperature);
    const Result& selected = mode_->GetSelection() == 2   ? analytical_
                             : mode_->GetSelection() == 1 ? de_
                             : de_.ok                     ? de_
                                                          : analytical_;
    const wxString selected_name =
        mode_->GetSelection() == 0 ? (de_.ok ? "DE440s" : "analytical fallback")
        : mode_->GetSelection() == 1 ? "DE440s required"
                                     : "analytical";
    primary_->SetLabel(selected.ok
                           ? "Primary result: " + selected_name
                           : "Primary result unavailable: " + selected.error);
    for (int r = 0; r < kRows; ++r) {
      const bool applicable = altitude_sight || (r != 2 && r != 5);
      results_->SetCellValue(r, 1,
                             !applicable      ? "N/A"
                             : analytical_.ok ? Number(analytical_.value[r])
                                              : "Unavailable");
      results_->SetCellValue(r, 2,
                             !applicable ? "N/A"
                             : de_.ok    ? Number(de_.value[r])
                                         : "Unavailable");
      double reference = 0;
      const bool has_ref = results_->GetCellValue(r, 0).ToDouble(&reference);
      const double scale = r == 5 ? 1.0 : 60.0;
      const auto difference = [r, reference, scale](double calculated) {
        double delta = calculated - reference;
        if (r == 0 || r == 4) delta = std::remainder(delta, 360.0);
        return Number(delta * scale);
      };
      results_->SetCellValue(r, 3,
                             analytical_.ok && applicable && has_ref
                                 ? difference(analytical_.value[r])
                                 : wxString());
      results_->SetCellValue(r, 4,
                             de_.ok && applicable && has_ref
                                 ? difference(de_.value[r])
                                 : wxString());
    }
    details_->SetValue(
        "Differences are arcminutes except intercept differences (NM).\n"
        "Reference must use matching body, time, limb and angle "
        "conventions.\n\n"
        "DE440s: " +
        (de_.ok ? "available" : de_.error) + "\n\nPrimary calculation:\n" +
        selected.detail);
    results_->ForceRefresh();
  }

  void Save(wxCommandEvent&) {
    wxFileDialog dialog(this, "Save test", wxEmptyString, wxEmptyString,
                        "Celestial Lab test (*.cnlab)|*.cnlab",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    wxFileConfig config("Celestial Navigation Lab", wxEmptyString,
                        dialog.GetPath(), wxEmptyString,
                        wxCONFIG_USE_LOCAL_FILE);
    config.DeleteAll();
    config.Write("/schema", 1L);
    config.Write("/source", source_->GetValue());
    config.Write("/kernel", kernel_->GetValue());
    config.Write("/body", body_->GetStringSelection());
    config.Write("/type", type_->GetSelection());
    config.Write("/mode", mode_->GetSelection());
    config.Write("/limb", limb_->GetSelection());
    const wxString keys[] = {"date", "time",  "lat",      "lon",        "hs",
                             "eye",  "index", "pressure", "temperature"};
    wxTextCtrl* fields[] = {date_, time_,  lat_,      lon_,        hs_,
                            eye_,  index_, pressure_, temperature_};
    for (int i = 0; i < 9; ++i)
      config.Write("/" + keys[i], fields[i]->GetValue());
    for (int r = 0; r < kRows; ++r)
      config.Write(wxString::Format("/reference/%d", r),
                   results_->GetCellValue(r, 0));
    if (!config.Flush())
      wxMessageBox("Could not save the test.", "Save error",
                   wxOK | wxICON_ERROR, this);
  }

  void Open(wxCommandEvent&) {
    wxFileDialog dialog(this, "Open test", wxEmptyString, wxEmptyString,
                        "Celestial Lab test (*.cnlab)|*.cnlab",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    wxFileConfig config("Celestial Navigation Lab", wxEmptyString,
                        dialog.GetPath(), wxEmptyString,
                        wxCONFIG_USE_LOCAL_FILE);
    long schema = 0;
    if (!config.Read("/schema", &schema) || schema != 1) {
      wxMessageBox("Unknown or invalid test format.", "Open error",
                   wxOK | wxICON_ERROR, this);
      return;
    }
    wxString value;
    config.Read("/source", &value);
    source_->SetValue(value);
    config.Read("/kernel", &value);
    kernel_->SetValue(value);
    config.Read("/body", &value);
    body_->SetStringSelection(value);
    long option = 0;
    config.Read("/type", &option);
    type_->SetSelection(option >= 0 && option < 2 ? option : 0);
    config.Read("/mode", &option);
    mode_->SetSelection(option >= 0 && option < 3 ? option : 0);
    config.Read("/limb", &option);
    limb_->SetSelection(option >= 0 && option < 3 ? option : 0);
    const wxString keys[] = {"date", "time",  "lat",      "lon",        "hs",
                             "eye",  "index", "pressure", "temperature"};
    wxTextCtrl* fields[] = {date_, time_,  lat_,      lon_,        hs_,
                            eye_,  index_, pressure_, temperature_};
    for (int i = 0; i < 9; ++i)
      if (config.Read("/" + keys[i], &value)) fields[i]->SetValue(value);
    for (int r = 0; r < kRows; ++r) {
      config.Read(wxString::Format("/reference/%d", r), &value, wxEmptyString);
      results_->SetCellValue(r, 0, value);
    }
    wxCommandEvent calculate(wxEVT_BUTTON);
    Calculate(calculate);
  }

  void Export(wxCommandEvent&) {
    wxFileDialog dialog(this, "Export comparison", wxEmptyString, wxEmptyString,
                        "Tab-separated values (*.tsv)|*.tsv",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    wxFFile file(dialog.GetPath(), "w");
    if (!file.IsOpened()) {
      wxMessageBox("Could not create the export.", "Export error",
                   wxOK | wxICON_ERROR, this);
      return;
    }
    file.Write("Celestial Navigation Lab 0.1\n");
    file.Write("Source\t" + source_->GetValue() + "\n");
    file.Write("UTC\t" + date_->GetValue() + "T" + time_->GetValue() + "Z\n");
    file.Write("Body\t" + body_->GetStringSelection() + "\n");
    file.Write("Test type\t" + type_->GetStringSelection() + "\n");
    file.Write("DE440s file\t" + kernel_->GetValue() + "\n");
    file.Write(
        "Quantity\tReference\tAnalytical\tDE440s\t"
        "Analytical minus reference\tDE440s minus reference\n");
    for (int r = 0; r < kRows; ++r) {
      wxString row = kNames[r];
      for (int c = 0; c < 5; ++c) row += "\t" + results_->GetCellValue(r, c);
      file.Write(row + "\n");
    }
    file.Close();
  }

  wxChoice *type_ = nullptr, *body_ = nullptr, *mode_ = nullptr;
  wxChoice* limb_ = nullptr;
  wxTextCtrl *kernel_ = nullptr, *date_ = nullptr, *time_ = nullptr;
  wxTextCtrl *lat_ = nullptr, *lon_ = nullptr, *hs_ = nullptr;
  wxTextCtrl *eye_ = nullptr, *index_ = nullptr, *pressure_ = nullptr;
  wxTextCtrl *temperature_ = nullptr, *source_ = nullptr, *details_ = nullptr;
  wxStaticText *status_ = nullptr, *note_ = nullptr, *primary_ = nullptr;
  wxGrid* results_ = nullptr;
  Result analytical_, de_;
};

class LabApp final : public wxApp {
public:
  bool OnInit() override {
    auto* window = new LabFrame();
    if (argc > 1 && wxString(argv[1]) == "--smoke-test") {
      const bool passed = window->SmokeTest();
      window->Destroy();
      std::exit(passed ? 0 : 1);
    }
    window->Show();
    return true;
  }
};

}  // namespace

wxIMPLEMENT_APP(LabApp);
