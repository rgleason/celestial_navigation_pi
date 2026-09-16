#ifndef CELESTIAL_NAVIGATION_DIALOG_TRANSACTION_STATE_H
#define CELESTIAL_NAVIGATION_DIALOG_TRANSACTION_STATE_H

// Small, GUI-independent state machine used by modal editors. Constructor
// population can emit control events, so changes are deliberately ignored
// until the dialog explicitly starts tracking user edits.
class DialogTransactionState {
public:
  void StartTracking() { m_tracking = true; }
  void MarkChanged() {
    if (m_tracking) m_dirty = true;
  }
  bool HasUnsavedChanges() const { return m_dirty; }

private:
  bool m_tracking = false;
  bool m_dirty = false;
};

#endif
