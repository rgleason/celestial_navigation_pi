/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Support
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2016 by Sean D'Epagnier                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 *
 */

#ifndef _CelestialNavigationDialog_h_
#define _CelestialNavigationDialog_h_

#include <list>

#include "celestial_navigation_pi.h"
#include "geodesic.h"
#include "CelestialNavigationUI.h"
#include "FixDialog.h"
#include "ClockCorrectionDialog.h"
#include "EclipseDialog.h"
#include "PlannerDialog.h"
#include "CoastalNavigationDialog.h"
#include "LunarToolsDialog.h"
#include "LunarSolutionRecord.h"

#include <vector>
#include <wx/timer.h>

#ifdef __OCPN__ANDROID__
#include <wx/qt/private/wxQtGesture.h>
#endif

class CelestialNavigationDialog : public CelestialNavigationDialogBase {
public:
  CelestialNavigationDialog(wxWindow* parent, celestial_navigation_pi* ppi);
  ~CelestialNavigationDialog();
  void UpdateSights();

  ClockCorrectionDialog* m_ClockCorrectionDialog;
  FixDialog* m_FixDialog;
  double m_pix_per_mm;
  std::vector<Sight> m_Sights;

  void OnFixClose();
  bool RenderEclipse(piDC* dc, PlugIn_ViewPort* viewport);
  bool RenderCoastal(piDC* dc, PlugIn_ViewPort* viewport);
  void RunEclipseIntegrationScenario();
  void RunPlannerIntegrationScenario();
  celestial_navigation_pi* GetPlugin() const { return m_Plugin; }
  const Sight* GetSelectedSight() const;
  bool GetLastFix(double* latitude, double* longitude,
                  wxDateTime* calculatedUtc = nullptr,
                  wxDateTime* epochUtc = nullptr) const;
  void SetLastFix(double latitude, double longitude,
                  const wxDateTime& epochUtc = wxDateTime());
  void CreatePlannedSight(const wxString& body, const wxDateTime& utc,
                          double drLat, double drLon);
  int GetClockCorrection() const { return m_ClockCorrection; }
  // Noninteractive file operations used by the manager and its regression tests.
  bool BackupSightsTo(const wxString& path, wxString* error);
  bool ImportSightsFile(const wxString& path, bool replace,
                        wxString* safetyBackup, wxString* error);
  void ApplyClockCorrection(int correction_seconds);
  bool SaveLunarSolution(LunarSolutionRecord record);
  void ShowLunarSolutions(wxWindow* parent);
  const std::vector<LunarSolutionRecord>& LunarSolutions() const {
    return m_lunarSolutions;
  }
  void OpenAlmanacForRoute(const wxString& routeGuid = wxString());
  bool GetMarkedUtc(wxDateTime* utcFields) const;

private:
  bool OpenXML(bool reportfailure);
  bool ReadSightsXml(const wxString& path, std::vector<Sight>* sights,
                     int* clockCorrection,
                     std::vector<LunarSolutionRecord>* lunarSolutions,
                     wxString* errorOut, bool strict = true);
  bool SaveXML();
  std::vector<LunarSolutionRecord> m_lunarSolutions;

  void RebuildList(bool persist = true);
  void OnManageSights(wxCommandEvent& event);
  void BackupSights();
  void ImportSights(bool replace);
  bool ApplyImportedSights(std::vector<Sight> incoming, int correction,
                           std::vector<LunarSolutionRecord> solutions,
                           bool replace, wxString* safetyBackup,
                           wxString* error);
  void UpdateButtons();  // Correct button state
  void UpdateFix();
  void BuildTimeIntegrityPanel(bool visible);
  void SetTimeIntegrityVisible(bool visible, bool resize);
  void UpdateTimeIntegrityPanel();
  void QueryChrony();
  void OnTimeTimer(wxTimerEvent& event);
  void OnTimeIntegrityToggle(wxCommandEvent& event);
  void OnMarkTime(wxCommandEvent& event);
  void OnCopyMarkedUtc(wxCommandEvent& event);

  // event handlers
  void OnNew(wxCommandEvent& event);
  void OnHorizonEvent(wxCommandEvent& event);
  void OnEclipse(wxCommandEvent& event);
  void OnPlanner(wxCommandEvent& event);
  void OnAnalyze(wxCommandEvent& event);
  void OnCoastal(wxCommandEvent& event);
  void OnLunarTools(wxCommandEvent& event);
  void OnGenerateAlmanac(wxCommandEvent& event);
  void OnDuplicate(wxCommandEvent& event);
  void OnEdit();
  void OnEditMouse(wxMouseEvent& event) { OnEdit(); }
  void OnEdit(wxCommandEvent& event) { OnEdit(); }
  void OnDelete(wxCommandEvent& event);
  void OnDeleteAll(wxCommandEvent& event);
  void OnFix(wxCommandEvent& event);
  void OnDRShift(wxCommandEvent& event);
  void OnClockOffset(wxCommandEvent& event);
  void OnDocumentation(wxCommandEvent& event);
  void OnPdfDocumentation(wxCommandEvent& event);
  void OnHide(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  void OnClockCorrection(wxSpinEvent& event);
  void OnSightListLeftDown(wxMouseEvent& event);
  void OnBtnLeftDown(
      wxMouseEvent& event);  // record control key state for some action buttons
  void OnEdit(wxListEvent& event) { OnEdit(); }
  void OnColumnHeaderClick(wxListEvent& event);
  void OnSightSelected(wxListEvent& event);
  wxString CurrentTimeCaptureSummary();
#ifdef __OCPN__ANDROID__
  void OnEvtPanGesture(wxQT_PanGestureEvent& event);
#endif

  void InsertSight(Sight* s);
  void UpdateSight(int idx);

  celestial_navigation_pi* m_Plugin;
  wxString m_sights_path;
  int m_ClockCorrection;

  wxStaticText* m_localTime;
  wxStaticText* m_utcTime;
  wxScrolledWindow* m_timeIntegrityPanel;
  wxToggleButton* m_timeIntegrityToggle;
  wxStaticText* m_gnssTime;
  wxStaticText* m_gnssDifference;
  wxStaticText* m_systemTimeStatus;
  wxStaticText* m_sightCorrection;
  wxButton* m_markTimeButton;
  wxButton* m_copyMarkedUtcButton;
  wxStaticText* m_markedTimeStatus;
  wxDateTime m_markedTime;
  wxButton* m_horizonEventButton;
  wxButton* m_eclipseButton;
  wxButton* m_plannerButton;
  wxButton* m_analyzeButton;
  wxButton* m_coastalButton;
  wxButton* m_lunarToolsButton;
  wxButton* m_almanacButton;
  wxButton* m_pdfDocumentationButton;
  wxButton* m_manageSightsButton;
  EclipseDialog* m_eclipseDialog;
  CoastalNavigationDialog* m_coastalDialog;
  wxTimer m_timeTimer;
  int m_chronyPollTicks;
  ChronyTrackingInfo m_chronyTracking;
  bool m_chronyAvailable;
  bool m_hasLastFix;
  double m_lastFixLatitude;
  double m_lastFixLongitude;
  wxDateTime m_lastFixCalculatedUtc;
  wxDateTime m_lastFixEpochUtc;

  wxPoint m_startPos;
  wxPoint m_startMouse;
  wxSize m_fullSize;
  int m_sortCol;
  bool m_bSortAsc;

  int m_lastPanX;
  int m_lastPanY;
};

#endif  // _CelestialNavigationDialog_h_
