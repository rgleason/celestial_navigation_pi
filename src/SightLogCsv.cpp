#include "StringVector.h"
#include "SightLogCsv.h"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

#include "NavigationUIUtils.h"
#include "Sight.h"
#include "UtcDateTime.h"

namespace {
wxString Number(double value) {
  if (!std::isfinite(value)) return wxString();
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(3) << value;
  std::string text = stream.str();
  while (text.back() == '0') text.pop_back();
  if (text.back() == '.') text.pop_back();
  if (text == "-0") text = "0";
  return wxString::FromUTF8(text.c_str());
}

wxDateTime SightUtc(const Sight& sight, int clockCorrection) {
  if (!sight.m_DateTime.IsValid()) return wxDateTime();
  wxDateTime utc = UtcDateTime::AddSeconds(sight.m_DateTime, clockCorrection);
  if (sight.m_Type == Sight::LUNAR && sight.m_LunarTimeIsWatch) {
    const int selected = sight.m_LunarSelectedCandidate;
    if (!sight.m_LunarSolutionValid || selected < 0 ||
        static_cast<size_t>(selected) >= sight.m_LunarCandidates.size())
      return wxDateTime();
    const double correction = sight.m_LunarCandidates[selected].offset_seconds;
    if (!std::isfinite(correction)) return wxDateTime();
    utc = UtcDateTime::AddSeconds(utc, correction);
  }
  return utc;
}

wxString Bearing(const Sight& sight, const wxDateTime& utc, bool validDr) {
  double bearing = NAN;
  bool magnetic = false;
  if (sight.m_Type == Sight::AZIMUTH) {
    bearing = sight.m_Measurement;
    magnetic = sight.m_bMagneticNorth;
  } else if (sight.m_Type == Sight::HORIZON) {
    if (sight.m_HorizonBearingProvided) {
      bearing = sight.m_HorizonBearing;
      magnetic = sight.m_HorizonBearingMagnetic;
    }
  } else if (utc.IsValid() && validDr) {
    // Only evaluate the body position; exporting must not run a lunar search
    // or change any cached calculations, selections or plot geometry.
    Sight body(Sight::ALTITUDE,
               sight.m_Type == Sight::LUNAR ? wxString("Moon") : sight.m_Body,
               Sight::CENTER, utc, 0, 0, 0, true);
    body.m_CorrectedDateTime = utc;
    body.m_DRLat = sight.m_DRLat;
    body.m_DRLon = sight.m_DRLon;
    body.m_AllowDe440 = sight.m_AllowDe440;
    body.m_AllowCompact = sight.m_AllowCompact;
    double altitude;
    body.CalculateAtDR(&altitude, &bearing);
  }
  if (!std::isfinite(bearing)) return wxString();
  bearing = std::fmod(bearing, 360.0);
  if (bearing < 0.0) bearing += 360.0;
  return Number(bearing) + (magnetic ? " M" : "");
}

wxString Limb(const Sight& sight) {
  if (sight.m_Type == Sight::LUNAR)
    return sight.m_BodyLimb == Sight::LUNAR_FAR ? "Far" : "Near";
  if (sight.m_Type != Sight::ALTITUDE) return wxString();
  switch (sight.m_BodyLimb) {
    case Sight::LOWER:
      return "Lower";
    case Sight::UPPER:
      return "Upper";
    default:
      return "Center";
  }
}

void AppendRow(wxString* csv, const celnav::StringVector& fields) {
  bool first = true;
  for (wxString field : fields) {
    if (!first) *csv += ',';
    first = false;
    field.Replace("\"", "\"\"");
    *csv += "\"" + field + "\"";
  }
  *csv += "\r\n";
}
}  // namespace

wxString SightLogCsv(const std::vector<Sight>& sights, int clockCorrection) {
  // Fixed English labels make reports portable between language settings.
  wxString csv =
      "Number,UTC date,UTC time,Type,Body / pair,Limb,Hs / LDo,"
      "Zn / bearing (deg),Horizon,Height of eye (m),"
      "Index error (arcmin),Temperature (C),Pressure (mb),"
      "DR latitude,DR longitude,Zone,Remarks\r\n";
  for (size_t index = 0; index < sights.size(); ++index) {
    const Sight& sight = sights[index];
    const wxDateTime utc = SightUtc(sight, clockCorrection);
    const bool validLat =
        std::isfinite(sight.m_DRLat) && std::abs(sight.m_DRLat) <= 90.0;
    const bool validLon =
        std::isfinite(sight.m_DRLon) && std::abs(sight.m_DRLon) <= 180.0;
    wxString type;
    switch (sight.m_Type) {
      case Sight::ALTITUDE:
        type = "Altitude";
        break;
      case Sight::AZIMUTH:
        type = "Azimuth";
        break;
      case Sight::LUNAR:
        type = "Lunar";
        break;
      case Sight::HORIZON:
        type = sight.m_HorizonEvent == Sight::SUNRISE ? "Sunrise" : "Sunset";
        break;
    }
    wxString measurement;
    if (sight.m_Type == Sight::HORIZON)
      measurement = "0";
    else if (std::isfinite(sight.m_Measurement))
      measurement = FormatNavigationAngle(sight.m_Measurement);
    wxString zone;
    if (validLon)
      zone = wxString::Format(
          "UTC%+d", static_cast<int>(std::round(sight.m_DRLon / 15.0)));
    wxString time = UtcDateTime::FormatUtc(utc, "%H:%M:%S");
    if (utc.IsValid() && UtcDateTime::Fields(utc).msec)
      time += UtcDateTime::FormatUtc(utc, ".%l");
    AppendRow(
        &csv,
        {wxString::Format("%zu", index + 1),
         UtcDateTime::FormatUtc(utc, "%Y-%m-%d"), time, type,
         sight.m_Type == Sight::LUNAR     ? "Moon / " + sight.m_Body
         : sight.m_Type == Sight::HORIZON ? wxString("Sun")
                                          : sight.m_Body,
         Limb(sight), measurement, Bearing(sight, utc, validLat && validLon),
         sight.m_ArtificialHorizon ? wxString("AH")
         : sight.m_DipShort        ? wxString("Dip short")
                                   : wxString("Natural"),
         Number(sight.m_EyeHeight), Number(sight.m_IndexError * 60.0),
         Number(sight.m_Temperature), Number(sight.m_Pressure),
         validLat ? FormatNavigationAngle(sight.m_DRLat,
                                          NavigationAngleKind::Latitude)
                  : wxString(),
         validLon ? FormatNavigationAngle(sight.m_DRLon,
                                          NavigationAngleKind::Longitude)
                  : wxString(),
         zone, sight.m_Remarks});
  }
  return csv;
}
