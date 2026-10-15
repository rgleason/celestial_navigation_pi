#include "LunarPresentation.h"
#include "NavigationUIUtils.h"
#include "Sight.h"
#include "UtcDateTime.h"

#include <algorithm>
#include <cmath>

namespace lunar_presentation {
namespace {
wxString Angle(double value,
               NavigationAngleKind kind = NavigationAngleKind::Generic) {
  return std::isfinite(value) ? FormatNavigationAngle(value, kind)
                              : _("Unavailable");
}
wxString Decimal(double value) {
  return std::isfinite(value) ? wxString::Format("%.8f", value)
                              : _("Unavailable");
}
wxString Minutes(double degrees) {
  return std::isfinite(degrees) ? wxString::Format("%.4f arcmin", degrees * 60)
                                : _("Unavailable");
}
wxString AltitudeLimb(lunar_distance::AltitudeLimb limb) {
  if (limb == lunar_distance::AltitudeLimb::Lower) return _("Lower limb");
  if (limb == lunar_distance::AltitudeLimb::Upper) return _("Upper limb");
  return _("Centre");
}
wxString Contact(lunar_distance::DistanceContact limb) {
  if (limb == lunar_distance::DistanceContact::Near) return _("Near limb");
  if (limb == lunar_distance::DistanceContact::Far) return _("Far limb");
  return _("Centre");
}
}  // namespace

wxString SolutionId(std::size_t index) {
  wxString label;
  do {
    label.Prepend(wxString(static_cast<wxChar>('A' + index % 26)));
    if (index < 26) break;
    index = index / 26 - 1;
  } while (true);
  return label;
}

wxString ResultUtc(const wxDateTime& utc) {
  if (!utc.IsValid()) return _("Unavailable");
  const int milliseconds = UtcDateTime::Fields(utc).msec;
  const auto rounded = UtcDateTime::AddSeconds(
      utc, (std::round(milliseconds / 100.0) * 100 - milliseconds) / 1000.0);
  return UtcDateTime::FormatUtc(rounded, "%Y-%m-%d %H:%M:%S") +
         wxString::Format(".%d", UtcDateTime::Fields(rounded).msec / 100);
}

Report BuildReport(const Sight& sight, int selected) {
  Report result;
  const auto observation = sight.LunarObservation();
  const auto ephemeris = sight.LunarEphemeris();
  const auto* candidate = selected >= 0 && static_cast<std::size_t>(selected) <
                                               sight.m_LunarCandidates.size()
                              ? &sight.m_LunarCandidates[selected]
                              : nullptr;
  const double offset = candidate ? candidate->offset_seconds : 0;
  const auto epoch = UtcDateTime::AddSeconds(sight.m_CorrectedDateTime, offset);
  const wxString provider = sight.m_LunarUsesDe440 ? _("JPL DE440s")
                            : sight.m_LunarUsesCompact
                                ? _("Compact")
                                : _("Classic analytical");
  result.heading =
      (candidate ? _("Solution ") + SolutionId(selected) : _("Entered UTC")) +
      " | " + UtcDateTime::FormatUtc(epoch, "%Y-%m-%d %H:%M:%S.%l UTC") +
      " | " + provider + " | WGS84";

  Table readings{_("Recorded observations"),
                 {_("Reading"), _("Entered time"), _("Sextant angle"),
                  _("Limb / contact")},
                 {},
                 wxEmptyString};
  readings.rows.push_back(
      {_("Lunar distance"),
       UtcDateTime::FormatUtc(sight.m_DateTime, "%Y-%m-%d %H:%M:%S.%l"),
       Angle(observation.raw_distance_deg),
       _("Moon: ") + Contact(observation.moon_contact) + "; " + sight.m_Body +
           ": " + Contact(observation.body_contact)});
  for (bool moon : {true, false}) {
    readings.rows.push_back(
        {moon ? _("Moon altitude") : sight.m_Body + _(" altitude"),
         UtcDateTime::FormatUtc(
             UtcDateTime::AddSeconds(
                 sight.m_DateTime, moon ? observation.moon_time_offset_seconds
                                        : observation.body_time_offset_seconds),
             "%Y-%m-%d %H:%M:%S.%l"),
         Angle(moon ? observation.moon_altitude_deg
                    : observation.body_altitude_deg),
         AltitudeLimb(moon ? observation.moon_altitude_limb
                           : observation.body_altitude_limb)});
  }
  readings.note = wxString::Format(
      _("Existing clock correction: %+.3f s. Entered UTC after this "
        "correction: %s."),
      UtcDateTime::SecondsBetween(sight.m_CorrectedDateTime, sight.m_DateTime),
      UtcDateTime::FormatUtc(sight.m_CorrectedDateTime,
                             "%Y-%m-%d %H:%M:%S.%l"));
  result.overview.push_back(readings);

  lunar_distance::EphemerisSample sample;
  std::string error;
  if (!ephemeris || !ephemeris(offset, &sample, &error)) {
    result.almanac.push_back({_("Almanac unavailable"),
                              {_("Reason")},
                              {{wxString::FromUTF8(error.c_str())}},
                              wxEmptyString});
    return result;
  }
  if (candidate && !candidate->positions.empty()) {
    std::size_t branch =
        sight.m_LunarSelectedPosition >= 0
            ? static_cast<std::size_t>(sight.m_LunarSelectedPosition)
            : 0;
    if (branch >= candidate->positions.size()) branch = 0;
    const auto prediction = lunar_distance::PredictTimeTaggedObservation(
        observation, ephemeris, offset, candidate->positions[branch]);
    if (prediction.valid) {
      Table check{_("Forward check at the selected solution"),
                  {_("Reading"), _("Measured (deg)"), _("Predicted (deg)"),
                   _("Model - observed (arcmin)")},
                  {},
                  _("These fitted residuals describe numerical agreement. "
                    "Sight accuracy is checked separately at entered UTC and "
                    "reference position.")};
      for (int reading = 0; reading < 3; ++reading) {
        const double measured = reading == 0   ? observation.raw_distance_deg
                                : reading == 1 ? observation.moon_altitude_deg
                                               : observation.body_altitude_deg;
        const double predicted = reading == 0   ? prediction.raw_distance_deg
                                 : reading == 1 ? prediction.moon_altitude_deg
                                                : prediction.body_altitude_deg;
        check.rows.push_back(
            {reading == 0   ? _("Raw lunar distance")
             : reading == 1 ? _("Moon Hs")
                            : sight.m_Body + " Hs",
             Decimal(measured), Decimal(predicted),
             wxString::Format("%+.6f", (predicted - measured) * 60)});
      }
      result.overview.push_back(check);
    }
  }

  auto gha = [](double longitude) {
    const double value = std::fmod(-longitude, 360.0);
    return value < 0 ? value + 360 : value;
  };
  result.almanac.push_back(
      {_("Geocentric almanac at the lunar-distance epoch"),
       {_("Quantity"), _("Moon (deg)"), _("Moon"), sight.m_Body + _(" (deg)"),
        sight.m_Body},
       {{_("GHA"), Decimal(gha(sample.moon_geographic_longitude_deg)),
         Angle(gha(sample.moon_geographic_longitude_deg)),
         Decimal(gha(sample.body_geographic_longitude_deg)),
         Angle(gha(sample.body_geographic_longitude_deg))},
        {_("Declination"), Decimal(sample.moon_geographic_latitude_deg),
         Angle(sample.moon_geographic_latitude_deg,
               NavigationAngleKind::Latitude),
         Decimal(sample.body_geographic_latitude_deg),
         Angle(sample.body_geographic_latitude_deg,
               NavigationAngleKind::Latitude)},
        {_("HP"), Decimal(sample.moon_horizontal_parallax_deg),
         Minutes(sample.moon_horizontal_parallax_deg),
         Decimal(sample.body_horizontal_parallax_deg),
         Minutes(sample.body_horizontal_parallax_deg)},
        {_("SD (geocentric)"), Decimal(sample.moon_semidiameter_deg),
         Minutes(sample.moon_semidiameter_deg),
         Decimal(sample.body_semidiameter_deg),
         Minutes(sample.body_semidiameter_deg)}},
       result.heading});
  result.almanac.push_back(
      {_("Instrument and atmosphere"),
       {_("Setting"), _("Value")},
       {{_("Index error (on arc +)"),
         wxString::Format("%+.4f arcmin", observation.index_error_arcmin)},
        {_("Index correction"),
         wxString::Format("%+.4f arcmin", -observation.index_error_arcmin)},
        {_("Eye height"), wxString::Format("%.2f m", observation.eye_height_m)},
        {_("Pressure / temperature"),
         wxString::Format("%.1f hPa / %.1f C", observation.pressure_hpa,
                          observation.temperature_c)},
        {_("Horizon"), observation.artificial_horizon ? _("Artificial")
                       : observation.dip_short        ? _("Short distance")
                                                      : _("Natural")}},
       _("Dip applies to horizon altitudes only.")});

  if (!observation.separate_times) {
    const auto clearance = lunar_distance::ClearDistance(observation, sample);
    if (clearance.valid) {
      Table reductions{
          _("Direct Triangle comparison: altitude reductions"),
          {_("Stage"), _("Moon (deg)"), _("Moon"), sight.m_Body + _(" (deg)"),
           sight.m_Body},
          {},
          _("Spherical comparison at the displayed UTC. The WGS84 solution "
            "fits the original observations with its forward model.")};
      auto add = [&](const wxString& label, double moon, double body) {
        reductions.rows.push_back(
            {label, Decimal(moon), Angle(moon), Decimal(body), Angle(body)});
      };
      add(_("Sextant altitude Hs"), observation.moon_altitude_deg,
          observation.body_altitude_deg);
      add(_("Horizon limb altitude"), clearance.moon_apparent_limb_altitude_deg,
          clearance.body_apparent_limb_altitude_deg);
      add(_("Apparent centre altitude Ha"),
          clearance.moon_apparent_center_altitude_deg,
          clearance.body_apparent_center_altitude_deg);
      add(_("Refraction (subtract)"), clearance.moon_refraction_deg,
          clearance.body_refraction_deg);
      add(_("Airless topocentric altitude"),
          clearance.moon_topocentric_altitude_deg,
          clearance.body_topocentric_altitude_deg);
      add(_("Parallax in altitude (add)"),
          clearance.moon_parallax_in_altitude_deg,
          clearance.body_parallax_in_altitude_deg);
      add(_("Geocentric altitude Ho"), clearance.moon_geocentric_altitude_deg,
          clearance.body_geocentric_altitude_deg);
      result.almanac.push_back(reductions);
      result.almanac.push_back(
          {_("Lunar-distance quantities"),
           {_("Quantity"), _("Degrees"), _("Navigation notation")},
           {{_("Observed raw LD"), Decimal(observation.raw_distance_deg),
             Angle(observation.raw_distance_deg)},
            {_("Apparent centre LD (spherical)"),
             Decimal(clearance.apparent_distance_deg),
             Angle(clearance.apparent_distance_deg)},
            {_("Cleared LD (Direct Triangle comparison)"),
             Decimal(clearance.cleared_distance_deg),
             Angle(clearance.cleared_distance_deg)},
            {candidate ? _("Geocentric LD at derived UTC")
                       : _("True geocentric LD at entered UTC"),
             Decimal(sample.predicted_distance_deg),
             Angle(sample.predicted_distance_deg)}},
           _("The Direct Triangle cleared distance is a comparison, not the "
             "WGS84 search target. Model-corrected clearing at the reference "
             "position is shown in the accuracy check.")});
    }
  } else {
    result.almanac.push_back(
        {_("Individual reading times"),
         {_("Reading"), _("Corrected UTC")},
         {{_("Lunar distance"),
           UtcDateTime::FormatUtc(epoch, "%Y-%m-%d %H:%M:%S.%l")},
          {_("Moon altitude"),
           UtcDateTime::FormatUtc(
               UtcDateTime::AddSeconds(epoch,
                                       observation.moon_time_offset_seconds),
               "%Y-%m-%d %H:%M:%S.%l")},
          {sight.m_Body + _(" altitude"),
           UtcDateTime::FormatUtc(
               UtcDateTime::AddSeconds(epoch,
                                       observation.body_time_offset_seconds),
               "%Y-%m-%d %H:%M:%S.%l")}},
         _("Each altitude is modelled at its own reading time. A simultaneous "
           "Direct Triangle reduction is not used for these readings.")});
  }

  if (sight.m_LunarCalculation) {
    const auto& options = sight.m_LunarSolveOptions;
    result.search.push_back(
        {_("UTC search"),
         {_("Setting"), _("Value")},
         {{_("Start UTC"),
           UtcDateTime::FormatUtc(
               UtcDateTime::AddSeconds(sight.m_CorrectedDateTime,
                                       options.start_offset_seconds),
               "%Y-%m-%d %H:%M:%S.%l")},
          {_("End UTC"), UtcDateTime::FormatUtc(UtcDateTime::AddSeconds(
                                                    sight.m_CorrectedDateTime,
                                                    options.end_offset_seconds),
                                                "%Y-%m-%d %H:%M:%S.%l")},
          {_("Scan interval"),
           wxString::Format("%.3f s", options.scan_step_seconds)},
          {_("Root tolerance"),
           wxString::Format("%.3f s", options.root_tolerance_seconds)}},
         _("For WGS84, fit both altitudes at each trial UTC, then test "
           "predicted raw LD minus observed raw LD. Full evaluations remain in "
           "Detailed trail.")});
    if (candidate) {
      const auto& trace = sight.m_LunarCalculation->match_trace;
      // Match the final evaluation by epoch, never by row order: root discovery
      // order and UTC-sorted candidate order can differ.
      int branch = -1;
      for (const auto& entry : trace)
        if (entry.phase == lunar_distance::MatchTracePhase::Refinement &&
            std::fabs(entry.offset_seconds - offset) < 1e-7)
          branch = entry.branch;
      Table scan{_("Coarse evaluations near the selected UTC"),
                 {_("Solver branch"), _("Offset (s)"), _("Trial UTC"),
                  _("Model (deg)"), _("Target (deg)"), _("Residual (arcmin)")},
                 {},
                 wxEmptyString};
      Table refine{_("Refinement brackets containing the selected UTC"),
                   {_("Solver branch"), _("Step"), _("Left (s)"),
                    _("Right (s)"), _("Trial (s)"), _("Residual (arcmin)")},
                   {},
                   _("Retain the half-interval with opposite residual signs. "
                     "Solver branch numbers identify the search trail; "
                     "solution letters identify the Results rows.")};
      for (const auto& entry : trace) {
        if (branch >= 0 && entry.branch != branch) continue;
        if (entry.phase == lunar_distance::MatchTracePhase::Scan &&
            std::fabs(entry.offset_seconds - offset) <=
                options.scan_step_seconds * 1.1 &&
            (std::fabs((entry.offset_seconds - options.start_offset_seconds) /
                           options.scan_step_seconds -
                       std::round((entry.offset_seconds -
                                   options.start_offset_seconds) /
                                  options.scan_step_seconds)) < 1e-7 ||
             std::fabs(entry.offset_seconds - options.end_offset_seconds) <
                 1e-7)) {
          scan.rows.push_back(
              {wxString::Format("%d", entry.branch),
               wxString::Format("%+.3f", entry.offset_seconds),
               UtcDateTime::FormatUtc(
                   UtcDateTime::AddSeconds(sight.m_CorrectedDateTime,
                                           entry.offset_seconds),
                   "%H:%M:%S.%l"),
               Decimal(entry.model_distance_deg),
               Decimal(entry.target_distance_deg),
               wxString::Format("%+.6f", entry.residual_arcmin)});
        }
        if (entry.phase == lunar_distance::MatchTracePhase::Refinement &&
            offset >= entry.bracket_start_seconds - 1e-8 &&
            offset <= entry.bracket_end_seconds + 1e-8) {
          refine.rows.push_back(
              {wxString::Format("%d", entry.branch),
               wxString::Format("%d", entry.iteration + 1),
               wxString::Format("%+.6f", entry.bracket_start_seconds),
               wxString::Format("%+.6f", entry.bracket_end_seconds),
               wxString::Format("%+.6f", entry.offset_seconds),
               wxString::Format("%+.6f", entry.residual_arcmin)});
        }
      }
      result.search.push_back(scan);
      result.search.push_back(refine);
      result.search.push_back(
          {_("Converged solution ") + SolutionId(selected),
           {_("Quantity"), _("Value")},
           {{_("Entered UTC + additional correction"),
             wxString::Format(
                 "%s + (%+.6f s) = %s",
                 UtcDateTime::FormatUtc(sight.m_CorrectedDateTime,
                                        "%H:%M:%S.%l"),
                 offset,
                 UtcDateTime::FormatUtc(epoch, "%Y-%m-%d %H:%M:%S.%l UTC"))},
            {_("Geocentric distance at derived UTC"),
             Decimal(sample.predicted_distance_deg) + " deg | " +
                 Angle(sample.predicted_distance_deg)}},
           _("Numerical convergence and measurement uncertainty are different. "
             "The full precision here does not imply millisecond sight "
             "accuracy.")});
    }
  }
  return result;
}
}  // namespace lunar_presentation
