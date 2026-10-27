#ifndef CELESTIAL_NAVIGATION_PLANNER_BODY_SELECTION_DIALOG_H
#define CELESTIAL_NAVIGATION_PLANNER_BODY_SELECTION_DIALOG_H

#include <deque>
#include <wx/dialog.h>
#include <vector>

class wxCheckBox;
class wxTextCtrl;
class wxSearchCtrl;
class wxScrolledWindow;
class wxStaticBoxSizer;
class wxStaticText;

class PlannerBodySelectionDialog : public wxDialog {
public:
  PlannerBodySelectionDialog(wxWindow* parent,
                             const std::deque<wxString>& selected);
  std::deque<wxString> Selection() const;

private:
  void UpdateSearch();
  void UpdateCount();
  void SelectAll(bool checked);
  void Reflow();
#ifdef __OCPN__ANDROID__
  wxTextCtrl* m_search;
#else
  wxSearchCtrl* m_search;
#endif
  wxScrolledWindow* m_list;
  wxStaticText* m_count;
  wxStaticText* m_note;
  wxStaticText* m_lunarNote;
  bool m_reflowing = false;
  struct Entry { wxString body; wxCheckBox* checkbox; unsigned group; };
  std::vector<Entry> m_entries;
  std::vector<wxStaticBoxSizer*> m_groups;
};

#endif
