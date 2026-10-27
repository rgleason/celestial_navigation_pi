// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CELNAV_STRING_VECTOR_H
#define CELNAV_STRING_VECTOR_H

#include <memory>
#include <vector>
#include <wx/string.h>

namespace celnav {

// wxWidgets 3.3 exports vector<wxString, allocator<wxString>> from its DLL.
// A newer MSVC STL can require private members absent from that DLL. Give
// plugin-owned vectors a distinct allocator type so their implementation is
// instantiated here, retaining contiguous storage and normal vector semantics.
template <typename T>
struct StringAllocator : std::allocator<T> {
  template <typename U>
  struct rebind {
    using other = StringAllocator<U>;
  };
  StringAllocator() noexcept = default;
  template <typename U>
  StringAllocator(const StringAllocator<U>&) noexcept {}
};

template <typename T, typename U>
bool operator==(const StringAllocator<T>&, const StringAllocator<U>&) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const StringAllocator<T>&, const StringAllocator<U>&) noexcept {
  return false;
}

using StringVector = std::vector<wxString, StringAllocator<wxString>>;

}  // namespace celnav
#endif
