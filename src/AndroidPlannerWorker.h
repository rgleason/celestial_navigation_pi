// POBsoft (1985-2026). Owned offline planning worker; GPL-3.0-or-later.
#pragma once
#include "NavigationAlgorithms.h"
#include "AndroidPlannerCancellation.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <limits>
#include <mutex>
#include <thread>

namespace celestial_android {
struct PlannerResults {
  ObserverMotion motion;
  DailyEventsResult events, noonEvents;
  std::vector<MoonPhaseEvent> phases;
  MoonInformation moon;
  BodyState moonState, sun, polaris;
  PlanningResult planning;
  std::vector<RankedBody> allBodies;
  std::vector<PlannerSkyPoint> ecliptic;
  std::array<std::vector<PlannerSkyPoint>, 3> moonPaths;
  std::vector<AlmanacRow> almanac;
  wxString error;
  long elapsedMs = 0;
};

// One persistent thread, at most one active and one replacement job. No GUI
// references escape into it. Generation numbers reject superseded results.
class PlannerWorker {
 public:
  PlannerWorker() : thread_([this] { Run(); }) {}
  ~PlannerWorker() {
    { std::lock_guard<std::mutex> lock(mutex_); stop_ = true; cancel_ = true; }
    condition_.notify_one();
    thread_.join();
  }
  unsigned Submit(const ObserverMotion& motion, double eye) {
    std::lock_guard<std::mutex> lock(mutex_);
    cancel_ = true;
    job_.motion = motion; job_.eye = eye; job_.generation = ++generation_;
    pending_ = true;
    ready_ = false;
    condition_.notify_one();
    return generation_;
  }
  void Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    cancel_ = true; pending_ = ready_ = false; ++generation_;
  }
  bool Take(unsigned generation, PlannerResults* output) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ready_ || completedGeneration_ != generation) return false;
    *output = std::move(result_); ready_ = false; return true;
  }
  int Stage() const { return stage_.load(); }
 private:
  struct Job { ObserverMotion motion; double eye = 0; unsigned generation = 0; };
  void Run() {
    for (;;) {
      Job job;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return stop_ || pending_; });
        if (stop_) return;
        job = job_; pending_ = false; cancel_ = false;
      }
      PlannerResults result;
      result.motion = job.motion;
      const auto start = std::chrono::steady_clock::now();
      try {
        PlannerCancellationScope scope(cancel_);
        const auto& m = job.motion;
        stage_ = 1;
        result.events = HorizonEventCalculator::Calculate(m.referenceUtc, m, job.eye);
        CheckPlannerCancellation();
        result.noonEvents = job.eye == 0 ? result.events
            : HorizonEventCalculator::Calculate(m.referenceUtc, m);
        stage_ = 2;
        result.phases = NextPrincipalMoonPhases(m.referenceUtc, m.latitude, m.longitude);
        CheckPlannerCancellation();
        result.moon = CalculateMoonInformation(m.referenceUtc, m.latitude, m.longitude);
        result.moonState = CelestialEphemeris::Evaluate("Moon", m.referenceUtc, m.latitude, m.longitude);
        result.sun = CelestialEphemeris::Evaluate("Sun", m.referenceUtc, m.latitude, m.longitude);
        result.polaris = CelestialEphemeris::Evaluate("Polaris", m.referenceUtc, m.latitude, m.longitude);
        stage_ = 3;
        result.planning = PlannerRecommendations::Calculate(
            m.referenceUtc, m.latitude, m.longitude);
        result.allBodies = result.planning.bodies;
        CheckPlannerCancellation();
        result.ecliptic = PlannerRecommendations::Ecliptic(
            m.referenceUtc, m.latitude, m.longitude);
        const int spans[] = {3, 6, 12};
        for (int i = 0; i < 3; ++i) {
          result.moonPaths[i] = PlannerRecommendations::MoonPath(m, spans[i]);
          CheckPlannerCancellation();
        }
        CheckPlannerCancellation();
        stage_ = 4;
        result.almanac = BuildAlmanac(m.referenceUtc, 24,
            {"Sun", "Moon", "Venus", "Mars", "Jupiter", "Saturn", "Polaris"}, m);
        CheckPlannerCancellation();
      } catch (const PlannerCancelled&) {
        continue;
      } catch (const std::exception& error) {
        result.error = wxString::FromUTF8(error.what());
      } catch (...) {
        result.error = "Unexpected planning calculation failure.";
      }
      result.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start).count();
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cancel_ || job.generation != generation_) continue;
        result_ = std::move(result); completedGeneration_ = job.generation; ready_ = true;
      }
      stage_ = 0;
    }
  }
  std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic<bool> cancel_{false};
  std::atomic<int> stage_{0};
  bool stop_ = false, pending_ = false, ready_ = false;
  unsigned generation_ = 0, completedGeneration_ = 0;
  Job job_;
  PlannerResults result_;
  std::thread thread_;
};
}
