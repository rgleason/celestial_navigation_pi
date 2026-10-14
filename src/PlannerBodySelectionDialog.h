#ifndef CELESTIAL_NAVIGATION_PLANNER_BODY_SELECTION_DIALOG_H
#define CELESTIAL_NAVIGATION_PLANNER_BODY_SELECTION_DIALOG_H

#include <wx/dialog.h>
#include <vector>

class wxCheckBox;
class wxSearchCtrl;
class wxScrolledWindow;
class wxStaticBoxSizer;
class wxStaticText;

class PlannerBodySelectionDialog : public wxDialog {
public:
  PlannerBodySelectionDialog(wxWindow* parent,
                             const std::vector<wxString>& selected);
  std::vector<wxString> Selection() const;

private:
  void UpdateSearch();
  void UpdateCount();
  void SelectAll(bool checked);
  wxSearchCtrl* m_search;
  wxScrolledWindow* m_list;
  wxStaticText* m_count;
  struct Entry { wxString body; wxCheckBox* checkbox; unsigned group; };
  std::vector<Entry> m_entries;
  std::vector<wxStaticBoxSizer*> m_groups;
};

#endif
