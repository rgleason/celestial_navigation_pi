#include "AndroidFileDialog.h"
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
#ifdef __OCPN__ANDROID__
#include <QTimer>
#include <memory>
#include <mutex>
#endif

namespace celestial_navigation {
namespace {
#ifdef __OCPN__ANDROID__
// POBsoft (1985-2026): the pinned Android wx/Qt loop does not dispatch a
// standalone handler's pending queue. Own the host events and drain them on
// the panel's Qt GUI timer, without pumping unrelated application events.
class AndroidDownloadEvents : public wxEvtHandler {
 public:
  void QueueEvent(wxEvent* event) override {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.emplace_back(event);
  }
  void Drain() {
    std::vector<std::unique_ptr<wxEvent>> events;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      events.swap(events_);
    }
    for (auto& event : events) SafelyProcessEvent(*event);
  }
  void Discard() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
  }
 private:
  std::mutex mutex_;
  std::vector<std::unique_ptr<wxEvent>> events_;
};
#endif
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
#ifdef __OCPN__ANDROID__
    auto* buttons=new wxBoxSizer(wxVERTICAL);
#else
    auto* buttons=new wxBoxSizer(wxHORIZONTAL);
#endif
    download_=new wxButton(this,wxID_ANY,CN_UTF8_("Check / download update…"));
    import_=new wxButton(this,wxID_ANY,CN_UTF8_("Import local file…"));
#ifdef __OCPN__ANDROID__
    buttons->Add(download_,0,wxEXPAND|wxBOTTOM,12);
    buttons->Add(import_,0,wxEXPAND|wxBOTTOM,12);
#else
    buttons->Add(download_,0,wxRIGHT,12);
    buttons->Add(import_,0);
#endif
#ifdef __OCPN__ANDROID__
    // POBsoft (1985-2026): one cancellable background transfer belongs to
    // this panel; the Android GUI must never wait in the synchronous JNI API.
    cancel_=new wxButton(this,wxID_ANY,_("Cancel download"));
    buttons->Add(cancel_,0,wxEXPAND);
    cancel_->Hide();
    cancel_->Bind(wxEVT_BUTTON,[this](wxCommandEvent&) {
      FinishDownload(false,_("Download cancelled. Existing offline data is unchanged."));
    });
    download_events_.Connect(wxID_ANY,wxEVT_DOWNLOAD_EVENT,
            wxEventHandler(UpdatePanel::OnDownloadEvent),nullptr,this);
    download_poll_=new QTimer(GetHandle());
    QObject::connect(download_poll_,&QTimer::timeout,GetHandle(),[this]() {
      download_events_.Drain();
    });
    timeout_=new QTimer(GetHandle());
    timeout_->setSingleShot(true);
    QObject::connect(timeout_,&QTimer::timeout,GetHandle(),[this]() {
      FinishDownload(false,_("Download timed out. Existing offline data is unchanged."));
    });
#endif
    layout->Add(buttons,0,
#ifdef __OCPN__ANDROID__
                wxEXPAND|
#endif
                wxLEFT|wxRIGHT|wxBOTTOM,18);
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
      CelestialFileDialog dialog(this,_("Import IERS Earth-rotation data"),wxEmptyString,
                          "finals2000A.all",_("All files (*)|*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST);
      if (dialog.ShowModal()==wxID_OK) Install(dialog.GetPath());
    });
  }
#ifdef __OCPN__ANDROID__
  ~UpdatePanel() override {
    download_events_.Disconnect(wxID_ANY,wxEVT_DOWNLOAD_EVENT,
               wxEventHandler(UpdatePanel::OnDownloadEvent),nullptr,this);
    StopDownload();
    delete download_poll_;
    delete timeout_;
  }
#endif
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
#ifdef __OCPN__ANDROID__
    if (downloading_) return;
    download_temp_=wxFileName::CreateTempFileName("celestial-dut1-");
    if (download_temp_.empty()) {
      SetText(message_,_("Could not create a download file. Existing data is unchanged."));
      return;
    }
    downloading_=true;
    download_->Disable(); import_->Disable(); cancel_->Show();
    SetText(message_,CN_UTF8_("Downloading IERS Earth-rotation data…"));
    timeout_->start(30000);
    download_poll_->start(50);
    const auto status=OCPN_downloadFileBackground(kUrl,download_temp_,&download_events_,&download_handle_);
    if (status==OCPN_DL_NO_ERROR) FinishDownload(true,wxEmptyString);
    else if (status!=OCPN_DL_STARTED)
      FinishDownload(false,_("Download failed. Existing offline data is unchanged."));
#else
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
#endif
  }
#ifdef __OCPN__ANDROID__
  void StopDownload() {
    timeout_->stop();
    download_poll_->stop();
    downloading_=false;
    if (download_handle_) OCPN_cancelDownloadFileBackground(download_handle_);
    download_handle_=0;
    // Discard queued events from this completed/cancelled generation before
    // admitting another transfer. This handler owns download events only.
    download_events_.Discard();
    if (!download_temp_.empty() && wxFileExists(download_temp_))
      wxRemoveFile(download_temp_);
    download_temp_.clear();
  }
  void FinishDownload(bool success,const wxString& message) {
    if (!downloading_) return;
    if (success) Install(download_temp_);
    else SetText(message_,message);
    StopDownload();
    download_->Enable(); import_->Enable(); cancel_->Hide();
    Rewrap();
  }
  void OnDownloadEvent(wxEvent& raw) {
    if (!downloading_) return;
    auto& event=static_cast<OCPN_downloadEvent&>(raw);
    if (event.getDLEventCondition()==OCPN_DL_EVENT_TYPE_PROGRESS) {
      SetText(message_,wxString::Format(_("Downloading IERS data: %ld of %ld bytes."),
                                        event.getTransferred(),event.getTotal()));
    } else if (event.getDLEventCondition()==OCPN_DL_EVENT_TYPE_END) {
      download_handle_=0;  // The host has finished this transfer.
      FinishDownload(event.getDLEventStatus()==OCPN_DL_NO_ERROR,
          _("Download failed or was cancelled. Existing offline data is unchanged."));
    }
  }
  wxButton* cancel_;
  QTimer* timeout_;
  QTimer* download_poll_;
  bool downloading_=false;
  long download_handle_=0;
  wxString download_temp_;
  AndroidDownloadEvents download_events_;
#endif
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
