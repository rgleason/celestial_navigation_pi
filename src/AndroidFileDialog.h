// Touch browser for app-accessible files; keeps all widgets on the Qt GUI thread.
// Based on xWeatherRouting's GPL-3.0-or-later WeatherRoutingFileDialog.
#pragma once
#include <wx/filedlg.h>
#ifndef __OCPN__ANDROID__
using CelestialFileDialog = wxFileDialog;
#else
#include "AndroidSurface.h"
#include <QDir>
#include <QFileInfo>
#include <QListWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFile>
#include <wx/filename.h>

class CelestialFileDialog {
 public:
  CelestialFileDialog(wxWindow* parent, const wxString& title,
      const wxString& directory, const wxString& name, const wxString& masks, long flags)
      : parent_(parent), title_(title), directory_(directory), name_(name), masks_(masks), flags_(flags) {}
  int ShowModal() {
    using namespace celestial_android;
    path_.clear();
    wxDialog sheet(parent_, wxID_ANY, title_);
    sheet.GetHandle()->setProperty("cnDocumentSurface", true);
    auto* body = new wxBoxSizer(wxVERTICAL);
    auto* panel = new wxPanel(&sheet, wxID_ANY);
    auto* layout = new QVBoxLayout(panel->GetHandle());
    auto* locations = new QComboBox(panel->GetHandle());
    const QString privatePath = QString::fromUtf8(GetpPrivateApplicationDataLocation()->utf8_str());
    locations->addItem("OpenCPN files", privatePath);
    if (!directory_.empty()) locations->addItem("Current folder", QString::fromUtf8(directory_.utf8_str()));
    locations->addItem("Downloads (if accessible)", "/storage/emulated/0/Download");
    locations->setItemDelegate(new CN_AndroidChoiceDelegate(locations));
    layout->addWidget(locations);
    auto* navigation = new QHBoxLayout;
    auto* up = new QPushButton("Up", panel->GetHandle());
    auto* folder = new QLabel(panel->GetHandle()); folder->setWordWrap(true);
    navigation->addWidget(up); navigation->addWidget(folder, 1); layout->addLayout(navigation);
    auto* files = new QListWidget(panel->GetHandle());
    files->setItemDelegate(new CN_AndroidChoiceDelegate(files));
    files->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    QScroller::grabGesture(files->viewport(), QScroller::TouchGesture);
    layout->addWidget(files, 1);
    auto* name = new QLineEdit(QString::fromUtf8(name_.utf8_str()), panel->GetHandle());
    name->setPlaceholderText("File name"); layout->addWidget(name);
    auto* error = new QLabel(panel->GetHandle()); error->setWordWrap(true); error->setStyleSheet("color: #9e2525;"); layout->addWidget(error);
    auto* choose = new QPushButton(flags_ & wxFD_SAVE ? "Save" : "Open", panel->GetHandle());
    layout->addWidget(choose);
    panel->GetHandle()->setStyleSheet(QString("QPushButton, QLineEdit, QComboBox { min-height: %1px; font-size: %2pt; } QLabel, QListWidget { font-size: %2pt; }")
        .arg(CN_TouchHeight()).arg(CN_FontPointSize()));
    QDir directory(directory_.empty() ? privatePath : QString::fromUtf8(directory_.utf8_str()));
    if (!directory.exists()) directory = QDir(privatePath);
    QStringList masks;
    const auto parts = QString::fromUtf8(masks_.utf8_str()).split('|');
    for (int i = 1; i < parts.size(); i += 2) masks.append(parts[i].split(';', QString::SkipEmptyParts));
    if (masks.isEmpty()) masks << "*";
    auto refresh = [&]() {
      files->clear(); folder->setText(directory.absolutePath()); up->setEnabled(!directory.isRoot());
      const auto entries = directory.entryInfoList(masks, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
          QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
      for (const auto& entry : entries) {
        auto* item = new QListWidgetItem((entry.isDir() ? "Folder: " : "") + entry.fileName(), files);
        item->setData(Qt::UserRole, entry.absoluteFilePath()); item->setData(Qt::UserRole + 1, entry.isDir());
      }
      error->setText(entries.empty() ? "No matching files. Scoped storage can restrict folders; use OpenCPN files or import into that folder." : "");
    };
    QObject::connect(up, &QPushButton::clicked, panel->GetHandle(), [&]() { directory.cdUp(); refresh(); });
    QObject::connect(locations, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated), panel->GetHandle(), [&](int index) {
      const QDir candidate(locations->itemData(index).toString());
      if (!candidate.exists() || !QFileInfo(candidate.absolutePath()).isReadable()) error->setText("This folder is not accessible to OpenCPN.");
      else { directory = candidate; refresh(); }
    });
    QObject::connect(files, &QListWidget::itemClicked, panel->GetHandle(), [&](QListWidgetItem* item) {
      if (item->data(Qt::UserRole + 1).toBool()) { directory = QDir(item->data(Qt::UserRole).toString()); refresh(); }
      else name->setText(QFileInfo(item->data(Qt::UserRole).toString()).fileName());
    });
    QObject::connect(choose, &QPushButton::clicked, panel->GetHandle(), [&]() {
      const auto filename = name->text().trimmed();
      if (filename.isEmpty() || filename == "." || filename == ".." || filename.contains('/')) {
        error->setText("Enter a file name without folder separators."); return;
      }
      const auto path = directory.absoluteFilePath(filename);
      if (!(flags_ & wxFD_SAVE) && (!QFileInfo(path).isFile() || !QFileInfo(path).isReadable())) {
        error->setText("Choose an existing readable file."); return;
      }
      if ((flags_ & wxFD_SAVE) && !QFileInfo(directory.absolutePath()).isWritable()) {
        error->setText("This folder is not writable. Choose OpenCPN files."); return;
      }
      if ((flags_ & wxFD_SAVE) && (flags_ & wxFD_OVERWRITE_PROMPT) && QFileInfo(path).exists() &&
          !Confirm(&sheet, title_, _("Replace the existing file?"), _("Replace"))) return;
      path_ = wxString::FromUTF8(path.toUtf8().constData()); sheet.EndModal(wxID_OK);
    });
    body->Add(panel, 1, wxEXPAND); sheet.SetSizer(body);
    Decorate(&sheet, title_); refresh();
    return ModalResult(sheet) == wxID_OK && !path_.empty() ? wxID_OK : wxID_CANCEL;
  }
  wxString GetPath() const { return path_; }
 private:
  wxWindow* parent_;
  wxString title_, directory_, name_, masks_, path_;
  long flags_;
};
#endif
