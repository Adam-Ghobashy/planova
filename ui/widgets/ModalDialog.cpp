#include "ModalDialog.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTextEdit>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <algorithm>

#include "theme/ThemeManager.h"

namespace {
constexpr int kMaxCardWidth = 580;
constexpr int kEdgeMargin = 20;       // gap between the card and the window edge
constexpr int kRadius = 20;
constexpr int kRisePixels = 8;
constexpr int kPopMs = 150;
constexpr double kMaxHeightShare = 0.9;

QString buttonName(ModalDialog::Role role)
{
    switch (role) {
    case ModalDialog::Role::Primary: return QStringLiteral("modalPrimaryButton");
    case ModalDialog::Role::Secondary: return QStringLiteral("modalSecondaryButton");
    case ModalDialog::Role::Danger: return QStringLiteral("modalDangerButton");
    }
    return QString();
}

// Widgets the user types into or picks from. Used to find the first field.
bool isInput(QWidget* w)
{
    return qobject_cast<QLineEdit*>(w) || qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w)
        || qobject_cast<QComboBox*>(w) || qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QCheckBox*>(w);
}
}  // namespace

ModalDialog::ModalDialog(const QString& title, QWidget* parent) : QDialog(parent)
{
    setObjectName("ModalDialog");
    setWindowTitle(title);
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    // The card is a plain child of the dialog; layoutCard() sets its rectangle.
    card_ = new QFrame(this);
    card_->setObjectName("modalCard");
    auto* cardLayout = new QVBoxLayout(card_);
    cardLayout->setContentsMargins(1, 1, 1, 1);   // keeps the children inside the painted 1 px border
    cardLayout->setSpacing(0);

    auto* header = new QFrame(card_);
    header->setObjectName("modalHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 14, 16, 14);
    auto* titleLabel = new QLabel(title, header);
    titleLabel->setObjectName("modalTitle");
    titleLabel->setWordWrap(true);
    auto* close = new QToolButton(header);
    close->setObjectName("modalClose");
    close->setText(QString::fromUtf16(u"✕"));
    close->setToolTip(tr("Close"));
    close->setAccessibleName(tr("Close"));
    close->setCursor(Qt::PointingHandCursor);
    close->setFocusPolicy(Qt::NoFocus);   // Esc does the same, and the first Tab should reach a field or button
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addWidget(close);
    cardLayout->addWidget(header);

    scroll_ = new QScrollArea(card_);
    scroll_->setObjectName("modalScroll");
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFocusPolicy(Qt::NoFocus);
    scroll_->viewport()->setObjectName("modalViewport");
    body_ = new QWidget;
    body_->setObjectName("modalBody");
    auto* bodyLayout = new QVBoxLayout(body_);
    bodyLayout->setContentsMargins(24, 24, 24, 24);
    bodyLayout->setSpacing(16);
    scroll_->setWidget(body_);
    cardLayout->addWidget(scroll_, 1);
    body_->installEventFilter(this);   // the card grows when the content grows (for example an error line)

    footer_ = new QFrame(card_);
    footer_->setObjectName("modalFooter");
    footerLayout_ = new QHBoxLayout(footer_);
    footerLayout_->setContentsMargins(24, 16, 24, 16);
    footerLayout_->setSpacing(12);
    footerLayout_->addStretch(1);
    cardLayout->addWidget(footer_);

    connect(close, &QToolButton::clicked, this, &QDialog::reject);

    pop_ = new QVariantAnimation(this);
    pop_->setDuration(kPopMs);
    pop_->setStartValue(0.0);
    pop_->setEndValue(1.0);
    pop_->setEasingCurve(QEasingCurve::OutCubic);
    connect(pop_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        progress_ = v.toReal();
        setWindowOpacity(progress_);
        layoutCard();
    });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

QVBoxLayout* ModalDialog::contentLayout()
{
    return static_cast<QVBoxLayout*>(body_->layout());
}

QPushButton* ModalDialog::addButton(const QString& text, Role role)
{
    auto* button = new QPushButton(text, footer_);
    button->setObjectName(buttonName(role));
    button->setCursor(Qt::PointingHandCursor);
    if (role == Role::Primary)
        button->setDefault(true);
    footerLayout_->addWidget(button);
    if (role == Role::Secondary)
        connect(button, &QPushButton::clicked, this, &QDialog::reject);
    else
        connect(button, &QPushButton::clicked, this, &QDialog::accept);
    return button;
}

bool ModalDialog::confirm(QWidget* parent, const QString& title, const QString& text,
                          const QString& okText, Role okRole)
{
    ModalDialog dlg(title, parent);
    auto* message = new QLabel(text);
    message->setObjectName("modalMessage");
    message->setWordWrap(true);
    dlg.contentLayout()->addWidget(message);
    dlg.addButton(tr("Cancel"), Role::Secondary);
    dlg.addButton(okText, okRole);
    return dlg.exec() == QDialog::Accepted;
}

void ModalDialog::info(QWidget* parent, const QString& title, const QString& text)
{
    ModalDialog dlg(title, parent);
    auto* message = new QLabel(text);
    message->setObjectName("modalMessage");
    message->setWordWrap(true);
    dlg.contentLayout()->addWidget(message);
    dlg.addButton(tr("OK"), Role::Primary);
    dlg.exec();
}

void ModalDialog::setVisible(bool visible)
{
    if (visible) {
        // Cover the window of the parent (the client area, without the title bar), or the screen when there is
        // no parent. Setting the geometry before the show also stops QDialog from centring the window itself.
        if (QWidget* top = parentWidget() ? parentWidget()->window() : nullptr)
            setGeometry(top->geometry());
        else if (QScreen* screen = QGuiApplication::primaryScreen())
            setGeometry(screen->availableGeometry());
        layoutCard();
    }
    QDialog::setVisible(visible);
}

void ModalDialog::layoutCard()
{
    const int width = std::min(kMaxCardWidth, std::max(200, this->width() - 2 * kEdgeMargin));

    // Natural height = header + body + footer. The body scrolls when that is more than 90% of the window.
    card_->ensurePolished();
    auto* cardLayout = card_->layout();
    const QMargins m = cardLayout->contentsMargins();
    int natural = m.top() + m.bottom();
    for (int i = 0; i < cardLayout->count(); ++i) {
        QWidget* part = cardLayout->itemAt(i)->widget();
        if (part == scroll_) {
            // Wrapped labels need the height for the real width; without any, the plain size hint is right.
            QLayout* body = body_->layout();
            natural += body->hasHeightForWidth() ? body->totalHeightForWidth(width - m.left() - m.right())
                                                 : body->totalSizeHint().height();
        } else
            natural += part->sizeHint().height();
    }
    const int maxHeight = static_cast<int>(this->height() * kMaxHeightShare);
    const int height = std::min(natural, std::max(120, maxHeight));

    const int rise = static_cast<int>((1.0 - progress_) * kRisePixels);
    const QRect rect((this->width() - width) / 2, (this->height() - height) / 2 + rise, width, height);
    if (card_->geometry() != rect) {
        card_->setGeometry(rect);
        update();
    }
}

void ModalDialog::paintEvent(QPaintEvent*)
{
    const ThemeManager& theme = ThemeManager::instance();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), theme.color(Token::Overlay));

    const QRectF card = card_->geometry();

    // Soft shadow: a few translucent rounded rectangles, each a little larger than the one before.
    QColor shadow = theme.color(Token::Shadow);
    shadow.setAlphaF(shadow.alphaF() * 0.25);
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    for (int i = 1; i <= 6; ++i)
        p.drawRoundedRect(card.adjusted(-i * 2, -i * 2 + 6, i * 2, i * 2 + 6), kRadius + i * 2, kRadius + i * 2);

    // Opaque Surface base first, so the translucent glass never lets the page behind make text hard to read.
    QPainterPath path;
    path.addRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
    p.fillPath(path, theme.color(Token::Surface));
    p.fillPath(path, theme.color(Token::SurfaceGlass));
    p.setPen(QPen(theme.color(Token::GlassEdge), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Glass sheen: a 1 px highlight just inside the top edge, between the two rounded corners.
    const double sheenY = card.top() + 1.5;
    p.setPen(QPen(theme.color(Token::GlassSheen), 1));
    p.drawLine(QPointF(card.left() + kRadius, sheenY), QPointF(card.right() - kRadius, sheenY));
}

void ModalDialog::showEvent(QShowEvent* e)
{
    QDialog::showEvent(e);
    layoutCard();
    focusFirstControl();
    progress_ = 0.0;
    setWindowOpacity(0.0);
    pop_->stop();
    pop_->start();
}

void ModalDialog::resizeEvent(QResizeEvent* e)
{
    QDialog::resizeEvent(e);
    layoutCard();
}

bool ModalDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == body_ && event->type() == QEvent::LayoutRequest)
        layoutCard();
    return QDialog::eventFilter(watched, event);
}

// First field when the body has one. Otherwise the Primary button, or Cancel when there is only a Danger
// button (so Enter never deletes by accident).
void ModalDialog::focusFirstControl()
{
    const auto children = body_->findChildren<QWidget*>();
    for (QWidget* w : children) {
        if (isInput(w) && w->isEnabled() && w->isVisibleTo(this) && (w->focusPolicy() & Qt::TabFocus)) {
            w->setFocus(Qt::TabFocusReason);
            return;
        }
    }
    QPushButton* target = findChild<QPushButton*>("modalPrimaryButton");
    if (!target) target = findChild<QPushButton*>("modalSecondaryButton");
    if (target) target->setFocus(Qt::TabFocusReason);
}
