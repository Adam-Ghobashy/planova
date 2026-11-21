#include "TaskColumnWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include "TaskCardWidget.h"

TaskColumnWidget::TaskColumnWidget(const QString& title, const QString& tone, QWidget* parent)
    : QFrame(parent) {
    setObjectName("TaskColumn");
    setMinimumWidth(260);   // four columns fit the 1440 px window; narrower windows scroll the board sideways

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 18, 12, 16);
    root->setSpacing(14);

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName("ColumnTitle");
    count_ = new QLabel("0");
    count_->setObjectName("ColumnCount");
    count_->setProperty("tone", tone);          // todo | progress | review | done
    header->addWidget(titleLabel);
    header->addWidget(count_);
    header->addStretch(1);
    root->addLayout(header);

    auto* divider = new QFrame;
    divider->setObjectName("ColumnDivider");
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    root->addWidget(divider);

    auto* scroll = new QScrollArea;
    scroll->setObjectName("ColumnScroll");
    scroll->setWidgetResizable(true);
    scroll->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setObjectName("ColumnViewport");
    scroll->viewport()->setAutoFillBackground(false);

    auto* container = new QWidget;
    container->setObjectName("ColumnContainer");
    cards_ = new QVBoxLayout(container);
    cards_->setContentsMargins(0, 0, 0, 0);
    cards_->setSpacing(14);
    cards_->addStretch(1);                      // keeps cards at the top

    scroll->setWidget(container);
    container->setAutoFillBackground(false);    // setWidget turns it on
    root->addWidget(scroll, 1);
}

void TaskColumnWidget::clear() {
    while (cards_->count() > 1) {               // last item is the stretch
        QLayoutItem* item = cards_->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
}

void TaskColumnWidget::addCard(TaskCardWidget* card) {
    cards_->insertWidget(cards_->count() - 1, card);
    connect(card, &TaskCardWidget::advanceRequested, this, &TaskColumnWidget::advanceRequested);
    connect(card, &TaskCardWidget::declineRequested, this, &TaskColumnWidget::declineRequested);
}

void TaskColumnWidget::setCount(int n) {
    count_->setText(QString::number(n));
}