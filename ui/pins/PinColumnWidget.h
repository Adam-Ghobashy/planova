#pragma once
#include <QFrame>
#include <vector>

#include "planova/pins/Pin.h"

class QLabel;
class QScrollArea;
class QVBoxLayout;

namespace planova::s5 {

class PinCardWidget;

// One category column: title, count, scrolling cards. Drop target for pin cards.
class PinColumnWidget : public QFrame {
    Q_OBJECT
public:
    PinColumnWidget(PinCategory category, const QString& title, QWidget* parent = nullptr);

    PinCategory category() const { return category_; }

    // Replaces all cards. Old cards are deleted later (safe inside their own signal handlers).
    // showEmptyLabel: false when the page already says that there are no pins at all (one hint, not five).
    void setCards(const std::vector<PinCardWidget*>& cards, bool showEmptyLabel = true);

    int scrollValue() const;
    void setScrollValue(int v);

signals:
    void pinDropped(const Id& pinId, PinCategory to);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void setDropActive(bool on);

    PinCategory category_;
    QLabel* count_ = nullptr;
    QLabel* empty_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QVBoxLayout* cardsLayout_ = nullptr;
};

}  // namespace planova::s5
