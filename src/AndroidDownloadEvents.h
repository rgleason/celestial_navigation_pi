#ifndef CELESTIAL_ANDROID_DOWNLOAD_EVENTS_H
#define CELESTIAL_ANDROID_DOWNLOAD_EVENTS_H

#ifdef __OCPN__ANDROID__
#include <wx/event.h>
#include <memory>
#include <mutex>
#include <vector>

// POBsoft (1985-2026): pinned wx/Qt does not drain host download events.
// Keep only this transfer's events and dispatch them on the Qt GUI timer.
class AndroidDownloadEvents : public wxEvtHandler {
 public:
  void QueueEvent(wxEvent* event) override {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.emplace_back(event);
  }
  void Drain() {
    std::vector<std::unique_ptr<wxEvent>> events;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      events.swap(m_events);
    }
    for (auto& event : events) SafelyProcessEvent(*event);
  }
  void Discard() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.clear();
  }
 private:
  std::mutex m_mutex;
  std::vector<std::unique_ptr<wxEvent>> m_events;
};
#endif
#endif
