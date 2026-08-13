#ifndef CELESTIAL_NAVIGATION_SIGHT_ANALYSIS_DIALOG_H
#define CELESTIAL_NAVIGATION_SIGHT_ANALYSIS_DIALOG_H

#include <wx/dialog.h>

class CelestialNavigationDialog;
class wxCheckBox;
class wxListCtrl;
class wxSpinCtrlDouble;
class wxStaticText;
class ResidualPlotPanel;

class SightAnalysisDialog : public wxDialog {
public:
  explicit SightAnalysisDialog(CelestialNavigationDialog* parent);

private:
  void Analyze(wxCommandEvent& event);

  CelestialNavigationDialog* m_parent;
  wxCheckBox* m_onlySelectedBody;
  wxCheckBox* m_moving;
  wxSpinCtrlDouble* m_latitude;
  wxSpinCtrlDouble* m_longitude;
  wxSpinCtrlDouble* m_course;
  wxSpinCtrlDouble* m_speed;
  wxStaticText* m_summary;
  ResidualPlotPanel* m_plot;
  wxListCtrl* m_results;
};

#endif
