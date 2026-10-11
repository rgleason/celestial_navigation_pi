#ifndef CELESTIAL_NAVIGATION_DIALOG_FOCUS_H
#define CELESTIAL_NAVIGATION_DIALOG_FOCUS_H

#ifndef __OCPN__ANDROID__
#include <wx/event.h>
#include <wx/eventfilter.h>
#include <wx/modalhook.h>
#include <wx/weakref.h>
#include <vector>

class wxDialog;

// Keep the modal child which needs attention accessible without changing the
// dialogs' modal lifetime or keeping plugin windows above other applications.
class DialogFocusController : public wxEvtHandler,
                              public wxEventFilter,
                              public wxModalDialogHook {
public:
  explicit DialogFocusController(wxDialog* root);
  ~DialogFocusController() override;

  // False means a modal dialog is open: callers should not start another tool.
  bool BringToFront();

private:
  int FilterEvent(wxEvent& event) override;
  int Enter(wxDialog* dialog) override;
  void Exit(wxDialog* dialog) override;
  wxDialog* ActiveModal() const;
  wxDialog* ModalChild() const;
  void ScheduleRecovery();

  wxWeakRef<wxDialog> m_root;
  std::vector<wxWeakRef<wxDialog>> m_modalStack;
  bool m_recoveryPending = false;
};
#endif

#endif
