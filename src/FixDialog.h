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

#ifndef _FIXDIALOG_H_
#define _FIXDIALOG_H_

#include "CelestialNavigationUI.h"
#include "CelestialNavigationDialog.h"
#include "NavigationAlgorithms.h"
#include "FixAmbiguity.h"

#include <list>

#ifdef __OCPN__ANDROID__
#include <wx/qt/private/wxQtGesture.h>
class NavigationAngleCtrl;
#endif

class NavigationAngleCtrl;
class Sight;
class wxChoice;
class wxDatePickerCtrl;
#ifdef __OCPN__ANDROID__
#include "NauticalTimeCtrl.h"
using CelestialTimePicker = NauticalTimeCtrl;
#else
class wxTimePickerCtrl;
using CelestialTimePicker = wxTimePickerCtrl;
#endif
class wxCloseEvent;
class wxScrolledWindow;

class FixDialog : public FixDialogBase {
public:
  FixDialog(CelestialNavigationDialog* parent);
  ~FixDialog() override;
  void Update(int clock_offset);
  void RunIntegrationScenario();
  void FocusStartingDr();

  int m_clock_offset;
  double m_fixlat, m_fixlon, m_fixerror;

private:
  wxScrolledWindow* m_desktopScroll = nullptr;
  void RefreshDrSources();
  void ChangeDrSource(wxCommandEvent&);
  void EditDr(wxCommandEvent&);
  bool ReadDr(double* latitude, double* longitude);
  std::string CalculationKey(double correction);
  bool ChooseCandidate(const std::vector<fix_selection::Position>& candidates,
                       const std::string& key);
  void HideCandidates();
  bool UpdateTwoSightFix(double correction, double latitude, double longitude);
  NavigationAngleCtrl* m_fixDrLatitude = nullptr;
  NavigationAngleCtrl* m_fixDrLongitude = nullptr;
  wxChoice* m_drSource = nullptr;
  wxStaticText* m_drExplanation = nullptr;
  wxStaticText* m_candidateWarning = nullptr;
  wxBoxSizer* m_candidateSizer = nullptr;
  std::vector<wxButton*> m_candidateButtons;
  std::vector<fix_selection::DrRecord> m_drRecords;
  wxString m_drSourceKey;
  bool m_hasDr = false;
  bool m_drSourcesReady = false;
  bool m_changingDr = false;
  fix_selection::Acceptance m_acceptance;
  std::string m_runningCandidateKey;
  std::vector<RunningFixResult> m_runningCandidates;
  wxDateTime ReadEpochUtc() const;
  void SetEpochControls(const wxDateTime& utc);
  void ChangeEpochTimeBasis(wxCommandEvent& event);
  void ChangeMotionMode(wxCommandEvent& event);
  void SetEpochToLatestVisibleSight(double clock_offset);
  void UpdateRunningFix(double clock_offset);
  void OnRunningControl(wxCommandEvent& event) { Update(m_clock_offset); }
  void OnGo(wxCommandEvent& event);
  void OnClose(wxCommandEvent& event);
  void OnWindowClose(wxCloseEvent& event);
  void OnUpdate(wxCommandEvent& event) { Update(m_clock_offset); }
  void OnUpdateSpin(wxSpinEvent& event) { Update(m_clock_offset); }
#ifdef __OCPN__ANDROID__
  void OnEvtPanGesture(wxQT_PanGestureEvent& event);
  wxStaticText* m_androidResiduals;
#endif

  CelestialNavigationDialog* m_Parent;
  wxCheckBox* m_runningFix;
  wxChoice* m_motionMode;
  wxChoice* m_lunarSolution;
  std::vector<Sight> m_workingSights;
  wxChoice* m_epochTimeBasis;
  wxDatePickerCtrl* m_epochDate;
  CelestialTimePicker* m_epochTime;
  wxSpinCtrlDouble* m_courseTrue;
  wxSpinCtrlDouble* m_speedKnots;
  wxStaticText* m_runningSummary;
  wxListCtrl* m_residuals;
  int m_lastEpochTimeBasis;
  int m_lastPanX;
  int m_lastPanY;
};

#endif
// _FIXDIALOG_H_
