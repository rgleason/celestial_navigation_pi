#pragma once

#ifdef __OCPN__ANDROID__

#include <wx/wx.h>
#include <wx/weakref.h>
#include <wx/spinctrl.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <QWidget>
#include <QLineEdit>
#include <QInputMethod>
#include <QScroller>
#include <QScrollEvent>
#include <QScrollPrepareEvent>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QSlider>
#include <QTouchEvent>
#include <QMouseEvent>
#include <QAbstractButton>
#include <QElapsedTimer>
#include <QPointer>
#include <QStyle>
#include <QTextEdit>
#include <QTextDocument>
#include <QAbstractSpinBox>
#include <QDateTimeEdit>
#include <QCalendarWidget>
#include <QTableView>
#include <QHeaderView>
#include <QToolButton>
#include <QMenu>
#include <wx/slider.h>
#include <QFontMetrics>
#include <QFileInfo>
#include <QAbstractItemView>
#include <QTabWidget>
#include <QTabBar>
#include "ocpn_plugin.h"
#include <functional>
#include <vector>

// Touch/scroll adapters adapted from xWeatherRouting (GPL-3.0-or-later).
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QtAndroid>
inline int CN_TouchHeight() {
  const auto resources = QtAndroid::androidActivity().callObjectMethod(
      "getResources", "()Landroid/content/res/Resources;");
  const auto metrics = resources.callObjectMethod(
      "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
  const float density = metrics.isValid() ? metrics.getField<jfloat>("density") : 1.5f;
  return qMax(48, qRound(48 * density));
}

inline int CN_FontPointSize(int base = 16) {
  const auto resources = QtAndroid::androidActivity().callObjectMethod(
      "getResources", "()Landroid/content/res/Resources;");
  const auto metrics = resources.callObjectMethod(
      "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
  if (!metrics.isValid()) return base;
  const float density = metrics.getField<jfloat>("density");
  const float scaled = metrics.getField<jfloat>("scaledDensity");
  return density > 0 ? qMax(base, qRound(base * scaled / density)) : base;
}
inline void CN_EnableAndroidCalendarScrolling(QCalendarWidget* calendar);
inline void CN_StyleAndroidCalendar(wxWindow* window) {
  auto* calendar = qobject_cast<QCalendarWidget*>(window->GetHandle());
  if (!calendar) return;
  const int target = CN_TouchHeight();
  CN_EnableAndroidCalendarScrolling(calendar);
  calendar->setStyleSheet(QString("QCalendarWidget { font-size: %1pt; } "
      "QToolButton { min-width: %2px; min-height: %2px; } "
      "QMenu::item { padding: %3px 16px; font-size: %1pt; }")
      .arg(CN_FontPointSize()).arg(target).arg(target / 3 + 2));
  for (auto* menu : calendar->findChildren<QMenu*>()) menu->setMinimumWidth(300);
  for (auto* view : calendar->findChildren<QTableView*>()) {
    QFont font = view->font(); font.setPointSize(CN_FontPointSize()); view->setFont(font);
    view->verticalHeader()->setMinimumSectionSize(target);
    view->verticalHeader()->setDefaultSectionSize(target);
    for (int row = 0; row < view->model()->rowCount(); ++row)
      view->setRowHeight(row, target);
  }
  window->SetMinSize(wxSize(7 * target, 8 * target));
}
inline void CN_EnableAndroidDocumentScrolling(QTextEdit* text);
inline void CN_StyleAndroidDocument(wxWindow* window) {
  auto* text = qobject_cast<QTextEdit*>(window->GetHandle());
  if (!text) return;
  // POBsoft (1985-2026): wxQt does not reliably map wxTE_READONLY at
  // creation. Use the declared wx style before choosing touch interaction.
  const auto* field = wxDynamicCast(window, wxTextCtrl);
  if (field && (field->GetWindowStyleFlag() & wxTE_READONLY))
    text->setReadOnly(true);
  if (text->isReadOnly()) {
    text->setTextInteractionFlags(Qt::NoTextInteraction);
    text->setFocusPolicy(Qt::NoFocus);
    text->setLineWrapMode(QTextEdit::WidgetWidth);
    text->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QScroller::grabGesture(text->viewport(), QScroller::TouchGesture);
    CN_EnableAndroidDocumentScrolling(text);
    QFont font = text->font(); font.setPointSize(CN_FontPointSize());
    text->document()->setDefaultFont(font);
  }
  QFont font = text->font(); font.setPointSize(CN_FontPointSize()); text->setFont(font);
}


class CN_AndroidChoiceDelegate : public QStyledItemDelegate {
public:
  explicit CN_AndroidChoiceDelegate(QObject* parent)
      : QStyledItemDelegate(parent) {}
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(qMax(size.height(), CN_TouchHeight()));
    return size;
  }
};

class CN_AndroidFileDelegate : public CN_AndroidChoiceDelegate {
public:
  explicit CN_AndroidFileDelegate(QObject* parent)
      : CN_AndroidChoiceDelegate(parent) {}
protected:
  void initStyleOption(QStyleOptionViewItem* option,
                       const QModelIndex& index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    option->text = QFileInfo(option->text).fileName();
  }
};

inline void CN_StyleAndroidFileList(wxWindow* window) {
  auto* view = qobject_cast<QAbstractItemView*>(window->GetHandle());
  if (!view) view = window->GetHandle()->findChild<QAbstractItemView*>();
  if (view) view->setItemDelegate(new CN_AndroidFileDelegate(view));
}

inline void CN_EnableAndroidChoiceScrolling(QComboBox* combo);

inline void CN_StyleAndroidCombo(wxWindow* window) {
  auto* combo = qobject_cast<QComboBox*>(window->GetHandle());
  if (!combo) combo = window->GetHandle()->findChild<QComboBox*>();
  if (combo) {
    if (wxDynamicCast(window, wxChoice)) combo->setEditable(false);
    combo->setItemDelegate(new CN_AndroidChoiceDelegate(combo));
    if (!combo->isEditable()) CN_EnableAndroidChoiceScrolling(combo);
  }
}

inline void CN_WrapAndroidText(wxStaticText* text, const wxString& value,
                               int width) {
  width = qMax(width, 160);
  if (text->GetLabel() != value) text->SetLabel(value);
  if (auto* label = qobject_cast<QLabel*>(text->GetHandle())) {
    label->ensurePolished();
    label->setWordWrap(true);
    const QRect bounds = QFontMetrics(label->font()).boundingRect(
        QRect(0, 0, width, 10000), Qt::TextWordWrap, label->text());
    text->SetMinSize(wxSize(0, bounds.height() + 8));
    text->SetMaxSize(wxSize(width, -1));
  }
}

// wxQt uses wxScrollHelper rather than QScrollArea's content model. Supply
// QScroller's geometry and apply its pixel positions through the wx API.
class CN_AndroidScrollFilter : public QObject {
public:
  CN_AndroidScrollFilter(wxScrolledWindow* window, QWidget* target)
      : QObject(target), m_window(window) { target->installEventFilter(this); }
protected:
  bool eventFilter(QObject*, QEvent* event) override {
    if (!m_window) return false;
    int xUnit, yUnit, x, y;
    m_window->GetScrollPixelsPerUnit(&xUnit, &yUnit);
    m_window->GetViewStart(&x, &y);
    if (event->type() == QEvent::ScrollPrepare) {
      auto* prepare = static_cast<QScrollPrepareEvent*>(event);
      const wxSize view = m_window->GetClientSize();
      const wxSize content = m_window->GetVirtualSize();
      prepare->setViewportSize(QSizeF(view.x, view.y));
      prepare->setContentPosRange(QRectF(0, 0,
          xUnit ? qMax(0, content.x - view.x) : 0,
          yUnit ? qMax(0, content.y - view.y) : 0));
      prepare->setContentPos(QPointF(x * xUnit, y * yUnit));
      prepare->accept();
      return true;
    }
    if (event->type() == QEvent::Scroll) {
      const QPointF position = static_cast<QScrollEvent*>(event)->contentPos();
      m_window->Scroll(xUnit ? qRound(position.x() / xUnit) : -1,
                       yUnit ? qRound(position.y() / yUnit) : -1);
      // wxQt can scroll the viewport pixels without moving reparented static
      // boxes. Lay out their controls at the actual wx scroll offset as well.
      m_window->Layout();
      event->accept();
      return true;
    }
    return false;
  }
private:
  wxWeakRef<wxScrolledWindow> m_window;
};

inline void CN_EnableAndroidScrolling(wxScrolledWindow* window) {
  QWidget* target = window->GetHandle();
  if (target->property("cnTouchScroll").toBool()) return;
  target->setProperty("cnTouchScroll", true);
  target->setAttribute(Qt::WA_AcceptTouchEvents);
  new CN_AndroidScrollFilter(window, target);
  QScroller::grabGesture(target, QScroller::TouchGesture);
  wxWeakRef<wxScrolledWindow> weakWindow(window);
  const auto afterScroll = [weakWindow](wxScrollWinEvent& event) {
    if (weakWindow) QTimer::singleShot(0, weakWindow->GetHandle(), [weakWindow]() {
      if (weakWindow) weakWindow->Layout();
    });
    event.Skip();
  };
  for (const auto& type : {wxEVT_SCROLLWIN_TOP, wxEVT_SCROLLWIN_BOTTOM,
                          wxEVT_SCROLLWIN_LINEUP, wxEVT_SCROLLWIN_LINEDOWN,
                          wxEVT_SCROLLWIN_PAGEUP, wxEVT_SCROLLWIN_PAGEDOWN,
                          wxEVT_SCROLLWIN_THUMBTRACK, wxEVT_SCROLLWIN_THUMBRELEASE})
    window->Bind(type, afterScroll);
}

// Forward drags to the scrolling sheet or an explicit native viewport; perform
// an action only on a stationary release, never on the initial press.
class CN_AndroidButtonDragFilter : public QObject {
public:
  explicit CN_AndroidButtonDragFilter(QWidget* widget, std::function<void(QPoint)> tap = {},
                                     QWidget* scrollTarget = nullptr)
      : QObject(widget), m_widget(widget), m_tap(std::move(tap)),
        m_button(qobject_cast<QAbstractButton*>(widget)),
        m_combo(qobject_cast<QComboBox*>(widget)), m_scrollTarget(scrollTarget) {
    widget->setAttribute(Qt::WA_AcceptTouchEvents);
    widget->installEventFilter(this);
  }
protected:
  bool eventFilter(QObject*, QEvent* event) override {
    const bool touch = event->type() == QEvent::TouchBegin ||
        event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd ||
        event->type() == QEvent::TouchCancel;
    const bool mouse = event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease;
    if (!touch && !mouse) return false;
    QPoint global;
    bool begin, end, cancel = false;
    if (touch) {
      auto* input = static_cast<QTouchEvent*>(event);
      if (input->touchPoints().isEmpty()) return false;
      global = input->touchPoints().first().screenPos().toPoint();
      begin = event->type() == QEvent::TouchBegin;
      end = event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel;
      cancel = event->type() == QEvent::TouchCancel;
    } else {
      auto* input = static_cast<QMouseEvent*>(event);
      // Android/Qt may synthesize mouse events after an accepted touch ends.
      // Our touch release already clicked the button; consume that second
      // sequence so destructive actions execute once.
      if (m_touchActive || (m_suppressMouse && m_clock.elapsed() < 500)) return true;
      m_suppressMouse = false;
      if (event->type() != QEvent::MouseMove && input->button() != Qt::LeftButton)
        return false;
      global = input->globalPos();
      begin = event->type() == QEvent::MouseButtonPress;
      end = event->type() == QEvent::MouseButtonRelease;
    }
    if (begin) {
      if (!m_widget->isEnabled()) return false;
      m_active = true;
      m_touchActive = touch;
      m_moved = false;
      m_origin = global;
      m_clock.start();
      m_scroll = m_scrollTarget;
      if (!m_scroll)
        for (QWidget* parent = m_widget->parentWidget(); parent; parent = parent->parentWidget())
          if (parent->property("cnTouchScroll").toBool()) { m_scroll = parent; break; }
      if (!m_button && !m_combo && !m_scroll && !m_tap) {
        m_active = m_touchActive = false;
        return false;
      }
      if (m_button) m_button->setDown(true);
      if (m_scroll) QScroller::scroller(m_scroll)->handleInput(QScroller::InputPress,
          m_scroll->mapFromGlobal(global), 0);
    } else if (!m_active) return false;
    if ((global - m_origin).manhattanLength() > qMax(12, QApplication::startDragDistance()))
      m_moved = true;
    if (m_moved && m_button) m_button->setDown(false);
    if (m_scroll && !begin)
      QScroller::scroller(m_scroll)->handleInput(end ? QScroller::InputRelease : QScroller::InputMove,
          m_scroll->mapFromGlobal(global), m_clock.elapsed());
    if (end) {
      const bool click = (m_button || m_combo || m_tap) && !cancel && !m_moved &&
          m_widget->isEnabled() &&
          m_widget->rect().contains(m_widget->mapFromGlobal(global));
      m_active = m_touchActive = false;
      if (m_button) m_button->setDown(false);
      if (touch) { m_suppressMouse = true; m_clock.restart(); }
      if (click) {
        if (m_tap) m_tap(m_widget->mapFromGlobal(global));
        else if (m_button) m_button->click();
        else m_combo->showPopup();
      }
    }
    event->accept();
    return true;
  }
private:
  QPointer<QWidget> m_widget;
  std::function<void(QPoint)> m_tap;
  QAbstractButton* m_button;
  QComboBox* m_combo;
  QPointer<QWidget> m_scrollTarget;
  QPointer<QWidget> m_scroll;
  QPoint m_origin;
  QElapsedTimer m_clock;
  bool m_active = false, m_touchActive = false, m_moved = false;
  bool m_suppressMouse = false;
};

inline void CN_EnableAndroidDocumentScrolling(QTextEdit* text) {
  if (text->property("cnDocumentDrag").toBool()) return;
  text->setProperty("cnDocumentDrag", true);
  // POBsoft (1985-2026): consume synthesized mouse selection as well as
  // touch drags; the report owns its viewport rather than a scrolling page.
  new CN_AndroidButtonDragFilter(text->viewport(), {}, text->viewport());
}

inline void CN_EnableAndroidCalendarScrolling(QCalendarWidget* calendar) {
  if (calendar->property("cnCalendarDrag").toBool()) return;
  calendar->setProperty("cnCalendarDrag", true);
  QPointer<QCalendarWidget> safeCalendar(calendar);
  for (auto* view : calendar->findChildren<QTableView*>()) {
    QPointer<QTableView> safeView(view);
    new CN_AndroidButtonDragFilter(view->viewport(), [safeCalendar, safeView](QPoint point) {
      if (!safeCalendar || !safeView) return;
      const auto index = safeView->indexAt(point);
      const int column = index.column() - (safeView->model()->columnCount() - 7);
      if (!index.isValid() || index.row() < 1 || column < 0) return;
      const QDate first(safeCalendar->yearShown(), safeCalendar->monthShown(), 1);
      const int offset = (first.dayOfWeek() - safeCalendar->firstDayOfWeek() + 7) % 7;
      const auto chosen = first.addDays((index.row() - 1) * 7 + column - offset);
      if (chosen >= safeCalendar->minimumDate() && chosen <= safeCalendar->maximumDate())
        safeCalendar->setSelectedDate(chosen);
    });
  }
}

inline void CN_EnableAndroidChoiceScrolling(QComboBox* combo) {
  if (combo->property("cnChoiceDrag").toBool()) return;
  combo->setProperty("cnChoiceDrag", true);
  new CN_AndroidButtonDragFilter(combo);
  auto* view = combo->view();
  view->window()->setProperty("cnChoiceOwner", QVariant::fromValue<QObject*>(combo));
  view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  QPointer<QComboBox> safeCombo(combo);
  QPointer<QAbstractItemView> safeView(view);
  QObject::connect(QApplication::primaryScreen(), &QScreen::geometryChanged,
                   combo, [safeCombo](const QRect&) {
    // Qt 5.12 retains the old popup geometry across Android rotation.
    // Dismiss without activation; reopening uses the new screen bounds.
    QTimer::singleShot(0, safeCombo, [safeCombo]() {
      if (safeCombo) safeCombo->hidePopup();
    });
  });
  new CN_AndroidButtonDragFilter(view->viewport(),
      [safeCombo, safeView](QPoint point) {
        if (!safeCombo || !safeView) return;
        const auto index = safeView->indexAt(point);
        if (!index.isValid() || !(index.flags() & Qt::ItemIsEnabled)) return;
        const int row = index.row();
        safeCombo->hidePopup();
        // wx handlers can rebuild forms. Finish the viewport callback first.
        QTimer::singleShot(0, safeCombo, [safeCombo, row]() {
          if (!safeCombo || row >= safeCombo->count()) return;
          safeCombo->setCurrentIndex(row);
          emit safeCombo->activated(row);
        });
      }, view->viewport());
}

inline void CN_EnableAndroidButton(wxButton* control) {
  auto* button = qobject_cast<QAbstractButton*>(control->GetHandle());
  if (!button || button->property("cnButtonDrag").toBool()) return;
  button->setProperty("cnButtonDrag", true);
  new CN_AndroidButtonDragFilter(button);
}

// wxQt labels consume synthesized mouse drags before an ancestor's QScroller
// sees them. They have no tap action; forward their drags like button drags.
inline void CN_EnableAndroidLabelScrolling(wxWindow* label) {
  QWidget* widget = label->GetHandle();
  if (widget->property("cnLabelDrag").toBool()) return;
  widget->setProperty("cnLabelDrag", true);
  new CN_AndroidButtonDragFilter(widget);
}

// Sliders inside a scrolling sheet must consume their own touch sequence.
class CN_AndroidSliderTouchFilter : public QObject {
public:
  explicit CN_AndroidSliderTouchFilter(QSlider* slider)
      : QObject(slider), m_slider(slider) {
    slider->setAttribute(Qt::WA_AcceptTouchEvents);
    slider->installEventFilter(this);
  }
protected:
  bool eventFilter(QObject*, QEvent* event) override {
    if (event->type() != QEvent::TouchBegin && event->type() != QEvent::TouchUpdate &&
        event->type() != QEvent::TouchEnd && event->type() != QEvent::TouchCancel)
      return false;
    const auto* touch = static_cast<QTouchEvent*>(event);
    if (!touch->touchPoints().isEmpty()) {
      const auto point = touch->touchPoints().first().pos();
      const bool horizontal = m_slider->orientation() == Qt::Horizontal;
      const int length = horizontal ? m_slider->width() : m_slider->height();
      const int position = qRound(horizontal ? point.x() : point.y());
      bool upsideDown = horizontal ? m_slider->invertedAppearance()
                                   : !m_slider->invertedAppearance();
      if (horizontal && m_slider->layoutDirection() == Qt::RightToLeft)
        upsideDown = !upsideDown;
      m_slider->setSliderDown(event->type() != QEvent::TouchEnd);
      m_slider->setValue(QStyle::sliderValueFromPosition(
          m_slider->minimum(), m_slider->maximum(), position - 16,
          qMax(1, length - 32), upsideDown));
    }
    if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel)
      m_slider->setSliderDown(false);
    event->accept();
    return true;
  }
private:
  QSlider* m_slider;
};

inline void CN_EnableAndroidSlider(wxSlider* control,
                                    std::function<void()> changed) {
  auto* slider = qobject_cast<QSlider*>(control->GetHandle());
  if (!slider) slider = control->GetHandle()->findChild<QSlider*>();
  if (!slider) return;
  const int target = CN_TouchHeight();
  control->SetMinSize(wxSize(180, target));
  slider->setMinimumHeight(target);
  slider->setStyleSheet(QString("QSlider::groove:horizontal { height: 10px; background: #9fb9c6; } "
      "QSlider::handle:horizontal { width: %1px; margin: -%2px 0; background: #173849; "
      "border: 1px solid white; border-radius: 8px; }").arg(target/2).arg(target/4));
  new CN_AndroidSliderTouchFilter(slider);
  QObject::connect(slider, &QSlider::valueChanged, slider,
                   [changed](int) { changed(); });
}

inline void CN_StyleAndroidControls(wxWindow* parent) {
  if (auto* scroll = wxDynamicCast(parent, wxScrolledWindow))
    CN_EnableAndroidScrolling(scroll);
  for (auto* child : parent->GetChildren()) {
    if (child->GetName() == "cn-android-header" ||
        child->GetName() == "cn-android-title") continue;
    CN_StyleAndroidCalendar(child);
    CN_StyleAndroidDocument(child);
    const bool label = wxDynamicCast(child, wxStaticText);
    if (label) CN_EnableAndroidLabelScrolling(child);
    const bool choice = wxDynamicCast(child, wxChoice) ||
                        wxDynamicCast(child, wxComboBox);
    const bool button = wxDynamicCast(child, wxButton);
    if (button) CN_EnableAndroidButton(static_cast<wxButton*>(child));
    const bool check = wxDynamicCast(child, wxCheckBox) ||
                       wxDynamicCast(child, wxRadioButton);
    if (check && !child->GetHandle()->property("cnButtonDrag").toBool()) {
      child->GetHandle()->setProperty("cnButtonDrag", true);
      new CN_AndroidButtonDragFilter(child->GetHandle());
    }
    const bool input = wxDynamicCast(child, wxTextCtrl) ||
                       wxDynamicCast(child, wxSpinCtrl) ||
                       wxDynamicCast(child, wxSpinCtrlDouble);
    auto* date = qobject_cast<QDateTimeEdit*>(child->GetHandle());
    if (!date) date = child->GetHandle()->findChild<QDateTimeEdit*>();
    if (date) {
      date->setMinimumHeight(CN_TouchHeight());
      date->setStyleSheet(QString("QDateTimeEdit { min-height: %1px; font-size: %2pt; }")
          .arg(CN_TouchHeight()).arg(CN_FontPointSize()));
      child->SetMinSize(wxSize(180, CN_TouchHeight()));
      if (date->calendarWidget()) {
        date->calendarWidget()->setStyleSheet(QString("QCalendarWidget { font-size: %1pt; } QToolButton { min-height: %2px; min-width: %2px; } QMenu::item { padding: %3px 16px; }")
            .arg(CN_FontPointSize()).arg(CN_TouchHeight()).arg(CN_TouchHeight()/3+2));
        for (auto* view : date->calendarWidget()->findChildren<QTableView*>()) {
          view->verticalHeader()->setMinimumSectionSize(CN_TouchHeight());
          view->verticalHeader()->setDefaultSectionSize(CN_TouchHeight());
        }
      }
    }
    if (auto* line = qobject_cast<QLineEdit*>(child->GetHandle())) {
      if (!line->property("cnInputDrag").toBool()) {
        line->setProperty("cnInputDrag", true);
        if (line->isReadOnly()) {
          line->setFocusPolicy(Qt::NoFocus);
          new CN_AndroidButtonDragFilter(line);
        } else {
          QPointer<QLineEdit> safeLine(line);
          new CN_AndroidButtonDragFilter(line, [safeLine](QPoint) {
            if (!safeLine) return;
            safeLine->setFocus(Qt::MouseFocusReason);
            QGuiApplication::inputMethod()->show();
          });
        }
      }
    }
    const bool list = wxDynamicCast(child, wxListBox) ||
                      wxDynamicCast(child, wxListCtrl);
    if (wxDynamicCast(child, wxSpinCtrl) || wxDynamicCast(child, wxSpinCtrlDouble)) {
      if (auto* spin = qobject_cast<QAbstractSpinBox*>(child->GetHandle()))
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
      child->SetMaxSize(wxSize(-1, -1));
      child->SetMinSize(wxSize(180, CN_TouchHeight()));
    }
    if (label || choice || button || check || input || list) {
      wxFont font = child->GetFont();
      font.SetPointSize(CN_FontPointSize());
      child->SetFont(font);
      if (button)
        child->GetHandle()->setStyleSheet(
            "QPushButton { font-size: 16pt; min-height: 62px; padding: 5px; "
            "border: 1px solid #9fb9c6; border-radius: 8px; "
            "color: #173849; background: white; } "
            "QPushButton:disabled { color: #74818a; background: #e4e8eb; }");
      else if (check)
        child->GetHandle()->setStyleSheet(
            "QCheckBox, QRadioButton { font-size: 16pt; min-height: 64px; } "
            "QCheckBox::indicator, QRadioButton::indicator { width: 30px; height: 30px; }");
      else if (choice) {
        CN_StyleAndroidCombo(child);
        child->GetHandle()->setStyleSheet(
            "QComboBox { font-size: 16pt; min-height: 64px; }");
      } else if (label) {
        const wxColour colour = child->GetForegroundColour();
        child->GetHandle()->setStyleSheet(colour.IsOk()
            ? QString("QLabel { font-size: 16pt; color: %1; }").arg(
                QString::fromUtf8(colour.GetAsString(wxC2S_HTML_SYNTAX).ToUTF8().data()))
            : QString("QLabel { font-size: 16pt; }"));
      }
      else if (input)
        child->GetHandle()->setStyleSheet(
            "QLineEdit, QSpinBox, QDoubleSpinBox { font-size: 16pt; min-height: 64px; } "
            "QTextEdit[readOnly=\"true\"] { font-size: 16pt; }");
      QString style = child->GetHandle()->styleSheet();
      style.replace("font-size: 16pt", QString("font-size: %1pt").arg(CN_FontPointSize()));
      child->GetHandle()->setStyleSheet(style);
      if (choice || button || check || input) {
        const wxSize minimum = child->GetMinSize();
        child->SetMinSize(wxSize(minimum.x, qMax(minimum.y, CN_TouchHeight())));
      }
      if (list) {
        auto* view = qobject_cast<QAbstractItemView*>(child->GetHandle());
        if (!view) view = child->GetHandle()->findChild<QAbstractItemView*>();
        if (view) view->setItemDelegate(new CN_AndroidChoiceDelegate(view));
      }
    }
    CN_StyleAndroidControls(child);
  }
}


#endif
