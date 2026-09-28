// Android-only wxQt presentation adapters. GPL-3.0-or-later.
#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidTouch.h"
#include "AndroidDialogBack.h"
#include <QAbstractSpinBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QInputMethod>
#include <QDialog>
#include <QDebug>
#include <wx/dialog.h>
#include <wx/weakref.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/calctrl.h>
#include <vector>

namespace celestial_android {
inline void AdaptDates(wxWindow* window);
inline void CommitNumbers(wxWindow* window) {
  for (auto* child : window->GetChildren()) {
    if (auto* control = wxDynamicCast(child, wxSpinCtrl)) {
      if (auto* native = qobject_cast<QSpinBox*>(control->GetHandle())) {
        native->interpretText();
        control->SetValue(native->value());
      }
    } else if (auto* control = wxDynamicCast(child, wxSpinCtrlDouble)) {
      if (auto* native = qobject_cast<QDoubleSpinBox*>(control->GetHandle())) {
        native->interpretText();
        control->SetValue(native->value());
      }
    }
    CommitNumbers(child);
  }
}

// Stack form grids, retaining the original controls and business handlers.
// Static boxes keep owning their children; no controller pointers are destroyed.
inline void StackForms(wxSizer* sizer) {
  if (!sizer) return;
  if (auto* row = dynamic_cast<wxBoxSizer*>(sizer)) {
    int labels = 0, entries = 0;
    bool meaningfulLabel = false;
    for (auto* item : row->GetChildren()) {
      auto* window = item->GetWindow();
      if (auto* label = wxDynamicCast(window, wxStaticText)) {
        ++labels;
        wxString text = label->GetLabel();
        meaningfulLabel |= !text.Trim().Trim(false).empty() && text != ":";
      }
      entries += window && (wxDynamicCast(window, wxTextCtrl) ||
                          wxDynamicCast(window, wxChoice) ||
                          wxDynamicCast(window, wxSpinCtrl) ||
                          wxDynamicCast(window, wxSpinCtrlDouble) ||
                          wxDynamicCast(window, wxDatePickerCtrl) ||
                          window->GetHandle()->findChild<QAbstractSpinBox*>());
    }
    // Labelled form rows need stacking in portrait, including mixed motion
    // checkboxes/course/speed. Colon-separated hour/minute/second rows retain
    // their grouping; action-button rows have no labels or entries.
    if (row->GetOrientation() == wxHORIZONTAL && labels && entries && meaningfulLabel) {
      row->SetOrientation(wxVERTICAL);
      for (auto* item : row->GetChildren()) item->SetProportion(0);
    }
  }
  if (auto* grid = dynamic_cast<wxFlexGridSizer*>(sizer)) {
    for (size_t col = 0; col < static_cast<size_t>(grid->GetCols()); ++col)
      if (grid->IsColGrowable(col)) grid->RemoveGrowableCol(col);
    for (size_t row = 0; row < static_cast<size_t>(grid->GetRows()); ++row)
      if (grid->IsRowGrowable(row)) grid->RemoveGrowableRow(row);
    grid->SetCols(1);
    grid->AddGrowableCol(0);
  }
  for (auto* item : sizer->GetChildren()) {
    if (item->IsSizer()) StackForms(item->GetSizer());
    if (item->IsWindow()) {
      if (auto* label = wxDynamicCast(item->GetWindow(), wxStaticText))
        label->SetMinSize(wxSize(0, -1));
      item->SetFlag(wxEXPAND | wxALL);
      item->SetBorder(8);
    }
  }
}

inline void WrapLabels(wxWindow* window, int width) {
  for (auto* child : window->GetChildren()) {
    if (auto* label = wxDynamicCast(child, wxStaticText))
      CN_WrapAndroidText(label, label->GetLabel(), qMax(160, width - 48));
    WrapLabels(child, width);
  }
}

inline void ScrollContent(wxScrolledWindow* scroll) {
  if (scroll->GetHandle()->property("cnContentPanel").toBool()) return;
  wxSizer* fields = scroll->GetSizer();
  if (!fields) return;
  scroll->SetSizer(nullptr, false);
  auto* panel = new wxPanel(scroll, wxID_ANY);
  std::vector<wxWindow*> children;
  for (auto* child : scroll->GetChildren())
    if (child != panel) children.push_back(child);
  for (auto* child : children) child->Reparent(panel);
  panel->SetSizer(fields);
  auto* root = new wxBoxSizer(wxVERTICAL);
  root->Add(panel, 0, wxEXPAND | wxALL, 8);
  root->AddSpacer(32);
  scroll->SetSizer(root);
  scroll->SetMinSize(wxSize(0, 0));
  scroll->SetScrollRate(0, 1);
  scroll->GetHandle()->setProperty("cnContentPanel", true);
  CN_EnableAndroidScrolling(scroll);
}

inline void PrepareBook(wxNotebook* book) {
  if (book->GetHandle()->property("cnBookPrepared").toBool()) return;
  book->GetHandle()->setProperty("cnBookPrepared", true);
  const int selection = book->GetSelection();
  std::vector<wxWindow*> pages;
  std::vector<wxString> names;
  while (book->GetPageCount()) {
    pages.push_back(book->GetPage(0));
    names.push_back(book->GetPageText(0));
    book->RemovePage(0);
  }
  for (size_t i = 0; i < pages.size(); ++i) {
    auto* page = pages[i];
    StackForms(page->GetSizer());
    if (page->GetHandle()->property("cnNoPageScroll").toBool()) {
      // A native results list owns its scrolling and fills the page. Wrapping
      // it in a wx viewport introduces a second competing drag target.
      page->SetMinSize(wxSize(0, 0));
      book->AddPage(page, names[i]);
      page->GetHandle()->show();
    } else if (auto* scroll = wxDynamicCast(page, wxScrolledWindow)) {
      ScrollContent(scroll);
      book->AddPage(page, names[i]);
    } else {
      auto* viewport = new wxScrolledWindow(book, wxID_ANY);
      viewport->SetMinSize(wxSize(0, 0));
      viewport->SetScrollRate(0, 1);
      page->Reparent(viewport);
      page->Show();
      // A non-growing content panel needs its computed height. A (0,0)
      // minimum bypasses wx GetBestSize and collapses the whole form.
      page->SetMinSize(wxSize(0, -1));
      auto* root = new wxBoxSizer(wxVERTICAL);
      root->Add(page, 0, wxEXPAND | wxALL, 8);
      root->AddSpacer(32);
      viewport->SetSizer(root);
      CN_EnableAndroidScrolling(viewport);
      book->AddPage(viewport, names[i]);
      // wxQt setParent hides the native widget without updating wx's shown
      // bit. Show() can consequently be a no-op after notebook removal.
      page->GetHandle()->show();
    }
  }
  if (selection >= 0) book->SetSelection(selection);
  book->SetMinSize(wxSize(0, 0));
  if (auto* native = qobject_cast<QTabWidget*>(book->GetHandle()))
    native->tabBar()->hide();
}

inline void LayoutScrolls(wxWindow* window) {
  window->Layout();
  for (auto* child : window->GetChildren()) LayoutScrolls(child);
  if (auto* scroll = wxDynamicCast(window, wxScrolledWindow)) {
    WrapLabels(scroll, scroll->GetClientSize().x);
    scroll->Layout();
    scroll->FitInside();
  }
}

// Lives only while the wx dialog is alive, including its modal event loops.
// Destroy it at wx's destroy notification, before its derived vtable is gone.
class Surface : public QObject {
 public:
  Surface(wxDialog* dialog, std::function<void()> close)
      : QObject(dialog->GetHandle()), dialog_(dialog), back_(dialog, close) {
    dialog->GetHandle()->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog->GetHandle()->ungrabGesture(Qt::PanGesture);
    dialog->SetMinSize(wxSize(0, 0));
    dialog->GetHandle()->installEventFilter(this);
    if (auto* canvas = GetCanvasByIndex(0)) {
      canvas_ = canvas->GetHandle();
      canvas_->installEventFilter(this);
    }
    QObject::connect(QApplication::primaryScreen(), &QScreen::geometryChanged,
                     this, [this](const QRect&) { Schedule(); });
    QObject::connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged,
                     this, [this]() { Schedule(); });
    dialog->Bind(wxEVT_DESTROY, [this, dialog](wxWindowDestroyEvent& event) {
      if (event.GetEventObject() == dialog) delete this;
      event.Skip();
    });
    Schedule();
  }
  void Fit() {
    if (!dialog_) return;
    auto* canvas = GetCanvasByIndex(0);
    if (!canvas) return;
    const wxSize size = canvas->GetClientSize();
    if (size.x < 100 || size.y < 100) return;
    dialog_->SetMinSize(wxSize(0, 0));
    dialog_->SetSize(wxSize(size.x - 24, size.y - 24));
    dialog_->Move(canvas->ClientToScreen(wxPoint(12, 12)));
    dialog_->Layout();
    LayoutScrolls(dialog_.get());
  }
 protected:
  bool eventFilter(QObject* target, QEvent* event) override {
    if (event->type() == QEvent::Show ||
        (event->type() == QEvent::Resize &&
         (target == canvas_ || (dialog_ && target == dialog_->GetHandle())))) Schedule();
    return false;
  }
 private:
  void Schedule() {
    if (pending_) return;
    pending_ = true;
    QTimer::singleShot(150, this, [this]() { pending_ = false; Fit(); });
  }
  bool pending_ = false;
  wxWeakRef<wxDialog> dialog_;
  CelestialAndroidBackFilter back_;
  QPointer<QWidget> canvas_;
};

inline void SendButton(wxWindow* button) {
  if (!button || !button->IsEnabled()) return;
  wxCommandEvent command(wxEVT_BUTTON, button->GetId());
  command.SetEventObject(button);
  button->GetEventHandler()->ProcessEvent(command);
}

class ActionState final : public QObject {
 public:
  ActionState(wxWindow* source, wxButton* proxy)
      : QObject(proxy->GetHandle()), source_(source->GetHandle()), proxy_(proxy->GetHandle()) {
    source_->installEventFilter(this);
    proxy_->setEnabled(source_->isEnabled());
  }
 protected:
  bool eventFilter(QObject*, QEvent* event) override {
    if (event->type() == QEvent::EnabledChange && source_ && proxy_)
      proxy_->setEnabled(source_->isEnabled());
    return false;
  }
 private:
  QPointer<QWidget> source_, proxy_;
};

inline Surface* Decorate(wxDialog* dialog, const wxString& title,
                         std::function<void()> close = {}) {
  if (dialog->GetHandle()->property("cnSurface").toBool()) return nullptr;
  dialog->GetHandle()->setProperty("cnSurface", true);
  if (!close) close = [dialog]() {
    if (dialog->IsModal()) dialog->EndModal(wxID_CANCEL);
    else dialog->Close();
  };
  auto* content = dialog->GetSizer();
  if (!content) return nullptr;
  dialog->SetSizer(nullptr, false);
  auto* root = new wxBoxSizer(wxVERTICAL);
  auto* header = new wxPanel(dialog, wxID_ANY);
  header->SetBackgroundColour(wxColour(25, 59, 76));
  auto* row = new wxBoxSizer(wxHORIZONTAL);
  auto* label = new wxStaticText(header, wxID_ANY, title);
  label->GetHandle()->setStyleSheet("QLabel { color: white; font-size: 20pt; }");
  row->Add(label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 12);
  auto* cancel = new wxButton(header, wxID_ANY, _("Cancel"));
  if (!dialog->FindWindow(wxID_OK) && !dialog->GetHandle()->property("cnCancellable").toBool()) cancel->SetLabel(_("Close"));
  cancel->Bind(wxEVT_BUTTON, [close](wxCommandEvent&) { close(); });
  row->Add(cancel, 0, wxALL, 8);
  if (auto* original = dialog->FindWindow(wxID_OK)) {
    wxString action = _("Save");
    if (auto* button = wxDynamicCast(original, wxButton)) {
      action = button->GetLabel();
      auto* native = qobject_cast<QAbstractButton*>(button->GetHandle());
      if (!native) native = button->GetHandle()->findChild<QAbstractButton*>();
      if (native) action = wxString::FromUTF8(native->text().toUtf8().constData());
      if (button->GetHandle()->property("cnActionText").isValid())
        action = wxString::FromUTF8(button->GetHandle()->property("cnActionText").toString().toUtf8().constData());
      action.Replace("&", "");
      if (action.empty()) action = _("Save");
      if (action == _("OK") || action == _("Save Changes") || action == _("Create Sight")) action = _("Save");
      if (action == _("Close")) cancel->Hide();
    }
    auto* apply = new wxButton(header, wxID_ANY, action);
    apply->Bind(wxEVT_BUTTON, [dialog, original](wxCommandEvent&) {
      CommitNumbers(dialog);
      SendButton(original);
    });
    new ActionState(original, apply);
    row->Add(apply, 0, wxALL, 8);
    original->Hide();
    if (auto* oldCancel = dialog->FindWindow(wxID_CANCEL)) oldCancel->Hide();
  }
  header->SetSizer(row);
  CN_StyleAndroidControls(header);
  label->GetHandle()->setStyleSheet("QLabel { color: white; font-size: 20pt; }");
  root->Add(header, 0, wxEXPAND);
  // A visible section picker replaces dense desktop notebook tabs.
  std::vector<wxNotebook*> books;
  for (auto* child : dialog->GetChildren())
    if (auto* book = wxDynamicCast(child, wxNotebook)) books.push_back(book);
  for (auto* book : books) {
    PrepareBook(book);
    auto* picker = new wxChoice(dialog, wxID_ANY);
    for (size_t i = 0; i < book->GetPageCount(); ++i) {
      wxString name = book->GetPageText(i);
      name.Replace("&&", "&");
      picker->Append(name);
    }
    picker->SetSelection(qMax(0, book->GetSelection()));
    picker->Bind(wxEVT_CHOICE, [picker, book](wxCommandEvent&) {
      book->SetSelection(picker->GetSelection());
      LayoutScrolls(book);
    });
    root->Add(picker, 0, wxEXPAND | wxALL, 8);
    CN_StyleAndroidCombo(picker);
    picker->SetMinSize(wxSize(-1, CN_TouchHeight()));
  }
  if (books.empty() && !dialog->GetHandle()->property("cnDocumentSurface").toBool()) {
    auto* viewport = new wxScrolledWindow(dialog, wxID_ANY);
    auto* panel = new wxPanel(viewport, wxID_ANY);
    panel->SetMinSize(wxSize(0, -1));
    std::vector<wxWindow*> children;
    for (auto* child : dialog->GetChildren())
      if (child != header && child != viewport) children.push_back(child);
    for (auto* child : children) child->Reparent(panel);
    StackForms(content);
    panel->SetSizer(content);
    auto* scrollSizer = new wxBoxSizer(wxVERTICAL);
    scrollSizer->Add(panel, 0, wxEXPAND | wxALL, 8);
    scrollSizer->AddSpacer(32);
    viewport->SetSizer(scrollSizer);
    viewport->SetScrollRate(0, 1);
    viewport->SetMinSize(wxSize(0, 0));
    CN_EnableAndroidScrolling(viewport);
    root->Add(viewport, 1, wxEXPAND);
  } else root->Add(content, 1, wxEXPAND);
  dialog->SetSizer(root);
  AdaptDates(dialog);
  CN_StyleAndroidControls(dialog);
  label->SetName("cn-android-title");
  label->GetHandle()->setStyleSheet("QLabel { color: white; font-size: 20pt; }");
  auto* surface = new Surface(dialog, close);
  CN_ApplyAndroidTheme(dialog);
  surface->Fit();
  return surface;
}

// The pinned wxQt ShowModal maps every nonzero QDialog::exec result to OK.
// EndModal(Cancel) is also nonzero: use wx's explicit return code instead.
inline int ModalResult(wxDialog& dialog) {
  dialog.SetReturnCode(0);
  Decorate(&dialog, dialog.GetTitle());
  const int nativeResult = dialog.ShowModal();
  return dialog.GetReturnCode() ? dialog.GetReturnCode() : nativeResult;
}

// Qt's stock Yes/No message box ignores Back. Owned confirmations use the
// same cancellation/lifecycle handling as editors, with no destructive default.
inline bool Confirm(wxWindow* parent, const wxString& title,
                    const wxString& message, const wxString& accept = _("Yes")) {
  wxDialog sheet(parent, wxID_ANY, title);
  sheet.GetHandle()->setProperty("cnCancellable", true);
  auto* body = new wxBoxSizer(wxVERTICAL);
  auto* text = new wxStaticText(&sheet, wxID_ANY, message);
  text->Wrap(520);
  body->Add(text, 0, wxEXPAND | wxALL, 16);
  auto* row = new wxBoxSizer(wxHORIZONTAL);
  auto* no = new wxButton(&sheet, wxID_ANY, _("No"));
  auto* yes = new wxButton(&sheet, wxID_ANY, accept);
  no->Bind(wxEVT_BUTTON, [&sheet](wxCommandEvent&) { sheet.EndModal(wxID_NO); });
  yes->Bind(wxEVT_BUTTON, [&sheet](wxCommandEvent&) { sheet.EndModal(wxID_YES); });
  row->Add(no, 1, wxEXPAND | wxALL, 8);
  row->Add(yes, 1, wxEXPAND | wxALL, 8);
  body->Add(row, 0, wxEXPAND);
  sheet.SetSizer(body);
  return ModalResult(sheet) == wxID_YES;
}

// wxQt's generic date picker embeds a desktop-sized text/popup composite.
// Keep its model/controller, but expose a full touch button and an explicit
// calendar transaction. A date is calendar fields, never a relabelled instant.
inline void AdaptDates(wxWindow* window) {
  std::vector<wxWindow*> children;
  for (auto* child : window->GetChildren()) children.push_back(child);
  for (auto* child : children) {
    auto* date = wxDynamicCast(child, wxDatePickerCtrl);
    if (!date) { AdaptDates(child); continue; }
    if (date->GetHandle()->property("cnDateAdapter").toBool()) continue;
    auto* sizer = date->GetContainingSizer();
    if (!sizer) continue;
    const bool shown = date->IsShown();
    auto* button = new wxButton(date->GetParent(), wxID_ANY, "");
    if (!sizer->Replace(date, button)) { button->Destroy(); continue; }
    date->GetHandle()->setProperty("cnDateAdapter", true);
    date->Hide();
    button->Show(shown);
    button->SetMinSize(wxSize(-1, CN_TouchHeight()));
    wxWeakRef<wxDatePickerCtrl> weakDate(date);
    wxWeakRef<wxButton> weakButton(button);
    auto refresh = [weakDate, weakButton]() {
      if (!weakDate || !weakButton) return;
      auto value = weakDate->GetValue();
      wxString caption = value.IsValid() ? value.Format("%Y-%m-%d") : _("Choose date");
      caption += "  "; caption += _("Choose date");
      if (weakButton->GetLabel() != caption) weakButton->SetLabel(caption);
      weakButton->Enable(weakDate->IsEnabled());
    };
    refresh();
    // Presets can SetValue without a date event. Reflect these changes too.
    auto* timer = new QTimer(button->GetHandle());
    QObject::connect(timer, &QTimer::timeout, button->GetHandle(), refresh);
    timer->start(250);
    button->Bind(wxEVT_BUTTON, [weakDate, weakButton, refresh](wxCommandEvent&) {
      if (!weakDate || !weakButton) return;
      wxDialog sheet(weakButton.get(), wxID_ANY, _("Choose date"));
      auto* root = new wxBoxSizer(wxVERTICAL);
      auto* calendar = new wxCalendarCtrl(&sheet, wxID_ANY, weakDate->GetValue());
      root->Add(calendar, 0, wxEXPAND | wxALL, 12);
      auto* apply = new wxButton(&sheet, wxID_OK, _("Apply date"));
      apply->GetHandle()->setProperty("cnActionText", "Apply date");
      root->Add(apply, 0, wxEXPAND | wxALL, 12);
      sheet.SetSizer(root);
      Decorate(&sheet, _("Choose date"));
      if (ModalResult(sheet) != wxID_OK || !weakDate) return;
      const wxDateTime selected = calendar->GetDate();
      if (selected.IsValid() && selected != weakDate->GetValue()) {
        weakDate->SetValue(selected);
        wxDateEvent event(weakDate.get(), selected, wxEVT_DATE_CHANGED);
        weakDate->GetEventHandler()->ProcessEvent(event);
      }
      refresh();
    });
  }
}
}  // namespace celestial_android
#endif
