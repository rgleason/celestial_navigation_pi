#include <gtest/gtest.h>

#include "TimeStatus.h"

TEST(TimeStatus, ParsesActiveRmcUtc) {
  wxDateTime utc;
  wxString source;
  ASSERT_TRUE(GnssTimeMonitor::ParseNmeaUtc(
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,"
      "003.1,W*6A",
      &utc, &source));
  EXPECT_EQ("RMC", source);
  EXPECT_EQ("1994-03-23 12:35:19",
            utc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC));
}

TEST(TimeStatus, ParsesZdaFractionalUtc) {
  wxDateTime utc;
  wxString source;
  ASSERT_TRUE(GnssTimeMonitor::ParseNmeaUtc(
      "$GPZDA,201530.25,04,07,2002,00,00*67", &utc, &source));
  EXPECT_EQ("ZDA", source);
  EXPECT_EQ("2002-07-04 20:15:30.250",
            utc.Format("%Y-%m-%d %H:%M:%S.250", wxDateTime::UTC));
  EXPECT_EQ(250, utc.GetMillisecond());
}

TEST(TimeStatus, RejectsBadChecksumAndVoidRmc) {
  wxDateTime utc;
  wxString source;
  EXPECT_FALSE(GnssTimeMonitor::ParseNmeaUtc(
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,"
      "003.1,W*00",
      &utc, &source));
  EXPECT_FALSE(GnssTimeMonitor::ParseNmeaUtc(
      "$GPRMC,123519,V,,,,,,,230394*33", &utc, &source));
}

TEST(TimeStatus, RejectsMissingChecksum) {
  wxDateTime utc;
  wxString source;
  EXPECT_FALSE(GnssTimeMonitor::ParseNmeaUtc(
      "$GPZDA,201530.00,04,07,2002,00,00", &utc, &source));
}

TEST(TimeStatus, RejectsImpossibleCalendarDate) {
  wxDateTime utc;
  wxString source;
  EXPECT_FALSE(GnssTimeMonitor::ParseNmeaUtc(
      "$GPZDA,201530.00,31,02,2026,00,00*65", &utc, &source));
}

TEST(TimeStatus, ParsesChronyTrackingCsv) {
  ChronyTrackingInfo tracking;
  ASSERT_TRUE(ParseChronyTrackingCsv(
      "B939BFE6,185.57.191.230,2,1786555139.120104290,0.002091084,"
      "-0.003184930,0.001386783,-13.807,-0.387,1.650,0.028799813,"
      "0.002415670,1044.5,Normal",
      &tracking));
  EXPECT_TRUE(tracking.valid);
  EXPECT_TRUE(tracking.synchronized);
  EXPECT_EQ("185.57.191.230", tracking.source);
  EXPECT_NEAR(0.002091084, tracking.system_offset_seconds, 1e-12);
}

TEST(TimeStatus, RecognizesUnsynchronizedChrony) {
  ChronyTrackingInfo tracking;
  ASSERT_TRUE(ParseChronyTrackingCsv(
      "00000000,0.0.0.0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,"
      "0.0,Not synchronised",
      &tracking));
  EXPECT_FALSE(tracking.synchronized);
}
