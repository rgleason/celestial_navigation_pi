#include <gtest/gtest.h>

#include <wx/init.h>
#include <wx/utils.h>

#include "EclipseVerificationWorker.h"

namespace {

bool AwaitResult(
    celestial_navigation::EclipseVerificationWorker* worker,
    celestial_navigation::EclipseVerificationWorker::Result* result) {
  for (int attempt = 0; attempt < 5000; ++attempt) {
    if (worker->TryTakeResult(result)) return true;
    wxMilliSleep(2);
  }
  return false;
}

}  // namespace

TEST(EclipseVerificationWorker, ReturnsBackgroundVerificationResult) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  using celestial_navigation::EclipseDataKind;
  using celestial_navigation::EclipseVerificationWorker;
  EclipseVerificationWorker worker(
      [](EclipseDataKind kind, const std::string& path) {
        EclipseVerificationWorker::Result result;
        result.valid = kind == EclipseDataKind::De440s && path == "test.bsp";
        result.error = result.valid ? "" : "unexpected input";
        return result;
      });

  wxString error;
  ASSERT_TRUE(worker.Start(EclipseDataKind::De440s, "test.bsp", &error))
      << error;
  EclipseVerificationWorker::Result result;
  ASSERT_TRUE(AwaitResult(&worker, &result));
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.error.empty());
}

TEST(EclipseVerificationWorker, RejectsASecondConcurrentTask) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  using celestial_navigation::EclipseDataKind;
  using celestial_navigation::EclipseVerificationWorker;
  EclipseVerificationWorker worker([](EclipseDataKind, const std::string&) {
    wxMilliSleep(30);
    EclipseVerificationWorker::Result result;
    result.valid = true;
    return result;
  });

  wxString error;
  ASSERT_TRUE(worker.Start(EclipseDataKind::De440s, "first", &error));
  EXPECT_FALSE(worker.Start(EclipseDataKind::De440s, "second", &error));
  EXPECT_FALSE(error.empty());
  EclipseVerificationWorker::Result result;
  ASSERT_TRUE(AwaitResult(&worker, &result));
  EXPECT_TRUE(result.valid);
}

TEST(EclipseVerificationWorker, DestructionWaitsForActiveTask) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  using celestial_navigation::EclipseDataKind;
  using celestial_navigation::EclipseVerificationWorker;
  bool completed = false;
  {
    EclipseVerificationWorker worker(
        [&completed](EclipseDataKind, const std::string&) {
          wxMilliSleep(20);
          completed = true;
          EclipseVerificationWorker::Result result;
          result.valid = true;
          return result;
        });
    wxString error;
    ASSERT_TRUE(worker.Start(EclipseDataKind::De440s, "test", &error));
  }
  EXPECT_TRUE(completed);
}

TEST(EclipseVerificationWorker, VerifiesTheRealDe440FileOffTheCallingThread) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  using celestial_navigation::EclipseDataKind;
  using celestial_navigation::EclipseVerificationWorker;
  EclipseVerificationWorker worker;

  wxString error;
  ASSERT_TRUE(
      worker.Start(EclipseDataKind::De440s, ECLIPSE_DE440_TEST_PATH, &error))
      << error;
  EclipseVerificationWorker::Result result;
  ASSERT_TRUE(AwaitResult(&worker, &result));
  EXPECT_TRUE(result.valid) << result.error;
}

TEST(EclipseVerificationWorker, CanVerifyInstalledFilesSequentially) {
  wxInitializer wx;
  ASSERT_TRUE(wx.IsOk());
  using celestial_navigation::EclipseDataKind;
  using celestial_navigation::EclipseVerificationWorker;
  EclipseVerificationWorker worker(
      [](EclipseDataKind, const std::string& path) {
        EclipseVerificationWorker::Result result;
        result.valid = path == "first";
        result.error = result.valid ? "" : "expected test rejection";
        return result;
      });

  wxString error;
  EclipseVerificationWorker::Result result;
  ASSERT_TRUE(worker.Start(EclipseDataKind::De440s, "first", &error));
  ASSERT_TRUE(AwaitResult(&worker, &result));
  EXPECT_TRUE(result.valid);

  ASSERT_TRUE(worker.Start(EclipseDataKind::LunarOrientation, "second",
                           &error));
  ASSERT_TRUE(AwaitResult(&worker, &result));
  EXPECT_FALSE(result.valid);
  EXPECT_EQ("expected test rejection", result.error);
}
