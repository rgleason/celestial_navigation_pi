#ifndef CELESTIAL_NAVIGATION_ECLIPSE_DIALOG_H
#define CELESTIAL_NAVIGATION_ECLIPSE_DIALOG_H

#include <wx/dialog.h>

#include "eclipse/engine.h"

class piDC;
class PlugIn_ViewPort;
class wxButton;
class wxCheckBox;
class wxCloseEvent;
class wxListCtrl;
class wxListEvent;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;
class celestial_navigation_pi;

class EclipseDialog : public wxDialog {
public:
  EclipseDialog(wxWindow* parent, celestial_navigation_pi* plugin);
  bool Render(piDC* dc, PlugIn_ViewPort* viewport);
  bool HasPlot() const { return !m_path.empty() || !m_contours.empty(); }
  void RunIntegrationScenario2027();

private:
  void BuildInterface();
  void UpdateDataStatus();
  bool OpenEngine(bool report_error);
  void OnImportDe440(wxCommandEvent& event);
  void OnImportPck(wxCommandEvent& event);
  void OnImportLola(wxCommandEvent& event);
  void OnFind(wxCommandEvent& event);
  void OnSelection(wxListEvent& event);
  void OnPlot(wxCommandEvent& event);
  void OnClear(wxCommandEvent& event);
  void OnBoatPosition(wxCommandEvent& event);
  void OnLocal(wxCommandEvent& event);
  void OnCloseButton(wxCommandEvent& event);
  void OnWindowClose(wxCloseEvent& event);
  bool SelectedEvent(eclipse::EclipseEvent* event) const;
  bool Observer(eclipse::GeoPoint* observer) const;
  wxString DataDirectory() const;
  wxString De440Path() const;
  wxString PckPath() const;
  wxString LolaPath() const;
  bool ImportFile(const wxString& title, const wxString& destination, int kind);

  celestial_navigation_pi* m_plugin;
  eclipse::EclipseEngine m_engine;
  bool m_engine_ready;
  std::vector<eclipse::EclipseEvent> m_events;
  std::vector<eclipse::PathPoint> m_path;
  std::vector<eclipse::MagnitudeContour> m_contours;
  double m_plotted_delta_t;

  wxStaticText* m_data_status;
  wxSpinCtrl* m_start_year;
  wxSpinCtrl* m_end_year;
  wxListCtrl* m_event_list;
  wxCheckBox* m_plot_path;
  wxCheckBox* m_plot_contours;
  wxTextCtrl* m_latitude;
  wxTextCtrl* m_longitude;
  wxCheckBox* m_use_lola;
  wxTextCtrl* m_local_results;
  wxButton* m_plot_button;
  wxButton* m_local_button;
};

#endif
