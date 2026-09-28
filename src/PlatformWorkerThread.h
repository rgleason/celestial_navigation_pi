// Preserve the desktop wx threading ABI, including the MSVC runtime workaround.
// Android plugins statically link wxQt: its separate wxThread module globals
// are not initialized by the host's wx module. Use native C++ threads there.
#pragma once
#include <wx/thread.h>
#ifndef __OCPN__ANDROID__
using CelestialWorkerThread = wxThread;
using CelestialWorkerMutex = wxCriticalSection;
using CelestialWorkerLock = wxCriticalSectionLocker;
#else
#include <thread>
#include <mutex>
using CelestialWorkerMutex = std::mutex;
using CelestialWorkerLock = std::lock_guard<std::mutex>;
class CelestialWorkerThread {
 public:
  using ExitCode = void*;
  explicit CelestialWorkerThread(wxThreadKind) {}
  virtual ~CelestialWorkerThread() = default;
  wxThreadError Create() { return wxTHREAD_NO_ERROR; }
  wxThreadError Run() {
    if (thread_.joinable()) return wxTHREAD_RUNNING;
    try { thread_ = std::thread([this]() { Entry(); }); }
    catch (...) { return wxTHREAD_NO_RESOURCE; }
    return wxTHREAD_NO_ERROR;
  }
  void Wait() { if (thread_.joinable()) thread_.join(); }
 private:
  virtual ExitCode Entry() = 0;
  std::thread thread_;
};
#endif
