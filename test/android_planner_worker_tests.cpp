#include <gtest/gtest.h>
#include "AndroidPlannerWorker.h"
#include <future>
#include "UtcDateTime.h"

TEST(AndroidPlannerWorker, CancellationScopeIsIsolatedAndRestored) {
  std::atomic<bool> cancelled{true};
  auto other = std::async(std::launch::async, [&] {
    celestial_android::PlannerCancellationScope scope(cancelled);
    EXPECT_THROW(celestial_android::CheckPlannerCancellation(),
                 celestial_android::PlannerCancelled);
  });
  EXPECT_NO_THROW(celestial_android::CheckPlannerCancellation());
  other.get();
  EXPECT_NO_THROW(celestial_android::CheckPlannerCancellation());
}

TEST(AndroidPlannerWorker, ReplacedContextPublishesOnlyLatestPreciseEpoch) {
  // Existing independent worksheet fixture, tolerance 0.1 arcminute.
  ObserverMotion motion;
  motion.referenceUtc = UtcDateTime::ToInstant(wxDateTime(20, wxDateTime::Jul,
      2025, 17, 16, 33)) + wxTimeSpan::Milliseconds(987);
  motion.latitude = 43.2366916666667;
  motion.longitude = -77.533415;
  celestial_android::PlannerWorker worker;
  const auto stale = worker.Submit(motion, 0);
  worker.Cancel();
  const auto latest = worker.Submit(motion, 0);
  celestial_android::PlannerResults result;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  bool received = false;
  while (std::chrono::steady_clock::now() < deadline) {
    EXPECT_FALSE(worker.Take(stale, &result));
    if (worker.Take(latest, &result)) { received = true; break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(received);
  ASSERT_TRUE(result.error.empty());
  ASSERT_EQ(175u, result.almanac.size());
  EXPECT_EQ(motion.referenceUtc.GetValue().GetValue(),
            result.almanac.front().utc.GetValue().GetValue());
  EXPECT_NEAR(67.27614828, result.sun.geometricAltitude, 0.1 / 60);
  EXPECT_NEAR(20.5129, result.sun.declination, 0.1 / 60);
  EXPECT_EQ(4u, result.phases.size());
  EXPECT_FALSE(result.events.events.empty());
  EXPECT_EQ(result.planning.bodies.size(), result.allBodies.size());
  EXPECT_FALSE(result.planning.bodies.empty());
  EXPECT_EQ(121u, result.ecliptic.size());
  EXPECT_LT(result.moonPaths[0].size(), result.moonPaths[1].size());
  EXPECT_LT(result.moonPaths[1].size(), result.moonPaths[2].size());
}
