#include <gtest/gtest.h>

#include <wx/init.h>
#include <wx/utils.h>

#include "LunarSessionWorker.h"

namespace {

bool AwaitResult(celestial_navigation::LunarSessionWorker* worker,
                 lunar_session::Result* result) {
  for (int attempt = 0; attempt < 5000; ++attempt) {
    if (worker->TryTakeResult(result)) return true;
    wxMilliSleep(2);
  }
  return false;
}

}  // namespace

TEST(LunarSessionWorker, ReturnsBackgroundSolverResult) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  celestial_navigation::LunarSessionWorker worker(
      [](const std::vector<lunar_session::SessionObservation>& entries,
         const lunar_session::Options&) {
        lunar_session::Result result;
        result.valid = entries.size() == 1;
        result.error = result.valid ? "" : "unexpected entries";
        return result;
      });

  std::vector<lunar_session::SessionObservation> entries(1);
  lunar_session::Options options;
  wxString error;
  ASSERT_TRUE(worker.Start(entries, options, &error)) << error;
  lunar_session::Result result;
  ASSERT_TRUE(AwaitResult(&worker, &result));
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.error.empty());
}

TEST(LunarSessionWorker, RejectsASecondConcurrentCalculation) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  celestial_navigation::LunarSessionWorker worker(
      [](const std::vector<lunar_session::SessionObservation>&,
         const lunar_session::Options&) {
        wxMilliSleep(30);
        lunar_session::Result result;
        result.valid = true;
        return result;
      });

  std::vector<lunar_session::SessionObservation> entries;
  lunar_session::Options options;
  wxString error;
  ASSERT_TRUE(worker.Start(entries, options, &error));
  EXPECT_FALSE(worker.Start(entries, options, &error));
  EXPECT_FALSE(error.empty());
  lunar_session::Result result;
  ASSERT_TRUE(AwaitResult(&worker, &result));
  EXPECT_TRUE(result.valid);
}

TEST(LunarSessionWorker, DestructionWaitsForActiveCalculation) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  bool completed = false;
  {
    celestial_navigation::LunarSessionWorker worker(
        [&completed](const std::vector<lunar_session::SessionObservation>&,
                     const lunar_session::Options&) {
          wxMilliSleep(20);
          completed = true;
          lunar_session::Result result;
          result.valid = true;
          return result;
        });
    std::vector<lunar_session::SessionObservation> entries;
    lunar_session::Options options;
    wxString error;
    ASSERT_TRUE(worker.Start(entries, options, &error));
  }
  EXPECT_TRUE(completed);
}
