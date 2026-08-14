#include "CoastalNavigationDialog.h"

#include "CelestialNavigationDialog.h"
#include "OcpnApiCompat.h"
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
  double boatLat = 0.0, boatLon = 0.0;
  celestial_navigation_pi_BoatPos(boatLat, boatLon);
  const wxString lat = wxString::Format("%.6f", boatLat);
  const wxString lon = wxString::Format("%.6f", boatLon);

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
  m_verticalTargetLat = AddField(verticalGrid, vertical, _("Target latitude"), lat,
                                 _("decimal degrees"));
  m_verticalTargetLon = AddField(verticalGrid, vertical, _("Target longitude"), lon,
                                 _("decimal degrees"));
  m_verticalAngle = AddField(verticalGrid, vertical, _("Observed vertical angle"),
                             _("0° 30.000'"), _("angle or decimal degrees"));
  m_verticalIndexError = AddField(verticalGrid, vertical, _("Index error (on the arc +)"),
                                  _("0.0"), _("arcmin"));
  m_verticalHeight = AddField(verticalGrid, vertical, _("Charted top height"),
                              _("30.0"), _("m above height datum"));
  m_verticalWaterLevel = AddField(verticalGrid, vertical,
                                  _("Water level above height datum"), _("0.0"),
                                  _("m (tide included)"));
  m_verticalEyeHeight = AddField(verticalGrid, vertical, _("Height of eye"),
                                 _("2.0"), _("m"));
  verticalRoot->Add(verticalGrid, 0, wxALL | wxEXPAND, 10);

  wxStaticBoxSizer* bearingBox =
      new wxStaticBoxSizer(wxVERTICAL, vertical, _("Optional bearing fix"));
  m_includeBearing = new wxCheckBox(
      bearingBox->GetStaticBox(), wxID_ANY,
      _("Include observed bearing from vessel to target"));
  bearingBox->Add(m_includeBearing, 0, wxALL, 5);
  wxFlexGridSizer* bearingGrid = new wxFlexGridSizer(0, 3, 6, 8);
  bearingGrid->AddGrowableCol(1, 1);
  bearingGrid->Add(new wxStaticText(bearingBox->GetStaticBox(), wxID_ANY,
                                    _("Bearing reference")),
                   0, wxALIGN_CENTER_VERTICAL);
  wxArrayString references;
  references.Add(_("True"));
  references.Add(_("Magnetic compass"));
  m_bearingReference = new wxChoice(bearingBox->GetStaticBox(), wxID_ANY,
                                    wxDefaultPosition, wxDefaultSize, references);
  m_bearingReference->SetSelection(0);
  bearingGrid->Add(m_bearingReference, 1, wxEXPAND);
  bearingGrid->AddSpacer(1);
  m_observedBearing = AddField(bearingGrid, bearingBox->GetStaticBox(),
                               _("Observed bearing"), _("0.0"), _("degrees"));
  m_variation = AddField(bearingGrid, bearingBox->GetStaticBox(),
                         _("Magnetic variation (E + / W -)"), _("0.0"),
                         _("degrees"));
  m_deviation = AddField(bearingGrid, bearingBox->GetStaticBox(),
                         _("Compass deviation (E + / W -)"), _("0.0"),
                         _("degrees"));
  bearingBox->Add(bearingGrid, 0, wxALL | wxEXPAND, 5);
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
  m_leftLat = AddField(horizontalGrid, horizontal, _("Left object latitude"), lat,
                       _("decimal degrees"));
  m_leftLon = AddField(horizontalGrid, horizontal, _("Left object longitude"), lon,
                       _("decimal degrees"));
  m_centreLat = AddField(horizontalGrid, horizontal, _("Centre object latitude"), lat,
                         _("decimal degrees"));
  m_centreLon = AddField(horizontalGrid, horizontal, _("Centre object longitude"), lon,
                         _("decimal degrees"));
  m_rightLat = AddField(horizontalGrid, horizontal, _("Right object latitude"), lat,
                        _("decimal degrees"));
  m_rightLon = AddField(horizontalGrid, horizontal, _("Right object longitude"), lon,
                        _("decimal degrees"));
  m_firstAngle = AddField(horizontalGrid, horizontal, _("Left-centre angle"),
                          _("30.0"), _("degrees"));
  m_secondAngle = AddField(horizontalGrid, horizontal, _("Centre-right angle"),
                           _("30.0"), _("degrees"));
  m_angleUncertainty = AddField(horizontalGrid, horizontal,
                                _("Angle uncertainty (1-sigma)"), _("0.2"),
                                _("arcmin"));
  m_initialLat = AddField(horizontalGrid, horizontal,
                          _("Approximate vessel latitude"), lat,
                          _("decimal degrees"));
  m_initialLon = AddField(horizontalGrid, horizontal,
                          _("Approximate vessel longitude"), lon,
                          _("decimal degrees"));
  horizontalRoot->Add(horizontalGrid, 0, wxALL | wxEXPAND, 10);
  wxBoxSizer* horizontalButtons = new wxBoxSizer(wxHORIZONTAL);
  wxButton* boat = new wxButton(horizontal, wxID_ANY, _("Use boat position"));
  wxButton* calculateHorizontal =
      new wxButton(horizontal, wxID_ANY, _("Solve and plot HSA fix"));
  horizontalButtons->Add(boat, 0, wxRIGHT, 8);
  horizontalButtons->Add(calculateHorizontal, 0);
  horizontalRoot->Add(horizontalButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
  m_horizontalResult = new wxStaticText(horizontal, wxID_ANY, _("No result yet."));
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
  wxButton* clear = new wxButton(this, wxID_ANY, _("Clear chart plots"));
  wxButton* close = new wxButton(this, wxID_CLOSE, _("Close"));
  bottom->Add(clear, 0, wxRIGHT, 8);
  bottom->Add(close, 0);
  root->Add(bottom, 0, wxALL | wxALIGN_RIGHT, 10);
  clear->Bind(wxEVT_BUTTON, &CoastalNavigationDialog::ClearPlots, this);
  close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Hide(); });
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { Hide(); });

  SetSizer(root);
  SetMinSize(wxSize(720, 500));
  CentreOnParent();
}

wxTextCtrl* CoastalNavigationDialog::AddField(
    wxSizer* sizer, wxWindow* parent, const wxString& label,
    const wxString& value, const wxString& units) {
  sizer->Add(new wxStaticText(parent, wxID_ANY, label), 0,
             wxALIGN_CENTER_VERTICAL);
  wxTextCtrl* control = new wxTextCtrl(parent, wxID_ANY, value);
  sizer->Add(control, 1, wxEXPAND);
  sizer->Add(new wxStaticText(parent, wxID_ANY, units), 0,
             wxALIGN_CENTER_VERTICAL);
  return control;
}

bool CoastalNavigationDialog::ReadDouble(wxTextCtrl* control,
                                         const wxString& label,
                                         double* value) {
  if (control->GetValue().ToDouble(value) && std::isfinite(*value)) return true;
  wxMessageBox(label + _(" is not a valid number."), _("Coastal navigation"),
               wxOK | wxICON_ERROR, this);
  return false;
}

cn::GeoPoint CoastalNavigationDialog::ReadPoint(wxTextCtrl* latitude,
                                                 wxTextCtrl* longitude,
                                                 const wxString& label,
                                                 bool* ok) {
  cn::GeoPoint point;
  *ok = ReadDouble(latitude, label + _(" latitude"), &point.latitude_deg) &&
        ReadDouble(longitude, label + _(" longitude"), &point.longitude_deg);
  if (*ok && (point.latitude_deg <= -90.0 || point.latitude_deg >= 90.0 ||
              point.longitude_deg < -180.0 || point.longitude_deg > 180.0)) {
    wxMessageBox(label + _(" is outside the valid latitude/longitude range."),
                 _("Coastal navigation"), wxOK | wxICON_ERROR, this);
    *ok = false;
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
  observation.angle_deg = fromDMM_Plugin(m_verticalAngle->GetValue());
  if (!ReadDouble(m_verticalIndexError, _("Index error"),
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
      _("Range %.3f NM; corrected angle %.6f%c; effective target height %.2f m."),
      result.range_nm, result.corrected_angle_deg, 0x00B0,
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
    m_verticalPosition = cn::Destination(target, bearing + 180.0, result.range_nm);
    m_hasVerticalPosition = true;
    text += wxString::Format(
        _("\nBearing/range estimate: %.6f%c, %.6f%c (true bearing %.2f%c)."),
        m_verticalPosition.latitude_deg, 0x00B0,
        m_verticalPosition.longitude_deg, 0x00B0, std::fmod(bearing + 360.0, 360.0),
        0x00B0);
  } else {
    text += _("\nThis is a range circle, not a fix. Add a bearing or another independent observation.");
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
  if (!ReadDouble(m_firstAngle, _("Left-centre angle"),
                  &observation.left_centre_angle_deg) ||
      !ReadDouble(m_secondAngle, _("Centre-right angle"),
                  &observation.centre_right_angle_deg) ||
      !ReadDouble(m_angleUncertainty, _("Angle uncertainty"),
                  &observation.angle_uncertainty_arcmin))
    return;
  const cn::HorizontalFixResult result =
      cn::SolveHorizontalThreePointFix(observation, initial);
  m_hsaLoci = cn::BuildHorizontalAngleLocus(
      observation.left, observation.centre,
      observation.left_centre_angle_deg);
  const auto second = cn::BuildHorizontalAngleLocus(
      observation.centre, observation.right,
      observation.centre_right_angle_deg);
  m_hsaLoci.insert(m_hsaLoci.end(), second.begin(), second.end());
  if (!result.valid) {
    m_hasHorizontalFix = false;
    m_horizontalResult->SetLabel(
        _("No fix: ") + wxString::FromUTF8(result.error.c_str()) +
        _("\nThe individual HSA loci are plotted where their coastal-scale chart construction is valid."));
  } else {
    m_hasHorizontalFix = true;
    m_horizontalFix = result.position;
    m_horizontalResult->SetLabel(wxString::Format(
        _("Fix %.6f%c, %.6f%c; residuals %+.4f' / %+.4f'; estimated 1-sigma geometry uncertainty %.3f NM; condition %.1f."),
        result.position.latitude_deg, 0x00B0, result.position.longitude_deg,
        0x00B0, result.first_residual_arcmin, result.second_residual_arcmin,
        result.estimated_uncertainty_nm, result.geometry_condition));
  }
  RefreshChart();
}

void CoastalNavigationDialog::UseBoatPosition(wxCommandEvent&) {
  double latitude = 0.0, longitude = 0.0;
  celestial_navigation_pi_BoatPos(latitude, longitude);
  m_initialLat->SetValue(wxString::Format("%.6f", latitude));
  m_initialLon->SetValue(wxString::Format("%.6f", longitude));
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
    drawLine(m_hsaLoci[index], index % 2 ? wxColour(255, 80, 210)
                                        : wxColour(255, 180, 0),
             2);
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
