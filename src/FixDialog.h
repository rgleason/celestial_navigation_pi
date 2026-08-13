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

#include <list>

#ifdef __OCPN__ANDROID__
#include <wx/qt/private/wxQtGesture.h>
#endif

class Sight;
class wxChoice;
class wxDatePickerCtrl;
class wxTimePickerCtrl;

class FixDialog : public FixDialogBase {
public:
  FixDialog(CelestialNavigationDialog* parent);
  void Update(int clock_offset);
  void RunIntegrationScenario();

  int m_clock_offset;
  double m_fixlat, m_fixlon, m_fixerror;

private:
  wxDateTime ReadEpochUtc() const;
  void SetEpochControls(const wxDateTime& utc);
  void ChangeEpochTimeBasis(wxCommandEvent& event);
  void UpdateRunningFix(int clock_offset);
  void OnRunningControl(wxCommandEvent& event) { Update(m_clock_offset); }
  void OnGo(wxCommandEvent& event);
  void OnClose(wxCommandEvent& event);
  void OnUpdate(wxCommandEvent& event) { Update(m_clock_offset); }
  void OnUpdateSpin(wxSpinEvent& event) { Update(m_clock_offset); }
#ifdef __OCPN__ANDROID__
  void OnEvtPanGesture(wxQT_PanGestureEvent& event);
#endif

  CelestialNavigationDialog* m_Parent;
  wxCheckBox* m_runningFix;
  wxChoice* m_epochTimeBasis;
  wxDatePickerCtrl* m_epochDate;
  wxTimePickerCtrl* m_epochTime;
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
