#include <gtest/gtest.h>
#include "Sight.h"
#include "LunarSessionEngine.h"
#include "UtcDateTime.h"
#include "tinyxml.h"
#include <algorithm>
#include <memory>
namespace lunar_session {
Result SolveAndroid(const std::vector<SessionObservation>& observations,
                    const Options& options);
}

// POBsoft (1985-2026): independent public USNO readings exercise production
// ephemerides rather than generating observations with the model under test.
TEST(LunarUsnoSession, PublicGreenwichClockAndJointReference) {
  TiXmlDocument xml(
      CMAKE_BINARY_DIR
      "/../validation/android-usno-greenwich-20240621/session-sights.xml");
  ASSERT_TRUE(xml.LoadFile());
  std::vector<std::shared_ptr<Sight>> retained;
  std::vector<lunar_session::SessionObservation> entries;
  for (auto* node = xml.RootElement()->FirstChildElement("Sight"); node;
       node = node->NextSiblingElement("Sight")) {
    auto number = [&](const char* key) {
      double value = 0;
      EXPECT_EQ(TIXML_SUCCESS, node->QueryDoubleAttribute(key, &value));
      return value;
    };
    wxDateTime utc(
        21, wxDateTime::Jun, 2024, 22,
        wxString::FromUTF8(node->Attribute("Time")).Mid(3, 2) == "05" ? 5 : 0,
        0);
    auto sight = std::make_shared<Sight>(
        Sight::LUNAR, wxString::FromUTF8(node->Attribute("Body")),
        Sight::LUNAR_NEAR, utc, 1, number("Measurement"), 0.5);
    sight->m_LunarMoonAltitude = number("LunarMoonAltitude");
    sight->m_LunarBodyAltitude = number("LunarBodyAltitude");
    sight->m_LunarMoonLimb = Sight::LOWER;
    sight->m_LunarBodyLimb = Sight::CENTER;
    sight->m_EyeHeight = 0;
    sight->m_IndexError = 0;
    sight->m_Pressure = 1013;
    sight->m_Temperature = 10;
    sight->m_DipShort = false;
    sight->m_ArtificialHorizon = false;
    sight->m_LunarMoonAltitudeUncertainty = 0.5;
    sight->m_LunarBodyAltitudeUncertainty = 0.5;
    sight->m_CorrectedDateTime = utc;
    sight->RecomputeLunar();
    retained.push_back(sight);
    lunar_session::SessionObservation entry;
    entry.label = node->Attribute("Body");
    entry.settings = sight->LunarObservation();
    entry.epoch_offset_seconds = utc.GetMinute() * 60;
    const double epoch = entry.epoch_offset_seconds;
    const auto model = sight->LunarEphemeris();
    entry.ephemeris = [model, epoch](double seconds,
                                     lunar_distance::EphemerisSample* sample,
                                     std::string* error) {
      return model(seconds - epoch, sample, error);
    };
    entry.reading_ids[1] = "Moon@" + std::to_string(epoch);
    entries.push_back(entry);
  }
  ASSERT_EQ(4u, entries.size());
  lunar_session::Options options;
  options.known_or_initial_position = {51.4779, 0};
  options.position_seeds = {{51.4779, 0}};
  options.start_correction_seconds = -900;
  options.end_correction_seconds = 900;
  options.correction_seed_step_seconds = 1800;
  options.maximum_iterations = 50;
  options.solve_position = false;
  const auto known = lunar_session::Solve(entries, options);
  ASSERT_TRUE(known.valid) << known.error;
  EXPECT_NEAR(0, known.candidates.front().clock_correction_seconds, 1);
  const auto android_known = lunar_session::SolveAndroid(entries, options);
  ASSERT_TRUE(android_known.valid) << android_known.error;
  EXPECT_NEAR(0, android_known.candidates.front().clock_correction_seconds, 1);
  EXPECT_NEAR(known.candidates.front().clock_correction_seconds,
              android_known.candidates.front().clock_correction_seconds, 0.05);
  options.solve_position = true;
  const auto joint = lunar_session::SolveAndroid(entries, options);
  ASSERT_TRUE(joint.valid) << joint.error;
  EXPECT_NEAR(0, joint.candidates.front().clock_correction_seconds, 60);
  EXPECT_LT(lunar_distance::GreatCircleDistanceNm(
                joint.candidates.front().reference_position, {51.4779, 0}),
            3);
  EXPECT_LT(joint.candidates.front().angular_rms_arcmin, 0.5);
  std::reverse(entries.begin(), entries.end());
  const auto reverse = lunar_session::SolveAndroid(entries, options);
  ASSERT_TRUE(reverse.valid) << reverse.error;
  EXPECT_NEAR(joint.candidates.front().clock_correction_seconds,
              reverse.candidates.front().clock_correction_seconds, 0.05);
  EXPECT_LT(lunar_distance::GreatCircleDistanceNm(
                joint.candidates.front().reference_position,
                reverse.candidates.front().reference_position),
            0.001);
}
