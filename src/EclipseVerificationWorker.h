#ifndef CELESTIAL_NAVIGATION_ECLIPSE_VERIFICATION_WORKER_H
#define CELESTIAL_NAVIGATION_ECLIPSE_VERIFICATION_WORKER_H

#include <functional>
#include <string>

#include <wx/string.h>
#include <wx/thread.h>

#include "EclipseDataFiles.h"

namespace celestial_navigation {

// Runs potentially expensive astronomy-data verification using wx's native
// threading ABI.  In particular, this deliberately avoids std::async: a
// recent MSVC-built plug-in can be loaded by OpenCPN alongside an older
// bundled msvcp140.dll, where the STL/PPL async task bridge is not safe.
class EclipseVerificationWorker {
public:
  struct Result {
    Result() : valid(false) {}
    bool valid;
    std::string error;
  };

  typedef std::function<Result(EclipseDataKind, const std::string&)>
      VerifyFunction;

  explicit EclipseVerificationWorker(
      const VerifyFunction& verify = VerifyFunction());
  ~EclipseVerificationWorker();

  EclipseVerificationWorker(const EclipseVerificationWorker&) = delete;
  EclipseVerificationWorker& operator=(const EclipseVerificationWorker&) =
      delete;

  bool Start(EclipseDataKind kind, const std::string& path, wxString* error);
  bool TryTakeResult(Result* result);
  void Wait();

private:
  class WorkerThread;

  void Publish(const Result& result);
  static Result VerifyFile(EclipseDataKind kind, const std::string& path);

  VerifyFunction m_verify;
  mutable wxCriticalSection m_result_lock;
  Result m_result;
  bool m_result_ready;
  WorkerThread* m_thread;
};

}  // namespace celestial_navigation

#endif
