#ifndef CELESTIAL_NAVIGATION_LUNAR_RESULTS_DIALOG_H
#define CELESTIAL_NAVIGATION_LUNAR_RESULTS_DIALOG_H

#include <wx/dialog.h>

class Sight;
class wxListCtrl;
class wxStaticText;
class wxTextCtrl;
class wxButton;

class LunarResultsDialog : public wxDialog {
public:
  LunarResultsDialog(wxWindow* parent, Sight& sight);
  ~LunarResultsDialog() override = default;

private:
  void UpdateResults();
  void UpdatePositions(long candidate_index);
  void ApplySelectedWatchOffset(wxCommandEvent& event);

  Sight& m_sight;
  wxStaticText* m_status;
  wxListCtrl* m_candidates;
  wxListCtrl* m_positions;
  wxTextCtrl* m_details;
  wxButton* m_applyOffset;
};

#endif
