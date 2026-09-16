#include "Dut1UpdatePanel.h"
#include "Utf8Translation.h"
#include "AtomicXmlFile.h"
#include "celestial_navigation_pi.h"
#include "eclipse/dut1.h"
#include "eclipse/time.h"
#include <wx/button.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <algorithm>
#include <vector>

namespace celestial_navigation {
namespace {
const wxString kUrl = "https://datacenter.iers.org/data/9/finals2000A.all";
wxString UpdatePath() {
  return GetpPrivateApplicationDataLocation()
      ? celestial_navigation_pi::StandardPath()+"earth-rotation/finals2000A.all"
      : wxString();
}
wxString Date(double jd) {
  const auto date=eclipse::JulianDateToCalendar(jd);
  return wxString::Format("%04d-%02d-%02d",date.year,date.month,date.day);
}
std::shared_ptr<const eclipse::Dut1Table> ReadTable(const wxString& path, wxString* error,
                                                 std::string* validated=nullptr) {
  wxFile file(path);
  if (!file.IsOpened() || file.Length()<1000000 || file.Length()>8*1024*1024) {
    *error=_("Not a complete IERS finals2000A file (missing or invalid size).");
    return nullptr;
  }
  std::string bytes(static_cast<std::size_t>(file.Length()),'\0');
  if (file.Read(&bytes[0],bytes.size())!=static_cast<wxFileOffset>(bytes.size())) {
    *error=_("Could not read the complete Earth-rotation data file.");
    return nullptr;
  }
  std::string detail;
  auto result=eclipse::ParseDut1Update(bytes,&detail);
  if (!result) *error=wxString::FromUTF8(detail.c_str());
  else if (validated) *validated=std::move(bytes);
  return result;
}
class UpdatePanel : public wxScrolledWindow {
 public:
  explicit UpdatePanel(wxWindow* parent) : wxScrolledWindow(parent) {
    SetScrollRate(0,10);
    auto* layout=new wxBoxSizer(wxVERTICAL);
    layout->AddSpacer(18);
    auto* title=new wxStaticText(this,wxID_ANY,CN_UTF8_("Earth-rotation data (DUT1) — precision updates"));
    title->SetFont(title->GetFont().Bold());
    AddText(layout,title);
    AddText(layout,new wxStaticText(this,wxID_ANY,
        CN_UTF8_("Celestial Navigation works offline. Bundled data already supports high-accuracy "
          "Sun–Moon lunar calculations; downloading an update is optional.")));
    AddText(layout,new wxStaticText(this,wxID_ANY,
        _("DUT1 is the small difference between Earth's rotation time (UT1) and UTC. "
          "IERS measurements and predictions refine the enhanced lunar solver. "
          "This is not a chart, weather or ephemeris download.")));
    status_=new wxStaticText(this,wxID_ANY,wxEmptyString);
    AddText(layout,status_);
    AddText(layout,new wxStaticText(this,wxID_ANY,
        _("There is no data-expiry lockout. Outside all available dates the plugin "
          "still calculates using UT1 = UTC and reports reduced accuracy. "
          "No internet connection is required for sights or solving.")));
    auto* buttons=new wxBoxSizer(wxHORIZONTAL);
    download_=new wxButton(this,wxID_ANY,CN_UTF8_("Check / download update…"));
    import_=new wxButton(this,wxID_ANY,CN_UTF8_("Import local file…"));
    buttons->Add(download_,0,wxRIGHT,12);
    buttons->Add(import_,0);
    layout->Add(buttons,0,wxLEFT|wxRIGHT|wxBOTTOM,18);
    AddText(layout,new wxStaticText(this,wxID_ANY,
        _("Only the download button connects to IERS (about 4 MB). The file is checked "
          "and prepared automatically; no compiler or restart is needed. "
          "For an offline computer, import an IERS finals2000A.all file copied from another computer.")));
    message_=new wxStaticText(this,wxID_ANY,wxEmptyString);
    AddText(layout,message_);
    SetSizer(layout);
    RefreshCoverage();
    Bind(wxEVT_SIZE,[this](wxSizeEvent& event) { Rewrap(); event.Skip(); });
    download_->Bind(wxEVT_BUTTON,[this](wxCommandEvent&) { Download(); });
    import_->Bind(wxEVT_BUTTON,[this](wxCommandEvent&) {
      wxFileDialog dialog(this,_("Import IERS Earth-rotation data"),wxEmptyString,
                          "finals2000A.all",_("All files (*)|*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
      if (dialog.ShowModal()==wxID_OK) Install(dialog.GetPath());
    });
  }
 private:
  void AddText(wxSizer* layout,wxStaticText* text) {
    texts_.push_back({text,text->GetLabel()});
    layout->Add(text,0,wxEXPAND|wxLEFT|wxRIGHT,18);
    layout->AddSpacer(14);
  }
  void SetText(wxStaticText* text,const wxString& value) {
    for (auto& item:texts_) if (item.first==text) item.second=value;
    Rewrap();
  }
  void Rewrap() {
    const int width=std::max(200,std::min(780,GetClientSize().x-40));
    for (auto& item:texts_) { item.first->SetLabel(item.second); item.first->Wrap(width); }
    Layout(); FitInside();
  }
  void RefreshCoverage() {
    wxString status=wxString::Format(_("Bundled coverage: %s to %s UTC (includes predictions)."),
        Date(eclipse::Dut1FirstUtcJd()),Date(eclipse::Dut1LastUtcJd()));
    const auto update=eclipse::GetDut1Update();
    status+=update ? wxString::Format(_("\nInstalled update: through %s UTC (includes predictions)."),
                                     Date(update->LastUtcJd()))
                   : _("\nNo downloaded update installed; the bundled data is ready to use.");
    status+=_("\nEach sight uses data for its own UTC date, not today's date.");
    SetText(status_,status);
  }
  void Install(const wxString& path) {
    wxString message;
    const bool okay=InstallDut1Update(path,UpdatePath(),&message);
    if (!okay) message+=_("\nExisting data is unchanged; offline calculations remain available.");
    SetText(message_,message); RefreshCoverage();
  }
  void Download() {
    download_->Disable(); import_->Disable();
    // The host owns the cancellable progress dialog. Only this explicit action
    // accesses the network; solving and startup never perform a download.
    const wxString temp=wxFileName::CreateTempFileName("celestial-dut1-");
    if (!temp.empty()) {
      const auto status=OCPN_downloadFile(kUrl,temp,_("Earth-rotation precision update"),
          CN_UTF8_("Checking the latest IERS DUT1 table…"),wxNullBitmap,this,
          OCPN_DLDS_CAN_ABORT|OCPN_DLDS_ELAPSED_TIME|OCPN_DLDS_SIZE|OCPN_DLDS_AUTO_CLOSE,30);
      if (status==OCPN_DL_NO_ERROR) Install(temp);
      else SetText(message_,_("Download failed or was cancelled. Existing data is unchanged; "
                              "offline calculations remain available."));
      wxRemoveFile(temp);
    } else SetText(message_,_("Could not create a download file. Existing data is unchanged."));
    download_->Enable(); import_->Enable();
  }
  wxStaticText *status_,*message_;
  wxButton *download_,*import_;
  std::vector<std::pair<wxStaticText*,wxString>> texts_;
};
}

bool InstallDut1Update(const wxString& source,const wxString& destination,wxString* message) {
  wxString detail;
  if (!message) message=&detail;
  std::string bytes;
  const auto candidate=ReadTable(source,message,&bytes);
  if (!candidate) return false;
  const auto active=eclipse::GetDut1Update();
  if (candidate->LastUtcJd()<eclipse::Dut1LastUtcJd() ||
      (active && candidate->LastUtcJd()<active->LastUtcJd())) {
    *message=_("This file has older or incomplete coverage; it was not installed.");
    return false;
  }
  if (active && candidate->Equivalent(*active)) {
    *message=_("Your installed Earth-rotation data is already up to date.");
    return true;
  }
  if (destination.empty()) { *message=_("The plugin data directory is unavailable."); return false; }
  if (!ReplaceFileAtomically(destination,[&](wxTempFile& file) {
        return file.Write(bytes.data(),bytes.size());
      },message)) return false;
  eclipse::SetDut1Update(candidate);
  *message=_("Earth-rotation data validated and installed. It is ready for offline use.");
  return true;
}
void LoadInstalledDut1Update() {
  eclipse::SetDut1Update(nullptr);
  const auto path=UpdatePath();
  if (path.empty() || !wxFileExists(path)) return;
  wxString error;
  const auto table=ReadTable(path,&error);
  if (table) eclipse::SetDut1Update(table);
  else wxLogWarning("Celestial Navigation: Earth-rotation update ignored: %s; using bundled data",error);
}
wxWindow* CreateDut1UpdatePanel(wxWindow* parent) { return new UpdatePanel(parent); }
}
