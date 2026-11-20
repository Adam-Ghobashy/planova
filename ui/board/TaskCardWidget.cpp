#include "TaskCardWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include "BoardUtil.h"
#include "ComplexityDotsWidget.h"

TaskCardWidget::TaskCardWidget(const TaskDto& task, const QString& assigneeName,
                               const QString& initials, Token /*avatarColor*/,
                               int pinCount, QWidget* parent)
    : QFrame(parent), taskId_(task.id) {
    setObjectName("TaskCard");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(14);

    // --- top row: skill chip, blocked badge, complexity dots
    auto* top = new QHBoxLayout;
    top->setSpacing(8);

    auto* chip = new QLabel(board::skillLabel(task.skill));
    chip->setObjectName("SkillChip");
    top->addWidget(chip, 0, Qt::AlignTop);

    const bool blocked = !task.blockedBy.empty() && task.status != TaskStatus::Done;
    if (blocked) {
        auto* badge = new QFrame;
        badge->setObjectName("BlockedBadge");
        badge->setToolTip(tr("Waiting on %1 unfinished task(s)")
                              .arg(static_cast<int>(task.blockedBy.size())));
        auto* bl = new QVBoxLayout(badge);
        bl->setContentsMargins(10, 4, 10, 4);
        bl->setSpacing(0);

        auto* icon = new QLabel(QString::fromUtf16(u"\u26A0\uFE0F"));
        icon->setObjectName("BlockedIcon");
        icon->setAlignment(Qt::AlignCenter);
        auto* text = new QLabel(tr("Blocked"));
        text->setObjectName("BlockedText");
        text->setAlignment(Qt::AlignCenter);
        bl->addWidget(icon);
        bl->addWidget(text);
        top->addWidget(badge, 0, Qt::AlignTop);
    }
    top->addStretch(1);
    top->addWidget(new ComplexityDotsWidget(task.complexity), 0, Qt::AlignTop);
    root->addLayout(top);

    // --- title
    auto* title = new QLabel(QString::fromStdString(task.title));
    title->setObjectName("TaskTitle");
    title->setWordWrap(true);
    root->addWidget(title);

    // --- divider
    auto* line = new QFrame;
    line->setObjectName("CardDivider");
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    root->addWidget(line);

    // --- bottom row: avatar, name, pins, hours, advance / done
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(10);

    auto* avatar = new QLabel(task.assignee ? initials : QStringLiteral("?"));
    avatar->setObjectName("CardAvatar");
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    bottom->addWidget(avatar);

    auto* name = new QLabel(task.assignee ? assigneeName : tr("Unassigned"));
    name->setObjectName("CardMeta");
    bottom->addWidget(name);

    if (pinCount > 0) {
        auto* pinChip = new QFrame;
        pinChip->setObjectName("PinChip");
        pinChip->setToolTip(tr("%1 pin(s) on this task").arg(pinCount));
        auto* pl = new QHBoxLayout(pinChip);
        pl->setContentsMargins(8, 3, 10, 3);
        pl->setSpacing(4);
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
        advance->setText(QString::fromUtf16(u"\u2192"));
        advance->setCursor(Qt::PointingHandCursor);
        advance->setAutoRaise(true);
        advance->setToolTip(tr("Move to %1").arg(board::statusLabel(next)));
        connect(advance, &QToolButton::clicked, this, [this] { emit advanceRequested(taskId_); });
        bottom->addWidget(advance);
    } else {
        auto* done = new QLabel(QString::fromUtf16(u"\u2713"));
        done->setObjectName("DoneMark");
        bottom->addWidget(done);
    }
    root->addLayout(bottom);
}