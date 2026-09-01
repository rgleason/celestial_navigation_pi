#ifndef CELESTIAL_NAVIGATION_COASTAL_NAVIGATION_DIALOG_H
#define CELESTIAL_NAVIGATION_COASTAL_NAVIGATION_DIALOG_H

#include "CoastalNavigationEngine.h"

#include <wx/dialog.h>

#include <vector>

class CelestialNavigationDialog;
class piDC;
class PlugIn_ViewPort;
class wxCheckBox;
class wxChoice;
class wxStaticText;
class wxTextCtrl;

class CoastalNavigationDialog : public wxDialog {
public:
  explicit CoastalNavigationDialog(CelestialNavigationDialog* parent);
  ~CoastalNavigationDialog() override = default;

  bool Render(piDC* dc, PlugIn_ViewPort* viewport);

private:
  wxTextCtrl* AddField(wxSizer* sizer, wxWindow* parent, const wxString& label,
                       const wxString& value, const wxString& units = wxEmptyString);
  bool ReadDouble(wxTextCtrl* control, const wxString& label, double* value);
  bool ReadAngle(wxTextCtrl* control, const wxString& label, double minimum,
                 double maximum, double* value);
  coastal_navigation::GeoPoint ReadPoint(wxTextCtrl* latitude,
                                         wxTextCtrl* longitude,
                                         const wxString& label, bool* ok);
  void CalculateVertical(wxCommandEvent& event);
  void CalculateHorizontal(wxCommandEvent& event);
  void UseBoatPosition(wxCommandEvent& event);
  void ClearPlots(wxCommandEvent& event);
  void RefreshChart();

  CelestialNavigationDialog* m_parent;
  wxChoice* m_verticalMode;
  wxTextCtrl* m_verticalTargetLat;
  wxTextCtrl* m_verticalTargetLon;
  wxTextCtrl* m_verticalAngle;
  wxTextCtrl* m_verticalIndexError;
  wxTextCtrl* m_verticalHeight;
  wxTextCtrl* m_verticalWaterLevel;
  wxTextCtrl* m_verticalEyeHeight;
  wxCheckBox* m_includeBearing;
  wxChoice* m_bearingReference;
  wxTextCtrl* m_observedBearing;
  wxTextCtrl* m_variation;
  wxTextCtrl* m_deviation;
  wxStaticText* m_verticalResult;

  wxTextCtrl* m_leftLat;
  wxTextCtrl* m_leftLon;
  wxTextCtrl* m_centreLat;
  wxTextCtrl* m_centreLon;
  wxTextCtrl* m_rightLat;
  wxTextCtrl* m_rightLon;
  wxTextCtrl* m_firstAngle;
  wxTextCtrl* m_secondAngle;
  wxTextCtrl* m_angleUncertainty;
  wxCheckBox* m_advanceHorizontalObserver;
  wxTextCtrl* m_horizontalInterval;
  wxTextCtrl* m_horizontalCourse;
  wxTextCtrl* m_horizontalSpeed;
  wxTextCtrl* m_initialLat;
  wxTextCtrl* m_initialLon;
  wxStaticText* m_horizontalResult;

  std::vector<coastal_navigation::GeoPoint> m_rangeCircle;
  std::vector<std::vector<coastal_navigation::GeoPoint>> m_hsaLoci;
  bool m_hasVerticalPosition;
  coastal_navigation::GeoPoint m_verticalPosition;
  bool m_hasHorizontalFix;
  coastal_navigation::GeoPoint m_horizontalFix;
};

#endif
