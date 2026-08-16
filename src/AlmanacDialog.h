#ifndef CELESTIAL_NAVIGATION_ALMANAC_DIALOG_H
#define CELESTIAL_NAVIGATION_ALMANAC_DIALOG_H

#include "AlmanacGenerator.h"

#include <wx/dialog.h>

class CelestialNavigationDialog;
class wxCheckBox;
class wxChoice;
class wxDatePickerCtrl;
class wxDateEvent;
class wxFilePickerCtrl;
class wxNotebook;
class wxSpinCtrl;
class wxSpinCtrlDouble;
class wxStaticText;
class wxTextCtrl;

class AlmanacDialog : public wxDialog {
public:
  explicit AlmanacDialog(CelestialNavigationDialog* parent,
                         const wxString& preferredRouteGuid = wxString());
  void SelectIntegrationPage(int page);

private:
  void BuildInterface();
  void LoadRoutes();
  void UpdateCoverageControls();
  void ApplyPresetSelection();
  AlmanacRequest ReadRequest(wxString* error, bool includeRoute = true) const;
  void UpdateSummary();
  void OnChanged(wxCommandEvent& event);
  void OnDateChanged(wxDateEvent& event);
  void OnPreset(wxCommandEvent& event);
  void OnCoverage(wxCommandEvent& event);
  void OnRoute(wxCommandEvent& event);
  void OnPreview(wxCommandEvent& event);
  void OnGenerate(wxCommandEvent& event);

  CelestialNavigationDialog* m_parent;
  wxChoice* m_preset;
  wxNotebook* m_notebook;
  wxDatePickerCtrl* m_from;
  wxDatePickerCtrl* m_to;
  wxChoice* m_coverage;
  wxChoice* m_route;
  wxTextCtrl* m_voyageName;
  wxSpinCtrlDouble* m_latitude;
  wxSpinCtrlDouble* m_longitude;
  wxSpinCtrlDouble* m_latSouth;
  wxSpinCtrlDouble* m_latNorth;
  wxSpinCtrlDouble* m_corridor;
  wxSpinCtrlDouble* m_speed;
  wxSpinCtrlDouble* m_dut1;
  wxCheckBox* m_dut1Known;
  wxChoice* m_safety;
  wxCheckBox* m_selfContained;
  wxCheckBox* m_sun;
  wxCheckBox* m_moon;
  wxCheckBox* m_aries;
  wxCheckBox* m_planets;
  wxCheckBox* m_stars;
  wxCheckBox* m_usefulPlanets;
  wxCheckBox* m_events;
  wxCheckBox* m_moonInfo;
  wxCheckBox* m_recommendations;
  wxCheckBox* m_charts;
  wxCheckBox* m_corrections;
  wxCheckBox* m_instructions;
  wxCheckBox* m_lunar;
  wxCheckBox* m_emergency;
  wxCheckBox* m_incrementTables;
  wxCheckBox* m_reductionTables;
  wxCheckBox* m_directTables;
  wxCheckBox* m_fullDirectTables;
  wxCheckBox* m_altitudeTables;
  wxCheckBox* m_visualAids;
  wxChoice* m_planningInterval;
  wxSpinCtrl* m_sightForms;
  wxSpinCtrl* m_runningForms;
  wxSpinCtrl* m_noonForms;
  wxSpinCtrl* m_lunarForms;
  wxSpinCtrl* m_watchForms;
  wxChoice* m_paper;
  wxCheckBox* m_landscape;
  wxCheckBox* m_duplex;
  wxCheckBox* m_monochrome;
  wxCheckBox* m_compact;
  wxCheckBox* m_booklet;
  wxSpinCtrl* m_signaturePages;
  wxFilePickerCtrl* m_output;
  wxStaticText* m_summary;
  wxStaticText* m_warning;
  std::vector<wxString> m_routeGuids;
  wxString m_preferredRouteGuid;
  bool m_applyingPreset;
};

#endif
