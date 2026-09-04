#ifndef CELESTIAL_NAVIGATION_PLANNER_DIALOG_H
#define CELESTIAL_NAVIGATION_PLANNER_DIALOG_H

#include "NavigationAlgorithms.h"
#include "WaypointPositionSource.h"

#include <wx/dialog.h>
#include <wx/timer.h>

class CelestialNavigationDialog;
class NavigationAngleCtrl;
class SkyPlotPanel;
class wxCheckBox;
class wxChoice;
class wxDatePickerCtrl;
class wxListCtrl;
class wxListEvent;
class wxNotebook;
class wxPanel;
class wxSpinCtrlDouble;
class wxStaticText;
class wxTextCtrl;
class wxTimePickerCtrl;

class PlannerDialog : public wxDialog {
public:
  explicit PlannerDialog(CelestialNavigationDialog* parent);
  ~PlannerDialog();
  void SelectPageForIntegration(unsigned page);
#ifdef CELESTIAL_PLANNER_INTEGRATION_TEST
  void ScheduleWaypointIntegration(const wxString& name);
#endif

private:
  ObserverMotion ReadMotion(bool showErrors);
  wxDateTime ReadUtc(bool showErrors);
  wxDateTime ReadEntryFields(int format, bool showErrors);
  void SetUtcControls(const wxDateTime& utc);
  void ChangeInputTimeBasis(wxCommandEvent& event);
  void ChangeEntryFormat(wxCommandEvent& event);
  void UpdateInputTimeLabels();
  void UpdateEntryFormatControls();
  void UpdateAutomaticZoneOffset();
  void ContextPositionEdited(wxCommandEvent& event);
  void ContextTimeEdited(wxCommandEvent& event);
  void ScheduleRefresh();
  void OnRefreshTimer(wxTimerEvent& event);
  void ApplyPositionSource();
  void ApplyPositionSourceAndRefresh();
  void ChangePositionSource(wxCommandEvent& event);
  bool UpdateCursorPosition();
  void OnCursorTimer(wxTimerEvent& event);
  bool ChooseWaypoint();
  std::vector<WaypointPosition> LoadWaypoints() const;
  bool ResolveSelectedWaypoint(WaypointPosition* waypoint) const;
  void ApplyTimeSource();
  wxString DisplayTime(const wxDateTime& utc) const;
  void RefreshAll(wxCommandEvent& event);
  void ClearCalculatedResults(const wxString& status);
  void RefreshEvents();
  void RefreshBodies();
  void RebuildBodyList();
  void RefreshSkyPlot();
  void SortBodies(wxListEvent& event);
  void RefreshAlmanac();
  void RefreshSpecial();
  void ExportAlmanac(wxCommandEvent& event);
  void CreateSelectedSight(wxCommandEvent& event);
  void SolveSpecialLatitude(wxCommandEvent& event);
#ifdef CELESTIAL_PLANNER_INTEGRATION_TEST
  bool SelectWaypointForIntegration(const wxString& name);
  void OnWaypointIntegrationTimer(wxTimerEvent& event);
#endif

  CelestialNavigationDialog* m_parent;
  wxChoice* m_positionSource;
  NavigationAngleCtrl* m_latitude;
  NavigationAngleCtrl* m_longitude;
  wxChoice* m_timeSource;
  wxChoice* m_inputTimeBasis;
  wxStaticText* m_dateLabel;
  wxStaticText* m_timeLabel;
  wxDatePickerCtrl* m_utcDate;
  wxTimePickerCtrl* m_utcTime;
  wxPanel* m_dateContainer;
  wxPanel* m_timeContainer;
  wxTextCtrl* m_nauticalDate;
  wxTextCtrl* m_nauticalTime;
  wxChoice* m_entryFormat;
  wxChoice* m_displayTime;
  wxSpinCtrlDouble* m_fixedOffset;
  wxCheckBox* m_autoZoneOffset;
  wxCheckBox* m_moving;
  wxSpinCtrlDouble* m_course;
  wxSpinCtrlDouble* m_speed;
  wxSpinCtrlDouble* m_eyeHeight;
  wxStaticText* m_status;
  wxNotebook* m_notebook;
  wxListCtrl* m_events;
  wxStaticText* m_moonSummary;
  wxListCtrl* m_bodies;
  wxListCtrl* m_combinations;
  wxCheckBox* m_limitRecommendationAltitude;
  wxSpinCtrlDouble* m_recommendationMinAltitude;
  wxSpinCtrlDouble* m_recommendationMaxAltitude;
  SkyPlotPanel* m_skyPlot;
  wxChoice* m_plotMagnitude;
  wxCheckBox* m_plotBelowHorizon;
  wxListCtrl* m_almanac;
  wxChoice* m_specialBody;
  NavigationAngleCtrl* m_specialAltitude;
  wxStaticText* m_specialSummary;
  int m_lastInputTimeBasis;
  int m_lastEntryFormat;
  int m_lastPositionSource;
  int m_bodySortColumn;
  bool m_bodySortAscending;
  wxString m_waypointGuid;
  wxString m_waypointName;
  wxTimer m_cursorTimer;
  wxTimer m_refreshTimer;
#ifdef CELESTIAL_PLANNER_INTEGRATION_TEST
  wxTimer m_waypointIntegrationTimer;
  wxString m_waypointIntegrationName;
  int m_waypointIntegrationAttempts;
#endif
  std::vector<RankedBody> m_rankedBodies;
  std::vector<AlmanacRow> m_almanacRows;
};

#endif
