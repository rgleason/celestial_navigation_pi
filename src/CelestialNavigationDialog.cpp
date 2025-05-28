/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Celestial Navigation Support
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2015 by Sean D'Epagnier                                 *
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
#include <wx/fileconf.h>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/imaglist.h>

#include "tinyxml.h"

#include "ocpn_plugin.h"

#include "Sight.h"
#include "SightDialog.h"
#include "CelestialNavigationDialog.h"
#include "celestial_navigation_pi.h"

/* XPM */
static const char* eye[] = {"20 20 7 1",
                            ". c none",
                            "# c #000000",
                            "a c #333333",
                            "b c #666666",
                            "c c #999999",
                            "d c #cccccc",
                            "e c #ffffff",
                            "....................",
                            "....................",
                            "....................",
                            "....................",
                            ".......######.......",
                            ".....#aabccb#a#.....",
                            "....#deeeddeebcb#...",
                            "..#aeeeec##aceaec#..",
                            ".#bedaeee####dbcec#.",
                            "#aeedbdabc###bcceea#",
                            ".#bedad######abcec#.",
                            "..#be#d######dadb#..",
                            "...#abac####abba#...",
                            ".....##acbaca##.....",
                            ".......######.......",
                            "....................",
                            "....................",
                            "....................",
                            "....................",
                            "...................."};

enum {
  rmVISIBLE = 0,
  rmTYPE,
  rmBODY,
  rmTIME,
  rmMEASUREMENT,
  rmCOLOR,
  rmMAX
};  // RMColumns;

wxString columns[] = {
    _(""), _("Type"), _("Body"), _("Time (UTC)"), _("Measurement"), _("Color"),
};

CelestialNavigationDialog::CelestialNavigationDialog(wxWindow* parent)
    : CelestialNavigationDialogBase(parent),
      m_FixDialog(this),
      m_ClockCorrectionDialog(this) {
  wxFileConfig* pConf = GetOCPNConfigObject();

  pConf->SetPath(_T("/PlugIns/CelestialNavigation"));

  // #ifdef __WXGTK__
  //     Move(0, 0);        // workaround for gtk autocentre dialog behavior
  // #endif
  //     Move(pConf->Read ( _T ( "DialogPosX" ), 20L ), pConf->Read ( _T (
  //     "DialogPosY" ), 20L ));
  wxPoint p = GetPosition();
  pConf->Read(_T ( "DialogX" ), &p.x, p.x);
  pConf->Read(_T ( "DialogY" ), &p.y, p.y);
  SetPosition(p);

  wxSize s = GetSize();
  pConf->Read(_T ( "DialogWidth" ), &s.x, s.x);
  pConf->Read(_T ( "DialogHeight" ), &s.y, s.y);
  SetSize(s);

  // create a image list for the list with just the eye icon
  wxImageList* imglist = new wxImageList(20, 20, true, 1);
  imglist->Add(wxBitmap(eye));
  m_lSights->AssignImageList(imglist, wxIMAGE_LIST_SMALL);

  m_lSights->InsertColumn(rmVISIBLE, wxT(""));
  m_lSights->SetColumnWidth(0, 28);

  for (int i = 1; i < rmMAX; i++) {
    m_lSights->InsertColumn(i, columns[i]);
    m_lSights->SetColumnWidth(i, wxLIST_AUTOSIZE_USEHEADER);
  }

  m_sights_path = celestial_navigation_pi::StandardPath() + _T("Sights.xml");

  m_sortCol = rmTIME;
  m_bSortAsc = false;

  if (!OpenXML(false)) {
    /* create directory for plugin files if it doesn't already exist */
    wxFileName fn(m_sights_path);
    wxFileName fn2 = fn.GetPath();
    if (!fn.DirExists()) {
      fn2.Mkdir();
      fn.Mkdir();
    }
  }

  // calculate scaler for minimum line width
  double mmx = PlugInGetDisplaySizeMM();
  int sx, sy;
  wxDisplaySize(&sx, &sy);
  m_pix_per_mm = ((double)sx) / (mmx);

//
#if 0  // TODO  (DSR) This Android GUI interface needs work
#ifdef __OCPN__ANDROID__
    GetHandle()->setAttribute(Qt::WA_AcceptTouchEvents);
    GetHandle()->grabGesture(Qt::PanGesture);
    GetHandle()->setStyleSheet( qtStyleSheet);
    m_lSights->GetHandle()->setAttribute(Qt::WA_AcceptTouchEvents);//
    m_lSights->GetHandle()->grabGesture(Qt::PanGesture);
    m_lSights->Connect( wxEVT_QT_PANGESTURE,
                       (wxObjectEventFunction) (wxEventFunction) &CelestialNavigationDialog::OnEvtPanGesture, NULL, this );
    GetHandle()->setStyleSheet( qtStyleSheet);//
   Move(0, 0);
#endif
#endif  // if 0

  //
}

#if 0  // TODO  (DSR) This Android GUI interface needs work

#ifdef __OCPN__ANDROID__
void CelestialNavigationDialog::OnEvtPanGesture( wxQT_PanGestureEvent &event)
{
    switch(event.GetState()){
        case GestureStarted:
            m_startPos = GetPosition();
            m_startMouse = event.GetCursorPos(); //g_mouse_pos_screen;
            break;
        default:
        {
            wxPoint pos = event.GetCursorPos();
            int x = wxMax(0, pos.x + m_startPos.x - m_startMouse.x);
            int y = wxMax(0, pos.y + m_startPos.y - m_startMouse.y);
            int xmax = ::wxGetDisplaySize().x - GetSize().x;
            x = wxMin(x, xmax);
            int ymax = ::wxGetDisplaySize().y - GetSize().y;          // Some fluff at the bottom
            y = wxMin(y, ymax);

            Move(x, y);
        } break;
    }
// master
}
#endif
#endif  // if 0

CelestialNavigationDialog::~CelestialNavigationDialog() {
  wxFileConfig* pConf = GetOCPNConfigObject();
  pConf->SetPath(_T("/PlugIns/CelestialNavigation"));

  wxPoint p = GetPosition();
  pConf->Write(_T ( "DialogX" ), p.x);
  pConf->Write(_T ( "DialogY" ), p.y);

  wxSize s = GetSize();
  pConf->Write(_T ( "DialogWidth" ), s.x);
  pConf->Write(_T ( "DialogHeight" ), s.y);

  SaveXML();
}

#define FAIL(X)  \
  do {           \
    error = X;   \
    goto failed; \
  } while (0)
double AttributeDouble(TiXmlElement* e, const char* name, double def) {
  const char* attr = e->Attribute(name);
  if (!attr) return def;
  char* end;
  double d = strtod(attr, &end);
  if (end == attr) return def;
  return d;
}

int AttributeInt(TiXmlElement* e, const char* name, int def) {
  const char* attr = e->Attribute(name);
  if (!attr) return def;
  char* end;
  long d = strtol(attr, &end, 10);
  if (end == attr) return def;
  return d;
}

bool AttributeBool(TiXmlElement* e, const char* name, bool def) {
  return AttributeInt(e, name, def) != 0;
}

bool CelestialNavigationDialog::OpenXML(bool reportfailure) {
  TiXmlDocument doc;
  wxString error;

  wxFileName fn(m_sights_path);

  if (!doc.LoadFile(m_sights_path.mb_str()))
    FAIL(_("Failed to load file: ") + m_sights_path);
  else {
    TiXmlHandle root(doc.RootElement());

    if (strcmp(root.Element()->Value(), "OpenCPNCelestialNavigation"))
      FAIL(_("Invalid xml file"));

    m_Sights.clear();

    for (TiXmlElement* e = root.FirstChild().Element(); e;
         e = e->NextSiblingElement()) {
      if (!strcmp(e->Value(), "ClockError")) {
        m_ClockCorrectionDialog.m_sClockCorrection->SetValue(
            AttributeInt(e, "Seconds", 0));
      } else if (!strcmp(e->Value(), "Sight")) {
        Sight s;

        s.m_bVisible = AttributeBool(e, "Visible", true);
        s.m_Type = (Sight::Type)AttributeInt(e, "Type", 0);
        s.m_Body = wxString::FromUTF8(e->Attribute("Body"));
        s.m_BodyLimb = (Sight::BodyLimb)AttributeInt(e, "BodyLimb", 0);

        s.m_DateTime.ParseISODate(wxString::FromUTF8(e->Attribute("Date")));

        wxDateTime time;
        time.ParseISOTime(wxString::FromUTF8(e->Attribute("Time")));

        if (s.m_DateTime.IsValid() && time.IsValid()) {
          s.m_DateTime.SetHour(time.GetHour());
          s.m_DateTime.SetMinute(time.GetMinute());
          s.m_DateTime.SetSecond(time.GetSecond());
        } else
          continue; /* skip if invalid */

        s.m_TimeCertainty = AttributeDouble(e, "TimeCertainty", 0);

        s.m_Measurement = AttributeDouble(e, "Measurement", 0);
        s.m_MeasurementCertainty =
            AttributeDouble(e, "MeasurementCertainty", .25);

        s.m_EyeHeight = AttributeDouble(e, "EyeHeight", 2);
        s.m_Temperature = AttributeDouble(e, "Temperature", 10);
        s.m_Pressure = AttributeDouble(e, "Pressure", 1010);
        s.m_IndexError = AttributeDouble(e, "IndexError", 0);

        s.m_ShiftNm = AttributeDouble(e, "ShiftNm", 0);
        s.m_ShiftBearing = AttributeDouble(e, "ShiftBearing", 0);
        s.m_bMagneticShiftBearing = AttributeBool(e, "MagneticShiftBearing", 0);

        s.m_ColourName = wxString::FromUTF8(e->Attribute("ColourName"));
        s.m_Colour = wxColour(wxString::FromUTF8(e->Attribute("Colour")));
        s.m_Colour.Set(s.m_Colour.Red(), s.m_Colour.Green(), s.m_Colour.Blue(),
                       AttributeInt(e, "Transparency", 150));
        s.m_bCalculated = false;

        if (s.m_bVisible) {
          s.Recompute(m_ClockCorrectionDialog.m_sClockCorrection->GetValue());
          s.RebuildPolygons();
        }
        m_Sights.push_back(std::move(s));
      } else
        FAIL(_("Unrecognized xml node"));
    }
  }

  RebuildList();
  if (m_lSights->GetItemCount() > 0) {
    m_Sights[0].SetSelected(true);
    m_lSights->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
  }
  RequestRefresh(GetParent());
  return true;
failed:

  if (reportfailure) {
    wxMessageDialog mdlg(this, error, _("Celestial Navigation"),
                         wxOK | wxICON_ERROR);
    mdlg.ShowModal();
  }
  return false;
}

void CelestialNavigationDialog::SaveXML() {
  TiXmlDocument doc;
  TiXmlDeclaration* decl = new TiXmlDeclaration("1.0", "utf-8", "");
  doc.LinkEndChild(decl);

  TiXmlElement* root = new TiXmlElement("OpenCPNCelestialNavigation");
  doc.LinkEndChild(root);

  char version[24];
  sprintf(version, "%d.%d", PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR);
  root->SetAttribute("version", version);
  root->SetAttribute("creator", "Opencpn Celestial Navigation plugin");

  TiXmlElement* c = new TiXmlElement("ClockError");
  c->SetAttribute("Seconds",
                  m_ClockCorrectionDialog.m_sClockCorrection->GetValue());
  root->LinkEndChild(c);

  for (Sight& s : m_Sights) {
    TiXmlElement* c = new TiXmlElement("Sight");

    c->SetAttribute("Visible", s.m_bVisible);
    c->SetAttribute("Type", s.m_Type);
    c->SetAttribute("Body", s.m_Body.mb_str());
    c->SetAttribute("BodyLimb", s.m_BodyLimb);

    c->SetAttribute("Date", s.m_DateTime.FormatISODate().mb_str());
    c->SetAttribute("Time", s.m_DateTime.FormatISOTime().mb_str());

    c->SetDoubleAttribute("TimeCertainty", s.m_TimeCertainty);

    c->SetDoubleAttribute("Measurement", s.m_Measurement);
    c->SetDoubleAttribute("MeasurementCertainty", s.m_MeasurementCertainty);

    c->SetDoubleAttribute("EyeHeight", s.m_EyeHeight);
    c->SetDoubleAttribute("Temperature", s.m_Temperature);
    c->SetDoubleAttribute("Pressure", s.m_Pressure);
    c->SetDoubleAttribute("IndexError", s.m_IndexError);

    c->SetDoubleAttribute("ShiftNm", s.m_ShiftNm);
    c->SetDoubleAttribute("ShiftBearing", s.m_ShiftBearing);
    c->SetDoubleAttribute("MagneticShiftBearing", s.m_bMagneticShiftBearing);

    c->SetAttribute("ColourName", s.m_ColourName.mb_str());
    c->SetAttribute("Colour", s.m_Colour.GetAsString().mb_str());
    c->SetAttribute("Transparency", s.m_Colour.Alpha());

    root->LinkEndChild(c);
  }

  if (!doc.SaveFile(m_sights_path.mb_str())) {
    wxMessageDialog mdlg(this, _("Failed to save xml file: ") + m_sights_path,
                         _("Celestial Navigation"), wxOK | wxICON_ERROR);
    mdlg.ShowModal();
  }
}

bool compareSightAsc(Sight a, Sight b, int sortCol) {
  switch (sortCol) {
    case rmVISIBLE:
      if (a.m_bVisible != b.m_bVisible) return a.m_bVisible < b.m_bVisible;
      break;
    case rmTYPE:
      if (a.m_Type != b.m_Type) return a.m_Type < b.m_Type;
      break;
    case rmBODY:
      if (a.m_Body != b.m_Body) return a.m_Body < b.m_Body;
      break;
    default:
    case rmTIME:
      if (a.m_DateTime != b.m_DateTime) return a.m_DateTime < b.m_DateTime;
      break;
    case rmMEASUREMENT:
      if (a.m_Measurement != b.m_Measurement)
        return a.m_Measurement < b.m_Measurement;
      break;
    case rmCOLOR:
      if (a.m_Colour.GetAsString() != b.m_Colour.GetAsString())
        return a.m_Colour.GetAsString() < b.m_Colour.GetAsString();
      break;
  }

  if (a.m_bVisible != b.m_bVisible) return a.m_bVisible < b.m_bVisible;
  if (a.m_Type != b.m_Type) return a.m_Type < b.m_Type;
  if (a.m_Body != b.m_Body) return a.m_Body < b.m_Body;
  if (a.m_DateTime != b.m_DateTime) return a.m_DateTime < b.m_DateTime;
  if (a.m_Measurement != b.m_Measurement)
    return a.m_Measurement < b.m_Measurement;
  if (a.m_Colour.GetAsString() != b.m_Colour.GetAsString())
    return a.m_Colour.GetAsString() < b.m_Colour.GetAsString();

  return true;
}

bool compareSight(Sight a, Sight b, int sortCol, bool sortAsc) {
  return sortAsc ? compareSightAsc(a, b, sortCol)
                 : !compareSightAsc(a, b, sortCol);
}

void CelestialNavigationDialog::RebuildList() {
  using namespace std::placeholders;
  std::stable_sort(m_Sights.begin(), m_Sights.end(),
                   std::bind(compareSight, _1, _2, m_sortCol, m_bSortAsc));

  wxListItem item;
  item.SetMask(wxLIST_MASK_TEXT);
  for (int i = 0; i < rmMAX; i++) {
    m_lSights->GetColumn(i, item);
    item.SetText(columns[i]);
    m_lSights->SetColumn(i, item);
  }
  m_lSights->GetColumn(m_sortCol, item);
  item.SetText(columns[m_sortCol] + (m_bSortAsc ? _T(" ↓") : _T(" ↑")));
  m_lSights->SetColumn(m_sortCol, item);

  m_lSights->DeleteAllItems();
  for (Sight& s : m_Sights) {
    wxListItem item;
    item.SetId(m_lSights->GetItemCount());
    item.SetMask(item.GetMask() | wxLIST_MASK_TEXT);
    int idx = m_lSights->InsertItem(item);
    m_lSights->SetItemImage(idx, s.IsVisible() ? 0 : -1);
    m_lSights->SetItem(idx, rmTYPE, SightType[s.m_Type]);
    m_lSights->SetItem(idx, rmBODY, s.m_Body);
    wxDateTime dt = s.m_DateTime;
    m_lSights->SetItem(idx, rmTIME,
                       dt.FormatISODate() + _T(" ") + dt.FormatISOTime());
    m_lSights->SetItem(idx, rmMEASUREMENT,
                       wxString::Format(_T("%.4f"), s.m_Measurement));
    if (s.m_Type == Sight::LUNAR)
      m_lSights->SetItem(
          idx, rmCOLOR,
          _("Time Correction") +
              wxString::Format(_T(": %.4f"), s.m_TimeCorrection));
    else
      m_lSights->SetItem(idx, rmCOLOR, s.m_ColourName);

    if (s.IsSelected())
      m_lSights->SetItemState(idx, wxLIST_STATE_SELECTED,
                              wxLIST_STATE_SELECTED);
  }

  UpdateButtons();
  UpdateFix(true);
  SaveXML();
}

void CelestialNavigationDialog::UpdateSight(int idx, bool warnings) {
  Sight& s = m_Sights[idx];

  // then add sights to the listctrl
  m_lSights->SetItem(idx, rmTYPE, SightType[s.m_Type]);
  m_lSights->SetItem(idx, rmBODY, s.m_Body);
  wxDateTime dt = s.m_DateTime;
  m_lSights->SetItem(idx, rmTIME,
                     dt.FormatISODate() + _T(" ") + dt.FormatISOTime());
  m_lSights->SetItem(idx, rmMEASUREMENT,
                     wxString::Format(_T("%.4f"), s.m_Measurement));
  if (s.m_Type == Sight::LUNAR)
    m_lSights->SetItem(idx, rmCOLOR,
                       _("Time Correction") +
                           wxString::Format(_T(": %.4f"), s.m_TimeCorrection));
  else
    m_lSights->SetItem(idx, rmCOLOR, s.m_ColourName);

  UpdateButtons();
  UpdateFix(warnings);
  SaveXML();
}

void CelestialNavigationDialog::UpdateSights() {
  for (int i = 0; i < m_lSights->GetItemCount(); i++) UpdateSight(i);
}

void CelestialNavigationDialog::UpdateButtons() {
  // enable/disable buttons
  long selectedIndex =
      m_lSights->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  bool enable = !(selectedIndex < 0);

  m_bEditSight->Enable(enable);
  m_bDeleteSight->Enable(enable);
  m_bDeleteAllSights->Enable(m_lSights->GetItemCount() > 0);
  m_bDeleteSight->Enable(enable);
}

void CelestialNavigationDialog::UpdateFix(bool warnings) {
  m_FixDialog.Update(m_ClockCorrectionDialog.m_sClockCorrection->GetValue(),
                     warnings);
}

void CelestialNavigationDialog::OnNew(wxCommandEvent& event) {
  wxDateTime now = wxDateTime::Now().ToUTC();

  Sight ns(Sight::ALTITUDE, _("Sun"), Sight::LOWER, now, 0, 0, 10);
  SightDialog dialog(this, ns,
                     m_ClockCorrectionDialog.m_sClockCorrection->GetValue());

  if (dialog.ShowModal() == wxID_OK) {
    if (ns.m_bVisible) {
      dialog.Recompute();
      ns.RebuildPolygons();
    }
    ns.SetSelected(true);
    for (Sight& s : m_Sights) s.SetSelected(false);
    m_Sights.push_back(std::move(ns));
    RebuildList();
    RequestRefresh(GetParent());
  }
}

void CelestialNavigationDialog::OnDuplicate(wxCommandEvent& event) {
  long selectedIndex =
      m_lSights->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (selectedIndex < 0) return;

  Sight& s = m_Sights[selectedIndex];
  s.SetSelected(false);
  Sight ns(s);
  ns.SetSelected(true);
  if (ns.m_bVisible) {
    ns.RebuildPolygons();
  }
  m_Sights.push_back(std::move(ns));
  RebuildList();
  RequestRefresh(GetParent());
}

void CelestialNavigationDialog::OnEdit() {
  // Manipulate selectedIndex sight/track
  long selectedIndex =
      m_lSights->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (selectedIndex < 0) return;

  Sight& s = m_Sights[selectedIndex];
  Sight originalsight = s; /* in case of cancel */

  SightDialog dialog(this, s,
                     m_ClockCorrectionDialog.m_sClockCorrection->GetValue());

  if (dialog.ShowModal() == wxID_OK) {
    if (s.m_bVisible) {
      dialog.Recompute();
      s.RebuildPolygons();
    }
    UpdateSight(selectedIndex);
    RebuildList();
  } else
    m_Sights[selectedIndex] = originalsight;

  RequestRefresh(GetParent());
}

void CelestialNavigationDialog::OnDelete(wxCommandEvent& event) {
  // Delete selectedIndex sight/track
  long selectedIndex =
      m_lSights->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (selectedIndex < 0) return;

  m_lSights->DeleteItem(selectedIndex);
  m_Sights.erase(m_Sights.begin() + selectedIndex);
  if ((selectedIndex >= m_lSights->GetItemCount()) && (selectedIndex > 0))
    selectedIndex--;
  if (m_lSights->GetItemCount() > 0) {
    m_Sights[selectedIndex].SetSelected(true);
    m_lSights->SetItemState(selectedIndex, wxLIST_STATE_SELECTED,
                            wxLIST_STATE_SELECTED);
  }
  SaveXML();
  RequestRefresh(GetParent());
}

void CelestialNavigationDialog::OnDeleteAll(wxCommandEvent& event) {
  wxMessageDialog mdlg(this, _("Are you sure you want to delete all sights?"),
                       _("Celestial Navigation"), wxYES_NO);
  if (mdlg.ShowModal() == wxID_YES) {
    m_lSights->DeleteAllItems();
    m_Sights.clear();
    SaveXML();
    RequestRefresh(GetParent());
  }
}

void CelestialNavigationDialog::OnFix(wxCommandEvent& event) {
  m_FixDialog.Show();
  RequestRefresh(GetParent());
}

void CelestialNavigationDialog::OnDRShift(wxCommandEvent& event) {
#if 0
    DRShiftDialog dialog;
    if(dialog.ShowModel() == wxID_OK) {
        double shiftnm, shiftbearing;
        dialog.m_tShiftNm->GetValue().ToDouble(&shiftnm);
        dialog.m_tShiftBearing->GetValue().ToDouble(&shiftbearing);
        bool MagneticShiftBearing = dialog.m_cbMagneticShiftBearing->GetValue();

        for (std::list<Sight*>::iterator it = m_SightList.begin(); it != m_SightList.end(); it++) {
            Sight *s = *it;
            if(!s->IsVisible())
                continue;

            if(s->m_bMagneticShiftBearing != MagneticShiftBearing
        }
    }
#endif
}

void CelestialNavigationDialog::OnClockOffset(wxCommandEvent& event) {
  m_ClockCorrectionDialog.Show();
}

void CelestialNavigationDialog::OnInformation(wxCommandEvent& event) {
  wxString infolocation = celestial_navigation_pi_DataDir() + _T("/data/") +
                          _T("Celestial_Navigation_Information.html");
  infolocation.Prepend(_T("file://"));
  infolocation.Replace(_T(" "), _T("%20"));
  wxLaunchDefaultBrowser(infolocation);
}

void CelestialNavigationDialog::OnHide(wxCommandEvent& event) {
  if (m_tbHide->GetValue()) {
    m_tbHide->SetLabel(_("Show"));
    m_fullSize = GetSize();
    m_lSights->Hide();
    Layout();
    Fit();
  } else {
    m_tbHide->SetLabel(_("Hide"));
    m_lSights->Show();
    Layout();
    Fit();
    SetSize(m_fullSize);
  }
}

void CelestialNavigationDialog::OnSightListLeftDown(wxMouseEvent& event) {
  wxPoint pos = event.GetPosition();
  int flags = 0;
  long clicked_index = m_lSights->HitTest(pos, flags);

  //    Clicking Visibility column?
  if (clicked_index > -1 && event.GetX() < m_lSights->GetColumnWidth(0)) {
    // Process the clicked item
    Sight& sight = m_Sights[clicked_index];
    sight.SetVisible(!sight.IsVisible());
    m_lSights->SetItemImage(clicked_index, sight.IsVisible() ? 0 : -1);

    if (sight.IsVisible() && !sight.IsCalculated()) {
      sight.Recompute(m_ClockCorrectionDialog.m_sClockCorrection->GetValue());
      sight.RebuildPolygons();
    }

    UpdateFix();
    SaveXML();
    RequestRefresh(GetParent());
  }

  // Allow wx to process...
  event.Skip();
}

void CelestialNavigationDialog::OnSightSelected(wxListEvent& event) {
  long selectedIndex =
      m_lSights->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  for (Sight& s : m_Sights) s.SetSelected(false);
  m_Sights[selectedIndex].SetSelected(true);
  UpdateButtons();
}

void CelestialNavigationDialog::OnColumnHeaderClick(wxListEvent& event) {
  if (m_sortCol != event.m_col)
    m_bSortAsc = false;
  else
    m_bSortAsc = !m_bSortAsc;
  m_sortCol = event.m_col;
  RebuildList();
}
