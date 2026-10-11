#ifndef CELESTIAL_NAVIGATION_SIGHT_ANALYSIS_DIALOG_H
#define CELESTIAL_NAVIGATION_SIGHT_ANALYSIS_DIALOG_H

#include <wx/dialog.h>
#include <memory>

class CelestialNavigationDialog;
class wxCheckBox;
class wxListCtrl;
class NavigationAngleCtrl;
class wxSpinCtrlDouble;
class wxStaticText;
class ResidualPlotPanel;
class ReportListLayout;

class SightAnalysisDialog : public wxDialog {
public:
  explicit SightAnalysisDialog(CelestialNavigationDialog* parent);
  ~SightAnalysisDialog() override;

private:
  void Analyze(wxCommandEvent& event);

  CelestialNavigationDialog* m_parent;
  wxCheckBox* m_onlySelectedBody;
  wxCheckBox* m_moving;
  NavigationAngleCtrl* m_latitude;
  NavigationAngleCtrl* m_longitude;
  wxSpinCtrlDouble* m_course;
  wxSpinCtrlDouble* m_speed;
  wxStaticText* m_summary;
  ResidualPlotPanel* m_plot;
  wxListCtrl* m_results;
#ifndef __OCPN__ANDROID__
  std::unique_ptr<ReportListLayout> m_columns;
#endif
#ifdef __OCPN__ANDROID__
  wxStaticText* m_androidResults;
#endif
};

#endif
