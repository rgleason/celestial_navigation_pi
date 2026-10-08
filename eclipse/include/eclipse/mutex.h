#ifndef CELESTIAL_ECLIPSE_MUTEX_H
#define CELESTIAL_ECLIPSE_MUTEX_H

// OpenCPN 5.12 and 5.14 Windows installers can place an older msvcp140.dll
// beside the executable.  std::mutex built with current MSVC headers is not
// compatible with that runtime.  Keep Windows locking inside the operating
// system while retaining the standard implementation on other platforms.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <mutex>
#endif

namespace eclipse {

#ifdef _WIN32
class Mutex {
 public:
  Mutex() noexcept = default;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void lock() noexcept { AcquireSRWLockExclusive(&lock_); }
  void unlock() noexcept { ReleaseSRWLockExclusive(&lock_); }

 private:
  SRWLOCK lock_ = SRWLOCK_INIT;
};
#else
using Mutex = std::mutex;
#endif

class MutexGuard {
 public:
  explicit MutexGuard(Mutex& mutex) : mutex_(mutex) { mutex_.lock(); }
  ~MutexGuard() { mutex_.unlock(); }

  MutexGuard(const MutexGuard&) = delete;
  MutexGuard& operator=(const MutexGuard&) = delete;

 private:
  Mutex& mutex_;
};

}  // namespace eclipse

#endif
