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

#include <wx/wx.h>
#include <wx/listimpl.cpp>  // toh, 2009.02.22
#include <wx/fileconf.h>

#include "LunarResultsDialog.h"

#include "ocpn_plugin.h"

#include "Sight.h"
#include "celestial_navigation_pi.h"
#include "geodesic.h"

#ifdef __OCPN__ANDROID__
#include <wx/qt/private/wxQtGesture.h>
#endif

LunarResultsDialog::LunarResultsDialog(wxWindow* parent, Sight& sight)
    : LunarResultsDialogBase(parent), m_Sight(sight) {
  int x, y;
  GetTextExtent(_T("000° 00.0000' S"), &x, &y);
  m_tLDC->SetSizeHints(x + 20, -1);
  m_tLonRevised->SetSizeHints(x + 20, -1);
  GetTextExtent(_T("0000-00-00 00:00:00"), &x, &y);
  m_tDateTimeRevised->SetSizeHints(x + 20, -1);

#ifdef __OCPN__ANDROID__
  GetHandle()->setAttribute(Qt::WA_AcceptTouchEvents);
  GetHandle()->grabGesture(Qt::PanGesture);
  Connect(wxEVT_QT_PANGESTURE,
          (wxObjectEventFunction)(wxEventFunction)&LunarResultsDialog::
              OnEvtPanGesture,
          NULL, this);
#endif

  Centre();
  Update();
}

#ifdef __OCPN__ANDROID__
void LunarResultsDialog::OnEvtPanGesture(wxQT_PanGestureEvent& event) {
  int x = event.GetOffset().x;
  int y = event.GetOffset().y;

  int dx = x - m_lastPanX;
  int dy = y - m_lastPanY;

  if (event.GetState() == GestureUpdated) {
    wxPoint p = GetPosition();
    wxSize s = GetSize();
    p.x = wxMax(0, p.x + dx);
    p.y = wxMax(0, p.y + dy);
    p.x = wxMin(p.x, ::wxGetDisplaySize().x - s.x);
    p.y = wxMin(p.y, ::wxGetDisplaySize().y - s.y);
    SetPosition(p);
  }
  m_lastPanX = x;
  m_lastPanY = y;
}
#endif

LunarResultsDialog::~LunarResultsDialog() {}

void LunarResultsDialog::OnUpdate(wxCommandEvent& event) { Update(); }

void LunarResultsDialog::Update() {
  m_tLDC->SetValue(toSDMM_PlugIn(0, m_Sight.m_LDC, true));
  wxDateTime dt = m_Sight.m_CorrectedDateTime +
                  wxTimeSpan::Seconds(m_Sight.m_TimeCorrection);
  dt.MakeFromUTC();
  m_tDateTimeRevised->SetValue(dt.Format("%Y-%m-%d %H:%M:%S", dt.UTC));
  m_tDateTimeChange->SetValue(
      wxString::Format(_T("%ld"), m_Sight.m_TimeCorrection));
  m_tLonRevised->SetValue(toSDMM_PlugIn(
      2, (m_Sight.m_DRLon - 0.25 * m_Sight.m_TimeCorrection / 60), true));
  m_tLonError->SetValue(
      wxString::Format(_T("%.5f"), 0.25 * m_Sight.m_TimeCorrection));
  m_tPosError->SetValue(wxString::Format(
      _T("%.5f"),
      cos(d_to_r(m_Sight.m_DRLat)) * 0.25 * m_Sight.m_TimeCorrection));
}
