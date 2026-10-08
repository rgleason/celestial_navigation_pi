// POBsoft (1985-2026). Owned offline planning worker; GPL-3.0-or-later.
#pragma once
#include "NavigationAlgorithms.h"
#include "AndroidPlannerCancellation.h"
#include "PlatformWorkerThread.h"
#include "CompactEphemerisProvider.h"
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
  PlannerWorker()
#ifndef __OCPN__ANDROID__
      : thread_(this) {
#else
      {
#endif
    celestial_navigation::InitializeCompactEphemerisPreference();
#ifdef __OCPN__ANDROID__
    thread_ = std::thread([this] { Run(); });
#else
    if (thread_.Create() != wxTHREAD_NO_ERROR ||
        thread_.Run() != wxTHREAD_NO_ERROR)
      throw std::runtime_error("Unable to start planning worker");
#endif
  }
  ~PlannerWorker() {
    { CelestialWorkerLock lock(mutex_); stop_ = true; cancel_ = true; }
#ifdef __OCPN__ANDROID__
    condition_.notify_one();
    thread_.join();
#else
    wake_.Post();
    thread_.Wait();
#endif
  }
  unsigned Submit(const ObserverMotion& motion, double eye,
                  int moonPathSpan = -2) {
    CelestialWorkerLock lock(mutex_);
    cancel_ = true;
    job_.motion = motion; job_.eye = eye; job_.generation = ++generation_;
    job_.moonPathSpan = moonPathSpan;
    pending_ = true;
    ready_ = false;
#ifdef __OCPN__ANDROID__
    condition_.notify_one();
#else
    wake_.Post();
#endif
    return generation_;
  }
  void Cancel() {
    CelestialWorkerLock lock(mutex_);
    cancel_ = true; pending_ = ready_ = false; ++generation_;
  }
  bool Take(unsigned generation, PlannerResults* output) {
    CelestialWorkerLock lock(mutex_);
    if (!ready_ || completedGeneration_ != generation) return false;
    *output = std::move(result_); ready_ = false; return true;
  }
  int Stage() const { return stage_.load(); }
 private:
  struct Job { ObserverMotion motion; double eye = 0; unsigned generation = 0;
               int moonPathSpan = -2; };
  void Run() {
    for (;;) {
      Job job;
#ifndef __OCPN__ANDROID__
      wake_.Wait();
#endif
      {
#ifdef __OCPN__ANDROID__
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return stop_ || pending_; });
#else
        CelestialWorkerLock lock(mutex_);
#endif
        if (stop_) return;
        if (!pending_) continue;
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
        // Solar upper transit is independent of the eye-height/dip correction.
        result.noonEvents = result.events;
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
        if (job.moonPathSpan == -2 &&
            celestial_navigation::CompactEphemerisEnabled()) {
          // The shorter paths are exact subsets of the same half-hour grid.
          // Android offers all three spans without recalculating their overlap.
          result.moonPaths[2] = PlannerRecommendations::MoonPath(m, spans[2]);
          for (const auto& point : result.moonPaths[2]) {
            const double seconds = std::abs(
                (point.utc - m.referenceUtc).GetMilliseconds().ToDouble()) / 1000;
            for (int i = 0; i < 2; ++i)
              if (seconds <= spans[i] * 3600) result.moonPaths[i].push_back(point);
          }
        } else {
          for (int i = 0; i < 3; ++i) {
            if (job.moonPathSpan != -2 && job.moonPathSpan != i) continue;
            result.moonPaths[i] = PlannerRecommendations::MoonPath(m, spans[i]);
            CheckPlannerCancellation();
          }
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
        CelestialWorkerLock lock(mutex_);
        if (cancel_ || job.generation != generation_) continue;
        result_ = std::move(result); completedGeneration_ = job.generation; ready_ = true;
      }
      stage_ = 0;
    }
  }
  CelestialWorkerMutex mutex_;
#ifdef __OCPN__ANDROID__
  std::condition_variable condition_;
#else
  wxSemaphore wake_{0, 1};
  class Thread : public CelestialWorkerThread {
   public:
    explicit Thread(PlannerWorker* owner)
        : CelestialWorkerThread(wxTHREAD_JOINABLE), owner_(owner) {}
   private:
    ExitCode Entry() override { owner_->Run(); return nullptr; }
    PlannerWorker* owner_;
  };
#endif
  std::atomic<bool> cancel_{false};
  std::atomic<int> stage_{0};
  bool stop_ = false, pending_ = false, ready_ = false;
  unsigned generation_ = 0, completedGeneration_ = 0;
  Job job_;
  PlannerResults result_;
#ifdef __OCPN__ANDROID__
  std::thread thread_;
#else
  Thread thread_;
#endif
};
}
