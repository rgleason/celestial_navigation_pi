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

#include <wx/wx.h>
#include "CelestialNavigationDialog.h"
#include "ClockCorrectionDialog.h"

#include "OcpnApiCompat.h"
#include "Sight.h"

ClockCorrectionDialog::ClockCorrectionDialog(CelestialNavigationDialog* parent,
                                             int value)
    : ClockCorrectionDialogBase(parent),
      m_Parent(parent),
      m_initialValue(value) {
  m_sClockCorrection->SetValue(value);
  m_sdbSizer7OK->SetLabel(_("Apply"));
  m_sdbSizer7OK->SetDefault();
  m_sdbSizer7->Layout();
  SetAffirmativeId(wxID_OK);
  SetEscapeId(wxID_CANCEL);
  Bind(wxEVT_CLOSE_WINDOW, &ClockCorrectionDialog::OnWindowClose, this);
}

void ClockCorrectionDialog::OnUpdate(wxSpinEvent& event) {}

void ClockCorrectionDialog::OnWindowClose(wxCloseEvent& event) {
  if (m_sClockCorrection->GetValue() != m_initialValue && event.CanVeto()) {
    wxMessageDialog confirm(
        this, _("Discard your changes?"), _("Unsaved Clock Correction"),
        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    confirm.SetYesNoLabels(_("Discard Changes"), _("Keep Editing"));
    if (confirm.ShowModal() != wxID_YES) {
      event.Veto();
      return;
    }
  }
  if (IsModal())
    EndModal(wxID_CANCEL);
  else
    event.Skip();
}
