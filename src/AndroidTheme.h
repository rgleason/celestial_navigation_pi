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

inline QColor CN_ThemeBackground() {
  const QColor background = CN_HostColour("DILG0", QColor(228, 228, 228));
  // Dusk's text/background greys have little contrast. Use the host's dark
  // input background for the entire low-light surface, with its text colour.
  return background.lightness() < 128
      ? CN_HostColour("DILG2", Qt::black) : background;
}

inline void CN_ApplyAndroidTheme(QWidget* root) {
  if (!root || root->property("cnPreserveColour").toBool()) return;
  const QColor background = CN_ThemeBackground();
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

inline void CN_ThemeWxColours(wxWindow* root) {
  if (!root || root->GetHandle()->property("cnPreserveColour").toBool()) return;
  // wxQt's erase/paint path uses wx colours, independent of the Qt palette.
  const QColor background = CN_ThemeBackground();
  const QColor ink = CN_HostColour("DILG3", QColor(23, 56, 73));
  root->SetBackgroundColour(wxColour(background.red(), background.green(), background.blue()));
  root->SetForegroundColour(wxColour(ink.red(), ink.green(), ink.blue()));
  for (auto* child : root->GetChildren()) CN_ThemeWxColours(child);
  root->Refresh();
}

inline void CN_ApplyAndroidTheme(wxWindow* root) {
  if (!root) return;
  CN_ThemeWxColours(root);
  CN_ApplyAndroidTheme(root->GetHandle());
}
#endif
