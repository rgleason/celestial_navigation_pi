#include "EclipseVerificationWorker.h"

#include <exception>
#include <new>

namespace celestial_navigation {

class EclipseVerificationWorker::WorkerThread : public wxThread {
public:
  WorkerThread(EclipseVerificationWorker* owner, EclipseDataKind kind,
               const std::string& path, const VerifyFunction& verify)
      : wxThread(wxTHREAD_JOINABLE),
        m_owner(owner),
        m_kind(kind),
        m_path(path),
        m_verify(verify) {}

private:
  ExitCode Entry() override {
    Result result;
    try {
      result = m_verify(m_kind, m_path);
    } catch (const std::exception& e) {
      result.valid = false;
      result.error = std::string("Verification failed: ") + e.what();
    } catch (...) {
      result.valid = false;
      result.error = "Verification failed with an unexpected error.";
    }
    m_owner->Publish(result);
    return static_cast<ExitCode>(0);
  }

  EclipseVerificationWorker* m_owner;
  EclipseDataKind m_kind;
  std::string m_path;
  VerifyFunction m_verify;
};

EclipseVerificationWorker::EclipseVerificationWorker(
    const VerifyFunction& verify)
    : m_verify(verify), m_result_ready(false), m_thread(NULL) {
  if (!m_verify) m_verify = VerifyFile;
}

EclipseVerificationWorker::~EclipseVerificationWorker() { Wait(); }

bool EclipseVerificationWorker::Start(EclipseDataKind kind,
                                      const std::string& path,
                                      wxString* error) {
  if (m_thread) {
    if (error) *error = "A verification task is already running.";
    return false;
  }
  {
    wxCriticalSectionLocker lock(m_result_lock);
    m_result = Result();
    m_result_ready = false;
  }

  WorkerThread* thread =
      new (std::nothrow) WorkerThread(this, kind, path, m_verify);
  if (!thread) {
    if (error) *error = "Unable to allocate the verification worker.";
    return false;
  }
  if (thread->Create() != wxTHREAD_NO_ERROR) {
    delete thread;
    if (error) *error = "Unable to create the verification worker.";
    return false;
  }
  m_thread = thread;
  if (thread->Run() != wxTHREAD_NO_ERROR) {
    m_thread = NULL;
    delete thread;
    if (error) *error = "Unable to start the verification worker.";
    return false;
  }
  return true;
}

bool EclipseVerificationWorker::TryTakeResult(Result* result) {
  if (!result || !m_thread) return false;
  {
    wxCriticalSectionLocker lock(m_result_lock);
    if (!m_result_ready) return false;
    *result = m_result;
    m_result_ready = false;
  }
  Wait();
  return true;
}

void EclipseVerificationWorker::Wait() {
  WorkerThread* thread = m_thread;
  if (!thread) return;
  thread->Wait();
  delete thread;
  m_thread = NULL;
}

void EclipseVerificationWorker::Publish(const Result& result) {
  wxCriticalSectionLocker lock(m_result_lock);
  m_result = result;
  m_result_ready = true;
}

EclipseVerificationWorker::Result EclipseVerificationWorker::VerifyFile(
    EclipseDataKind kind, const std::string& path) {
  const eclipse::DataPackStatus status = VerifyEclipseDataFile(kind, path);
  Result result;
  result.valid = status.valid;
  result.error = status.error;
  return result;
}

}  // namespace celestial_navigation
