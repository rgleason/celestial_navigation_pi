// UTC adapters. Desktop retains the historical calendar-field representation.
// Android stores real UTC instants: local DST gaps cannot corrupt an observation.
#ifndef CELESTIAL_NAVIGATION_UTC_DATE_TIME_H
#define CELESTIAL_NAVIGATION_UTC_DATE_TIME_H

#include <cmath>
#include <ctime>
#include <array>

#include <wx/datetime.h>
#include <wx/string.h>
#ifdef __OCPN__ANDROID__
#include <QDateTime>
#endif

namespace UtcDateTime {

// Construct recorded UTC/calendar fields without passing through local time.
inline wxDateTime Create(int year, int month, int day, int hour = 0,
                         int minute = 0, int second = 0, int millisecond = 0) {
#ifdef __OCPN__ANDROID__
  const QDateTime utc(QDate(year, month, day),
                     QTime(hour, minute, second, millisecond), Qt::UTC);
  return utc.isValid() ? wxDateTime(wxLongLong(utc.toMSecsSinceEpoch()))
                       : wxDateTime();
#else
  return wxDateTime(day, static_cast<wxDateTime::Month>(month - 1), year,
                    hour, minute, second, millisecond);
#endif
}

inline wxDateTime::Tm Fields(const wxDateTime& value) {
#ifndef __OCPN__ANDROID__
  return value.GetTm();
#else
  const auto utc = QDateTime::fromMSecsSinceEpoch(value.GetValue().GetValue(), Qt::UTC);
  wxDateTime::Tm fields;
  fields.year = utc.date().year();
  fields.mon = static_cast<wxDateTime::Month>(utc.date().month() - 1);
  fields.mday = utc.date().day();
  fields.hour = utc.time().hour(); fields.min = utc.time().minute();
  fields.sec = utc.time().second(); fields.msec = utc.time().msec();
  return fields;
#endif
}
inline wxDateTime CopyFields(const wxDateTime& value) {
  if (!value.IsValid()) return wxDateTime();
#ifdef __OCPN__ANDROID__
  return value;
#else
  return wxDateTime(value.GetDay(), value.GetMonth(), value.GetYear(),
                    value.GetHour(), value.GetMinute(), value.GetSecond(),
                    value.GetMillisecond());
#endif
}
inline wxDateTime ToInstant(const wxDateTime& utcFields) {
  if (!utcFields.IsValid()) return wxDateTime();
#ifdef __OCPN__ANDROID__
  return utcFields;
#else
  wxDateTime instant = CopyFields(utcFields);
  instant.MakeFromUTC();
  return instant;
#endif
}
inline wxDateTime FromInstant(const wxDateTime& instant) {
  if (!instant.IsValid()) return wxDateTime();
#ifdef __OCPN__ANDROID__
  return instant;
#else
  return CopyFields(instant.ToUTC());
#endif
}
inline bool ParseUtc(const wxString& text, wxDateTime* value) {
  if (!value) return false;
#ifdef __OCPN__ANDROID__
  QString iso = QString::fromUtf8(text.utf8_str());
  iso.replace(' ', 'T');
  if (!iso.endsWith('Z')) iso += 'Z';
  const auto utc = QDateTime::fromString(iso, Qt::ISODate);
  if (!utc.isValid()) return false;
  *value = wxDateTime(wxLongLong(utc.toMSecsSinceEpoch()));
  return true;
#else
  return value->ParseISOCombined(text, ' ');
#endif
}
// Native calendar/date widgets contain local date fields only. Noon avoids
// local midnight changes; their time is never used as an observation instant.
inline wxDateTime CalendarDate(const wxDateTime& value) {
#ifdef __OCPN__ANDROID__
  if (!value.IsValid()) return wxDateTime();
  const auto f = Fields(value);
  return wxDateTime(f.mday, f.mon, f.year, 12, 0, 0);
#else
  return value;
#endif
}
inline wxDateTime FromCalendar(const wxDateTime& date, int hour = 0,
                               int minute = 0, double second = 0) {
  if (!date.IsValid()) return wxDateTime();
  return Create(date.GetYear(), date.GetMonth() + 1, date.GetDay(), hour,
                minute, static_cast<int>(second),
                static_cast<int>(std::llround((second - std::floor(second)) * 1000)));
}
#ifdef __OCPN__ANDROID__
inline wxDateTime LocalWallToInstant(const wxDateTime& fields) {
  if (!fields.IsValid()) return wxDateTime();
  const auto f = Fields(fields);
  const QDate date(f.year, f.mon + 1, f.mday);
  const QTime time(f.hour, f.min, f.sec, f.msec);
  const QDateTime local(date, time, Qt::LocalTime);
  // Refuse a nonexistent wall time rather than silently normalizing it.
  if (!local.isValid() || local.date() != date || local.time() != time)
    return wxDateTime();
  // Ambiguous local clock times need an explicit UTC entry. Check all real
  // transition offsets used by current zones, including half-hour changes.
  for (int offset : {-7200, -3600, -1800, 1800, 3600, 7200}) {
    const auto alternative = local.addSecs(offset).toLocalTime();
    if (alternative.date() == date && alternative.time() == time) return wxDateTime();
  }
  return wxDateTime(wxLongLong(local.toMSecsSinceEpoch()));
}
inline wxDateTime InstantToLocalWall(const wxDateTime& instant) {
  if (!instant.IsValid()) return wxDateTime();
  const auto local = QDateTime::fromMSecsSinceEpoch(instant.GetValue().GetValue()).toLocalTime();
  return Create(local.date().year(), local.date().month(), local.date().day(),
                local.time().hour(), local.time().minute(), local.time().second(),
                local.time().msec());
}
#endif

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

inline wxString FormatInstant(const wxDateTime& instant, const wxString& format,
                               bool local = false) {
  if (!instant.IsValid()) return wxString();
#ifdef __OCPN__ANDROID__
  // wxQt's copied timezone cache can disagree with Android's timezone. Read
  // the instant directly; neither formatting nor UTC arithmetic uses that cache.
  const time_t seconds = instant.GetValue().GetValue() / 1000;
  std::tm fields{};
  if (!(local ? localtime_r(&seconds, &fields) : gmtime_r(&seconds, &fields)))
    return wxString();
  wxString pattern = format;
  pattern.Replace("%l", wxString::Format("%03d", instant.GetMillisecond()));
  std::array<char, 1024> buffer{};
  if (!std::strftime(buffer.data(), buffer.size(), pattern.utf8_str(), &fields))
    return wxString();
  return wxString::FromUTF8(buffer.data());
#else
  return instant.Format(format, local ? wxDateTime::Local : wxDateTime::UTC);
#endif
}
inline wxString FormatUtc(const wxDateTime& utcFields, const wxString& format) {
  return FormatInstant(ToInstant(utcFields), format);
}
inline wxString FormatRecordedFields(const wxDateTime& fields, const wxString& format) {
#ifdef __OCPN__ANDROID__
  return FormatUtc(fields, format);
#else
  return fields.Format(format);
#endif
}
inline wxString FormatLocal(const wxDateTime& utcFields, const wxString& format) {
  return FormatInstant(ToInstant(utcFields), format, true);
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
