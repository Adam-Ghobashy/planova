#include "PinCardWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QDrag>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

#include "PinUiHelpers.h"
#include "RelativeTime.h"
#include "StyledComboBox.h"

namespace planova::s5 {

namespace {

constexpr int kMarginH = 14;
constexpr int kMarginV = 12;

// Buttons and the comment input handle their own mouse events and must not start a drag.
bool isInteractive(QWidget* w, const QWidget* stopAt)
{
    for (; w && w != stopAt; w = w->parentWidget())
        if (qobject_cast<QAbstractButton*>(w) || qobject_cast<QLineEdit*>(w) || qobject_cast<QComboBox*>(w))
            return true;
    return false;
}

QString nameOf(const std::map<Id, QString>& names, const Id& id)
{
    const auto it = names.find(id);
    return it != names.end() ? it->second : QStringLiteral("Unknown");
}

}  // namespace

PinCardWidget::PinCardWidget(const PinDto& pin, QWidget* parent)
    : PinCardWidget(PinView{pin, std::nullopt, {}}, {}, 1, QString(),
                    QDateTime::currentMSecsSinceEpoch(), parent)
{
}

PinCardWidget::PinCardWidget(const PinView& view, const std::map<Id, QString>& memberNames,
                             int memberIndex, const QString& taskTitle, qint64 nowMs, QWidget* parent)
    : QFrame(parent), id_(view.pin.id)
{
    setObjectName("PinCard");
    setProperty("category", categoryKey(view.pin.category));
    setProperty("dragging", false);
    setFrameShape(QFrame::NoFrame);

    root_ = new QVBoxLayout(this);
    root_->setContentsMargins(kMarginH, kMarginV, kMarginH, kMarginV);
    root_->setSpacing(8);

    // Top row: time + menu
    auto* top = new QHBoxLayout;
    auto* time = new QLabel(formatRelative(view.pin.ts, nowMs), this);
    time->setObjectName("PinCardTime");
    auto* menuBtn = new QToolButton(this);
    menuBtn->setObjectName("PinMenuButton");
    menuBtn->setText(QStringLiteral("···"));
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setToolTip(tr("Pin actions"));
    menuBtn->setAccessibleName(tr("Pin actions"));   // the visible text is only dots
    menuBtn->setMinimumSize(28, 24);                 // touch and pointer target of at least 24 px
    auto* menu = new QMenu(menuBtn);
    menu->setObjectName("PinCardMenu");
    applyPopupChrome(menu, true);
    QAction* editAct = menu->addAction(tr("Edit pin"));
    QAction* deleteAct = menu->addAction(tr("Delete pin"));
    menuBtn->setMenu(menu);
    top->addWidget(time);
    top->addStretch();
    top->addWidget(menuBtn);
    root_->addLayout(top);

    // Text
    auto* text = new QLabel(QString::fromStdString(view.pin.text), this);
    text->setObjectName("PinCardText");
    text->setWordWrap(true);
    root_->addWidget(text);

    // Linked task
    if (!taskTitle.isEmpty()) {
        auto* task = new QLabel(tr("Task: %1").arg(taskTitle), this);
        task->setObjectName("PinTaskLine");
        task->setWordWrap(true);
        root_->addWidget(task);
    }

    // Author
    const QString authorName = nameOf(memberNames, view.pin.authorId);
    auto* authorRow = new QHBoxLayout;
    authorRow->setSpacing(8);
    auto* avatar = new QLabel(initialsOf(authorName), this);
    avatar->setObjectName("PinAvatar");
    avatar->setProperty("member", QString::number(memberIndex));
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(24, 24);
    auto* author = new QLabel(authorName, this);
    author->setObjectName("PinAuthorName");
    authorRow->addWidget(avatar);
    authorRow->addWidget(author);
    authorRow->addStretch();
    root_->addLayout(authorRow);

    // Tags
    if (!view.pin.tags.empty()) {
        QStringList tagTexts;
        for (const auto& t : view.pin.tags) tagTexts << QStringLiteral("#") + QString::fromStdString(t);
        auto* tags = new QLabel(tagTexts.join(' '), this);
        tags->setObjectName("PinCardTag");
        tags->setWordWrap(true);
        root_->addWidget(tags);
    }

    // Footer
    auto* footer = new QHBoxLayout;
    chip_ = new QToolButton(this);
    chip_->setObjectName("PinCommentChip");
    chip_->setText(tr("Comments (%1)").arg(view.comments.size()));
    chip_->setCheckable(true);
    chip_->setCursor(Qt::PointingHandCursor);
    chip_->setToolTip(tr("Show or hide comments"));
    chip_->setMinimumHeight(24);
    auto* resolve = new QPushButton(tr("Resolve"), this);
    resolve->setObjectName("PinResolveButton");
    resolve->setCursor(Qt::PointingHandCursor);
    footer->addWidget(chip_);
    footer->addStretch();
    footer->addWidget(resolve);
    root_->addLayout(footer);

    // Comments panel (hidden until the chip is checked)
    panel_ = new QFrame(this);
    panel_->setObjectName("PinCommentsPanel");
    auto* pl = new QVBoxLayout(panel_);
    pl->setContentsMargins(10, 8, 10, 8);
    pl->setSpacing(6);
    if (view.comments.empty()) {
        auto* none = new QLabel(tr("No comments yet"), panel_);
        none->setObjectName("DialogHint");
        pl->addWidget(none);
    }
    for (const auto& c : view.comments) {
        auto* meta = new QLabel(QStringLiteral("%1 · %2").arg(nameOf(memberNames, c.authorId),
                                                                  formatRelative(c.ts, nowMs)), panel_);
        meta->setObjectName("PinCommentMeta");
        auto* body = new QLabel(QString::fromStdString(c.text), panel_);
        body->setObjectName("PinCommentText");
        body->setWordWrap(true);
        pl->addWidget(meta);
        pl->addWidget(body);
    }
    author_ = new StyledComboBox(panel_);
    for (const auto& [memberId, name] : memberNames)
        author_->addItem(tr("Comment as %1").arg(name), QString::fromStdString(memberId));
    pl->addWidget(author_);
    auto* inputRow = new QHBoxLayout;
    input_ = new QLineEdit(panel_);
    input_->setObjectName("PinCommentInput");
    input_->setPlaceholderText(tr("Write a comment"));
    auto* send = new QPushButton(tr("Send"), panel_);
    send->setObjectName("PinCommentSend");
    inputRow->addWidget(input_, 1);
    inputRow->addWidget(send);
    pl->addLayout(inputRow);
    panel_->hide();
    root_->addWidget(panel_);

    connect(resolve, &QPushButton::clicked, this, [this] { emit resolveRequested(id_); });
    connect(editAct, &QAction::triggered, this, [this] { emit editRequested(id_); });
    connect(deleteAct, &QAction::triggered, this, [this] { emit deleteRequested(id_); });
    connect(chip_, &QToolButton::toggled, this, [this](bool open) {
        animatePanel(open);
        emit commentsToggled(id_, open);
    });
    connect(send, &QPushButton::clicked, this, &PinCardWidget::submitComment);
    connect(input_, &QLineEdit::returnPressed, this, &PinCardWidget::submitComment);
}

void PinCardWidget::setCommentAuthor(const Id& memberId)
{
    const int idx = author_->findData(QString::fromStdString(memberId));
    if (idx >= 0) author_->setCurrentIndex(idx);
}

void PinCardWidget::setCommentsExpanded(bool expanded)
{
    QSignalBlocker block(chip_);
    chip_->setChecked(expanded);
    panel_->setMaximumHeight(QWIDGETSIZE_MAX);
    panel_->setVisible(expanded);
}

bool PinCardWidget::commentsExpanded() const { return chip_->isChecked(); }

void PinCardWidget::submitComment()
{
    const QString t = input_->text().trimmed();
    if (t.isEmpty() || author_->count() == 0) return;
    input_->clear();
    emit commentSubmitted(id_, t, author_->currentData().toString().toStdString());
}

void PinCardWidget::setSlide(int px)
{
    slide_ = px;
    root_->setContentsMargins(kMarginH, kMarginV + px, kMarginH, kMarginV);
}

int PinCardWidget::panelTargetHeight() const
{
    QLayout* l = panel_->layout();
    l->activate();
    const int w = qMax(width() - 2 * kMarginH, 200);
    const int forWidth = l->hasHeightForWidth() ? l->totalHeightForWidth(w) : 0;
    return qMax(forWidth, panel_->sizeHint().height());
}

void PinCardWidget::animatePanel(bool open)
{
    if (panelAnim_) panelAnim_->stop();
    auto* anim = new QPropertyAnimation(panel_, "maximumHeight", this);
    anim->setDuration(180);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    if (open) {
        panel_->setMaximumHeight(0);
        panel_->show();
        anim->setStartValue(0);
        anim->setEndValue(panelTargetHeight());
        connect(anim, &QPropertyAnimation::finished, panel_, [p = panel_] { p->setMaximumHeight(QWIDGETSIZE_MAX); });
    } else {
        anim->setStartValue(panel_->height());
        anim->setEndValue(0);
        connect(anim, &QPropertyAnimation::finished, panel_, [p = panel_] {
            p->hide();
            p->setMaximumHeight(QWIDGETSIZE_MAX);
        });
    }
    panelAnim_ = anim;
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PinCardWidget::animateIn(int delayMs)
{
    auto* fx = new QGraphicsOpacityEffect(this);
    fx->setOpacity(0.0);
    setGraphicsEffect(fx);
    setSlide(8);

    auto* group = new QParallelAnimationGroup(this);
    auto* fade = new QPropertyAnimation(fx, "opacity", group);
    fade->setDuration(180);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    auto* slideAnim = new QPropertyAnimation(this, "slide", group);
    slideAnim->setDuration(180);
    slideAnim->setStartValue(8);
    slideAnim->setEndValue(0);
    slideAnim->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(fade);
    group->addAnimation(slideAnim);
    inGroup_ = group;

    connect(group, &QParallelAnimationGroup::finished, this, [this, group] {
        setSlide(0);
        setGraphicsEffect(nullptr);  // a lingering effect degrades text rendering
        group->deleteLater();
    });
    QTimer::singleShot(delayMs, group, [group] { group->start(); });
}

void PinCardWidget::animateOut(std::function<void()> done)
{
    if (inGroup_) {
        inGroup_->stop();
        inGroup_->deleteLater();
    }
    setEnabled(false);  // no double clicks while fading

    // A refresh can delete this card during the fade; the command must still run once.
    auto pending = std::make_shared<std::function<void()>>(std::move(done));
    auto fire = [pending] {
        if (!*pending) return;
        auto fn = std::move(*pending);
        *pending = nullptr;
        fn();
    };
    connect(this, &QObject::destroyed, fire);

    auto* fx = new QGraphicsOpacityEffect(this);
    fx->setOpacity(1.0);
    setGraphicsEffect(fx);
    auto* anim = new QPropertyAnimation(fx, "opacity", this);
    anim->setDuration(150);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, fire);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PinCardWidget::mousePressEvent(QMouseEvent* e)
{
    dragArmed_ = e->button() == Qt::LeftButton && !isInteractive(childAt(e->pos()), this);
    pressPos_ = e->pos();
    QFrame::mousePressEvent(e);
}

void PinCardWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (dragArmed_ && (e->buttons() & Qt::LeftButton)
        && (e->pos() - pressPos_).manhattanLength() >= QApplication::startDragDistance()) {
        dragArmed_ = false;
        startDrag();
        return;
    }
    QFrame::mouseMoveEvent(e);
}

void PinCardWidget::mouseReleaseEvent(QMouseEvent* e)
{
    dragArmed_ = false;
    QFrame::mouseReleaseEvent(e);
}

void PinCardWidget::startDrag()
{
    auto* mime = new QMimeData;
    mime->setData(kPinMime, QByteArray::fromStdString(id_));
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->setHotSpot(pressPos_);

    QPointer<PinCardWidget> self(this);
    setProperty("dragging", true);
    repolish(this);
    drag->exec(Qt::MoveAction);
    // The page rebuilds cards through a queued refresh; this card can be gone by now.
    if (self) {
        setProperty("dragging", false);
        repolish(this);
    }
}

}  // namespace planova::s5
