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

#include "CelestialNavigationDialog.h"
#include "OcpnApiCompat.h"

#include "Sight.h"
#include "celestial_navigation_pi.h"
#include "geodesic.h"
#include "WaypointPickerDialog.h"
#include <cmath>
#include <wx/choice.h>
#include <wx/stattext.h>

#ifdef __OCPN__ANDROID__
#include <wx/qt/private/wxQtGesture.h>
#endif

FindBodyDialog::FindBodyDialog(wxWindow* parent, Sight& sight,
                               CopyHsHandler copyHs)
    : FindBodyDialogBase(parent), m_Sight(sight), m_copyHs(copyHs),
      m_appliedPositionSource(sight.m_DRBoatPosition ? 1 : 0) {
  SetTitle(wxString::Format(_("Find %s"), m_Sight.m_Body));
  m_cbBoatPosition->Hide();
  auto* positionControls = m_Body->GetItem(size_t(2))->GetSizer();
  m_positionSource = new wxChoice(this, wxID_ANY);
  m_positionSource->SetName("Find position source");
  m_positionSource->Append(_("Manual"));
  m_positionSource->Append(_("Current boat position (live)"));
  m_positionSource->Append(_("Chart cursor"));
  m_positionSource->Append(_("Last calculated fix"));
  m_positionSource->Append(_("Waypoint or place..."));
  m_positionSource->SetSelection(sight.m_DRBoatPosition ? 1 : 0);
  positionControls->Add(new wxStaticText(this, wxID_ANY, _("Position source")),
                        0, wxLEFT | wxRIGHT | wxTOP, 5);
  positionControls->Add(m_positionSource, 0, wxALL | wxEXPAND, 5);
  m_positionInfo = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_positionInfo->SetMinSize(wxSize(250, -1));
  positionControls->Add(m_positionInfo, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
  m_positionSource->SetToolTip(
      _("Choose the assumed position for the sight's UTC. Boat position "
        "uses the current fix, which may be wrong for a historical sight."));
  m_positionSource->Bind(wxEVT_CHOICE,
                         &FindBodyDialog::ChangePositionSource, this);
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
  auto* cancel = m_sFindDialogButtonCancel;
  m_sFindDialogButton->Detach(cancel);
  cancel->SetLabel(_("Cancel"));
  cancel->Enable();
  cancel->Show();
  reset->SetToolTip(
      _("Restore the position and live-position setting shown when Find "
        "opened. Find stays open; explicitly copied Hs is not undone."));
  cancel->SetToolTip(
      _("Discard position edits and close Find. Explicitly copied Hs is not "
        "undone."));
  m_copyHsButton->SetToolTip(
      _("Replace the altitude in Sight Properties with this estimate. Find "
        "stays open."));
  close->SetToolTip(
      _("Keep the position and return to Sight Properties without copying Hs. "
        "Save Changes there to keep the sight."));
  // Reuse the generated controls, but arrange the tool actions alongside the
  // values they affect. Keep all layout customisation out of generated code.
  positionControls->Add(reset, 0, wxALL, 5);
  auto* towards = m_Body->GetItem(size_t(7))->GetSizer();
  auto* away = m_Body->GetItem(size_t(8))->GetSizer();
  m_Body->Detach(towards);
  m_Body->Detach(away);
  towards->Add(away, 0, wxEXPAND);
  auto* hoBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Altitude (Ho)"));
  m_observedAltitude = new wxTextCtrl(hoBox->GetStaticBox(), wxID_ANY,
      wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  m_observedAltitude->SetToolTip(
      _("Observed altitude after reduction of the entered Hs. For lunar "
        "helpers this is the selected Moon or body's altitude, not lunar "
        "distance."));
  hoBox->Add(m_observedAltitude, 0, wxALL | wxEXPAND, 5);
  m_Body->Insert(6, hoBox, 1, wxALL | wxEXPAND, 5);
  m_Body->Insert(8, towards, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  m_Body->Add(m_copyHsButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  actions->AddStretchSpacer();
  actions->Add(cancel, 0, wxALL, 5);
  actions->Add(close, 0, wxALL, 5);
  GetSizer()->Add(actions, 0, wxEXPAND);
  reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ResetPosition(); });
  m_copyHsButton->Bind(wxEVT_BUTTON,
                       [this](wxCommandEvent&) { CopyEstimatedHs(); });
  close->Bind(wxEVT_BUTTON,
              [this](wxCommandEvent&) { CloseKeepingPosition(); });
  cancel->Bind(wxEVT_BUTTON,
               [this](wxCommandEvent&) { CancelPosition(); });
  Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { CancelPosition(); });
  SetAffirmativeId(wxID_CLOSE);
  SetEscapeId(cancel->GetId());
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

CelestialNavigationDialog* FindBodyDialog::NavigationDialog() const {
  for (wxWindow* parent = GetParent(); parent; parent = parent->GetParent())
    if (auto* navigation = dynamic_cast<CelestialNavigationDialog*>(parent))
      return navigation;
  return nullptr;
}

void FindBodyDialog::SetCoordinates(double latitude, double longitude) {
  m_Sight.m_DRLat = latitude;
  m_Sight.m_DRLon = longitude;
  m_tLatitude->ChangeValue(toSDMM_PlugIn(1, latitude, true));
  m_tLongitude->ChangeValue(toSDMM_PlugIn(2, longitude, true));
}

void FindBodyDialog::ChangePositionSource(wxCommandEvent&) {
  ApplyPositionSource();
}

void FindBodyDialog::ApplyPositionSource() {
  const int source = m_positionSource->GetSelection();
  double latitude = m_Sight.m_DRLat;
  double longitude = m_Sight.m_DRLon;
  wxString info;
  auto* navigation = NavigationDialog();
  bool available = true;
  if (source == 1) {
    available = navigation && navigation->GetPlugin()->GetBoatPosition(
                                 &latitude, &longitude);
    info = _("Current boat fix; check the sight's UTC.");
  } else if (source == 2) {
    available = navigation && navigation->GetPlugin()->GetCursorPosition(
                                 &latitude, &longitude);
    info = _("Chart cursor position captured.");
  } else if (source == 3) {
    wxDateTime calculatedUtc, epochUtc;
    available = navigation && navigation->GetLastFix(
                                 &latitude, &longitude, &calculatedUtc,
                                 &epochUtc);
    if (available) {
      if (epochUtc.IsValid())
        info = wxString::Format(
            _("Fix epoch: %s UTC"),
            epochUtc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC).c_str());
      else
        info = wxString::Format(
            _("Calculated: %s UTC; no single fix epoch"),
            calculatedUtc.Format("%Y-%m-%d %H:%M:%S", wxDateTime::UTC)
                .c_str());
    }
  } else if (source == 4) {
    const auto waypoints = LoadOpenCpnWaypoints();
    if (waypoints.empty()) {
      available = false;
    } else {
      WaypointPickerDialog picker(this, waypoints, wxEmptyString);
      if (picker.ShowModal() != wxID_OK) {
        m_positionSource->SetSelection(m_appliedPositionSource);
        return;
      }
      const WaypointPosition* waypoint = picker.GetSelectedWaypoint();
      available = waypoint != nullptr;
      if (waypoint) {
        latitude = waypoint->latitude;
        longitude = waypoint->longitude;
        info = wxString::Format(_("Waypoint/place: %s"),
                                waypoint->name.c_str());
      }
    }
  } else {
    info = _("Enter the DR position at the sight's UTC.");
  }
  if (!available) {
    wxMessageBox(_("This position source is unavailable; the previous "
                   "coordinates were retained."),
                 _("Position unavailable"), wxOK | wxICON_INFORMATION, this);
    m_positionSource->SetSelection(m_appliedPositionSource);
    return;
  }
  m_cbBoatPosition->SetValue(source == 1);
  m_Sight.m_DRBoatPosition = source == 1;
  if (source != 0) SetCoordinates(latitude, longitude);
  m_tLatitude->Enable(source == 0);
  m_tLongitude->Enable(source == 0);
  m_positionInfo->SetLabel(info);
  m_positionInfo->Wrap(250);
  m_appliedPositionSource = source;
  Update();
  GetSizer()->Layout();
  GetSizer()->Fit(this);
}

void FindBodyDialog::ResetPosition() {
  m_Sight.m_DRLat = m_initialLatitude;
  m_Sight.m_DRLon = m_initialLongitude;
  m_positionSource->SetSelection(m_initialBoatPosition ? 1 : 0);
  m_appliedPositionSource = m_positionSource->GetSelection();
  m_cbBoatPosition->SetValue(m_initialBoatPosition);
  m_Sight.m_DRBoatPosition = m_initialBoatPosition;
  m_tLatitude->ChangeValue(toSDMM_PlugIn(1, m_initialLatitude, true));
  m_tLongitude->ChangeValue(toSDMM_PlugIn(2, m_initialLongitude, true));
  m_tLatitude->Enable(!m_initialBoatPosition);
  m_tLongitude->Enable(!m_initialBoatPosition);
  m_positionInfo->SetLabel(m_initialBoatPosition
                               ? _("Current boat fix; check the sight's UTC.")
                               : _("Enter the DR position at the sight's UTC."));
  m_positionInfo->Wrap(250);
  GetSizer()->Fit(this);
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

void FindBodyDialog::CancelPosition() {
  ResetPosition();
  // Explicit Hs copies have already been applied to Sight Properties. Do not
  // apply this popup's remaining local settings when cancelling its position.
  if (IsModal())
    EndModal(wxID_CANCEL);
  else
    Hide();
}

void FindBodyDialog::OnUpdate(wxCommandEvent& event) { Update(); }

void FindBodyDialog::OnUpdateBoatPosition(wxCommandEvent& event) {
  UpdateBoatPosition();
}

void FindBodyDialog::UpdateBoatPosition() {
  if (m_positionSource->GetSelection() == 1) {
    double lat, lon;
    celestial_navigation_pi_BoatPos(lat, lon);
    SetCoordinates(lat, lon);
    m_Sight.m_DRBoatPosition = true;
    m_tLatitude->Enable(false);
    m_tLongitude->Enable(false);
    m_positionInfo->SetLabel(_("Current boat fix; check the sight's UTC."));
  } else {
    m_Sight.m_DRBoatPosition = false;
    m_tLatitude->Enable(true);
    m_tLongitude->Enable(true);
    m_positionInfo->SetLabel(_("Enter the DR position at the sight's UTC."));
  }
  m_cbBoatPosition->SetValue(m_Sight.m_DRBoatPosition);
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
  m_observedAltitude->SetValue(
      m_Sight.m_Type == Sight::ALTITUDE &&
              std::isfinite(m_Sight.m_ObservedAltitude)
          ? toSDMM_PlugIn(0, m_Sight.m_ObservedAltitude, true)
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
