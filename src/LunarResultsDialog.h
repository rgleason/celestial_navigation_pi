#pragma once

#include "LunarDistanceEngine.h"
#include <wx/dialog.h>

class Sight;
class wxListCtrl;
class wxStaticText;
class wxTextCtrl;
class wxButton;
class wxNotebook;
class wxScrolledWindow;
class wxPanel;
class NavigationAngleCtrl;

class LunarResultsDialog : public wxDialog {
public:
  LunarResultsDialog(wxWindow* parent, Sight& sight);
  ~LunarResultsDialog() override;

private:
  void UpdateResults();
  void SelectCandidate(long index);
  void UpdatePositions();
  void UpdateReferenceCheck();
  void UpdateCalculations();
  void Relayout();
  wxString ReportText() const;
  void ApplySelectedWatchOffset(wxCommandEvent& event);

  Sight& m_sight;
  long m_selected = -1;
  bool m_updating = false;
  bool m_referenceEdited = false;
  lunar_distance::PositionResult m_enteredPositions;
  wxString m_checkReport;
  wxString m_summaryText;
  wxStaticText* m_summary;
  wxStaticText* m_status;
  wxStaticText* m_dr;
  wxListCtrl* m_candidates = nullptr;
  wxListCtrl* m_positions = nullptr;
  wxTextCtrl* m_details;
  wxButton* m_applyOffset;
  wxStaticText* m_geometry;
  NavigationAngleCtrl* m_knownLatitude;
  NavigationAngleCtrl* m_knownLongitude;
  wxStaticText* m_referenceStatus;
  wxStaticText* m_trueDistance;
  wxStaticText* m_clearedDistance;
  wxStaticText* m_lunarError;
  wxNotebook* m_pages;
  wxNotebook* m_calculationPages;
  wxScrolledWindow* m_resultsPage;
  wxScrolledWindow* m_overview;
  wxScrolledWindow* m_almanac;
  wxScrolledWindow* m_search;
#ifdef __OCPN__ANDROID__
  void RefreshAndroidCards();
  wxPanel* m_androidCandidates = nullptr;
  wxPanel* m_androidPositions = nullptr;
  bool m_androidRefreshPending = false;
#endif
};
