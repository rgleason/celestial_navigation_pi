#include "../src/AndroidDownloadEvents.h"
#include <wx/init.h>
#include <cassert>
#include <thread>
#include <iostream>

int main() {
  wxInitializer init;
  assert(init.IsOk());
  AndroidDownloadEvents receiver;
  const auto gui = std::this_thread::get_id();
  int count = 0;
  receiver.Bind(wxEVT_THREAD, [&](wxThreadEvent& event) {
    assert(std::this_thread::get_id() == gui);
    assert(event.GetInt() == ++count);
  });
  std::thread host([&]() {
    wxThreadEvent progress(wxEVT_THREAD);
    progress.SetInt(1);
    receiver.AddPendingEvent(progress);
    wxThreadEvent end(wxEVT_THREAD);
    end.SetInt(2);
    receiver.AddPendingEvent(end);
  });
  host.join();
  assert(count == 0);
  receiver.Drain();
  assert(count == 2);
  receiver.Drain();
  assert(count == 2);
  wxThreadEvent cancelled(wxEVT_THREAD);
  receiver.AddPendingEvent(cancelled);
  receiver.Discard();
  receiver.Drain();
  assert(count == 2);
  std::cout << "Host AddPendingEvent clones dispatch once on GUI thread; cancellation discards queued events. PASS\n";
}
