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

#include "FindBodyDialog.h"

#include "ocpn_plugin.h"

#include "Sight.h"
#include "celestial_navigation_pi.h"
#include "geodesic.h"

FindBodyDialog::FindBodyDialog(wxWindow* parent, Sight& sight)
    : FindBodyDialogBase(parent), m_Sight(sight) {
  if (!sight.m_DRBoatPosition) {
    m_tLatitude->ChangeValue(wxString::Format(_T("%.4f"), m_Sight.m_DRLat));
    m_tLongitude->ChangeValue(wxString::Format(_T("%.4f"), m_Sight.m_DRLon));
  }
  m_cbBoatPosition->SetValue(sight.m_DRBoatPosition);
  m_cbMagneticAzimuth->SetValue(sight.m_DRMagneticAzimuth);
  m_Lunar->Show(m_Sight.m_Type == Sight::LUNAR);

  Centre();
  UpdateBoatPosition();
}

FindBodyDialog::~FindBodyDialog() {}

void FindBodyDialog::OnUpdate(wxCommandEvent& event) { Update(); }

void FindBodyDialog::OnUpdateBoatPosition(wxCommandEvent& event) {
  UpdateBoatPosition();
}

void FindBodyDialog::OnDone(wxCommandEvent& event) { EndModal(wxID_OK); }

void FindBodyDialog::UpdateBoatPosition() {
  m_Sight.m_DRBoatPosition = m_cbBoatPosition->GetValue();
  if (m_Sight.m_DRBoatPosition) {
    double lat, lon;
    celestial_navigation_pi_BoatPos(lat, lon);
    m_Sight.m_DRLat = lat;
    m_Sight.m_DRLon = lon;
    m_tLatitude->Enable(false);
    m_tLongitude->Enable(false);
  } else {
    m_tLatitude->GetValue().ToDouble(&m_Sight.m_DRLat);
    m_tLongitude->GetValue().ToDouble(&m_Sight.m_DRLon);
    m_tLatitude->Enable(true);
    m_tLongitude->Enable(true);
  }
  m_tLatitude->ChangeValue(wxString::Format(_T("%.4f"), m_Sight.m_DRLat));
  m_tLongitude->ChangeValue(wxString::Format(_T("%.4f"), m_Sight.m_DRLon));
  Update();
}

void FindBodyDialog::Update() {
  /* NOTE: we do not peform any altitude corrections here */
  double lat, lon, hc, zn;

  m_Sight.m_DRMagneticAzimuth = m_cbMagneticAzimuth->GetValue();
  if (!m_Sight.m_DRBoatPosition) {
    m_tLatitude->GetValue().ToDouble(&m_Sight.m_DRLat);
    m_tLongitude->GetValue().ToDouble(&m_Sight.m_DRLon);
  }

  m_Sight.BodyLocation(m_Sight.m_DateTime, &lat, &lon, 0, 0, 0);
  m_Sight.AltitudeAzimuth(m_Sight.m_DRLat, m_Sight.m_DRLon, lat, lon, &hc, &zn);

  if (m_Sight.m_DRMagneticAzimuth) {
    zn -=
        celestial_navigation_pi_GetWMM(m_Sight.m_DRLat, m_Sight.m_DRLon,
                                       m_Sight.m_EyeHeight, m_Sight.m_DateTime);
    zn = resolve_heading_positive(zn);
  }

  m_tAltitude->SetValue(wxString::Format(_T("%f"), hc));
  m_tAzimuth->SetValue(wxString::Format(_T("%f"), zn));
  m_tIntercept->SetValue(
      wxString::Format(_T("%f"), fabs(hc - m_Sight.m_ObservedAltitude) * 60));
  if (hc >= m_Sight.m_ObservedAltitude) {
    m_cbAway->SetValue(true);
    m_cbTowards->SetValue(false);
  } else {
    m_cbTowards->SetValue(true);
    m_cbAway->SetValue(false);
  }

  if (m_Sight.m_Type == Sight::LUNAR) {
    m_tLDC->SetValue(wxString::Format(_T("%.4f"), m_Sight.m_LDC));
    wxDateTime dt = m_Sight.m_CorrectedDateTime +
                    wxTimeSpan::Seconds(m_Sight.m_TimeCorrection);
    dt.MakeFromUTC();
    m_tDateTimeRevised->SetValue(dt.Format("%Y-%m-%d %H:%M:%S", dt.UTC));
    m_tDateTimeChange->SetValue(
        wxString::Format(_T("%ld"), m_Sight.m_TimeCorrection));
    m_tLonRevised->SetValue(wxString::Format(
        _T("%.4f"), m_Sight.m_DRLon - 0.25 * m_Sight.m_TimeCorrection / 60));
    m_tLonError->SetValue(
        wxString::Format(_T("%.4f"), 0.25 * m_Sight.m_TimeCorrection));
    m_tPosError->SetValue(wxString::Format(
        _T("%.4f"),
        cos(d_to_r(m_Sight.m_DRLat)) * 0.25 * m_Sight.m_TimeCorrection));
  }
}
