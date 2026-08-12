/******************************************************************************
 * Time-source parsing and monitoring for the Celestial Navigation plugin.
 ******************************************************************************/

#include "TimeStatus.h"

#include <cmath>
#include <vector>

namespace {

std::vector<wxString> SplitCsv(const wxString& text) {
  std::vector<wxString> fields;
  size_t begin = 0;
  for (size_t i = 0; i <= text.length(); ++i) {
    if (i == text.length() || text[i] == ',') {
      fields.push_back(text.Mid(begin, i - begin));
      begin = i + 1;
    }
  }
  return fields;
}

bool HexDigit(wxChar c, unsigned int* value) {
  if (c >= '0' && c <= '9')
    *value = c - '0';
  else if (c >= 'A' && c <= 'F')
    *value = c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    *value = c - 'a' + 10;
  else
    return false;
  return true;
}

bool ParseTwoDigits(const wxString& value, size_t offset, int* result) {
  if (offset + 2 > value.length() || value[offset] < '0' ||
      value[offset] > '9' || value[offset + 1] < '0' ||
      value[offset + 1] > '9')
    return false;
  *result = static_cast<int>(value[offset].GetValue() - '0') * 10 +
            static_cast<int>(value[offset + 1].GetValue() - '0');
  return true;
}

bool ParseTime(const wxString& field, int* hour, int* minute, int* second,
               int* millisecond) {
  if (field.length() < 6 || !ParseTwoDigits(field, 0, hour) ||
      !ParseTwoDigits(field, 2, minute) ||
      !ParseTwoDigits(field, 4, second))
    return false;

  *millisecond = 0;
  if (field.length() > 6) {
    if (field[6] != '.') return false;
    int scale = 100;
    for (size_t i = 7; i < field.length() && i < 10; ++i) {
      if (field[i] < '0' || field[i] > '9') return false;
      *millisecond +=
          static_cast<int>(field[i].GetValue() - '0') * scale;
      scale /= 10;
    }
    for (size_t i = 10; i < field.length(); ++i)
      if (field[i] < '0' || field[i] > '9') return false;
  }

  return *hour < 24 && *minute < 60 && *second < 60;
}

bool BuildUtc(int day, int month, int year, int hour, int minute, int second,
              int millisecond, wxDateTime* utc) {
  if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31)
    return false;

  const wxDateTime::Month wxMonth =
      static_cast<wxDateTime::Month>(month - 1);
  if (day > static_cast<int>(wxDateTime::GetNumberOfDays(wxMonth, year)))
    return false;

  wxDateTime value(day, wxMonth, year, hour, minute, second, millisecond);
  if (!value.IsValid()) return false;
  value.MakeFromTimezone(wxDateTime::UTC);
  *utc = value;
  return true;
}

bool VerifyChecksum(const wxString& sentence, size_t dollar, size_t star) {
  if (star == wxString::npos) return false;
  if (star + 2 >= sentence.length()) return false;

  unsigned int high = 0, low = 0;
  if (!HexDigit(sentence[star + 1], &high) ||
      !HexDigit(sentence[star + 2], &low))
    return false;

  unsigned int checksum = 0;
  for (size_t i = dollar + 1; i < star; ++i)
    checksum ^= static_cast<unsigned int>(sentence[i].GetValue()) & 0xff;
  return checksum == ((high << 4) | low);
}

}  // namespace

GnssTimeMonitor::GnssTimeMonitor() : m_valid(false) {}

bool GnssTimeMonitor::ParseNmeaUtc(const wxString& sentence,
                                   wxDateTime* utc, wxString* source) {
  if (!utc || !source) return false;

  const size_t dollar = sentence.find('$');
  if (dollar == wxString::npos) return false;
  const size_t star = sentence.find('*', dollar + 1);
  if (!VerifyChecksum(sentence, dollar, star)) return false;

  const size_t end = star == wxString::npos ? sentence.length() : star;
  const std::vector<wxString> fields =
      SplitCsv(sentence.Mid(dollar + 1, end - dollar - 1));
  if (fields.empty() || fields[0].length() < 3) return false;

  const wxString type = fields[0].Right(3).Upper();
  int hour = 0, minute = 0, second = 0, millisecond = 0;

  if (type == "RMC") {
    if (fields.size() < 10 || fields[2].Upper() != "A" ||
        !ParseTime(fields[1], &hour, &minute, &second, &millisecond) ||
        fields[9].length() != 6)
      return false;

    int day = 0, month = 0, short_year = 0;
    if (!ParseTwoDigits(fields[9], 0, &day) ||
        !ParseTwoDigits(fields[9], 2, &month) ||
        !ParseTwoDigits(fields[9], 4, &short_year))
      return false;
    const int year = short_year >= 80 ? 1900 + short_year : 2000 + short_year;
    if (!BuildUtc(day, month, year, hour, minute, second, millisecond, utc))
      return false;
  } else if (type == "ZDA") {
    if (fields.size() < 5 ||
        !ParseTime(fields[1], &hour, &minute, &second, &millisecond))
      return false;

    long day = 0, month = 0, year = 0;
    if (!fields[2].ToLong(&day) || !fields[3].ToLong(&month) ||
        !fields[4].ToLong(&year) ||
        !BuildUtc(static_cast<int>(day), static_cast<int>(month),
                  static_cast<int>(year), hour, minute, second, millisecond,
                  utc))
      return false;
  } else {
    return false;
  }

  *source = type;
  return true;
}

bool GnssTimeMonitor::Update(const wxString& sentence) {
  wxDateTime utc;
  wxString source;
  if (!ParseNmeaUtc(sentence, &utc, &source)) return false;

  std::lock_guard<std::mutex> lock(m_mutex);
  m_valid = true;
  m_utc = utc;
  m_source = source;
  m_received = std::chrono::steady_clock::now();
  return true;
}

GnssTimeSnapshot GnssTimeMonitor::Snapshot() const {
  GnssTimeSnapshot snapshot;
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_valid) return snapshot;

  snapshot.valid = true;
  snapshot.utc = m_utc;
  snapshot.source = m_source;
  snapshot.age_milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - m_received)
          .count();
  return snapshot;
}

bool ParseChronyTrackingCsv(const wxString& line,
                            ChronyTrackingInfo* result) {
  if (!result) return false;
  const std::vector<wxString> fields = SplitCsv(line.BeforeFirst('\n'));
  if (fields.size() < 14) return false;

  double reference = 0.0, offset = 0.0;
  long stratum = 0;
  if (!fields[2].ToLong(&stratum) || !fields[3].ToDouble(&reference) ||
      !fields[4].ToDouble(&offset))
    return false;

  ChronyTrackingInfo parsed;
  parsed.valid = true;
  parsed.synchronized = stratum > 0 && fields[13].CmpNoCase("Normal") == 0;
  parsed.reference_unix_seconds = reference;
  parsed.system_offset_seconds = offset;
  parsed.source = fields[1];
  parsed.leap_status = fields[13];
  parsed.leap_status.Trim(true).Trim(false);
  *result = parsed;
  return true;
}
