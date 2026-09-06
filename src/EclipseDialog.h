#ifndef CELESTIAL_NAVIGATION_ECLIPSE_DIALOG_H
#define CELESTIAL_NAVIGATION_ECLIPSE_DIALOG_H

#include <future>
#include <string>
#include <vector>

#include <wx/dialog.h>
#include <wx/timer.h>

#include "EclipseDataFiles.h"
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
  ~EclipseDialog() override;
  bool Render(piDC* dc, PlugIn_ViewPort* viewport);
  bool HasPlot() const { return !m_path.empty() || !m_contours.empty(); }
  void RunIntegrationScenario2027();

private:
  void BuildInterface();
  void UpdateDataStatus();
  bool OpenEngine(bool report_error);
  void OnImportDe440(wxCommandEvent& event);
  void OnDownloadDe440(wxCommandEvent& event);
  void OnOptionalData(wxCommandEvent& event);
  void OnCancelInstall(wxCommandEvent& event);
  void OnDownloadEvent(wxEvent& event);
  void OnVerificationTimer(wxTimerEvent& event);
  void BeginInstall(celestial_navigation::EclipseDataKind requested);
  void StartNextDownload();
  void TryCurrentSource();
  void BeginVerification(celestial_navigation::EclipseDataKind kind,
                         const wxString& path, int purpose);
  void FinishDownloadedVerification(bool valid, const wxString& error);
  void StartInstalledDataCheck();
  void StartNextInstalledDataCheck();
  void FinishInstallation(bool success, const wxString& message);
  void SetInstallationControls(bool busy);
  bool EnsureInstallationSpace(
      const std::vector<celestial_navigation::EclipseDataKind>& plan);
  void SelectAndImport(celestial_navigation::EclipseDataKind kind);
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
  wxString DataPath(celestial_navigation::EclipseDataKind kind) const;
  bool DataVerified(celestial_navigation::EclipseDataKind kind) const;

  struct VerificationResult {
    bool valid;
    std::string error;
  };

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
  wxButton* m_import_de;
  wxButton* m_download_de;
  wxButton* m_optional_data;
  wxButton* m_cancel_install;

  std::vector<celestial_navigation::EclipseDataKind> m_install_queue;
  std::vector<std::string> m_download_sources;
  std::size_t m_install_index;
  std::size_t m_source_index;
  int m_download_kind;
  long m_download_handle;
  wxString m_download_temp;
  bool m_cancel_requested;

  wxTimer m_verification_timer;
  std::future<VerificationResult> m_verification_future;
  bool m_verifying;
  int m_verification_purpose;
  celestial_navigation::EclipseDataKind m_verification_kind;
  wxString m_verification_path;
  std::vector<celestial_navigation::EclipseDataKind> m_installed_check_queue;
  std::size_t m_installed_check_index;
  bool m_invalid_data[3];
};

#endif
