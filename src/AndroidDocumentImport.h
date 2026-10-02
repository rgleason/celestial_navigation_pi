// Android Storage Access Framework imports. No host Java copy or GUI-thread IO.
#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidJob.h"
#include <QtAndroidExtras/QtAndroid>
#include <QtAndroidExtras/QAndroidActivityResultReceiver>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QStorageInfo>
#include <QDir>
#include <array>
#include <memory>
#include <stdexcept>
#include <unistd.h>

namespace celestial_android {
// Most callers read synchronously. Eclipse verification retains this lease
// until its asynchronous verifier and installation have both finished.
struct ImportedDocument {
  QString path;
  ~ImportedDocument() { if (!path.isEmpty()) QFile::remove(path); }
};
inline void CleanAbandonedImports() {
  // Init runs before any document leases/workers exist. Only remove our own
  // incomplete staging files; installed data and the user's files are separate.
  const QDir folder(QString::fromUtf8(GetpPrivateApplicationDataLocation()->utf8_str()) + "/celestial-imports");
  for (const auto& entry : folder.entryInfoList({"import-*"}, QDir::Files)) QFile::remove(entry.absoluteFilePath());
}
inline void CheckDocumentJava(const char* operation) {
  QAndroidJniEnvironment env;
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    throw std::runtime_error(operation);
  }
}
struct DocumentSelection {
  std::mutex mutex;
  bool done = false, cancel = false;
  QAndroidJniObject uri;
  std::string failure;
};
class DocumentReceiver final : public QAndroidActivityResultReceiver {
 public:
  explicit DocumentReceiver(std::shared_ptr<DocumentSelection> state) : state_(state) {}
  void handleActivityResult(int, int result, const QAndroidJniObject& intent) override {
    std::lock_guard<std::mutex> lock(state_->mutex);
    try {
      if (result == -1 && intent.isValid()) {
        state_->uri = intent.callObjectMethod("getData", "()Landroid/net/Uri;");
        CheckDocumentJava("Android did not return a readable document.");
      }
    } catch (const std::exception& ex) { state_->failure = ex.what(); }
    state_->done = true;
  }
 private:
  std::shared_ptr<DocumentSelection> state_;
};
struct DocumentJavaResource {
  QAndroidJniObject object;
  ~DocumentJavaResource() {
    if (object.isValid()) object.callMethod<void>("close");
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
};
inline std::shared_ptr<ImportedDocument> ChooseDeviceDocument(
    wxWindow* parent, const QStringList& masks, wxString* error) {
  auto selection = std::make_shared<DocumentSelection>();
  auto receiver = std::make_shared<DocumentReceiver>(selection);
  wxDialog waiting(parent, wxID_ANY, _("Choose a device file"));
  auto* layout = new wxBoxSizer(wxVERTICAL);
  auto* message = new wxStaticText(&waiting, wxID_ANY,
      _("Opening Android's document chooser. Choose a file, or use Back in the chooser to cancel."));
  layout->Add(message, 0, wxEXPAND | wxALL, 16);
  waiting.SetSizer(layout);
  waiting.GetHandle()->setProperty("cnCancellable", true);
  auto cancel = [&]() {
    std::lock_guard<std::mutex> lock(selection->mutex);
    selection->cancel = true;
    message->SetLabel(_("Cancelling. Close the Android chooser to return."));
  };
  Decorate(&waiting, _("Choose a device file"), cancel);
  waiting.Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event) { cancel(); event.Veto(); });
  // Shared ownership covers the queued Android-thread launch and result callback.
  // The receiver uses its own Qt-assigned request code; no host chooser callback
  // runs and OpenCPN's Java onActivityResult delegates this result back to Qt.
  QtAndroid::runOnAndroidThread([selection, receiver]() {
    try {
      {
        std::lock_guard<std::mutex> lock(selection->mutex);
        if (selection->cancel) { selection->done = true; return; }
      }
      auto action = QAndroidJniObject::fromString("android.intent.action.OPEN_DOCUMENT");
      QAndroidJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object());
      auto type = QAndroidJniObject::fromString("*/*");
      auto category = QAndroidJniObject::fromString("android.intent.category.OPENABLE");
      intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;", type.object());
      intent.callObjectMethod("addCategory", "(Ljava/lang/String;)Landroid/content/Intent;", category.object());
      intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", jint(1));
      CheckDocumentJava("The Android document chooser could not be prepared.");
      QtAndroid::startActivity(intent, 1, receiver.get());
      CheckDocumentJava("This host cannot open the Android document chooser.");
    } catch (const std::exception& ex) {
      std::lock_guard<std::mutex> lock(selection->mutex);
      selection->failure = ex.what(); selection->done = true;
    }
  });
  QTimer timer(waiting.GetHandle());
  QObject::connect(&timer, &QTimer::timeout, &timer, [&]() {
    bool done;
    { std::lock_guard<std::mutex> lock(selection->mutex); done = selection->done; }
    if (done) { timer.stop(); waiting.EndModal(wxID_OK); }
  });
  timer.start(50);
  waiting.ShowModal();
  QAndroidJniObject uri;
  {
    std::lock_guard<std::mutex> lock(selection->mutex);
    if (error && !selection->failure.empty()) *error = wxString::FromUTF8(selection->failure.c_str());
    if (selection->cancel || !selection->failure.empty() || !selection->uri.isValid()) return {};
    uri = selection->uri;
  }
  std::shared_ptr<ImportedDocument> imported;
  const QString folder = QString::fromUtf8(GetpPrivateApplicationDataLocation()->utf8_str()) + "/celestial-imports";
  const bool completed = RunJob(parent, _("Copy device file"), [&](JobState& state) {
    state.Progress("Reading the selected device file...");
    const auto resolver = QtAndroid::androidActivity().callObjectMethod(
        "getContentResolver", "()Landroid/content/ContentResolver;");
    CheckDocumentJava("Android document access is unavailable.");
    DocumentJavaResource cursor;
    cursor.object = resolver.callObjectMethod("query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
        uri.object(), jobjectArray(nullptr), jstring(nullptr), jobjectArray(nullptr), jstring(nullptr));
    CheckDocumentJava("The selected file's details could not be read.");
    QString filename;
    qint64 length = -1;
    if (cursor.object.isValid() && cursor.object.callMethod<jboolean>("moveToFirst")) {
      auto nameColumn = QAndroidJniObject::fromString("_display_name");
      auto sizeColumn = QAndroidJniObject::fromString("_size");
      const int ni = cursor.object.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I", nameColumn.object());
      const int si = cursor.object.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I", sizeColumn.object());
      if (ni >= 0) filename = cursor.object.callObjectMethod("getString", "(I)Ljava/lang/String;", jint(ni)).toString();
      if (si >= 0 && !cursor.object.callMethod<jboolean>("isNull", "(I)Z", jint(si)))
        length = cursor.object.callMethod<jlong>("getLong", "(I)J", jint(si));
      CheckDocumentJava("The selected file has invalid document details.");
    }
    filename = QFileInfo(filename).fileName();
    if (filename.isEmpty() || !QDir::match(masks, filename))
      throw std::runtime_error("Choose a file of the requested type.");
    if (!QDir().mkpath(folder)) throw std::runtime_error("The import staging folder could not be created.");
    QStorageInfo storage(folder);
    if (length > 0 && storage.isValid() && storage.bytesAvailable() >= 0 &&
        length > storage.bytesAvailable() - 64LL * 1024 * 1024)
      throw std::runtime_error("Insufficient storage to copy this document; 64 MB is reserved for OpenCPN.");
    DocumentJavaResource descriptor;
    auto read = QAndroidJniObject::fromString("r");
    descriptor.object = resolver.callObjectMethod("openFileDescriptor",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;", uri.object(), read.object());
    CheckDocumentJava("The selected file could not be opened. Check that it is available offline.");
    if (!descriptor.object.isValid()) throw std::runtime_error("Android returned no readable file.");
    const int fd = descriptor.object.callMethod<jint>("detachFd");
    CheckDocumentJava("The selected document's file descriptor could not be opened.");
    QFile input;
    if (!input.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
      ::close(fd); throw std::runtime_error("The selected document could not be read.");
    }
    QTemporaryFile output(folder + "/import-XXXXXX-" + filename);
    if (!output.open()) throw std::runtime_error("The import staging file could not be created.");
    std::array<char, 256 * 1024> buffer;
    qint64 copied = 0, lastProgress = -1;
    for (;;) {
      state.Checkpoint();
      const auto count = input.read(buffer.data(), buffer.size());
      if (count < 0) throw std::runtime_error("The selected document could not be completely read.");
      if (!count) break;
      if (output.write(buffer.data(), count) != count) throw std::runtime_error("The document could not be copied; check free storage.");
      copied += count;
      const auto mb = copied / (1024 * 1024);
      if (mb != lastProgress) {
        lastProgress = mb;
        state.Progress("Copying " + filename.toStdString() + ": " + std::to_string(mb) + " MB" +
            (length > 0 ? " / " + std::to_string(length / (1024 * 1024)) + " MB" : ""));
      }
    }
    if (length >= 0 && copied != length) throw std::runtime_error("The copied document has an unexpected size.");
    if (!output.flush()) throw std::runtime_error("The copied document could not be saved.");
    auto lease = std::make_shared<ImportedDocument>();
    lease->path = output.fileName();
    state.Commit([&]() { output.close(); output.setAutoRemove(false); imported = lease; return true; });
  }, error);
  return completed ? imported : std::shared_ptr<ImportedDocument>();
}
inline bool InstallDocument(wxWindow* parent, const wxString& source,
                            const wxString& destination, wxString* error) {
  const bool completed = RunJob(parent, _("Install verified astronomy data"), [&](JobState& state) {
    QFile input(QString::fromUtf8(source.utf8_str()));
    QSaveFile output(QString::fromUtf8(destination.utf8_str()));
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
      throw std::runtime_error("The verified data file could not be opened for installation.");
    std::array<char, 256 * 1024> buffer;
    qint64 copied = 0, lastProgress = -1;
    for (;;) {
      state.Checkpoint();
      const auto count = input.read(buffer.data(), buffer.size());
      if (count < 0) throw std::runtime_error("The verified data file could not be completely read.");
      if (!count) break;
      if (output.write(buffer.data(), count) != count)
        throw std::runtime_error("The verified data file could not be written; check free storage.");
      copied += count;
      const auto mb = copied / (1024 * 1024);
      if (mb != lastProgress) {
        lastProgress = mb; state.Progress("Installing verified data: " + std::to_string(mb) + " MB");
      }
    }
    if (!state.Commit([&]() { return output.commit(); }))
      throw std::runtime_error("The verified data file could not be installed atomically.");
  }, error);
  if (!completed && error && error->empty()) *error = _("Astronomy-data installation was cancelled; the previous installed file was retained.");
  return completed;
}
} // namespace celestial_android
#endif
