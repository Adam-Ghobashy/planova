#include "StyledComboBox.h"

#include <QEnterEvent>
#include <QEvent>
#include <QListView>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QStyledItemDelegate>
#include <QVariantAnimation>

#include "PinUiHelpers.h"

namespace planova::s5 {

namespace {
constexpr qreal kArrowCenterFromRight = 16.0;  // inside the 30 px drop-down column from pins.qss
constexpr int kTurnMs = 160;
}  // namespace

StyledComboBox::StyledComboBox(QWidget* parent) : QComboBox(parent)
{
    setObjectName(QStringLiteral("PinCombo"));
    setCursor(Qt::PointingHandCursor);

    // Fallbacks only (palette roles). pins.qss overrides them.
    arrowColor_ = palette().color(QPalette::PlaceholderText);
    arrowActiveColor_ = palette().color(QPalette::Highlight);

    auto* list = new QListView;
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setFrameShape(QFrame::NoFrame);
    setView(list);                                  // the combo takes ownership
    setItemDelegate(new QStyledItemDelegate(this));  // needed so QSS ::item rules apply
    setMaxVisibleItems(8);

    popup_ = view()->window();
    applyPopupChrome(popup_, false);  // opaque: no black flash, no see-through list
    popup_->installEventFilter(this);

    turn_ = new QVariantAnimation(this);
    turn_->setDuration(kTurnMs);
    turn_->setEasingCurve(QEasingCurve::OutCubic);
    connect(turn_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        turnValue_ = v.toReal();
        update();
    });
}

void StyledComboBox::setArrowColor(const QColor& c)
{
    arrowColor_ = c;
    update();
}

void StyledComboBox::setArrowActiveColor(const QColor& c)
{
    arrowActiveColor_ = c;
    update();
}

void StyledComboBox::animateTurn(qreal target)
{
    turn_->stop();
    turn_->setStartValue(turnValue_);
    turn_->setEndValue(target);
    turn_->start();
}

bool StyledComboBox::eventFilter(QObject* watched, QEvent* ev)
{
    if (watched == popup_) {
        if (ev->type() == QEvent::Show) {
            open_ = true;
            animateTurn(1.0);
        } else if (ev->type() == QEvent::Hide) {
            open_ = false;
            animateTurn(0.0);
        }
    }
    return QComboBox::eventFilter(watched, ev);
}

void StyledComboBox::enterEvent(QEnterEvent* e)
{
    QComboBox::enterEvent(e);
    update();
}

void StyledComboBox::leaveEvent(QEvent* e)
{
    QComboBox::leaveEvent(e);
    update();
}

void StyledComboBox::paintEvent(QPaintEvent* e)
{
    QComboBox::paintEvent(e);  // frame, text; the native arrow is hidden by `::down-arrow { image: none; }`

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor c = (open_ || underMouse() || hasFocus()) ? arrowActiveColor_ : arrowColor_;
    if (!isEnabled()) c.setAlphaF(0.4);

    p.translate(width() - kArrowCenterFromRight, height() / 2.0);
    p.rotate(180.0 * turnValue_);
    p.setPen(QPen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF chevron[3] = {QPointF(-4.0, -2.0), QPointF(0.0, 2.0), QPointF(4.0, -2.0)};
    p.drawPolyline(chevron, 3);
}

}  // namespace planova::s5
