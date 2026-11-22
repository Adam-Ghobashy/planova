#include "PinColumnWidget.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>

#include "PinCardWidget.h"
#include "PinUiHelpers.h"

namespace planova::s5 {

PinColumnWidget::PinColumnWidget(PinCategory category, const QString& title, QWidget* parent)
    : QFrame(parent), category_(category)
{
    setObjectName("PinColumn");
    setProperty("dropActive", false);
    setAcceptDrops(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto* header = new QHBoxLayout;
    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName("PinColumnTitle");
    count_ = new QLabel(QStringLiteral("0"), this);
    count_->setObjectName("PinColumnCount");
    count_->setAlignment(Qt::AlignCenter);
    count_->setFixedHeight(20);  // Qt drops border-radius 10 (pins.qss) when the label is shorter than 20
    header->addWidget(titleLabel);
    header->addStretch();
    header->addWidget(count_);
    root->addLayout(header);

    scroll_ = new QScrollArea(this);
    scroll_->setObjectName("PinColumnScroll");
    scroll_->setWidgetResizable(true);
    scroll_->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget;
    body->setObjectName("PinColumnBody");
    cardsLayout_ = new QVBoxLayout(body);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(10);
    cardsLayout_->addStretch();
    scroll_->setWidget(body);
    root->addWidget(scroll_, 1);

    empty_ = new QLabel(tr("No pins here"), body);
    empty_->setObjectName("PinColumnEmpty");
    empty_->setAlignment(Qt::AlignCenter);
    cardsLayout_->insertWidget(0, empty_);
}

void PinColumnWidget::setCards(const std::vector<PinCardWidget*>& cards, bool showEmptyLabel)
{
    // Remove every card (all layout items except the trailing stretch and the empty label).
    for (int i = cardsLayout_->count() - 1; i >= 0; --i) {
        auto* w = cardsLayout_->itemAt(i)->widget();
        if (auto* card = qobject_cast<PinCardWidget*>(w)) {
            cardsLayout_->removeWidget(card);
            card->hide();
            card->deleteLater();
        }
    }
    int index = 0;
    int widestCard = 0;
    for (auto* card : cards) {
        card->setParent(scroll_->widget());
        cardsLayout_->insertWidget(index++, card);
        card->show();
        widestCard = std::max(widestCard, card->minimumSizeHint().width());
    }
    // A scroll area does not pass its content's minimum width on, so the column would get narrower than its
    // cards and cut them off. Ask for the widest card plus room for the vertical scroll bar instead; the page
    // scrolls sideways when the columns do not fit.
    scroll_->setMinimumWidth(widestCard > 0 ? widestCard + scroll_->verticalScrollBar()->sizeHint().width() : 0);
    // The empty label is item 0 only while it is the first widget; keep it first, hide when cards exist.
    cardsLayout_->removeWidget(empty_);
    cardsLayout_->insertWidget(0, empty_);
    empty_->setVisible(cards.empty() && showEmptyLabel);
    count_->setText(QString::number(cards.size()));
}

int PinColumnWidget::scrollValue() const { return scroll_->verticalScrollBar()->value(); }
void PinColumnWidget::setScrollValue(int v) { scroll_->verticalScrollBar()->setValue(v); }

void PinColumnWidget::setDropActive(bool on)
{
    if (property("dropActive").toBool() == on) return;
    setProperty("dropActive", on);
    repolish(this);
}

void PinColumnWidget::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat(kPinMime)) {
        setDropActive(true);
        e->acceptProposedAction();
    }
}

void PinColumnWidget::dragMoveEvent(QDragMoveEvent* e)
{
    if (e->mimeData()->hasFormat(kPinMime)) e->acceptProposedAction();
}

void PinColumnWidget::dragLeaveEvent(QDragLeaveEvent* e)
{
    setDropActive(false);
    e->accept();
}

void PinColumnWidget::dropEvent(QDropEvent* e)
{
    setDropActive(false);
    if (!e->mimeData()->hasFormat(kPinMime)) return;
    const Id id = e->mimeData()->data(kPinMime).toStdString();
    e->acceptProposedAction();
    emit pinDropped(id, category_);
}

}  // namespace planova::s5
