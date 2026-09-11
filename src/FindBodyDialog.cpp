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

#include "OcpnApiCompat.h"

#include "Sight.h"
#include "celestial_navigation_pi.h"
#include "geodesic.h"
#include <cmath>

#ifdef __OCPN__ANDROID__
#include <wx/qt/private/wxQtGesture.h>
#endif

FindBodyDialog::FindBodyDialog(wxWindow* parent, Sight& sight,
                               CopyHsHandler copyHs)
    : FindBodyDialogBase(parent), m_Sight(sight), m_copyHs(copyHs) {
  m_cbBoatPosition->SetLabel(_("Current boat position (live)"));
  m_cbBoatPosition->SetToolTip(
      _("Uses OpenCPN's current boat position, not the boat position at the "
        "sight's UTC. For a historical sight, clear this option and enter the "
        "DR position at the sight time."));
  if (!sight.m_DRBoatPosition) {
    m_tLatitude->ChangeValue(toSDMM_PlugIn(1, m_Sight.m_DRLat, true));
    m_tLongitude->ChangeValue(toSDMM_PlugIn(2, m_Sight.m_DRLon, true));
  }
  m_cbBoatPosition->SetValue(sight.m_DRBoatPosition);
  m_cbMagneticAzimuth->SetValue(sight.m_DRMagneticAzimuth);
  // Replace the generated accept/cancel row with independent tool actions.
  // Keep behaviour here so regenerating the base form cannot restore copying
  // on acceptance. The unused generated buttons cannot receive default input.
  GetSizer()->Hide(m_sFindDialogButton);
  m_sFindDialogButtonOK->Disable();
  m_sFindDialogButtonCancel->Disable();
  auto* actions = new wxBoxSizer(wxHORIZONTAL);
  auto* reset = new wxButton(this, wxID_ANY, _("Reset position"));
  m_copyHsButton = new wxButton(this, wxID_ANY, _("Copy estimated Hs"));
  auto* close = new wxButton(this, wxID_CLOSE, _("Close"));
  reset->SetToolTip(
      _("Restore the position and live-position setting shown when Find "
        "opened."));
  m_copyHsButton->SetToolTip(
      _("Replace the altitude in Sight Properties with this estimate. Find "
        "stays open."));
  close->SetToolTip(
      _("Keep the position and return to Sight Properties without copying Hs. "
        "Save Changes there to keep the sight."));
  actions->Add(reset, 0, wxALL, 5);
  actions->Add(m_copyHsButton, 0, wxALL, 5);
  actions->AddStretchSpacer();
  actions->Add(close, 0, wxALL, 5);
  GetSizer()->Add(actions, 0, wxEXPAND);
  reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ResetPosition(); });
  m_copyHsButton->Bind(wxEVT_BUTTON,
                       [this](wxCommandEvent&) { CopyEstimatedHs(); });
  close->Bind(wxEVT_BUTTON,
              [this](wxCommandEvent&) { CloseKeepingPosition(); });
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { CloseKeepingPosition(); });
  SetAffirmativeId(wxID_CLOSE);
  SetEscapeId(wxID_CLOSE);
  close->SetDefault();

  const int coordinateWidth =
      m_tLongitude->GetTextExtent(toSDMM_PlugIn(2, -179.99999, true)).x + 32;
  m_tLatitude->SetMinSize(wxSize(coordinateWidth, -1));
  m_tLongitude->SetMinSize(wxSize(coordinateWidth, -1));

#ifdef __OCPN__ANDROID__
  GetHandle()->setAttribute(Qt::WA_AcceptTouchEvents);
  GetHandle()->grabGesture(Qt::PanGesture);
  Connect(
      wxEVT_QT_PANGESTURE,
      (wxObjectEventFunction)(wxEventFunction)&FindBodyDialog::OnEvtPanGesture,
      NULL, this);
#endif

  UpdateBoatPosition();
  m_initialLatitude = m_Sight.m_DRLat;
  m_initialLongitude = m_Sight.m_DRLon;
  m_initialBoatPosition = m_Sight.m_DRBoatPosition;
  GetSizer()->Fit(this);
  Centre();
}

#ifdef __OCPN__ANDROID__
void FindBodyDialog::OnEvtPanGesture(wxQT_PanGestureEvent& event) {
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

FindBodyDialog::~FindBodyDialog() {}

void FindBodyDialog::ResetPosition() {
  m_Sight.m_DRLat = m_initialLatitude;
  m_Sight.m_DRLon = m_initialLongitude;
  m_cbBoatPosition->SetValue(m_initialBoatPosition);
  m_Sight.m_DRBoatPosition = m_initialBoatPosition;
  m_tLatitude->ChangeValue(toSDMM_PlugIn(1, m_initialLatitude, true));
  m_tLongitude->ChangeValue(toSDMM_PlugIn(2, m_initialLongitude, true));
  m_tLatitude->Enable(!m_initialBoatPosition);
  m_tLongitude->Enable(!m_initialBoatPosition);
  Update();
}

void FindBodyDialog::CopyEstimatedHs() {
  Update();
  if (!m_copyHs || !m_copyHsButton->IsEnabled()) return;
  const wxString value = m_tEstimatedHs->GetValue();
  m_copyHs(value);
  // Refresh the popup's intercept against the deliberately copied altitude.
  // Retain the already corrected epoch, limb and reduction settings.
  m_Sight.m_Measurement = fromDMM_Plugin(value);
  m_Sight.m_CalcStr.clear();
  m_Sight.RecomputeAltitude();
  Update();
}

void FindBodyDialog::CloseKeepingPosition() {
  Update();
  if (IsModal())
    EndModal(wxID_OK);
  else
    Hide();
}

void FindBodyDialog::OnUpdate(wxCommandEvent& event) { Update(); }

void FindBodyDialog::OnUpdateBoatPosition(wxCommandEvent& event) {
  UpdateBoatPosition();
}

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
    m_Sight.m_DRLat = fromDMM_Plugin(m_tLatitude->GetValue());
    m_Sight.m_DRLon = fromDMM_Plugin(m_tLongitude->GetValue());
    m_tLatitude->Enable(true);
    m_tLongitude->Enable(true);
  }
  m_tLatitude->ChangeValue(toSDMM_PlugIn(1, m_Sight.m_DRLat, true));
  m_tLongitude->ChangeValue(toSDMM_PlugIn(2, m_Sight.m_DRLon, true));
  Update();
}

void FindBodyDialog::Update() {
  /* NOTE: we do not peform any altitude corrections here */
  double hc, zn;

  m_Sight.m_DRMagneticAzimuth = m_cbMagneticAzimuth->GetValue();
  if (!m_Sight.m_DRBoatPosition) {
    m_Sight.m_DRLat = fromDMM_Plugin(m_tLatitude->GetValue());
    m_Sight.m_DRLon = fromDMM_Plugin(m_tLongitude->GetValue());
  }

  m_Sight.CalculateAtDR(&hc, &zn);

  if (m_Sight.m_DRMagneticAzimuth) {
    zn -= celestial_navigation_pi_GetWMM(m_Sight.m_DRLat, m_Sight.m_DRLon,
                                         m_Sight.m_EyeHeight,
                                         m_Sight.m_CorrectedDateTime);
    zn = resolve_heading_positive(zn);
  }

  m_tAltitude->SetValue(std::isfinite(hc) ? toSDMM_PlugIn(0, hc, true)
                                          : _("N/A"));
  m_tAzimuth->SetValue(std::isfinite(zn) ? toSDMM_PlugIn(0, zn, true)
                                         : _("N/A"));
  m_tIntercept->SetValue(
      wxString::Format(_T("%f"), fabs(hc - m_Sight.m_ObservedAltitude) * 60));
  if (hc >= m_Sight.m_ObservedAltitude) {
    m_cbAway->SetValue(true);
    m_cbTowards->SetValue(false);
  } else {
    m_cbTowards->SetValue(true);
    m_cbAway->SetValue(false);
  }

  double estimatedHs, estimatedError;
  m_Sight.EstimateHs(hc, &estimatedHs, &estimatedError);
  if (!isnan(estimatedHs)) {
    m_tEstimatedHs->SetValue(toSDMM_PlugIn(0, estimatedHs, true));
    m_copyHsButton->Enable(static_cast<bool>(m_copyHs));
  } else {
    m_tEstimatedHs->SetValue("   N/A");
    m_copyHsButton->Disable();
  }
}
