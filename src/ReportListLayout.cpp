#include "ReportListLayout.h"
#include "WxDpiCompat.h"

#ifndef __OCPN__ANDROID__
#include <algorithm>
#include <wx/fileconf.h>
#include <wx/listctrl.h>
#include "OcpnApiCompat.h"

namespace {
wxString WidthKey(int column) {
  return wxString::Format("Column%dWidthDIP", column);
}
}  // namespace

ReportListLayout::ReportListLayout(wxListCtrl* list, const wxString& key,
                                   int firstColumn)
    : m_list(list),
      m_path("/PlugIns/CelestialNavigation/ListColumns/" + key),
      m_firstColumn(firstColumn),
      m_manualWidths(list->GetColumnCount(), false) {
  if (auto* config = GetOCPNConfigObject()) {
    wxConfigPathChanger path(config, m_path + "/");
    for (int column = m_firstColumn; column < m_list->GetColumnCount(); ++column) {
      long width = 0;
      if (config->Read(WidthKey(column), &width) && width >= 16 && width <= 4096) {
        m_list->SetColumnWidth(column, celestial_ui::FromDIP(m_list, static_cast<int>(width)));
        m_manualWidths[column] = true;
      }
    }
  }
  m_list->Bind(wxEVT_LIST_COL_END_DRAG, &ReportListLayout::OnColumnResized, this);
}

ReportListLayout::~ReportListLayout() {
  Save();
  m_list->Unbind(wxEVT_LIST_COL_END_DRAG, &ReportListLayout::OnColumnResized, this);
}

void ReportListLayout::OnColumnResized(wxListEvent& event) {
  const int column = event.GetColumn();
  if (column >= m_firstColumn && column < static_cast<int>(m_manualWidths.size()))
    m_manualWidths[column] = true;
  event.Skip();
}

void ReportListLayout::FitContents(const std::vector<wxString>& samples) {
  const int padding = celestial_ui::FromDIP(m_list, 24);  // cell edges and sort indicator
  for (int column = m_firstColumn; column < m_list->GetColumnCount(); ++column) {
    if (m_manualWidths[column]) continue;
    wxListItem header;
    m_list->GetColumn(column, header);
    int minimum = m_list->GetTextExtent(header.GetText()).x;
    if (column < static_cast<int>(samples.size()))
      minimum = std::max(minimum, m_list->GetTextExtent(samples[column]).x);
    int content = minimum;
    for (long row = 0; row < m_list->GetItemCount(); ++row)
      content = std::max(content,
                        m_list->GetTextExtent(m_list->GetItemText(row, column)).x);
    // Long remarks can be expanded manually without giving every initial
    // report an enormous horizontal scroll range.
    const int width = std::max(minimum + padding,
        std::min(content + padding, celestial_ui::FromDIP(m_list, 600)));
    m_list->SetColumnWidth(column, width);
  }
}

void ReportListLayout::Save() const {
  if (auto* config = GetOCPNConfigObject()) {
    wxConfigPathChanger path(config, m_path + "/");
    for (int column = m_firstColumn; column < m_list->GetColumnCount(); ++column)
      if (m_manualWidths[column])
        config->Write(WidthKey(column), celestial_ui::ToDIP(m_list, m_list->GetColumnWidth(column)));
  }
}
#endif
