#include "LunarSessionWorker.h"

#include <exception>
#include <new>

namespace celestial_navigation {

class LunarSessionWorker::WorkerThread : public wxThread {
public:
  WorkerThread(LunarSessionWorker* owner,
               const std::vector<lunar_session::SessionObservation>& entries,
               const lunar_session::Options& options,
               const SolveFunction& solve)
      : wxThread(wxTHREAD_JOINABLE),
        m_owner(owner),
        m_entries(entries),
        m_options(options),
        m_solve(solve) {}

private:
  ExitCode Entry() override {
    lunar_session::Result result;
    try {
      result = m_solve(m_entries, m_options);
    } catch (const std::exception& e) {
      result.valid = false;
      result.error =
          std::string("Lunar-session calculation failed: ") + e.what();
    } catch (...) {
      result.valid = false;
      result.error =
          "Lunar-session calculation failed with an unexpected error.";
    }
    m_owner->Publish(result);
    return static_cast<ExitCode>(0);
  }

  LunarSessionWorker* m_owner;
  std::vector<lunar_session::SessionObservation> m_entries;
  lunar_session::Options m_options;
  SolveFunction m_solve;
};

LunarSessionWorker::LunarSessionWorker(const SolveFunction& solve)
    : m_solve(solve), m_result_ready(false), m_thread(NULL) {
  if (!m_solve) m_solve = Solve;
}

LunarSessionWorker::~LunarSessionWorker() { Wait(); }

bool LunarSessionWorker::Start(
    const std::vector<lunar_session::SessionObservation>& entries,
    const lunar_session::Options& options, wxString* error) {
  if (m_thread) {
    if (error) *error = "A lunar-session calculation is already running.";
    return false;
  }
  {
    wxCriticalSectionLocker lock(m_result_lock);
    m_result = lunar_session::Result();
    m_result_ready = false;
  }

  WorkerThread* thread =
      new (std::nothrow) WorkerThread(this, entries, options, m_solve);
  if (!thread) {
    if (error) *error = "Unable to allocate the lunar-session worker.";
    return false;
  }
  if (thread->Create() != wxTHREAD_NO_ERROR) {
    delete thread;
    if (error) *error = "Unable to create the lunar-session worker.";
    return false;
  }
  m_thread = thread;
  if (thread->Run() != wxTHREAD_NO_ERROR) {
    m_thread = NULL;
    delete thread;
    if (error) *error = "Unable to start the lunar-session worker.";
    return false;
  }
  return true;
}

bool LunarSessionWorker::TryTakeResult(lunar_session::Result* result) {
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

void LunarSessionWorker::Wait() {
  WorkerThread* thread = m_thread;
  if (!thread) return;
  thread->Wait();
  delete thread;
  m_thread = NULL;
}

void LunarSessionWorker::Publish(const lunar_session::Result& result) {
  wxCriticalSectionLocker lock(m_result_lock);
  m_result = result;
  m_result_ready = true;
}

lunar_session::Result LunarSessionWorker::Solve(
    const std::vector<lunar_session::SessionObservation>& entries,
    const lunar_session::Options& options) {
  return lunar_session::Solve(entries, options);
}

}  // namespace celestial_navigation
