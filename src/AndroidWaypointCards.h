#pragma once
#ifdef __OCPN__ANDROID__
#include "AndroidTouch.h"
#include <wx/panel.h>
#include <QListWidget>
#include <QScrollBar>
#include <QVBoxLayout>

// POBsoft (1985-2026): waypoint cards own their viewport. Their row height
// follows wrapping and the Android font, rather than desktop column widths.
class CN_WaypointCardDelegate : public QStyledItemDelegate {
 public:
  explicit CN_WaypointCardDelegate(QListWidget* list)
      : QStyledItemDelegate(list), m_list(list) {}
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override {
    const QRect bounds = QFontMetrics(option.font).boundingRect(
        QRect(0, 0, qMax(100, m_list->viewport()->width() - 40), 10000),
        Qt::TextWordWrap, index.data(Qt::DisplayRole).toString());
    return QSize(0, qMax(CN_TouchHeight(), bounds.height() + 40));
  }
 private:
  QListWidget* m_list;
};

class CN_WaypointCardScroll : public QObject {
 public:
  explicit CN_WaypointCardScroll(QListWidget* list)
      : QObject(list), m_list(list) {
    list->viewport()->installEventFilter(this);
  }
 protected:
  bool eventFilter(QObject*, QEvent* event) override {
    auto* bar = m_list->verticalScrollBar();
    if (event->type() == QEvent::ScrollPrepare) {
      auto* prepare = static_cast<QScrollPrepareEvent*>(event);
      prepare->setViewportSize(m_list->viewport()->size());
      prepare->setContentPosRange(QRectF(0, bar->minimum(), 0,
                                        bar->maximum() - bar->minimum()));
      prepare->setContentPos(QPointF(0, bar->value()));
      prepare->accept();
      return true;
    }
    if (event->type() == QEvent::Scroll) {
      bar->setValue(qRound(static_cast<QScrollEvent*>(event)->contentPos().y()));
      event->accept();
      return true;
    }
    if (event->type() == QEvent::Resize) {
      QPointer<QListWidget> cards(m_list);
      QTimer::singleShot(0, m_list, [cards]() {
        if (cards) cards->doItemsLayout();
      });
    }
    return false;
  }
 private:
  QListWidget* m_list;
};

inline QListWidget* CN_WaypointCards(wxWindow* parent, wxSizer* root) {
  auto* panel = new wxPanel(parent);
  panel->SetMinSize(wxSize(0, CN_TouchHeight() * 2));
  auto* layout = new QVBoxLayout(panel->GetHandle());
  layout->setContentsMargins(0, 0, 0, 0);
  auto* list = new QListWidget(panel->GetHandle());
  list->setWordWrap(true);
  list->setTextElideMode(Qt::ElideNone);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  list->setItemDelegate(new CN_WaypointCardDelegate(list));
  list->setStyleSheet(QString(
      "QListWidget { font-size: %1pt; background: #f5f8fa; color: #17313e; } "
      "QListWidget::item { padding: 12px; border-bottom: 1px solid #afbdc4; } "
      "QListWidget::item:selected { background: #d1e8f1; color: #102e3b; }")
      .arg(CN_FontPointSize()));
  QPointer<QListWidget> cards(list);
  new CN_AndroidButtonDragFilter(list->viewport(), [cards](QPoint point) {
    if (!cards) return;
    auto* item = cards->itemAt(point);
    if (item && (item->flags() & Qt::ItemIsEnabled))
      cards->setCurrentItem(item);
  }, list->viewport());
  new CN_WaypointCardScroll(list);
  layout->addWidget(list);
  root->Add(panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  return list;
}
#endif
