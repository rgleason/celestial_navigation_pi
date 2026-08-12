/******************************************************************************
 * Time-source parsing and monitoring for the Celestial Navigation plugin.
 ******************************************************************************/

#ifndef _CELESTIAL_NAVIGATION_TIME_STATUS_H_
#define _CELESTIAL_NAVIGATION_TIME_STATUS_H_

#include <chrono>
#include <mutex>

#include <wx/datetime.h>
#include <wx/string.h>

struct GnssTimeSnapshot {
  GnssTimeSnapshot() : valid(false), age_milliseconds(0) {}

  bool valid;
  wxDateTime utc;
  long long age_milliseconds;
  wxString source;
};

class GnssTimeMonitor {
public:
  GnssTimeMonitor();

  // Returns true only for a valid, active RMC or valid ZDA time sentence.
  bool Update(const wxString& sentence);
  GnssTimeSnapshot Snapshot() const;

  static bool ParseNmeaUtc(const wxString& sentence, wxDateTime* utc,
                           wxString* source);

private:
  mutable std::mutex m_mutex;
  bool m_valid;
  wxDateTime m_utc;
  wxString m_source;
  std::chrono::steady_clock::time_point m_received;
};

struct ChronyTrackingInfo {
  ChronyTrackingInfo()
      : valid(false),
        synchronized(false),
        reference_unix_seconds(0.0),
        system_offset_seconds(0.0) {}

  bool valid;
  bool synchronized;
  double reference_unix_seconds;
  double system_offset_seconds;
  wxString source;
  wxString leap_status;
};

bool ParseChronyTrackingCsv(const wxString& line, ChronyTrackingInfo* result);

#endif
