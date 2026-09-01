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
class wxNotebook;
class wxSpinCtrlDouble;
class wxStaticText;
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
  void SetUtcControls(const wxDateTime& utc);
  void ChangeInputTimeBasis(wxCommandEvent& event);
  void UpdateInputTimeLabels();
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
  void RefreshEvents();
  void RefreshBodies();
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
  wxChoice* m_displayTime;
  wxSpinCtrlDouble* m_fixedOffset;
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
  SkyPlotPanel* m_skyPlot;
  wxListCtrl* m_almanac;
  wxChoice* m_specialBody;
  NavigationAngleCtrl* m_specialAltitude;
  wxStaticText* m_specialSummary;
  int m_lastInputTimeBasis;
  int m_lastPositionSource;
  wxString m_waypointGuid;
  wxString m_waypointName;
  wxTimer m_cursorTimer;
#ifdef CELESTIAL_PLANNER_INTEGRATION_TEST
  wxTimer m_waypointIntegrationTimer;
  wxString m_waypointIntegrationName;
  int m_waypointIntegrationAttempts;
#endif
  std::vector<RankedBody> m_rankedBodies;
  std::vector<AlmanacRow> m_almanacRows;
};

#endif
