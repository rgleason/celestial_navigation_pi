#ifndef CELESTIAL_NAVIGATION_LUNAR_SESSION_WORKER_H
#define CELESTIAL_NAVIGATION_LUNAR_SESSION_WORKER_H

#include <functional>
#include <string>
#include <vector>

#include <wx/string.h>
#include <wx/thread.h>

#include "LunarSessionEngine.h"

namespace celestial_navigation {

// Runs the bounded lunar-session solver without using the MSVC STL/PPL async
// bridge. See EclipseVerificationWorker for the host-runtime compatibility
// issue which requires wx-native threading here as well.
class LunarSessionWorker {
public:
  typedef std::function<lunar_session::Result(
      const std::vector<lunar_session::SessionObservation>&,
      const lunar_session::Options&)>
      SolveFunction;

  explicit LunarSessionWorker(const SolveFunction& solve = SolveFunction());
  ~LunarSessionWorker();

  LunarSessionWorker(const LunarSessionWorker&) = delete;
  LunarSessionWorker& operator=(const LunarSessionWorker&) = delete;

  bool Start(const std::vector<lunar_session::SessionObservation>& entries,
             const lunar_session::Options& options, wxString* error);
  bool TryTakeResult(lunar_session::Result* result);
  void Wait();

private:
  class WorkerThread;

  void Publish(const lunar_session::Result& result);
  static lunar_session::Result Solve(
      const std::vector<lunar_session::SessionObservation>& entries,
      const lunar_session::Options& options);

  SolveFunction m_solve;
  mutable wxCriticalSection m_result_lock;
  lunar_session::Result m_result;
  bool m_result_ready;
  WorkerThread* m_thread;
};

}  // namespace celestial_navigation

#endif
