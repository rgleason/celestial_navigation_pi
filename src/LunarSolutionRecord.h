#ifndef CELESTIAL_LUNAR_SOLUTION_RECORD_H
#define CELESTIAL_LUNAR_SOLUTION_RECORD_H

#include "Sight.h"
#include "UtcDateTime.h"
#include "tinyxml.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

inline void SetPreciseXmlDouble(TiXmlElement* element, const char* name,
                                double value) {
  std::ostringstream text;
  text.imbue(std::locale::classic());
  text << std::setprecision(17) << value;
  element->SetAttribute(name, text.str().c_str());
}

inline bool ReadXmlDouble(const TiXmlElement* element, const char* name,
                          double* value) {
  const char* text = element->Attribute(name);
  return text && wxString::FromUTF8(text).ToCDouble(value);
}

// Derived, append-only results. Snapshots retain the exact inputs even if a
// sight is later edited or deleted. Neither saving nor viewing a result changes
// the recorded observations or the global ClockError.
struct LunarSolutionRecord {
  wxString name, created_utc, reference_time, method, report;
  double base_correction_seconds = 0;
  double additional_correction_seconds = 0;
  double time_sigma_seconds = 0;
  std::vector<wxString> inputs;

  double TotalCorrection() const {
    return base_correction_seconds + additional_correction_seconds;
  }
  wxString Summary() const {
    return wxString::Format("%s | %s | UTC correction %+.3f s", name,
                            reference_time, TotalCorrection());
  }
  wxString Details() const {
    wxString text =
        Summary() + "\nSaved UTC: " + created_utc + "\nMethod: " + method +
        wxString::Format(
            "\nExisting correction: %+.3f s\nAdditional correction: %+.3f s\n"
            "Time uncertainty: %.3f s (1-sigma)\n"
            "Recorded readings and global correction were not changed.\n\n",
            base_correction_seconds, additional_correction_seconds,
            time_sigma_seconds);
    for (const auto& input : inputs) text += input + "\n";
    return text + "\n" + report;
  }
};

inline wxString LunarInputSnapshot(const Sight& sight) {
  const auto o = sight.LunarObservation();
  std::ostringstream s;
  s.imbue(std::locale::classic());
  s << std::setprecision(17) << "Moon-" << sight.m_Body.ToStdString()
    << " recorded="
    << UtcDateTime::FormatUtc(sight.m_DateTime, "%Y-%m-%d %H:%M:%S.%l")
           .ToStdString()
    << " LD=" << o.raw_distance_deg << " HsMoon=" << o.moon_altitude_deg
    << " HsBody=" << o.body_altitude_deg
    << " altitudeLimbs=" << int(o.moon_altitude_limb) << ','
    << int(o.body_altitude_limb) << " distanceContacts=" << int(o.moon_contact)
    << ',' << int(o.body_contact) << " IE_arcmin=" << o.index_error_arcmin
    << " eye_m=" << o.eye_height_m << " pressure_hPa=" << o.pressure_hpa
    << " temperature_C=" << o.temperature_c
    << " artificialHorizon=" << o.artificial_horizon
    << " dipShort=" << o.dip_short << " dipShort_m=" << o.dip_short_distance_m
    << " sigmas_arcmin=" << o.distance_uncertainty_arcmin << ','
    << o.moon_altitude_uncertainty_arcmin << ','
    << o.body_altitude_uncertainty_arcmin
    << " separateTimes=" << o.separate_times
    << " offsets_s=" << o.moon_time_offset_seconds << ','
    << o.body_time_offset_seconds << " motion=" << o.moving_observer
    << " COG=" << o.course_true_deg << " SOG=" << o.speed_knots
    << " DR=" << sight.m_DRLat << ',' << sight.m_DRLon
    << " searchSpan_s=" << sight.m_TimeCertainty << " recordedTimeBasis="
    << (sight.m_LunarTimeIsWatch ? "watch" : "nominalUTC")
    << " earthModel=" << (o.use_ellipsoid ? "WGS84" : "sphere") << " ephemeris="
    << (sight.m_LunarUsesDe440 ? "DE440s-apparent" : "analytical-fallback")
    << " solverVersion=2.8.5.1";
  return wxString::FromUTF8(s.str().c_str());
}

inline void WriteLunarSolutions(
    TiXmlElement* clock, const std::vector<LunarSolutionRecord>& records) {
  for (const auto& record : records) {
    auto* e = new TiXmlElement("LunarSolution");
    e->SetAttribute("format", 1);
    e->SetAttribute("name", record.name.ToUTF8().data());
    e->SetAttribute("createdUTC", record.created_utc.ToUTF8().data());
    e->SetAttribute("referenceTime", record.reference_time.ToUTF8().data());
    e->SetAttribute("method", record.method.ToUTF8().data());
    SetPreciseXmlDouble(e, "baseCorrectionSeconds",
                        record.base_correction_seconds);
    SetPreciseXmlDouble(e, "additionalCorrectionSeconds",
                        record.additional_correction_seconds);
    if (std::isfinite(record.time_sigma_seconds))
      SetPreciseXmlDouble(e, "timeSigmaSeconds", record.time_sigma_seconds);
    for (const auto& input : record.inputs) {
      auto* snapshot = new TiXmlElement("InputSnapshot");
      snapshot->LinkEndChild(new TiXmlText(input.ToUTF8().data()));
      e->LinkEndChild(snapshot);
    }
    auto* report = new TiXmlElement("Report");
    report->LinkEndChild(new TiXmlText(record.report.ToUTF8().data()));
    e->LinkEndChild(report);
    clock->LinkEndChild(e);
  }
}

inline bool PrepareLunarFixSights(const std::vector<Sight>& recorded,
                                  const LunarSolutionRecord* solution,
                                  double correction,
                                  std::vector<Sight>* working,
                                  wxString* error) {
  working->clear();
  wxDateTime reference;
  if (solution && !reference.ParseISOCombined(solution->reference_time, ' ')) {
    *error = "The saved solution has no usable reference epoch.";
    return false;
  }
  for (const auto& sight : recorded) {
    if (!sight.IsVisible() ||
        (sight.m_Type != Sight::ALTITUDE && sight.m_Type != Sight::HORIZON))
      continue;
    if (solution && std::fabs(UtcDateTime::SecondsBetween(
                        sight.m_DateTime, reference)) > 12 * 3600) {
      *error =
          "Visible sights extend beyond this lunar watch (12 hours from "
          "reference). Select sights from the same watch and clock.";
      working->clear();
      return false;
    }
    working->push_back(sight);
    working->back().Recompute(correction);
  }
  return true;
}

inline std::vector<LunarSolutionRecord> ReadLunarSolutions(
    const TiXmlElement* clock) {
  std::vector<LunarSolutionRecord> records;
  if (!clock) return records;
  auto attr = [](const TiXmlElement* e, const char* key) {
    const char* value = e->Attribute(key);
    return value ? wxString::FromUTF8(value) : wxString();
  };
  for (const auto* e = clock->FirstChildElement("LunarSolution"); e;
       e = e->NextSiblingElement("LunarSolution")) {
    LunarSolutionRecord r;
    int format = 0;
    if (e->QueryIntAttribute("format", &format) != TIXML_SUCCESS || format != 1)
      continue;
    if (!ReadXmlDouble(e, "baseCorrectionSeconds",
                       &r.base_correction_seconds) ||
        !ReadXmlDouble(e, "additionalCorrectionSeconds",
                       &r.additional_correction_seconds) ||
        !std::isfinite(r.TotalCorrection()))
      continue;
    r.time_sigma_seconds = INFINITY;
    ReadXmlDouble(e, "timeSigmaSeconds", &r.time_sigma_seconds);
    if (!(r.time_sigma_seconds >= 0.0)) r.time_sigma_seconds = INFINITY;
    r.name = attr(e, "name");
    r.created_utc = attr(e, "createdUTC");
    r.reference_time = attr(e, "referenceTime");
    r.method = attr(e, "method");
    for (const auto* s = e->FirstChildElement("InputSnapshot"); s;
         s = s->NextSiblingElement("InputSnapshot"))
      if (s->GetText()) r.inputs.push_back(wxString::FromUTF8(s->GetText()));
    const auto* report = e->FirstChildElement("Report");
    if (report && report->GetText())
      r.report = wxString::FromUTF8(report->GetText());
    records.push_back(r);
  }
  return records;
}
#endif
