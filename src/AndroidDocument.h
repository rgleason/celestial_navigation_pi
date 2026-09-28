#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidSurface.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QUrl>
#include <QTextDocument>

namespace celestial_android {
// Adapt resources, not setHtml snapshots: QTextBrowser keeps its source URL,
// relative images/documents, anchors and navigation history intact.
class DocumentBrowser : public QTextBrowser {
 public:
  explicit DocumentBrowser(QWidget* parent) : QTextBrowser(parent) {
    QFont font = this->font();
    font.setPointSize(CN_FontPointSize());
    setFont(font);
    document()->setDefaultFont(font);
  }

  QVariant loadResource(int type, const QUrl& name) override {
    QVariant resource = QTextBrowser::loadResource(type, name);
    if (type != QTextDocument::HtmlResource || !resource.isValid()) return resource;
    QString html = QString::fromUtf8(resource.toByteArray());
    const int size = CN_FontPointSize();
    QString css = QString(
        "body, p, li, td, th, figcaption, .small, .edition, .principle, code "
        "{ font-size: %1pt; }"
        "h1, .cover h1 { font-size: %2pt; } h2 { font-size: %3pt; }"
        "h3, .subtitle { font-size: %4pt; } h4 { font-size: %1pt; }")
        .arg(size).arg(size + 7).arg(size + 4).arg(size + 2);
    const QColor background = CN_ThemeBackground();
    if (background.lightness() < 128) {
      const QString ink = CN_HostColour("DILG3", Qt::lightGray).name();
      css += QString(
          "body, p, li, h1, h2, h3, h4, a, td, th, figcaption, .small,"
          " .edition, .subtitle, .label, .quick td:first-child, .principle"
          " { color: %1; }"
          "body, td, th, .warning, .note, .example, .principle, .formula,"
          " tr:nth-child(even) td { background-color: %2; }"
          "a { text-decoration: underline; }").arg(ink, background.name());
    }
    const QString style = "<style type=\"text/css\">" + css + "</style>";
    const int headEnd = html.indexOf("</head>", 0, Qt::CaseInsensitive);
    if (headEnd >= 0) html.insert(headEnd, style);
    else html.prepend(style);
    return html.toUtf8();
  }
};

inline void ShowDocument(wxWindow* parent, const wxString& title,
                         const wxString& content, bool file = false) {
  wxDialog sheet(parent, wxID_ANY, title);
  sheet.GetHandle()->setProperty("cnDocumentSurface", true);
  auto* root = new wxBoxSizer(wxVERTICAL);
  auto* panel = new wxPanel(&sheet, wxID_ANY);
  auto* layout = new QVBoxLayout(panel->GetHandle());
  layout->setContentsMargins(0, 0, 0, 0);
  auto* text = new DocumentBrowser(panel->GetHandle());
  // POBsoft (1985-2026): remote references belong in the Android browser;
  // QTextBrowser itself only renders local help and otherwise shows a blank page.
  // Relative bundled documents and fragment anchors still navigate in this sheet.
  text->setOpenExternalLinks(true);
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
