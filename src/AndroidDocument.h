#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidSurface.h"
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QUrl>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextTable>
#include <QVector>
#include <QDesktopServices>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>

namespace celestial_android {
// Format the rendered document without replacing source HTML: native URLs,
// images, anchor metadata and browser history remain owned by QTextBrowser.
class DocumentBrowser : public QTextBrowser {
 public:
  explicit DocumentBrowser(QWidget* parent) : QTextBrowser(parent) {
    QFont font = this->font();
    font.setPointSize(CN_FontPointSize());
    setFont(font);
    document()->setDefaultFont(font);
    QObject::connect(this, &QTextBrowser::sourceChanged, this,
                     [this](const QUrl& source) {
      qDebug() << "CNHELP SOURCE" << source;
      QScroller::scroller(viewport())->stop();
      AdaptDocument();
      // Font reflow must finish before resolving the requested anchor.
      const QString fragment = source.fragment();
      if (!fragment.isEmpty()) QTimer::singleShot(0, this,
          [this, fragment]() {
            document()->documentLayout()->documentSize();
            scrollToAnchor(fragment);
            viewport()->repaint();
            qDebug() << "CNHELP ANCHOR" << fragment << verticalScrollBar()->value();
          });
    });
  }

  QVariant loadResource(int type, const QUrl& name) override {
    QVariant resource = QTextBrowser::loadResource(type, name);
    if (type != QTextDocument::HtmlResource || !resource.isValid()) return resource;
    QString html = QString::fromUtf8(resource.toByteArray());
    // Qt5 rich text does not lay out HTML5 figure/caption blocks.
    html.replace("<figure>", "<div><p>", Qt::CaseInsensitive);
    html.replace("</figure>", "</div>", Qt::CaseInsensitive);
    html.replace("<figcaption>", "</p><p>", Qt::CaseInsensitive);
    html.replace("</figcaption>", "</p>", Qt::CaseInsensitive);
    return html.toUtf8();
  }

 private:
  void AdaptDocument() {
    const QColor background = CN_ThemeBackground();
    const bool lowLight = background.lightness() < 128;
    const QColor ink = CN_HostColour("DILG3", Qt::lightGray);
    const int size = CN_FontPointSize();
    struct Range { int position; int length; QTextCharFormat format; };
    QVector<Range> ranges;
    // Collect before merging formats, which can split/coalesce fragments.
    for (auto block = document()->begin(); block.isValid(); block = block.next()) {
      if (lowLight) {
        QTextCursor cursor(block);
        QTextBlockFormat format;
        format.setBackground(background);
        cursor.mergeBlockFormat(format);
      }
      for (auto it = block.begin(); !it.atEnd(); ++it) {
        const auto fragment = it.fragment();
        if (!fragment.isValid()) continue;
        QTextCharFormat format;
        format.setFontPointSize(qMax(double(size), fragment.charFormat().fontPointSize()));
        if (lowLight) {
          format.setForeground(ink);
          format.setBackground(background);
        }
        ranges.append({fragment.position(), fragment.length(), format});
      }
    }
    for (const auto& range : ranges) {
      QTextCursor cursor(document());
      cursor.setPosition(range.position);
      cursor.setPosition(range.position + range.length, QTextCursor::KeepAnchor);
      cursor.mergeCharFormat(range.format);
    }
    if (lowLight) DimFrame(document()->rootFrame(), background);
    document()->setModified(false);
  }

  static void DimFrame(QTextFrame* frame, const QColor& background) {
    auto format = frame->frameFormat();
    format.setBackground(background);
    frame->setFrameFormat(format);
    if (auto* table = qobject_cast<QTextTable*>(frame)) {
      for (int row = 0; row < table->rows(); ++row) {
        for (int column = 0; column < table->columns(); ++column) {
          auto cell = table->cellAt(row, column);
          auto cellFormat = cell.format();
          cellFormat.setBackground(background);
          cell.setFormat(cellFormat);
        }
      }
    }
    for (auto* child : frame->childFrames()) DimFrame(child, background);
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
  // The owned filter feeds handleInput itself; a native gesture recognizer
  // would process the same touch sequence and can swallow the first link tap.
  QScroller::scroller(text->viewport());
  text->viewport()->setProperty("cnHelpDiagnostic", true);
  QPointer<DocumentBrowser> safeText(text);
  new CN_AndroidButtonDragFilter(text->viewport(), [safeText](QPoint point) {
    if (!safeText) return;
    const QString anchor = safeText->anchorAt(point);
    qDebug() << "CNHELP HIT" << point << anchor << safeText->source();
    if (anchor.isEmpty()) return;
    QScroller::scroller(safeText->viewport())->stop();
    const QUrl url = safeText->source().resolved(QUrl(anchor));
    if (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "mailto")
      QDesktopServices::openUrl(url);
    else safeText->setSource(url);
  }, text->viewport());
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
