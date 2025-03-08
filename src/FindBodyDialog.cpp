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
#include <wx/listimpl.cpp>    // toh, 2009.02.22
#include <wx/fileconf.h>

#include "FindBodyDialog.h"

#include "ocpn_plugin.h"

#include "Sight.h"
#include "celestial_navigation_pi.h"
#include "geodesic.h"

FindBodyDialog::FindBodyDialog( wxWindow* parent, Sight &sight )
: FindBodyDialogBase(parent), m_Sight(sight)
{
    double lat, lon;
    celestial_navigation_pi_BoatPos(lat, lon);
    wxFileConfig *pConf = GetOCPNConfigObject();
    pConf->SetPath( _T("/PlugIns/CelestialNavigation/FindBody") );
    
    bool boat_position;
    pConf->Read( _T("BoatPosition"), &boat_position, true );
    m_cbBoatPosition->SetValue(boat_position);
    
    bool magnetic_azimuth;
    pConf->Read( _T("MagneticAzimuth"), &magnetic_azimuth, false);
    m_cbMagneticAzimuth->SetValue(magnetic_azimuth);
    
    if(!m_cbBoatPosition->GetValue()) {
        pConf->Read( _T("Lat"), &lat, lat );
        pConf->Read( _T("Lon"), &lon, lon );
    }

    m_tLatitude->SetValue(wxString::Format(_T("%.4f"), lat));
    m_tLongitude->SetValue(wxString::Format(_T("%.4f"), lon));

    Centre();
    Update();
}

FindBodyDialog::~FindBodyDialog( )
{
    wxFileConfig *pConf = GetOCPNConfigObject();
    pConf->SetPath( _T("/PlugIns/CelestialNavigation/FindBody") );
    pConf->Write( _T("BoatPosition"), m_cbBoatPosition->GetValue());
    pConf->Write( _T("MagneticAzimuth"), m_cbMagneticAzimuth->GetValue());
    double lat, lon;
    if(m_tLatitude->GetValue().ToDouble(&lat))
        pConf->Write( _T("Lat"), lat );
    if(m_tLongitude->GetValue().ToDouble(&lon))
        pConf->Write( _T("Lon"), lon );
}

void FindBodyDialog::OnUpdate( wxCommandEvent& event )
{
    Update();
}

void FindBodyDialog::OnDone( wxCommandEvent& event )
{
   EndModal(wxID_OK);
}

void FindBodyDialog::Update()
{
    /* NOTE: we do not peform any altitude corrections here */
    double lat1, lon1, lat2, lon2, hc, zn;
    m_tLatitude->GetValue().ToDouble(&lat1);
    m_tLongitude->GetValue().ToDouble(&lon1);

    m_Sight.BodyLocation(m_Sight.m_DateTime, &lat2, &lon2, 0, 0);
    m_Sight.AltitudeAzimuth(lat1, lon1, lat2, lon2, &hc, &zn);

    if(m_cbMagneticAzimuth->GetValue()) {
        zn -= celestial_navigation_pi_GetWMM(lat1, lon1, m_Sight.m_EyeHeight, m_Sight.m_DateTime);
        zn = resolve_heading_positive(zn);
    }

    m_tAltitude->SetValue(wxString::Format(_T("%f"), hc));
    m_tAzimuth->SetValue(wxString::Format(_T("%f"), zn));
    m_tIntercept->SetValue(wxString::Format(_T("%f"), fabs(hc - m_Sight.m_ObservedAltitude)*60));
    if (hc >= m_Sight.m_ObservedAltitude) {
        m_cbAway->SetValue(true);
        m_cbTowards->SetValue(false);
    } else {
        m_cbTowards->SetValue(true);
        m_cbAway->SetValue(false);
    }

}
