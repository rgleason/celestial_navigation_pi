#ifndef CELESTIAL_ANDROID_CLIPBOARD_H
#define CELESTIAL_ANDROID_CLIPBOARD_H

#ifdef __OCPN__ANDROID__
#include <QClipboard>
#include <QGuiApplication>
#include <QString>
#include <wx/string.h>

namespace celestial_android {
// POBsoft (1985-2026): pinned wxQt exports wchar bytes as clipboard text.
// Both copy commands run on the UI thread; provide native Unicode text.
inline bool CopyText(const wxString& text) {
  auto* clipboard = QGuiApplication::clipboard();
  if (!clipboard) return false;
  const auto utf8 = text.ToUTF8();
  clipboard->setText(QString::fromUtf8(utf8.data(), utf8.length()));
  return true;
}
}
#endif

#endif
