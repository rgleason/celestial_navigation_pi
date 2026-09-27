#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidSurface.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QUrl>

namespace celestial_android {
inline void ShowDocument(wxWindow* parent, const wxString& title,
                         const wxString& content, bool file = false) {
  wxDialog sheet(parent, wxID_ANY, title);
  sheet.GetHandle()->setProperty("cnDocumentSurface", true);
  auto* root = new wxBoxSizer(wxVERTICAL);
  auto* panel = new wxPanel(&sheet, wxID_ANY);
  auto* layout = new QVBoxLayout(panel->GetHandle());
  layout->setContentsMargins(0, 0, 0, 0);
  auto* text = new QTextBrowser(panel->GetHandle());
  text->setStyleSheet(QString("QTextBrowser { font-size: %1pt; padding: 16px; "
      "color: #173849; background: white; border: none; }").arg(CN_FontPointSize()));
  text->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
  QScroller::grabGesture(text->viewport(), QScroller::TouchGesture);
  if (file) text->setSource(QUrl::fromLocalFile(QString::fromUtf8(content.utf8_str())));
  else text->setPlainText(QString::fromUtf8(content.utf8_str()));
  layout->addWidget(text);
  root->Add(panel, 1, wxEXPAND);
  sheet.SetSizer(root);
  Decorate(&sheet, title);
  sheet.ShowModal();
}
} // namespace celestial_android
#endif
