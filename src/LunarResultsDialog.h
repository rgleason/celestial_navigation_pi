#ifndef CELESTIAL_NAVIGATION_LUNAR_RESULTS_DIALOG_H
#define CELESTIAL_NAVIGATION_LUNAR_RESULTS_DIALOG_H

#include <wx/dialog.h>

class Sight;
class wxListCtrl;
class wxStaticText;
class wxTextCtrl;
class wxButton;
class wxChoice;
class wxPanel;
class NavigationAngleCtrl;
#ifdef __OCPN__ANDROID__
class wxPanel;
#endif

class LunarResultsDialog : public wxDialog {
public:
  LunarResultsDialog(wxWindow* parent, Sight& sight);
  ~LunarResultsDialog() override;

private:
  void UpdateResults();
  void UpdatePositions(long candidate_index);
  void ApplySelectedWatchOffset(wxCommandEvent& event);

  Sight& m_sight;
  wxStaticText* m_status;
  wxStaticText* m_dr;
  wxListCtrl* m_candidates;
  wxListCtrl* m_positions;
  wxTextCtrl* m_details;
  wxButton* m_applyOffset;
  wxChoice* m_mode;
  wxStaticText* m_geometry;
  wxPanel* m_accuracyPanel;
  NavigationAngleCtrl* m_knownLatitude;
  NavigationAngleCtrl* m_knownLongitude;
  wxStaticText* m_distanceCheck;
#ifdef __OCPN__ANDROID__
  void RefreshAndroidCards();
  wxPanel* m_androidCandidates = nullptr;
  wxPanel* m_androidPositions = nullptr;
  bool m_androidRefreshPending = false;
  long m_androidSelectedCandidate = -1;
#endif
};

#endif
