#include "slicer/SlicerTableDelegate.h"
#include "slicer/AssignmentTableModel.h"
#include "theme/ThemeManager.h"
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <algorithm>

namespace planova::slicer {

namespace {

constexpr int kPillHeight = 22;

void drawPill(QPainter* painter, const QRectF& rect, const QString& text, const QColor& background,
              const QColor& foreground, const QColor& border, qreal radius) {
    painter->setPen(QPen(border, 1));
    painter->setBrush(background);
    painter->drawRoundedRect(rect, radius, radius);
    painter->setPen(foreground);
    painter->drawText(rect, Qt::AlignCenter, text);
}

QRectF pillRectFor(const QRect& cell, const QFontMetrics& fm, const QString& text, int height = kPillHeight) {
    const int width = fm.horizontalAdvance(text) + 16;
    return QRectF(cell.left() + 8, cell.top() + (cell.height() - height) / 2.0, width, height);
}

QFont boldFont(const QFont& base, int pointSize) {
    QFont font = base;
    font.setPointSize(pointSize);
    font.setBold(true);
    return font;
}

QColor complexityColor(int complexity) {
    static const Token ramp[5] = {Token::Member3, Token::Complexity2, Token::PizzaCheese,
                                  Token::Complexity4, Token::Accent};
    const int level = std::clamp(complexity, 1, 5);
    return ThemeManager::instance().color(ramp[level - 1]);
}

}

QColor slicerRowBackground(const QStyleOptionViewItem& option, const QModelIndex& index) {
    auto& tm = ThemeManager::instance();
    if ((option.state & QStyle::State_Selected) ||
        index.data(AssignmentTableModel::HighlightedRole).toBool()) {
        return tm.color(Token::AccentSoft);
    }
    if (option.state & QStyle::State_MouseOver) return tm.color(Token::SurfaceAlt);
    return tm.color(Token::Surface);
}

SlicerTableDelegate::SlicerTableDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QSize SlicerTableDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
    switch (index.column()) {
        case AssignmentTableModel::TaskComplexity: return QSize(260, 56);
        case AssignmentTableModel::Skill: return QSize(110, 56);
        case AssignmentTableModel::Priority: return QSize(95, 56);
        case AssignmentTableModel::Assignee: return QSize(180, 56);
        case AssignmentTableModel::Rationale: return QSize(240, 56);
        case AssignmentTableModel::Status: return QSize(135, 56);
        case AssignmentTableModel::ActionOverride: return QSize(140, 56);
        default: return QSize(option.rect.width(), 56);
    }
}

void SlicerTableDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
    if (index.column() == AssignmentTableModel::ActionOverride) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    auto& tm = ThemeManager::instance();
    const QRect rect = option.rect;

    painter->fillRect(rect, slicerRowBackground(option, index));
    painter->setPen(QPen(tm.color(Token::SurfaceAlt), 1));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    const int col = index.column();

    if (col == AssignmentTableModel::TaskComplexity) {
        QFont titleFont = painter->font();
        titleFont.setPointSize(10);
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(tm.color(Token::TextPrimary));
        painter->drawText(QRect(rect.left() + 12, rect.top() + 8, rect.width() - 20, 20),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          index.data(AssignmentTableModel::TaskTitleRole).toString());

        const int complexity = index.data(AssignmentTableModel::TaskComplexityRole).toInt();
        const int dotY = rect.top() + 36;
        const int dotX = rect.left() + 14;
        painter->setPen(Qt::NoPen);
        for (int d = 0; d < 5; ++d) {
            painter->setBrush(d < complexity ? complexityColor(complexity) : tm.color(Token::Track));
            painter->drawEllipse(QPointF(dotX + d * 10, dotY), 3, 3);
        }

        QFont levelFont = painter->font();
        levelFont.setPointSize(9);
        levelFont.setBold(false);
        painter->setFont(levelFont);
        painter->setPen(tm.color(Token::TextMuted));
        painter->drawText(QRect(dotX + 54, dotY - 8, 40, 16), Qt::AlignLeft | Qt::AlignVCenter,
                          QString("C%1").arg(complexity));

    } else if (col == AssignmentTableModel::Skill) {
        const QFont font = boldFont(painter->font(), 9);
        painter->setFont(font);
        const QString skill = index.data(AssignmentTableModel::RequiredSkillRole).toString();
        drawPill(painter, pillRectFor(rect, QFontMetrics(font), skill), skill,
                 tm.color(Token::AccentSoft), tm.color(Token::Accent), tm.color(Token::Accent), 4);

    } else if (col == AssignmentTableModel::Priority) {
        const QFont font = boldFont(painter->font(), 9);
        painter->setFont(font);
        const QString priority = index.data(AssignmentTableModel::PriorityRole).toString();

        QColor background = tm.color(Token::SurfaceAlt);
        QColor foreground = tm.color(Token::TextMuted);
        QColor border = tm.color(Token::Border);
        if (priority == "CRITICAL") {
            background = tm.color(Token::DangerSoft);
            foreground = tm.color(Token::DangerText);
            border = tm.color(Token::DangerSoft);
        } else if (priority == "HIGH") {
            background = tm.color(Token::AccentSoft);
            foreground = tm.color(Token::Accent);
            border = tm.color(Token::Accent);
        } else if (priority == "MEDIUM") {
            background = tm.color(Token::StatusBar);
        }
        drawPill(painter, pillRectFor(rect, QFontMetrics(font), priority), priority, background, foreground,
                 border, 4);

    } else if (col == AssignmentTableModel::Assignee) {
        const QString initials = index.data(AssignmentTableModel::AssigneeInitialsRole).toString();
        const QString name = index.data(AssignmentTableModel::AssigneeNameRole).toString();
        const int matchPct = index.data(AssignmentTableModel::MatchPctRole).toInt();
        const bool manual = index.data(AssignmentTableModel::ManualOverrideRole).toBool();

        const int cy = rect.top() + rect.height() / 2;
        const int avatarX = rect.left() + 16;

        painter->setPen(Qt::NoPen);
        painter->setBrush(tm.color(Token::TextPrimary));
        painter->drawEllipse(QPointF(avatarX, cy), 11, 11);
        painter->setFont(boldFont(painter->font(), 8));
        painter->setPen(tm.color(Token::AccentText));
        painter->drawText(QRectF(avatarX - 11, cy - 11, 22, 22), Qt::AlignCenter, initials);

        QFont nameFont = painter->font();
        nameFont.setPointSize(10);
        nameFont.setBold(false);
        painter->setFont(nameFont);
        const int nameWidth = QFontMetrics(nameFont).horizontalAdvance(name);
        painter->setPen(tm.color(Token::TextPrimary));
        painter->drawText(QRect(avatarX + 16, cy - 10, nameWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, name);

        if (manual || matchPct > 0) {
            const QString badge = manual ? QString("Manual") : QString("%1%").arg(matchPct);
            const QFont badgeFont = boldFont(painter->font(), 9);
            painter->setFont(badgeFont);
            const int badgeWidth = QFontMetrics(badgeFont).horizontalAdvance(badge) + 12;
            const QRectF badgeRect(avatarX + 22 + nameWidth, cy - 9, badgeWidth, 18);
            if (manual) {
                drawPill(painter, badgeRect, badge, tm.color(Token::SurfaceAlt), tm.color(Token::TextMuted),
                         tm.color(Token::Border), 9);
            } else {
                drawPill(painter, badgeRect, badge, tm.color(Token::SuccessSoft), tm.color(Token::TextPrimary),
                         tm.color(Token::SuccessSoft), 9);
            }
        }

    } else if (col == AssignmentTableModel::Rationale) {
        QFont font = painter->font();
        font.setPointSize(9);
        painter->setFont(font);
        painter->setPen(tm.color(Token::TextPrimary));
        painter->drawText(rect.adjusted(10, 6, -10, -6), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                          index.data(AssignmentTableModel::RationaleRole).toString());

    } else if (col == AssignmentTableModel::Status) {
        const bool waiting = index.data(AssignmentTableModel::WaitingOnDepRole).toBool();
        const QString text = waiting ? QString::fromUtf8("▲ Waiting on Dep") : QString("Ready");
        const QFont font = boldFont(painter->font(), 9);
        painter->setFont(font);
        const QColor background = tm.color(waiting ? Token::WarningSoft : Token::SuccessSoft);
        const QColor foreground = tm.color(waiting ? Token::WarningText : Token::TextPrimary);
        drawPill(painter, pillRectFor(rect, QFontMetrics(font), text), text, background, foreground, background, 11);
    }

    painter->restore();
}

}
