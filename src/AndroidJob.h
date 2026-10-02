// Android worker handoff with a responsive, owned, cancellable modal surface.
#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidSurface.h"
#include <wx/gauge.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <exception>
#include <string>
#include <fstream>
#include <memory>
#include <sstream>
#include <cstdint>

namespace celestial_android {
struct JobCancelled : std::exception {};
struct JobState {
  std::atomic<bool> cancel{false}, done{false}, committed{false};
  std::mutex mutex;
  std::string message;
  void Checkpoint() const { if (!committed.load() && cancel.load()) throw JobCancelled(); }
  bool Commit(const std::function<bool()>& operation) {
    std::lock_guard<std::mutex> guard(mutex);
    Checkpoint();
    const bool ok = operation();
    if (ok) committed.store(true);
    return ok;
  }
  void Progress(const std::string& text) {
    Checkpoint(); std::lock_guard<std::mutex> guard(mutex); message = text;
  }
};
inline std::uint64_t AvailableMemoryBytes() {
  std::ifstream memory("/proc/meminfo");
  std::string line;
  while (std::getline(memory, line)) {
    if (line.compare(0, 13, "MemAvailable:") != 0) continue;
    std::istringstream fields(line.substr(13));
    std::uint64_t kib = 0; fields >> kib; return kib * 1024;
  }
  return 0; // Unknown is reported in the audit, not fabricated as zero RAM.
}
inline bool CheckHeadroom(std::uint64_t workingBytes, wxString* error) {
  const auto available = AvailableMemoryBytes();
  const std::uint64_t reserve = 256ULL * 1024 * 1024;
  if (available && (available < reserve || workingBytes > available - reserve)) {
    if (error) *error = wxString::Format(_("Insufficient physical memory for this calculation: %.0f MB available, %.0f MB estimated work plus 256 MB reserved for OpenCPN. Close other activity or choose a smaller request; calculation precision is unchanged."),
        available / 1048576.0, workingBytes / 1048576.0);
    return false;
  }
  return true;
}
inline bool RunJob(wxWindow* parent, const wxString& title,
                   const std::function<void(JobState&)>& work,
                   wxString* error = nullptr) {
  if (!CheckHeadroom(64ULL * 1024 * 1024, error)) return false;
  static std::atomic<bool> occupied{false};
  if (occupied.exchange(true)) {
    if (error) *error = _("Another calculation is running. Finish or cancel it first.");
    return false;
  }
  struct Admission { std::atomic<bool>& busy; ~Admission() { busy.store(false); } } admission{occupied};
  JobState state;
  std::string failure;
  wxDialog sheet(parent, wxID_ANY, title);
  auto* layout = new wxBoxSizer(wxVERTICAL);
  auto* status = new wxStaticText(&sheet, wxID_ANY, _("Starting calculation..."));
  layout->Add(status, 0, wxEXPAND | wxALL, 16);
  auto* gauge = new wxGauge(&sheet, wxID_ANY, 100);
  layout->Add(gauge, 0, wxEXPAND | wxALL, 16);
  auto* explanation = new wxStaticText(&sheet, wxID_ANY,
      _("Inputs remain unchanged until the calculation completes. Cancel stops the work safely."));
  layout->Add(explanation, 0, wxEXPAND | wxALL, 16);
  sheet.SetSizer(layout);
  auto cancel = [&]() {
    std::lock_guard<std::mutex> guard(state.mutex);
    if (!state.committed.load()) { state.cancel.store(true); status->SetLabel(_("Cancelling...")); }
  };
  sheet.GetHandle()->setProperty("cnCancellable", true);
  Decorate(&sheet, title, cancel);
  sheet.Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event) { cancel(); event.Veto(); });
  bool cancelled = false;
  std::thread worker;
  try { worker = std::thread([&]() {
    try { state.Checkpoint(); work(state); state.Checkpoint(); }
    catch (const JobCancelled&) { cancelled = true; }
    catch (const std::exception& ex) { failure = ex.what(); }
    catch (...) { failure = "The calculation failed unexpectedly."; }
    state.done.store(true);
  }); } catch (const std::exception& ex) {
    if (error) *error = wxString::FromUTF8(ex.what());
    return false;
  }
  QTimer timer(sheet.GetHandle());
  QObject::connect(&timer, &QTimer::timeout, &timer, [&]() {
    if (state.done.load()) { timer.stop(); sheet.EndModal(wxID_OK); return; }
    gauge->Pulse();
    if (!state.cancel.load()) {
      std::lock_guard<std::mutex> guard(state.mutex);
      if (!state.message.empty()) status->SetLabel(wxString::FromUTF8(state.message.c_str()));
    }
  });
  timer.start(50);
  sheet.ShowModal();
  // The owned modal remains alive until done; join never waits on active work.
  worker.join();
  if (error && !failure.empty()) *error = wxString::FromUTF8(failure.c_str());
  return !cancelled && (state.committed.load() || !state.cancel.load()) && failure.empty();
}
} // namespace celestial_android
#endif
