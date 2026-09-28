// POBsoft (1985-2026). Android planner cancellation; GPL-3.0-or-later.
#pragma once
#include <atomic>
namespace celestial_android {
struct PlannerCancelled {};
inline const std::atomic<bool>*& PlannerCancellationToken() {
  static thread_local const std::atomic<bool>* token = nullptr;
  return token;
}
inline void CheckPlannerCancellation() {
  if (PlannerCancellationToken() && PlannerCancellationToken()->load()) throw PlannerCancelled{};
}
class PlannerCancellationScope {
 public:
  explicit PlannerCancellationScope(const std::atomic<bool>& cancel)
      : previous_(PlannerCancellationToken()) { PlannerCancellationToken() = &cancel; }
  ~PlannerCancellationScope() { PlannerCancellationToken() = previous_; }
 private:
  const std::atomic<bool>* previous_;
};
}
