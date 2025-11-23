/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Support
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2013 by Sean D'Epagnier                                 *
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

#ifndef _LUNARRESULTSDIALOG_H_
#define _LUNARRESULTSDIALOG_H_

#include "CelestialNavigationUI.h"

#ifdef __OCPN__ANDROID__
#include <wx/qt/private/wxQtGesture.h>
#endif

class Sight;

class LunarResultsDialog : public LunarResultsDialogBase {
public:
  LunarResultsDialog(wxWindow* parent, Sight& sight);
  ~LunarResultsDialog();

  void OnUpdate(wxCommandEvent& event);
  void Update();
#ifdef __OCPN__ANDROID__
  void OnEvtPanGesture(wxQT_PanGestureEvent& event);
#endif

  Sight& m_Sight;
  int m_lastPanX;
  int m_lastPanY;
};

#endif
// _LUNARRESULTSDIALOG_H_
