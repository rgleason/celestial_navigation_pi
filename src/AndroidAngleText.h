#pragma once
#ifdef __OCPN__ANDROID__
#include <QString>
#include <QRegularExpression>
#include <wx/string.h>
#include <cmath>
#include <limits>

namespace celestial_android {
// Display the shortest significant-digit form which preserves the stored
// double. This removes binary tails such as 0.10000000000000001 without
// rounding a navigator's angles, corrections or uncertainty. File/report
// serialization remains unchanged. QString uses the C decimal separator.
inline wxString NumberText(double value) {
  if (value == 0) return std::signbit(value) ? "-0" : "0";
  for (int precision = 1; precision <= 17; ++precision) {
    const QString text = QString::number(value, 'g', precision);
    bool ok = false;
    const double parsed = text.toDouble(&ok);
    if (ok && parsed == value) {
      // Keep ordinary navigational values in decimal form.  One significant
      // digit formats -20 as -2e+01, which is exact but awkward in an editor.
      if (text.contains('e') && std::abs(value) >= 1e-12 &&
          std::abs(value) < 1e12) {
        for (int places = 0; places <= 17; ++places) {
          const QString decimal = QString::number(value, 'f', places);
          if (decimal.toDouble(&ok) == value && ok)
            return wxString::FromUTF8(decimal.toUtf8().constData());
        }
      }
      return wxString::FromUTF8(text.toUtf8().constData());
    }
  }
  return wxString::FromUTF8(QString::number(value, 'g', 17).toUtf8().constData());
}
// Parse UI text without the host's 64-character stack parser or ambiguity
// between scientific exponent E and the east hemisphere. No partial parses.
inline bool ParseAngleText(const wxString& value, double* result) {
  QString text = QString::fromUtf8(value.utf8_str()).trimmed().toUpper();
  static const QRegularExpression decimal("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:E[+-]?[0-9]+)?$");
  bool ok = false;
  if (decimal.match(text).hasMatch()) {
    const double number = text.toDouble(&ok);
    if (ok && std::isfinite(number)) { *result = number; return true; }
    return false;
  }
  int sign = 1;
  QChar hemisphere;
  const QString directions("NSEW");
  if (!text.isEmpty() && directions.contains(text.front())) { hemisphere = text.front(); text.remove(0, 1); }
  if (!text.isEmpty() && directions.contains(text.back())) {
    if (!hemisphere.isNull()) return false;
    hemisphere = text.back(); text.chop(1);
  }
  text = text.trimmed();
  if (hemisphere == 'S' || hemisphere == 'W') sign = -1;
  if (text.startsWith('-')) {
    if (!hemisphere.isNull() && sign > 0) return false;
    sign = -1; text.remove(0, 1);
  } else if (text.startsWith('+')) text.remove(0, 1);
  text.replace(QChar(0x00b0), ' '); text.replace(QChar(0x2032), ' ');
  text.replace(QChar(0x2033), ' '); text.replace('\'', ' '); text.replace('"', ' ');
  text.replace(':', ' ');
  static const QRegularExpression spaces("\\s+");
  const auto parts = text.trimmed().split(spaces, QString::SkipEmptyParts);
  if (parts.isEmpty() || parts.size() > 3) return false;
  double angle = 0;
  for (int i = 0; i < parts.size(); ++i) {
    // Exponents only in complete decimal-degree text, never in D/M/S fields.
    QString token = parts[i];
    if (!decimal.match(token).hasMatch() || token.contains('E') || token.startsWith('-') || token.startsWith('+')) return false;
    const double number = token.toDouble(&ok);
    if (!ok || !std::isfinite(number) || number < 0 || (i > 0 && number >= 60)) return false;
    if (i + 1 < parts.size() && number != std::floor(number)) return false;
    angle += number / (i == 0 ? 1.0 : i == 1 ? 60.0 : 3600.0);
  }
  *result = angle * sign;
  return std::isfinite(*result);
}
inline double AngleTextValue(const wxString& text) {
  double value = std::numeric_limits<double>::quiet_NaN();
  ParseAngleText(text, &value); return value;
}
}
#endif
