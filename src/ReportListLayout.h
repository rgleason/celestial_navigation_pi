#ifndef CELESTIAL_REPORT_LIST_LAYOUT_H
#define CELESTIAL_REPORT_LIST_LAYOUT_H

#ifndef __OCPN__ANDROID__
#include <wx/event.h>
#include <wx/string.h>
#include <vector>

class wxListCtrl;
class wxListEvent;

// Native report lists need widths measured with their current font. Keep
// manually resized columns unchanged during refresh and across reopening.
class ReportListLayout : public wxEvtHandler {
public:
  ReportListLayout(wxListCtrl* list, const wxString& key, int firstColumn = 0);
  ~ReportListLayout() override;
  void FitContents(const std::vector<wxString>& samples = {});
  void Save() const;

private:
  void OnColumnResized(wxListEvent& event);
  wxListCtrl* m_list;
  wxString m_path;
  int m_firstColumn;
  std::vector<bool> m_manualWidths;
};
#endif
#endif
