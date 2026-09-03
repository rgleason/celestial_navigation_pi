#include "CoastalNavigationDialog.h"

#include "CelestialNavigationDialog.h"
#include "NavigationUIUtils.h"
#include "OcpnApiCompat.h"
#include "UtcDateTime.h"
#include "celestial_navigation_pi.h"
#include "plugin_dc/dc_utils/include/pidc.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <cmath>

namespace cn = coastal_navigation;

CoastalNavigationDialog::CoastalNavigationDialog(
    CelestialNavigationDialog* parent)
    : wxDialog(parent, wxID_ANY, _("Coastal sextant navigation"),
               wxDefaultPosition, wxSize(900, 760),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_parent(parent),
      m_hasVerticalPosition(false),
      m_hasHorizontalFix(false) {
  const CelestialNavigationDefaults defaults =
      LoadCelestialNavigationDefaults();
  double boatLat = 0.0, boatLon = 0.0;
  celestial_navigation_pi_BoatPos(boatLat, boatLon);
  const wxString lat =
      FormatNavigationAngle(boatLat, NavigationAngleKind::Latitude, true);
  const wxString lon =
      FormatNavigationAngle(boatLon, NavigationAngleKind::Longitude, true);
  const wxString unknownLat;
  const wxString unknownLon;

  wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
  wxStaticText* intro = new wxStaticText(
      this, wxID_ANY,
      _("These are coastal sextant methods. A vertical angle gives distance "
        "from an object; one horizontal angle gives a line of position. Two "
        "independent horizontal angles can give a fix."));
  intro->Wrap(710);
  root->Add(intro, 0, wxALL | wxEXPAND, 10);

  wxNotebook* notebook = new wxNotebook(this, wxID_ANY);
  wxScrolledWindow* vertical = new wxScrolledWindow(notebook, wxID_ANY);
  vertical->SetScrollRate(0, 12);
  wxBoxSizer* verticalRoot = new wxBoxSizer(wxVERTICAL);
  wxFlexGridSizer* verticalGrid = new wxFlexGridSizer(0, 3, 6, 8);
  verticalGrid->AddGrowableCol(1, 1);
  verticalGrid->Add(new wxStaticText(vertical, wxID_ANY, _("Observation")), 0,
                    wxALIGN_CENTER_VERTICAL);
  wxArrayString verticalModes;
  verticalModes.Add(_("Waterline to top (waterline visible)"));
  verticalModes.Add(_("Sea horizon to top (object beyond horizon)"));
  m_verticalMode = new wxChoice(vertical, wxID_ANY, wxDefaultPosition,
                                wxDefaultSize, verticalModes);
  m_verticalMode->SetSelection(0);
  verticalGrid->Add(m_verticalMode, 1, wxEXPAND);
  verticalGrid->AddSpacer(1);
  m_verticalTargetLat = AddField(verticalGrid, vertical, _("Target latitude"),
                                 unknownLat, _("angle"));
  m_verticalTargetLon = AddField(verticalGrid, vertical, _("Target longitude"),
                                 unknownLon, _("angle"));
  m_verticalAngle =
      AddField(verticalGrid, vertical, _("Observed vertical angle"),
               _("0° 30.000'"), _("angle or decimal degrees"));
  m_verticalIndexError =
      AddField(verticalGrid, vertical, _("Index error (on the arc +)"),
               wxString::Format("%.2f", defaults.indexError), _("arcmin"));
  m_verticalHeight = AddField(verticalGrid, vertical, _("Charted top height"),
                              _("30.0"), _("m above height datum"));
  m_verticalWaterLevel =
      AddField(verticalGrid, vertical, _("Water level above height datum"),
               _("0.0"), _("m (tide included)"));
  m_verticalEyeHeight =
      AddField(verticalGrid, vertical, _("Height of eye"),
               wxString::Format("%.1f", defaults.eyeHeight), _("m"));
  verticalRoot->Add(verticalGrid, 0, wxALL | wxEXPAND, 10);

  wxStaticBoxSizer* bearingBox =
      new wxStaticBoxSizer(wxVERTICAL, vertical, _("Optional bearing fix"));
  m_includeBearing =
      new wxCheckBox(bearingBox->GetStaticBox(), wxID_ANY,
                     _("Include observed bearing from vessel to target"));
  bearingBox->Add(m_includeBearing, 0, wxALL, 5);
  wxStaticText* bearingTiming = new wxStaticText(
      bearingBox->GetStaticBox(), wxID_ANY,
      _("Record the bearing at effectively the same time as the vertical "
        "angle, or reduce it to that position epoch before entry."));
  bearingTiming->Wrap(650);
  bearingBox->Add(bearingTiming, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);
  wxFlexGridSizer* bearingGrid = new wxFlexGridSizer(0, 3, 6, 8);
  bearingGrid->AddGrowableCol(1, 1);
  bearingGrid->Add(new wxStaticText(bearingBox->GetStaticBox(), wxID_ANY,
                                    _("Bearing reference")),
                   0, wxALIGN_CENTER_VERTICAL);
  wxArrayString references;
  references.Add(_("True"));
  references.Add(_("Magnetic compass"));
  m_bearingReference =
      new wxChoice(bearingBox->GetStaticBox(), wxID_ANY, wxDefaultPosition,
                   wxDefaultSize, references);
  m_bearingReference->SetSelection(0);
  bearingGrid->Add(m_bearingReference, 1, wxEXPAND);
  bearingGrid->AddSpacer(1);
  m_observedBearing =
      AddField(bearingGrid, bearingBox->GetStaticBox(), _("Observed bearing"),
               wxEmptyString, _("degrees"));
  m_variation =
      AddField(bearingGrid, bearingBox->GetStaticBox(),
               _("Magnetic variation (E + / W -)"), _("0.0"), _("degrees"));
  m_deviation =
      AddField(bearingGrid, bearingBox->GetStaticBox(),
               _("Compass deviation (E + / W -)"), _("0.0"), _("degrees"));
  bearingBox->Add(bearingGrid, 0, wxALL | wxEXPAND, 5);
  auto* variationRow = new wxBoxSizer(wxHORIZONTAL);
  m_useWmmVariation = new wxButton(bearingBox->GetStaticBox(), wxID_ANY,
                                   _("Use WMM at current boat/time"));
  m_variationSource = new wxStaticText(
      bearingBox->GetStaticBox(), wxID_ANY,
      _("Variation is ignored when the bearing reference is True."));
  m_variationSource->Wrap(520);
  variationRow->Add(m_useWmmVariation, 0, wxRIGHT, 8);
  variationRow->Add(m_variationSource, 1, wxALIGN_CENTER_VERTICAL);
  bearingBox->Add(variationRow, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);
  verticalRoot->Add(bearingBox, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  wxButton* calculateVertical =
      new wxButton(vertical, wxID_ANY, _("Calculate and plot range"));
  verticalRoot->Add(calculateVertical, 0, wxALL, 10);
  m_verticalResult = new wxStaticText(vertical, wxID_ANY, _("No result yet."));
  m_verticalResult->Wrap(690);
  verticalRoot->Add(m_verticalResult, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
                    10);
  vertical->SetSizer(verticalRoot);
  calculateVertical->Bind(wxEVT_BUTTON,
                          &CoastalNavigationDialog::CalculateVertical, this);
  notebook->AddPage(vertical, _("Vertical angle / distance"), true);

  wxScrolledWindow* horizontal = new wxScrolledWindow(notebook, wxID_ANY);
  horizontal->SetScrollRate(0, 12);
  wxBoxSizer* horizontalRoot = new wxBoxSizer(wxVERTICAL);
  wxStaticText* hsaHelp = new wxStaticText(
      horizontal, wxID_ANY,
      _("Enter three charted objects in left-centre-right visual order and "
        "the two included angles. The approximate position chooses the "
        "correct solution when the geometry has an ambiguity."));
  hsaHelp->Wrap(690);
  horizontalRoot->Add(hsaHelp, 0, wxALL | wxEXPAND, 10);
  wxFlexGridSizer* horizontalGrid = new wxFlexGridSizer(0, 3, 6, 8);
  horizontalGrid->AddGrowableCol(1, 1);
  m_leftLat = AddField(horizontalGrid, horizontal, _("Left object latitude"),
                       unknownLat, _("angle"));
  m_leftLon = AddField(horizontalGrid, horizontal, _("Left object longitude"),
                       unknownLon, _("angle"));
  m_centreLat = AddField(horizontalGrid, horizontal,
                         _("Centre object latitude"), unknownLat, _("angle"));
  m_centreLon = AddField(horizontalGrid, horizontal,
                         _("Centre object longitude"), unknownLon, _("angle"));
  m_rightLat = AddField(horizontalGrid, horizontal, _("Right object latitude"),
                        unknownLat, _("angle"));
  m_rightLon = AddField(horizontalGrid, horizontal, _("Right object longitude"),
                        unknownLon, _("angle"));
  m_firstAngle =
      AddField(horizontalGrid, horizontal, _("Left-centre measurement"),
               wxEmptyString, _("degrees"));
  m_secondAngle =
      AddField(horizontalGrid, horizontal, _("Centre-right measurement"),
               wxEmptyString, _("degrees"));
  m_horizontalIndexError =
      AddField(horizontalGrid, horizontal, _("Index error (on the arc +)"),
               wxString::Format("%.2f", defaults.indexError), _("arcmin"));
  m_angleUncertainty =
      AddField(horizontalGrid, horizontal, _("Angle uncertainty (1-sigma)"),
               _("0.2"), _("arcmin"));
  m_initialLat = AddField(horizontalGrid, horizontal,
                          _("Approximate vessel latitude"), lat, _("angle"));
  m_initialLon = AddField(horizontalGrid, horizontal,
                          _("Approximate vessel longitude"), lon, _("angle"));
  horizontalRoot->Add(horizontalGrid, 0, wxALL | wxEXPAND, 10);

  wxStaticBoxSizer* sequence = new wxStaticBoxSizer(
      wxVERTICAL, horizontal, _("Sequential angle readings"));
  m_advanceHorizontalObserver =
      new wxCheckBox(sequence->GetStaticBox(), wxID_ANY,
                     _("Advance the vessel between the two HSA readings"));
  sequence->Add(m_advanceHorizontalObserver, 0, wxALL, 5);
  wxFlexGridSizer* sequenceGrid = new wxFlexGridSizer(0, 3, 6, 8);
  sequenceGrid->AddGrowableCol(1, 1);
  m_horizontalInterval =
      AddField(sequenceGrid, sequence->GetStaticBox(),
               _("Second-angle interval from first"), _("0"), _("seconds"));
  m_horizontalCourse = AddField(sequenceGrid, sequence->GetStaticBox(),
                                _("COG true"), _("0.0"), _("degrees"));
  m_horizontalSpeed = AddField(sequenceGrid, sequence->GetStaticBox(), _("SOG"),
                               _("0.0"), _("kn"));
  sequence->Add(sequenceGrid, 0, wxALL | wxEXPAND, 5);
  horizontalRoot->Add(sequence, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  auto updateSequence = [this]() {
    const bool enabled = m_advanceHorizontalObserver->GetValue();
    m_horizontalInterval->Enable(enabled);
    m_horizontalCourse->Enable(enabled);
    m_horizontalSpeed->Enable(enabled);
  };
  m_advanceHorizontalObserver->Bind(
      wxEVT_CHECKBOX, [updateSequence](wxCommandEvent&) { updateSequence(); });
  updateSequence();
  wxBoxSizer* horizontalButtons = new wxBoxSizer(wxHORIZONTAL);
  wxButton* boat = new wxButton(horizontal, wxID_ANY, _("Use boat position"));
  wxButton* calculateHorizontal =
      new wxButton(horizontal, wxID_ANY, _("Solve and plot HSA fix"));
  horizontalButtons->Add(boat, 0, wxRIGHT, 8);
  horizontalButtons->Add(calculateHorizontal, 0);
  horizontalRoot->Add(horizontalButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  m_horizontalResult =
      new wxStaticText(horizontal, wxID_ANY, _("No result yet."));
  m_horizontalResult->Wrap(690);
  horizontalRoot->Add(m_horizontalResult, 0,
                      wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
  horizontal->SetSizer(horizontalRoot);
  boat->Bind(wxEVT_BUTTON, &CoastalNavigationDialog::UseBoatPosition, this);
  calculateHorizontal->Bind(
      wxEVT_BUTTON, &CoastalNavigationDialog::CalculateHorizontal, this);
  notebook->AddPage(horizontal, _("Horizontal angles / fix"), false);
  root->Add(notebook, 1, wxLEFT | wxRIGHT | wxEXPAND, 10);

  wxBoxSizer* bottom = new wxBoxSizer(wxHORIZONTAL);
  wxButton* newObservation =
      new wxButton(this, wxID_ANY, _("New / clear observation"));
  wxButton* clear = new wxButton(this, wxID_ANY, _("Clear chart plots"));
  wxButton* close = new wxButton(this, wxID_CLOSE, _("Close"));
  bottom->Add(newObservation, 0, wxRIGHT, 8);
  bottom->Add(clear, 0, wxRIGHT, 8);
  bottom->Add(close, 0);
  root->Add(bottom, 0, wxALL | wxALIGN_RIGHT, 10);
  auto* retention = new wxStaticText(
      this, wxID_ANY,
      _("Entries are retained when this window is closed. Use New / clear "
        "observation before starting a different observation."));
  retention->Wrap(690);
  root->Insert(root->GetItemCount() - 1, retention, 0,
               wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 10);
  newObservation->Bind(wxEVT_BUTTON, &CoastalNavigationDialog::NewObservation,
                       this);
  clear->Bind(wxEVT_BUTTON, &CoastalNavigationDialog::ClearPlots, this);
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Hide(); });
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { Hide(); });
  m_includeBearing->Bind(wxEVT_CHECKBOX,
                         [this](wxCommandEvent&) { UpdateBearingControls(); });
  m_bearingReference->Bind(
      wxEVT_CHOICE, [this](wxCommandEvent&) { UpdateBearingControls(); });
  m_useWmmVariation->Bind(wxEVT_BUTTON,
                          &CoastalNavigationDialog::UseWmmVariation, this);
  UpdateBearingControls();

  SetSizer(root);
  SetMinSize(wxSize(720, 500));
  CentreOnParent();
}

wxTextCtrl* CoastalNavigationDialog::AddField(wxSizer* sizer, wxWindow* parent,
                                              const wxString& label,
                                              const wxString& value,
                                              const wxString& units) {
  sizer->Add(new wxStaticText(parent, wxID_ANY, label), 0,
             wxALIGN_CENTER_VERTICAL);
  wxTextCtrl* control = new wxTextCtrl(parent, wxID_ANY, value);
  sizer->Add(control, 1, wxEXPAND);
  sizer->Add(new wxStaticText(parent, wxID_ANY, units), 0,
             wxALIGN_CENTER_VERTICAL);
  return control;
}

bool CoastalNavigationDialog::ReadDouble(wxTextCtrl* control,
                                         const wxString& label, double* value) {
  if (control->GetValue().ToDouble(value) && std::isfinite(*value)) return true;
  wxMessageBox(label + _(" is not a valid number."), _("Coastal navigation"),
               wxOK | wxICON_ERROR, this);
  return false;
}

bool CoastalNavigationDialog::ReadAngle(wxTextCtrl* control,
                                        const wxString& label, double minimum,
                                        double maximum, double* value) {
  if (ParseNavigationAngle(control->GetValue(), NavigationAngleKind::Generic,
                           minimum, maximum, value)) {
    control->ChangeValue(FormatNavigationAngle(*value));
    return true;
  }
  wxMessageBox(
      label + _(" is not a valid angle. Decimal degrees, degrees and minutes, "
                "and degrees/minutes/seconds are accepted."),
      _("Coastal navigation"), wxOK | wxICON_ERROR, this);
  return false;
}

cn::GeoPoint CoastalNavigationDialog::ReadPoint(wxTextCtrl* latitude,
                                                wxTextCtrl* longitude,
                                                const wxString& label,
                                                bool* ok) {
  cn::GeoPoint point;
  *ok =
      ParseNavigationAngle(latitude->GetValue(), NavigationAngleKind::Latitude,
                           -90.0, 90.0, &point.latitude_deg) &&
      ParseNavigationAngle(longitude->GetValue(),
                           NavigationAngleKind::Longitude, -180.0, 180.0,
                           &point.longitude_deg);
  if (!*ok) {
    wxMessageBox(
        label + _(" must contain a valid latitude and longitude. Decimal "
                  "degrees, degrees and minutes, and degrees/minutes/seconds "
                  "are accepted."),
        _("Coastal navigation"), wxOK | wxICON_ERROR, this);
    return point;
  }
  if (*ok && (point.latitude_deg <= -90.0 || point.latitude_deg >= 90.0 ||
              point.longitude_deg < -180.0 || point.longitude_deg > 180.0)) {
    wxMessageBox(label + _(" is outside the valid latitude/longitude range."),
                 _("Coastal navigation"), wxOK | wxICON_ERROR, this);
    *ok = false;
  }
  if (*ok) {
    latitude->ChangeValue(FormatNavigationAngle(
        point.latitude_deg, NavigationAngleKind::Latitude, true));
    longitude->ChangeValue(FormatNavigationAngle(
        point.longitude_deg, NavigationAngleKind::Longitude, true));
  }
  return point;
}

void CoastalNavigationDialog::CalculateVertical(wxCommandEvent&) {
  bool ok = false;
  const cn::GeoPoint target = ReadPoint(
      m_verticalTargetLat, m_verticalTargetLon, _("Target position"), &ok);
  if (!ok) return;
  cn::VerticalAngleObservation observation;
  observation.mode = m_verticalMode->GetSelection() == 0
                         ? cn::VerticalAngleMode::WaterlineToTop
                         : cn::VerticalAngleMode::SeaHorizonToTopBeyondHorizon;
  if (!ReadAngle(m_verticalAngle, _("Observed vertical angle"), 0.0, 180.0,
                 &observation.angle_deg) ||
      !ReadDouble(m_verticalIndexError, _("Index error"),
                  &observation.index_error_arcmin) ||
      !ReadDouble(m_verticalHeight, _("Charted top height"),
                  &observation.charted_top_height_m) ||
      !ReadDouble(m_verticalWaterLevel, _("Water level"),
                  &observation.water_level_above_height_datum_m) ||
      !ReadDouble(m_verticalEyeHeight, _("Height of eye"),
                  &observation.eye_height_m))
    return;
  const cn::RangeResult result = cn::SolveVerticalAngle(observation);
  if (!result.valid) {
    m_verticalResult->SetLabel(_("No range: ") +
                               wxString::FromUTF8(result.error.c_str()));
    m_rangeCircle.clear();
    m_hasVerticalPosition = false;
    RefreshChart();
    return;
  }
  wxString text = wxString::Format(
      _("Range %.3f NM; corrected angle %s; effective target height %.2f m."),
      result.range_nm,
      FormatNavigationAngle(result.corrected_angle_deg).c_str(),
      result.effective_height_m);
  for (const std::string& warning : result.warnings)
    text += _("\nWarning: ") + wxString::FromUTF8(warning.c_str());
  m_rangeCircle.clear();
  for (int bearing = 0; bearing <= 360; bearing += 2)
    m_rangeCircle.push_back(cn::Destination(target, bearing, result.range_nm));
  m_hasVerticalPosition = false;
  if (m_includeBearing->GetValue()) {
    double bearing = 0.0, variation = 0.0, deviation = 0.0;
    if (!ReadDouble(m_observedBearing, _("Observed bearing"), &bearing) ||
        !ReadDouble(m_variation, _("Magnetic variation"), &variation) ||
        !ReadDouble(m_deviation, _("Compass deviation"), &deviation))
      return;
    if (m_bearingReference->GetSelection() == 1)
      bearing += variation + deviation;
    m_verticalPosition =
        cn::Destination(target, bearing + 180.0, result.range_nm);
    m_hasVerticalPosition = true;
    text += wxString::Format(
        _("\nBearing/range estimate: %s, %s (true bearing %.2f%c)."),
        FormatNavigationAngle(m_verticalPosition.latitude_deg,
                              NavigationAngleKind::Latitude, true)
            .c_str(),
        FormatNavigationAngle(m_verticalPosition.longitude_deg,
                              NavigationAngleKind::Longitude, true)
            .c_str(),
        std::fmod(bearing + 360.0, 360.0), 0x00B0);
  } else {
    text +=
        _("\nThis is a range circle, not a fix. Add a bearing or another "
          "independent observation.");
  }
  m_verticalResult->SetLabel(text);
  RefreshChart();
}

void CoastalNavigationDialog::CalculateHorizontal(wxCommandEvent&) {
  bool ok = false;
  cn::HorizontalAngleObservation observation;
  observation.left = ReadPoint(m_leftLat, m_leftLon, _("Left object"), &ok);
  if (!ok) return;
  observation.centre =
      ReadPoint(m_centreLat, m_centreLon, _("Centre object"), &ok);
  if (!ok) return;
  observation.right = ReadPoint(m_rightLat, m_rightLon, _("Right object"), &ok);
  if (!ok) return;
  const cn::GeoPoint initial =
      ReadPoint(m_initialLat, m_initialLon, _("Approximate position"), &ok);
  if (!ok) return;
  if (!ReadAngle(m_firstAngle, _("Left-centre angle"), 0.0, 180.0,
                 &observation.left_centre_angle_deg) ||
      !ReadAngle(m_secondAngle, _("Centre-right angle"), 0.0, 180.0,
                 &observation.centre_right_angle_deg) ||
      !ReadDouble(m_horizontalIndexError, _("Index error"),
                  &observation.index_error_arcmin) ||
      !ReadDouble(m_angleUncertainty, _("Angle uncertainty"),
                  &observation.angle_uncertainty_arcmin))
    return;
  observation.moving_observer = m_advanceHorizontalObserver->GetValue();
  if (observation.moving_observer &&
      (!ReadDouble(m_horizontalInterval, _("Second-angle interval"),
                   &observation.second_time_offset_seconds) ||
       !ReadDouble(m_horizontalCourse, _("COG true"),
                   &observation.course_true_deg) ||
       !ReadDouble(m_horizontalSpeed, _("SOG"), &observation.speed_knots)))
    return;
  const cn::HorizontalFixResult result =
      cn::SolveHorizontalThreePointFix(observation, initial);
  m_hsaLoci =
      cn::BuildHorizontalAngleLocus(observation.left, observation.centre,
                                    observation.left_centre_angle_deg -
                                        observation.index_error_arcmin / 60.0);
  const auto second =
      cn::BuildHorizontalAngleLocus(observation.centre, observation.right,
                                    observation.centre_right_angle_deg -
                                        observation.index_error_arcmin / 60.0);
  if (observation.moving_observer && observation.speed_knots != 0.0) {
    const double signed_distance = observation.speed_knots *
                                   observation.second_time_offset_seconds /
                                   3600.0;
    const double reverse_course =
        observation.course_true_deg + (signed_distance >= 0.0 ? 180.0 : 0.0);
    for (const auto& branch : second) {
      std::vector<cn::GeoPoint> propagated;
      for (const cn::GeoPoint& point : branch)
        propagated.push_back(
            cn::Destination(point, reverse_course, std::fabs(signed_distance)));
      m_hsaLoci.push_back(std::move(propagated));
    }
  } else {
    m_hsaLoci.insert(m_hsaLoci.end(), second.begin(), second.end());
  }
  if (!result.valid) {
    m_hasHorizontalFix = false;
    m_horizontalResult->SetLabel(
        _("No fix: ") + wxString::FromUTF8(result.error.c_str()) +
        _("\nThe individual HSA loci are plotted where their coastal-scale "
          "chart construction is valid."));
  } else {
    m_hasHorizontalFix = true;
    m_horizontalFix = result.position;
    wxString resultText = wxString::Format(
        _("Fix %s, %s; residuals %+.4f' / %+.4f'; estimated 1-sigma geometry "
          "uncertainty %.3f NM; condition %.1f."),
        FormatNavigationAngle(result.position.latitude_deg,
                              NavigationAngleKind::Latitude, true)
            .c_str(),
        FormatNavigationAngle(result.position.longitude_deg,
                              NavigationAngleKind::Longitude, true)
            .c_str(),
        result.first_residual_arcmin, result.second_residual_arcmin,
        result.estimated_uncertainty_nm, result.geometry_condition);
    if (observation.moving_observer)
      resultText +=
          _(" The fix and both plotted loci are reduced to the "
            "first-angle reference epoch.");
    m_horizontalResult->SetLabel(resultText);
  }
  RefreshChart();
}

void CoastalNavigationDialog::UseBoatPosition(wxCommandEvent&) {
  double latitude = 0.0, longitude = 0.0;
  celestial_navigation_pi_BoatPos(latitude, longitude);
  m_initialLat->SetValue(
      FormatNavigationAngle(latitude, NavigationAngleKind::Latitude, true));
  m_initialLon->SetValue(
      FormatNavigationAngle(longitude, NavigationAngleKind::Longitude, true));
}

void CoastalNavigationDialog::UseWmmVariation(wxCommandEvent&) {
  double latitude = 0.0, longitude = 0.0;
  celestial_navigation_pi_BoatPos(latitude, longitude);
  const wxDateTime utc = UtcDateTime::Now();
  const double variation =
      celestial_navigation_pi_GetWMM(latitude, longitude, 0.0, utc);
  m_variation->ChangeValue(wxString::Format("%.2f", variation));
  m_variationSource->SetLabel(wxString::Format(
      _("WMM estimate for current boat position at %s UTC; editable."),
      UtcDateTime::FormatUtc(utc, "%Y-%m-%d %H:%M")));
  m_variationSource->Wrap(520);
  Layout();
}

void CoastalNavigationDialog::UpdateBearingControls() {
  const bool included = m_includeBearing->GetValue();
  const bool magnetic = included && m_bearingReference->GetSelection() == 1;
  m_bearingReference->Enable(included);
  m_observedBearing->Enable(included);
  m_variation->Enable(magnetic);
  m_deviation->Enable(magnetic);
  m_useWmmVariation->Enable(magnetic);
  if (!magnetic) {
    m_variationSource->SetLabel(
        _("Variation and deviation are ignored for True bearings."));
  } else if (!m_variationSource->GetLabel().StartsWith(_("WMM estimate"))) {
    m_variationSource->SetLabel(
        _("Enter variation manually or use the current-position WMM estimate; "
          "compass deviation remains manual."));
  }
  m_variationSource->Wrap(520);
  Layout();
}

void CoastalNavigationDialog::NewObservation(wxCommandEvent&) {
  if (wxMessageBox(
          _("Clear the retained coastal-observation entries and chart plots?"),
          _("New coastal observation"),
          wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
    return;
  const CelestialNavigationDefaults defaults =
      LoadCelestialNavigationDefaults();
  double boatLatitude = 0.0, boatLongitude = 0.0;
  celestial_navigation_pi_BoatPos(boatLatitude, boatLongitude);
  m_verticalMode->SetSelection(0);
  m_verticalTargetLat->Clear();
  m_verticalTargetLon->Clear();
  m_verticalAngle->Clear();
  m_verticalIndexError->ChangeValue(
      wxString::Format("%.2f", defaults.indexError));
  m_verticalHeight->ChangeValue(_("30.0"));
  m_verticalWaterLevel->ChangeValue(_("0.0"));
  m_verticalEyeHeight->ChangeValue(
      wxString::Format("%.1f", defaults.eyeHeight));
  m_includeBearing->SetValue(false);
  m_bearingReference->SetSelection(0);
  m_observedBearing->Clear();
  m_variation->ChangeValue(_("0.0"));
  m_deviation->ChangeValue(_("0.0"));
  m_leftLat->Clear();
  m_leftLon->Clear();
  m_centreLat->Clear();
  m_centreLon->Clear();
  m_rightLat->Clear();
  m_rightLon->Clear();
  m_firstAngle->Clear();
  m_secondAngle->Clear();
  m_horizontalIndexError->ChangeValue(
      wxString::Format("%.2f", defaults.indexError));
  m_angleUncertainty->ChangeValue(_("0.2"));
  m_advanceHorizontalObserver->SetValue(false);
  m_horizontalInterval->ChangeValue(_("0"));
  m_horizontalCourse->ChangeValue(_("0.0"));
  m_horizontalSpeed->ChangeValue(_("0.0"));
  m_horizontalInterval->Enable(false);
  m_horizontalCourse->Enable(false);
  m_horizontalSpeed->Enable(false);
  m_initialLat->ChangeValue(
      FormatNavigationAngle(boatLatitude, NavigationAngleKind::Latitude, true));
  m_initialLon->ChangeValue(FormatNavigationAngle(
      boatLongitude, NavigationAngleKind::Longitude, true));
  m_verticalResult->SetLabel(_("No result yet."));
  m_horizontalResult->SetLabel(_("No result yet."));
  m_variationSource->SetLabel(
      _("Variation and deviation are ignored for True bearings."));
  m_variationSource->Wrap(520);
  wxCommandEvent unused;
  ClearPlots(unused);
  UpdateBearingControls();
}

void CoastalNavigationDialog::ClearPlots(wxCommandEvent&) {
  m_rangeCircle.clear();
  m_hsaLoci.clear();
  m_hasVerticalPosition = false;
  m_hasHorizontalFix = false;
  RefreshChart();
}

void CoastalNavigationDialog::RefreshChart() {
  RequestRefresh(m_parent->GetParent());
}

bool CoastalNavigationDialog::Render(piDC* dc, PlugIn_ViewPort* viewport) {
  if (!dc || !viewport) return false;
  bool rendered = false;
  auto drawLine = [&](const std::vector<cn::GeoPoint>& points,
                      const wxColour& colour, int width) {
    if (points.size() < 2) return;
    dc->SetPen(wxPen(colour, width));
    wxPoint previous;
    GetCanvasPixLL(viewport, &previous, points.front().latitude_deg,
                   points.front().longitude_deg);
    for (std::size_t index = 1; index < points.size(); ++index) {
      wxPoint current;
      GetCanvasPixLL(viewport, &current, points[index].latitude_deg,
                     points[index].longitude_deg);
      dc->DrawLine(previous.x, previous.y, current.x, current.y);
      previous = current;
    }
    rendered = true;
  };
  drawLine(m_rangeCircle, wxColour(0, 180, 255), 2);
  for (std::size_t index = 0; index < m_hsaLoci.size(); ++index)
    drawLine(m_hsaLoci[index],
             index % 2 ? wxColour(255, 80, 210) : wxColour(255, 180, 0), 2);
  auto marker = [&](const cn::GeoPoint& point, const wxColour& colour) {
    wxPoint pixel;
    GetCanvasPixLL(viewport, &pixel, point.latitude_deg, point.longitude_deg);
    dc->SetPen(wxPen(colour, 2));
    dc->SetBrush(*wxTRANSPARENT_BRUSH);
    dc->DrawCircle(pixel.x, pixel.y, 8);
    dc->DrawLine(pixel.x - 11, pixel.y, pixel.x + 11, pixel.y);
    dc->DrawLine(pixel.x, pixel.y - 11, pixel.x, pixel.y + 11);
    rendered = true;
  };
  if (m_hasVerticalPosition) marker(m_verticalPosition, wxColour(0, 180, 255));
  if (m_hasHorizontalFix) marker(m_horizontalFix, wxColour(255, 60, 60));
  return rendered;
}
