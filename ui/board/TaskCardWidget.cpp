#include "TaskCardWidget.h"
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include "BoardUtil.h"
#include "ComplexityDotsWidget.h"

namespace {
// A one-line label that shortens a long name with "..." instead of making the card wider than its column.
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(const QString& text) : QLabel(text), full_(text) {
        setToolTip(text);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
    QSize sizeHint() const override {
        return QSize(qMin(fontMetrics().horizontalAdvance(full_), 80), QLabel::sizeHint().height());
    }
    QSize minimumSizeHint() const override { return QSize(24, QLabel::sizeHint().height()); }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        setText(fontMetrics().elidedText(full_, Qt::ElideRight, width()));
    }

private:
    QString full_;
};
}

TaskCardWidget::TaskCardWidget(const TaskDto& task, const QString& assigneeName,
                               const QString& initials, std::optional<Token> avatarColor,
                               int pinCount, QWidget* parent)
    : QFrame(parent), taskId_(task.id) {
    setObjectName("TaskCard");

    // Only a card with an assignee has something to do from the keyboard (the Menu key opens "Decline task...").
    // An unassigned card has no menu, so it is no Tab stop. Its advance button stays reachable.
    // With Qt::ActionsContextMenu Qt shows the menu for both the mouse and the keyboard.
    if (task.assignee) {
        setFocusPolicy(Qt::StrongFocus);
        setContextMenuPolicy(Qt::ActionsContextMenu);
        auto* decline = new QAction(tr("Decline task…"), this);
        connect(decline, &QAction::triggered, this, [this] { emit declineRequested(taskId_); });
        addAction(decline);
    } else {
        setFocusPolicy(Qt::NoFocus);
    }

    // What a screen reader says: the task and its status first, then who has it and the pins.
    const QString who = task.assignee ? assigneeName : tr("Unassigned");
    QString description = tr("Assignee: %1.").arg(who);
    if (pinCount > 0)
        description += QStringLiteral(" ") + (pinCount == 1 ? tr("1 pin") : tr("%1 pins").arg(pinCount)) + QStringLiteral(".");
    setAccessibleName(QStringLiteral("%1, %2").arg(QString::fromStdString(task.title), board::statusLabel(task.status)));
    setAccessibleDescription(description);

    // The qss border is 1 px (2 px with focus) and the padding 1 px (0 with focus), so a focused card does not move
    // its content. The layout margin is what remains of the design's 20 px.
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 15, 14, 12);
    root->setSpacing(12);

    // Top row: skill chip, complexity dots
    auto* top = new QHBoxLayout;
    top->setSpacing(8);

    auto* chip = new QLabel(board::skillLabel(task.skill));
    chip->setObjectName("SkillChip");
    top->addWidget(chip, 0, Qt::AlignTop);
    top->addStretch(1);
    top->addWidget(new ComplexityDotsWidget(task.complexity), 0, Qt::AlignTop);
    root->addLayout(top);

    // A blocked task says so in words, on its own row so the chip row never gets wider than the column.
    const bool blocked = !task.blockedBy.empty() && task.status != TaskStatus::Done;
    if (blocked) {
        auto* badgeRow = new QHBoxLayout;
        auto* badge = new QFrame;
        badge->setObjectName("BlockedBadge");
        badge->setToolTip(tr("Waiting on %1 unfinished task(s)")
                              .arg(static_cast<int>(task.blockedBy.size())));
        auto* bl = new QHBoxLayout(badge);
        bl->setContentsMargins(10, 3, 10, 3);
        bl->setSpacing(6);
        auto* icon = new QLabel(QString::fromUtf16(u"⚠️"));
        icon->setObjectName("BlockedIcon");
        auto* text = new QLabel(tr("Blocked"));
        text->setObjectName("BlockedText");
        bl->addWidget(icon);
        bl->addWidget(text);
        badgeRow->addWidget(badge);
        badgeRow->addStretch(1);
        root->addLayout(badgeRow);
    }

    // The title wraps inside the card: Ignored makes the layout use the card width instead of the one-line width.
    auto* title = new QLabel(QString::fromStdString(task.title));
    title->setObjectName("TaskTitle");
    title->setWordWrap(true);
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    root->addWidget(title);

    auto* line = new QFrame;
    line->setObjectName("CardDivider");
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    root->addWidget(line);

    // Bottom row: avatar, name, pins, hours, advance / done
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(6);

    auto* avatar = new QLabel(task.assignee ? initials : QStringLiteral("?"));
    avatar->setObjectName("CardAvatar");
    avatar->setFixedSize(28, 28);
    avatar->setAlignment(Qt::AlignCenter);
    // board.qss maps memberColor 1..6 to the Member1..Member6 colours. Without a colour (unassigned, or a member who
    // left the team) the avatar keeps the plain look.
    if (avatarColor)
        avatar->setProperty("memberColor", static_cast<int>(*avatarColor) - static_cast<int>(Token::Member1) + 1);
    bottom->addWidget(avatar);

    auto* name = new ElidedLabel(who);
    name->setObjectName("CardMeta");
    bottom->addWidget(name);

    if (pinCount > 0) {
        auto* pinChip = new QFrame;
        pinChip->setObjectName("PinChip");
        pinChip->setToolTip(tr("%1 pin(s) on this task").arg(pinCount));
        auto* pl = new QHBoxLayout(pinChip);
        pl->setContentsMargins(7, 2, 8, 2);
        pl->setSpacing(3);
        auto* pinIcon = new QLabel(QString::fromUcs4(U"\U0001F4CC"));
        pinIcon->setObjectName("PinIcon");
        auto* pinNum = new QLabel(QString::number(pinCount));
        pinNum->setObjectName("PinCount");
        pl->addWidget(pinIcon);
        pl->addWidget(pinNum);
        bottom->addWidget(pinChip);
    }
    bottom->addStretch(1);

    auto* hours = new QLabel(QString("%1h").arg(task.estHours));
    hours->setObjectName("CardMeta");
    bottom->addWidget(hours);

    TaskStatus next;
    if (board::nextStatus(task.status, next)) {
        auto* advance = new QToolButton;
        advance->setObjectName("AdvanceButton");
        advance->setText(QString::fromUtf16(u"→"));
        advance->setCursor(Qt::PointingHandCursor);
        advance->setAutoRaise(true);
        advance->setToolTip(tr("Move to %1").arg(board::statusLabel(next)));
        advance->setAccessibleName(tr("Move \"%1\" to %2").arg(QString::fromStdString(task.title), board::statusLabel(next)));
        connect(advance, &QToolButton::clicked, this, [this] { emit advanceRequested(taskId_); });
        bottom->addWidget(advance);
    } else {
        auto* done = new QLabel(QString::fromUtf16(u"✓"));
        done->setObjectName("DoneMark");
        done->setAccessibleName(tr("Done"));
        bottom->addWidget(done);
    }
    root->addLayout(bottom);
}
