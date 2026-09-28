// POBsoft (1985-2026): honour the host palette in owned Android Qt surfaces.
#pragma once
#ifdef __OCPN__ANDROID__
#include "ocpn_plugin.h"
#include <QWidget>
#include <QPalette>

inline QColor CN_HostColour(const char* name, QColor fallback) {
  wxColour colour;
  if (!GetGlobalColor(wxString::FromUTF8(name), &colour) || !colour.IsOk())
    return fallback;
  return QColor(colour.Red(), colour.Green(), colour.Blue());
}

inline void CN_ApplyAndroidTheme(QWidget* root) {
  if (!root || root->property("cnPreserveColour").toBool()) return;
  const QColor background = CN_HostColour("DILG0", QColor(228, 228, 228));
  const QColor field = CN_HostColour("DILG2", Qt::white);
  const QColor ink = CN_HostColour("DILG3", QColor(23, 56, 73));
  const QColor selected = CN_HostColour("UIBCK", QColor(90, 133, 155));
  const QColor muted((ink.red()+background.red())/2,
                     (ink.green()+background.green())/2,
                     (ink.blue()+background.blue())/2);
  QPalette palette = root->palette();
  for (auto role : {QPalette::Window, QPalette::AlternateBase})
    palette.setColor(role, background);
  for (auto role : {QPalette::Base, QPalette::Button}) palette.setColor(role, field);
  for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    palette.setColor(role, ink);
    palette.setColor(QPalette::Disabled, role, muted);
  }
  palette.setColor(QPalette::Highlight, selected);
  palette.setColor(QPalette::HighlightedText, ink);
  root->setPalette(palette);
  // Replace only our appended colour rules; sizing/gesture styles stay intact.
  QString style = root->styleSheet();
  const QString marker = "/* cn-host-colours */";
  const int previous = style.indexOf(marker);
  if (previous >= 0) style.truncate(previous);
  style += marker + QString(
      " QWidget { background-color: %1; color: %3; }"
      " QPushButton, QToolButton, QLineEdit, QSpinBox, QDoubleSpinBox,"
      " QDateTimeEdit, QComboBox, QTextEdit, QTextBrowser, QListWidget, QTableView"
      " { background-color: %2; color: %3; selection-background-color: %4;"
      " selection-color: %3; }"
      " QLabel, QCheckBox, QRadioButton, QGroupBox { color: %3; }"
      " QPushButton:disabled, QToolButton:disabled, QLineEdit:disabled,"
      " QComboBox:disabled, QLabel:disabled { color: %5; background-color: %2; }"
      " QListWidget::item:selected, QMenu::item:selected { background-color: %4; color: %3; }")
      .arg(background.name(), field.name(), ink.name(), selected.name(), muted.name());
  root->setStyleSheet(style);
  const auto children = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
  for (auto* child : children) CN_ApplyAndroidTheme(child);
}
#endif
