#include "PlatformMessageBox.h"
// Android's in-process PDF renderer. One page/bitmap at a time, API 21+.
#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidJob.h"
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <android/bitmap.h>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QImage>
#include <QPixmap>
#include <stdexcept>

namespace celestial_android {
inline void CheckJava(const char* operation) {
  QAndroidJniEnvironment env;
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    throw std::runtime_error(operation);
  }
}
struct PdfResources {
  QAndroidJniObject descriptor, renderer, page, bitmap;
  bool pixelsLocked = false;
  ~PdfResources() {
    if (pixelsLocked && bitmap.isValid()) {
      QAndroidJniEnvironment env;
      AndroidBitmap_unlockPixels(env, bitmap.object());
    }
    if (bitmap.isValid()) bitmap.callMethod<void>("recycle");
    if (page.isValid()) page.callMethod<void>("close");
    if (renderer.isValid()) renderer.callMethod<void>("close");
    if (descriptor.isValid()) descriptor.callMethod<void>("close");
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
};
inline QImage RenderPdfPage(const wxString& path, int number, int width,
                            int* count, JobState& state) {
  state.Progress("Rendering PDF page " + std::to_string(number + 1));
  PdfResources resource;
  auto name = QAndroidJniObject::fromString(QString::fromUtf8(path.utf8_str()));
  QAndroidJniObject file("java/io/File", "(Ljava/lang/String;)V", name.object());
  CheckJava("The PDF filename could not be opened.");
  resource.descriptor = QAndroidJniObject::callStaticObjectMethod(
      "android/os/ParcelFileDescriptor", "open", "(Ljava/io/File;I)Landroid/os/ParcelFileDescriptor;",
      file.object(), jint(0x10000000));
  CheckJava("The PDF cannot be read. Check that it is a local, readable file.");
  resource.renderer = QAndroidJniObject("android/graphics/pdf/PdfRenderer",
      "(Landroid/os/ParcelFileDescriptor;)V", resource.descriptor.object());
  CheckJava("Android could not decode this PDF (encrypted or invalid PDF).");
  if (!resource.renderer.isValid()) throw std::runtime_error("Android PDF rendering is unavailable.");
  *count = resource.renderer.callMethod<jint>("getPageCount");
  if (number < 0 || number >= *count) throw std::runtime_error("PDF page is outside the document.");
  resource.page = resource.renderer.callObjectMethod("openPage",
      "(I)Landroid/graphics/pdf/PdfRenderer$Page;", jint(number));
  CheckJava("The PDF page could not be opened.");
  const int pageWidth = resource.page.callMethod<jint>("getWidth");
  const int pageHeight = resource.page.callMethod<jint>("getHeight");
  if (pageWidth < 1 || pageHeight < 1) throw std::runtime_error("Invalid PDF page dimensions.");
  double scale = double(qBound(320, width, 2400)) / pageWidth;
  const double pixels = pageWidth * double(pageHeight) * scale * scale;
  // Bound only the display bitmap, never calculation precision or PDF output.
  if (pixels > 4000000.0) scale *= std::sqrt(4000000.0 / pixels);
  const int w = qMax(1, int(pageWidth * scale));
  const int h = qMax(1, int(pageHeight * scale));
  auto format = QAndroidJniObject::getStaticObjectField("android/graphics/Bitmap$Config",
      "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
  resource.bitmap = QAndroidJniObject::callStaticObjectMethod("android/graphics/Bitmap", "createBitmap",
      "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;", jint(w), jint(h), format.object());
  CheckJava("Insufficient memory to display this PDF page.");
  resource.bitmap.callMethod<void>("eraseColor", "(I)V", jint(-1));
  state.Checkpoint();
  resource.page.callMethod<void>("render", "(Landroid/graphics/Bitmap;Landroid/graphics/Rect;Landroid/graphics/Matrix;I)V",
      resource.bitmap.object(), jobject(nullptr), jobject(nullptr), jint(1));
  CheckJava("Android failed to render this PDF page.");
  state.Checkpoint();
  QAndroidJniEnvironment env;
  AndroidBitmapInfo info{};
  void* pixelsAddress = nullptr;
  if (AndroidBitmap_getInfo(env, resource.bitmap.object(), &info) != ANDROID_BITMAP_RESULT_SUCCESS ||
      info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 ||
      AndroidBitmap_lockPixels(env, resource.bitmap.object(), &pixelsAddress) != ANDROID_BITMAP_RESULT_SUCCESS)
    throw std::runtime_error("The rendered PDF bitmap is unavailable.");
  resource.pixelsLocked = true;
  QImage result(static_cast<const uchar*>(pixelsAddress), info.width, info.height, info.stride, QImage::Format_RGBA8888);
  return result.copy();
}
inline bool ShowPdf(wxWindow* parent, const wxString& title, const wxString& path) {
  wxDialog sheet(parent, wxID_ANY, title);
  sheet.GetHandle()->setProperty("cnDocumentSurface", true);
  auto* root = new wxBoxSizer(wxVERTICAL);
  auto* panel = new wxPanel(&sheet, wxID_ANY);
  auto* layout = new QVBoxLayout(panel->GetHandle());
  auto* row = new QHBoxLayout;
  auto* previous = new QPushButton("Previous", panel->GetHandle());
  auto* selector = new QSpinBox(panel->GetHandle());
  auto* total = new QLabel(panel->GetHandle());
  auto* next = new QPushButton("Next", panel->GetHandle());
  auto* go = new QPushButton("Go to page", panel->GetHandle());
  auto* zoom = new QComboBox(panel->GetHandle());
  zoom->setItemDelegate(new CN_AndroidChoiceDelegate(zoom));
  CN_EnableAndroidChoiceScrolling(zoom);
  zoom->addItem("Fit width", 1.0); zoom->addItem("150%", 1.5); zoom->addItem("200%", 2.0);
  selector->setButtonSymbols(QAbstractSpinBox::NoButtons);
  row->addWidget(previous); row->addWidget(next); row->addWidget(zoom);
  layout->addLayout(row);
  auto* pageRow = new QHBoxLayout;
  pageRow->addWidget(new QLabel("Page", panel->GetHandle()));
  pageRow->addWidget(selector, 1); pageRow->addWidget(total);
  pageRow->addWidget(go);
  layout->addLayout(pageRow);
  auto* scroll = new QScrollArea(panel->GetHandle());
  auto* image = new QLabel(scroll);
  image->setScaledContents(true);
  image->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  scroll->setWidget(image);
  scroll->setAlignment(Qt::AlignHCenter);
  QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
  layout->addWidget(scroll, 1);
  root->Add(panel, 1, wxEXPAND); sheet.SetSizer(root);
  panel->GetHandle()->setStyleSheet(QString("QPushButton, QSpinBox, QComboBox { min-height: %1px; font-size: %2pt; } QLabel { font-size: %2pt; }")
      .arg(CN_TouchHeight()).arg(CN_FontPointSize()));
  zoom->setItemDelegate(new CN_AndroidChoiceDelegate(zoom));
  Decorate(&sheet, title);
  int count = 0, current = 0, fittedWidth = 0;
  bool rendering = false;
  auto display = [&](int number) {
    if (rendering) return false;
    rendering = true;
    struct RenderGuard { bool& active; ~RenderGuard() { active = false; } } guard{rendering};
    if (auto* focus = QApplication::focusWidget()) focus->clearFocus();
    QGuiApplication::inputMethod()->hide();
    QImage page;
    wxString error;
    const int viewportWidth = scroll->isVisible() ? scroll->viewport()->width()
        : GetCanvasByIndex(0)->GetClientSize().x - 96;
    const int width = qMax(320, viewportWidth - 24) * zoom->currentData().toDouble();
    if (!RunJob(&sheet, _("Open PDF"), [&](JobState& state) {
      page = RenderPdfPage(path, number, width, &count, state);
    }, &error)) {
      if (!error.empty()) CelestialMessageBox(error, title, wxOK | wxICON_ERROR, &sheet);
      selector->blockSignals(true); selector->setValue(current + 1); selector->blockSignals(false);
      return false;
    }
    current = number;
    fittedWidth = viewportWidth;
    image->setPixmap(QPixmap::fromImage(page));
    image->resize(width, qRound(page.height() * double(width) / page.width()));
    selector->blockSignals(true); selector->setRange(1, count); selector->setValue(number + 1); selector->blockSignals(false);
    total->setText(QString("of %1").arg(count)); previous->setEnabled(number > 0); next->setEnabled(number + 1 < count);
    scroll->verticalScrollBar()->setValue(0); scroll->horizontalScrollBar()->setValue(0);
    return true;
  };
  if (!display(0)) return false;
  QObject::connect(previous, &QPushButton::clicked, panel->GetHandle(), [&]() { display(current - 1); });
  QObject::connect(next, &QPushButton::clicked, panel->GetHandle(), [&]() { display(current + 1); });
  // Rendering on each digit steals focus and truncates multi-digit entry.
  // Commit only with the visible Go action or the keyboard's Enter action.
  const auto goToPage = [&]() { selector->interpretText(); display(selector->value() - 1); };
  QObject::connect(go, &QPushButton::clicked, panel->GetHandle(), goToPage);
  if (auto* line = selector->findChild<QLineEdit*>())
    QObject::connect(line, &QLineEdit::returnPressed, panel->GetHandle(), goToPage);
  QObject::connect(zoom, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), panel->GetHandle(), [&](int) { display(current); });
  QTimer resizeTimer(panel->GetHandle());
  QObject::connect(&resizeTimer, &QTimer::timeout, &resizeTimer, [&]() {
    if (!rendering && scroll->isVisible() && zoom->currentIndex() == 0 &&
        std::abs(scroll->viewport()->width() - fittedWidth) > 8) display(current);
  });
  resizeTimer.start(250);
  sheet.ShowModal();
  return true;
}
} // namespace celestial_android
#endif
