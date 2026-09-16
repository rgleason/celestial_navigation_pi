/******************************************************************************
 * Helpers for the UTC representation historically used by this plugin.
 *
 * Sight stores UTC as calendar fields in a wxDateTime whose default timezone
 * is local.  This is intentionally retained for compatibility with the
 * existing ephemeris code.  These helpers provide an explicit boundary
 * between those UTC fields and real instants so formatting and arithmetic do
 * not accidentally apply the computer's timezone twice.
 ******************************************************************************/

#ifndef CELESTIAL_NAVIGATION_UTC_DATE_TIME_H
#define CELESTIAL_NAVIGATION_UTC_DATE_TIME_H

#include <cmath>

#include <wx/datetime.h>
#include <wx/string.h>

namespace UtcDateTime {

inline wxDateTime CopyFields(const wxDateTime& value) {
  if (!value.IsValid()) return wxDateTime();
  return wxDateTime(value.GetDay(), value.GetMonth(), value.GetYear(),
                    value.GetHour(), value.GetMinute(), value.GetSecond(),
                    value.GetMillisecond());
}

// Convert the plugin's UTC calendar fields to an actual instant represented
// by wxDateTime in the computer's local timezone.
inline wxDateTime ToInstant(const wxDateTime& utcFields) {
  if (!utcFields.IsValid()) return wxDateTime();
  wxDateTime instant = CopyFields(utcFields);
  instant.MakeFromUTC();
  return instant;
}

// Convert an actual wxDateTime instant to the plugin's UTC calendar fields.
inline wxDateTime FromInstant(const wxDateTime& instant) {
  if (!instant.IsValid()) return wxDateTime();
  return CopyFields(instant.ToUTC());
}

inline wxDateTime Now() { return FromInstant(wxDateTime::UNow()); }

inline wxDateTime FromLocalFields(const wxDateTime& localFields) {
  return FromInstant(localFields);
}

inline wxDateTime ToLocalFields(const wxDateTime& utcFields) {
  return ToInstant(utcFields);
}

inline wxDateTime AddSeconds(const wxDateTime& utcFields, double seconds) {
  const wxDateTime instant = ToInstant(utcFields);
  if (!instant.IsValid()) return wxDateTime();
  return FromInstant(
      instant + wxTimeSpan::Milliseconds(static_cast<long long>(
                    std::llround(seconds * 1000.0))));
}

inline double SecondsBetween(const wxDateTime& aUtcFields,
                             const wxDateTime& bUtcFields) {
  const wxDateTime a = ToInstant(aUtcFields);
  const wxDateTime b = ToInstant(bUtcFields);
  if (!a.IsValid() || !b.IsValid()) return 0.0;
  return static_cast<double>((a - b).GetMilliseconds().GetValue()) / 1000.0;
}

inline bool IsEarlier(const wxDateTime& aUtcFields,
                      const wxDateTime& bUtcFields) {
  return ToInstant(aUtcFields).IsEarlierThan(ToInstant(bUtcFields));
}

inline bool IsLater(const wxDateTime& aUtcFields,
                    const wxDateTime& bUtcFields) {
  return ToInstant(aUtcFields).IsLaterThan(ToInstant(bUtcFields));
}

inline wxString FormatUtc(const wxDateTime& utcFields,
                          const wxString& format) {
  const wxDateTime instant = ToInstant(utcFields);
  return instant.IsValid() ? instant.Format(format, wxDateTime::UTC)
                           : wxString();
}

inline wxString FormatLocal(const wxDateTime& utcFields,
                            const wxString& format) {
  const wxDateTime instant = ToInstant(utcFields);
  return instant.IsValid() ? instant.Format(format, wxDateTime::Local)
                           : wxString();
}

inline wxString FormatOffset(const wxDateTime& utcFields, long offsetSeconds,
                             const wxString& format) {
  return FormatUtc(AddSeconds(utcFields, offsetSeconds), format);
}

inline wxString FormatIsoUtc(const wxDateTime& utcFields) {
  return FormatUtc(utcFields, "%Y-%m-%dT%H:%M:%S") + "Z";
}

}  // namespace UtcDateTime

#endif
