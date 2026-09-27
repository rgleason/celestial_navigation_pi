#include "PlatformMessageBox.h"
#ifdef __OCPN__ANDROID__
#include "CelestialNavigationDialog.h"
#include "AndroidSurface.h"
#include "Sight.h"
#include "celestial_navigation_pi.h"
#include "UtcDateTime.h"
#include "HtmlHelp.h"

void CelestialNavigationDialog::BuildAndroidWorkspace() {
  using namespace celestial_android;
  for (auto* child : GetChildren()) child->Hide();
  auto* root = new wxBoxSizer(wxVERTICAL);
  auto* header = new wxPanel(this, wxID_ANY);
  header->SetBackgroundColour(wxColour(25, 59, 76));
  auto* row = new wxBoxSizer(wxHORIZONTAL);
  auto* title = new wxStaticText(header, wxID_ANY, _("Celestial Navigation"));
  title->GetHandle()->setStyleSheet("QLabel { color: white; font-size: 20pt; }");
  row->Add(title, 1, wxALIGN_CENTER_VERTICAL | wxALL, 12);
  m_markTimeButton->GetContainingSizer()->Detach(m_markTimeButton);
  m_markTimeButton->Reparent(header);
  m_markTimeButton->Show();
  row->Add(m_markTimeButton, 0, wxALL, 8);
  auto* chart = new wxButton(header, wxID_ANY, _("Chart"));
  chart->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_Plugin->OnDialogClose(); });
  row->Add(chart, 0, wxALL, 8);
  header->SetSizer(row);
  root->Add(header, 0, wxEXPAND);
  auto* book = new wxNotebook(this, wxID_ANY);
  book->SetMinSize(wxSize(0, 0));
  auto* nav = new wxPanel(this, wxID_ANY);
  auto* navRow = new wxBoxSizer(wxHORIZONTAL);
  const wxString names[] = {_("Observe"), _("Fix"), _("Plan"), _("Tools")};
  wxScrolledWindow* pages[4];
  wxPanel* contents[4];
  wxBoxSizer* layouts[4];
  for (int i = 0; i < 4; ++i) {
    pages[i] = new wxScrolledWindow(book, wxID_ANY);
    pages[i]->SetScrollRate(0, 1);
    pages[i]->SetMinSize(wxSize(0, 0));
    contents[i] = new wxPanel(pages[i], wxID_ANY);
    layouts[i] = new wxBoxSizer(wxVERTICAL);
    contents[i]->SetSizer(layouts[i]);
    auto* viewport = new wxBoxSizer(wxVERTICAL);
    viewport->Add(contents[i], 0, wxEXPAND | wxALL, 12);
    viewport->AddSpacer(32);
    pages[i]->SetSizer(viewport);
    book->AddPage(pages[i], names[i]);
    auto* button = new wxButton(nav, wxID_ANY, names[i]);
    button->Bind(wxEVT_BUTTON, [book, i](wxCommandEvent&) {
      book->SetSelection(i);
      LayoutScrolls(book);
    });
    navRow->Add(button, 1, wxEXPAND | wxALL, 6);
  }
  nav->SetSizer(navRow);
  if (auto* native = qobject_cast<QTabWidget*>(book->GetHandle())) native->tabBar()->hide();
  root->Add(nav, 0, wxEXPAND);
  root->Add(book, 1, wxEXPAND);
  auto move = [&](wxWindow* control, int page, const wxString& caption) {
    if (control->GetContainingSizer()) control->GetContainingSizer()->Detach(control);
    control->Reparent(contents[page]);
    control->SetLabel(caption);
    control->SetMinSize(wxSize(0, CN_TouchHeight()));
    control->Show();
    layouts[page]->Add(control, 0, wxEXPAND | wxALL, 6);
  };
  move(m_bNewSight, 0, _("New sight"));
  move(m_horizonEventButton, 0, _("Record sunrise / sunset"));
  auto* sort = new wxChoice(contents[0], wxID_ANY);
  for (const wxString& name : {_("Newest first"), _("Oldest first"),
                               _("Body"), _("Type"), _("Measurement")}) sort->Append(name);
  sort->SetSelection(0);
  sort->Bind(wxEVT_CHOICE, [this, sort](wxCommandEvent&) {
    const int columns[] = {3, 3, 2, 1, 4};
    m_sortCol = columns[sort->GetSelection()];
    m_bSortAsc = sort->GetSelection() != 0;
    RebuildList();
  });
  layouts[0]->Add(sort, 0, wxEXPAND | wxALL, 6);
  m_androidObservations = pages[0];
  m_androidCards = new wxPanel(contents[0], wxID_ANY);
  m_androidCards->SetSizer(new wxBoxSizer(wxVERTICAL));
  layouts[0]->Add(m_androidCards, 0, wxEXPAND | wxALL, 6);
  move(m_bEditSight, 0, _("Edit selected sight"));
  move(m_bDuplicateSight, 0, _("Duplicate selected sight"));
  auto* showSight = new wxButton(contents[0], wxID_ANY, _("Show selected sight on chart"));
  showSight->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    const Sight* sight = GetSelectedSight();
    if (!sight) { CelestialMessageBox(_("Select an observation first."), _("Sight chart"), wxOK | wxICON_INFORMATION, this); return; }
    double latitude = sight->m_DRLat, longitude = sight->m_DRLon;
    if (sight->m_DRBoatPosition) celestial_navigation_pi_BoatPos(latitude, longitude);
    JumpToPosition(latitude, longitude, 0.003);
    m_Plugin->OnDialogClose();
    RequestRefresh(GetOCPNCanvasWindow());
  });
  layouts[0]->Add(showSight, 0, wxEXPAND | wxALL, 6);

  m_androidInclude = new wxButton(contents[0], wxID_ANY, _("Include / exclude selected"));
  m_androidInclude->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    const Sight* selected = GetSelectedSight();
    if (!selected) return;
    Sight* sight = &m_Sights[selected - m_Sights.data()];
    sight->SetVisible(!sight->IsVisible());
    if (sight->IsVisible()) {
      sight->Recompute(m_ClockCorrection);
      sight->RebuildPolygons();
    }
    SaveXML();
    UpdateFix();
    RefreshAndroidCards();
    RequestRefresh(GetParent());
  });
  layouts[0]->Add(m_androidInclude, 0, wxEXPAND | wxALL, 6);
  move(m_bDeleteSight, 0, _("Delete selected sight"));
  move(m_bDeleteAllSights, 3, _("Delete all observations"));
  move(m_bFix, 1, _("Calculate celestial fix"));
  move(m_analyzeButton, 1, _("Analyze sight sequence"));
  move(m_plannerButton, 2, _("Sun, Moon and best sights"));
  move(m_almanacButton, 2, _("Generate voyage almanac"));
  move(m_eclipseButton, 2, _("Eclipse search and local circumstances"));
  move(m_lunarToolsButton, 3, _("Lunar sessions, pairs and sextant check"));
  move(m_coastalButton, 3, _("Coastal sextant"));
  move(m_bClockOffset, 3, _("Apply clock correction"));
  auto* guide = new wxButton(contents[3], wxID_ANY, _("Android quick guide"));
  guide->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
    ShowBundledHtmlHelp(this, _("Android quick guide"), _("Android_Quick_Guide.html"));
  });
  layouts[3]->Add(guide, 0, wxEXPAND | wxALL, 6);
  move(m_bDocumentation, 3, _("Offline manual"));
  move(m_pdfDocumentationButton, 3, _("PDF manual"));
  if (m_timeIntegrityPanel->GetContainingSizer())
    m_timeIntegrityPanel->GetContainingSizer()->Detach(m_timeIntegrityPanel);
  m_timeIntegrityPanel->Reparent(contents[3]);
  m_timeIntegrityPanel->Hide();
  auto* timing = new wxButton(contents[3], wxID_ANY, _("UTC, local and GNSS clock status"));
  layouts[3]->Add(timing, 0, wxEXPAND | wxALL, 6);
  timing->Bind(wxEVT_BUTTON, [this, timing](wxCommandEvent&) {
    wxDialog sheet(this, wxID_ANY, _("Clock status"));
    auto* body = new wxBoxSizer(wxVERTICAL);
    m_timeIntegrityPanel->Reparent(&sheet);
    m_timeIntegrityPanel->SetMinSize(wxSize(0, 0));
    StackForms(m_timeIntegrityPanel->GetSizer());
    ScrollContent(m_timeIntegrityPanel);
    m_timeIntegrityPanel->Show();
    body->Add(m_timeIntegrityPanel, 1, wxEXPAND);
    sheet.SetSizer(body);
    sheet.GetHandle()->setProperty("cnDocumentSurface", true);
    Decorate(&sheet, _("Clock status"));
    UpdateTimeIntegrityPanel();
    sheet.ShowModal();
    body->Detach(m_timeIntegrityPanel);
    m_timeIntegrityPanel->Reparent(timing->GetParent());
    m_timeIntegrityPanel->Hide();
  });
  m_androidClockTimer = new QTimer(GetHandle());
  QObject::connect(m_androidClockTimer, &QTimer::timeout, GetHandle(), [this]() { UpdateTimeIntegrityPanel(); });
  m_androidClockTimer->start(100);
  SetSizer(root, true);
  CN_StyleAndroidControls(this);
  title->GetHandle()->setStyleSheet("QLabel { color: white; font-size: 20pt; }");
  new Surface(this, [this]() { m_Plugin->OnDialogClose(); });
  LayoutScrolls(this);
  RefreshAndroidCards();
}

void CelestialNavigationDialog::SelectAndroidSight(size_t index) {
  if (index >= m_Sights.size()) return;
  for (size_t i = 0; i < m_Sights.size(); ++i)
    m_lSights->SetItemState(i, i == index ? wxLIST_STATE_SELECTED : 0,
                           wxLIST_STATE_SELECTED);
  for (size_t i = 0; i < m_Sights.size(); ++i) m_Sights[i].SetSelected(i == index);
  UpdateButtons();
  RefreshAndroidCards();
}

void CelestialNavigationDialog::RefreshAndroidCards() {
  if (!m_androidCards || m_androidRefreshPending) return;
  m_androidRefreshPending = true;
  wxWeakRef<CelestialNavigationDialog> weak(this);
  QTimer::singleShot(0, GetHandle(), [weak]() {
    if (!weak) return;
    auto* self = weak.get();
    self->m_androidRefreshPending = false;
    auto* layout = self->m_androidCards->GetSizer();
    layout->Clear(true);
    if (self->m_Sights.empty())
      layout->Add(new wxStaticText(self->m_androidCards, wxID_ANY,
                    _("No observations yet. Mark time, then choose New sight.")),
                  0, wxEXPAND | wxALL, 12);
    for (size_t i = 0; i < self->m_Sights.size(); ++i) {
      const auto& sight = self->m_Sights[i];
      const wxString type = sight.m_Type == Sight::HORIZON
          ? sight.HorizonEventName() : SightType[sight.m_Type];
      const wxString angle = sight.m_Type == Sight::HORIZON
          ? sight.HorizonMeasurementText()
          : toSDMM_PlugIn(0, sight.m_Measurement, true);
      const wxString time = UtcDateTime::FormatUtc(sight.m_DateTime, "%Y-%m-%d %H:%M:%S") + wxString::Format(".%03d", sight.m_DateTime.GetMillisecond()) +
          (sight.m_Type == Sight::LUNAR && sight.m_LunarTimeIsWatch ? _(" watch") : _(" UTC"));
      const wxString caption = sight.m_Body + " / " + type + "\n" + angle +
          "\n" + time + "\n" +
          (sight.IsVisible() ? _("Included") : _("Excluded")) +
          (sight.IsSelected() ? _(" / selected") : wxString());
      auto* card = new wxButton(self->m_androidCards, wxID_ANY, caption);
      card->SetMinSize(wxSize(0, CN_TouchHeight() * 3));
      card->Bind(wxEVT_BUTTON, [weak, i](wxCommandEvent&) {
        if (weak) weak->SelectAndroidSight(i);
      });
      layout->Add(card, 0, wxEXPAND | wxALL, 6);
    }
    self->m_androidInclude->Enable(self->GetSelectedSight() != nullptr);
    self->UpdateButtons();
    self->m_bDuplicateSight->Enable(self->GetSelectedSight() != nullptr);
    CN_StyleAndroidControls(self->m_androidCards);
    self->m_androidCards->Layout();
    self->Layout();
    celestial_android::LayoutScrolls(self);
  });
}
#endif
