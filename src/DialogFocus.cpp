#include "DialogFocus.h"

#ifndef __OCPN__ANDROID__
#include <algorithm>
#include <wx/dialog.h>
#include <wx/toplevel.h>

#include "DialogGeometry.h"

namespace {
int DepthBelow(wxWindow* window, wxWindow* ancestor) {
  int depth = 0;
  for (; window; window = window->GetParent(), ++depth)
    if (window == ancestor) return depth;
  return -1;
}

void Present(wxDialog* dialog) {
  if (dialog->IsIconized()) dialog->Iconize(false);
  dialog_geometry::EnsureVisible(dialog);
  dialog->Raise();
  // Leave the active field and its selection alone when focus is already in
  // this dialog. SetFocus on the dialog otherwise restores its last child.
  if (DepthBelow(wxWindow::FindFocus(), dialog) < 0) dialog->SetFocus();
}
}  // namespace

DialogFocusController::DialogFocusController(wxDialog* root) : m_root(root) {
  wxEvtHandler::AddFilter(this);
  Register();
}

DialogFocusController::~DialogFocusController() {
  Unregister();
  wxEvtHandler::RemoveFilter(this);
  // Recovery is queued on this handler, never on wxApp: nothing may call back
  // into the plugin after its windows are deleted and its library unloaded.
  DeletePendingEvents();
}

int DialogFocusController::Enter(wxDialog* dialog) {
  m_modalStack.emplace_back(dialog);
  return wxID_NONE;
}

void DialogFocusController::Exit(wxDialog* dialog) {
  m_modalStack.erase(std::remove_if(m_modalStack.begin(), m_modalStack.end(),
                                    [dialog](const wxWeakRef<wxDialog>& entry) {
                                      return !entry || entry.get() == dialog;
                                    }),
                     m_modalStack.end());
}

wxDialog* DialogFocusController::ActiveModal() const {
  if (!m_root || m_root->IsBeingDeleted()) return nullptr;
  // Cocoa's native modal loop doesn't necessarily disable the other wx
  // windows. Record ShowModal order, including unrelated OpenCPN dialogs,
  // instead of using enabled state alone to determine who needs attention.
  for (auto entry = m_modalStack.rbegin(); entry != m_modalStack.rend();
       ++entry)
    if (*entry && !(*entry)->IsBeingDeleted() && (*entry)->IsShown() &&
        (*entry)->IsModal())
      return entry->get();

  // A modal may already exist when this controller is registered. Recover an
  // owned child only if no unrelated modal could be blocking it.
  wxDialog* result = nullptr;
  int greatestDepth = 0;
  for (auto* window : wxTopLevelWindows) {
    auto* dialog = wxDynamicCast(window, wxDialog);
    if (!dialog || dialog->IsBeingDeleted() || !dialog->IsShown() ||
        !dialog->IsModal() || !dialog->IsEnabled())
      continue;
    const int depth = DepthBelow(dialog, m_root.get());
    if (depth < 0) return dialog;
    if (depth > greatestDepth) {
      result = dialog;
      greatestDepth = depth;
    }
  }
  return result;
}

wxDialog* DialogFocusController::ModalChild() const {
  auto* modal = ActiveModal();
  return modal && modal->IsEnabled() && DepthBelow(modal, m_root.get()) > 0
             ? modal
             : nullptr;
}

bool DialogFocusController::BringToFront() {
  if (!m_root || m_root->IsBeingDeleted()) return false;
  if (auto* modal = ActiveModal()) {
    if (modal->IsEnabled() && DepthBelow(modal, m_root.get()) > 0)
      Present(modal);
  } else if (m_root->IsEnabled()) {
    m_root->Show();
    Present(m_root.get());
    return true;
  }
  return false;
}

void DialogFocusController::ScheduleRecovery() {
  if (m_recoveryPending) return;
  m_recoveryPending = true;
  CallAfter([this] {
    if (!m_recoveryPending) return;
    m_recoveryPending = false;
    // The child may have closed while this was queued. Re-evaluate rather
    // than retaining a pointer to a stack-allocated modal dialog.
    if (auto* modal = ModalChild()) Present(modal);
  });
}

int DialogFocusController::FilterEvent(wxEvent& event) {
  const auto type = event.GetEventType();
  if (type != wxEVT_ACTIVATE_APP && type != wxEVT_ACTIVATE) return Event_Skip;

  const auto& activation = static_cast<wxActivateEvent&>(event);
  if (type == wxEVT_ACTIVATE_APP && !activation.GetActive()) {
    m_recoveryPending = false;
    return Event_Skip;
  }
  if (!activation.GetActive()) return Event_Skip;

  auto* modal = ModalChild();
  if (!modal) return Event_Skip;
  if (type == wxEVT_ACTIVATE_APP) {
    ScheduleRecovery();
  } else {
    auto* activated = wxDynamicCast(event.GetEventObject(), wxWindow);
    // Only recover when an ancestor (including OpenCPN's frame) is activated.
    // Let unrelated windows and modeless sibling tools keep their focus.
    if (activated && activated != modal && DepthBelow(modal, activated) > 0)
      ScheduleRecovery();
  }
  return Event_Skip;
}
#endif
