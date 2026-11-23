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

QFont boldFont(const QFont& base, int pixelSize) {
    QFont font = base;
    font.setPixelSize(pixelSize);
    font.setWeight(QFont::DemiBold);
    return font;
}

// Cell width for a pill: text, the pill's own padding (16) and the 8 px margin on each side.
int pillWidth(const QStyleOptionViewItem& option, const QModelIndex& index, int role) {
    const QFontMetrics fm(boldFont(option.font, 12));
    return fm.horizontalAdvance(index.data(role).toString()) + 16 + 16;
}

// Cell width for the assignee: avatar, name, the match or "Manual" badge, laid out as in paint(), plus an 8 px margin.
int assigneeWidth(const QStyleOptionViewItem& option, const QModelIndex& index) {
    const QFont nameFont = ThemeManager::instance().textFont(TextStyle::Body);
    const QString name = index.data(AssignmentTableModel::AssigneeNameRole).toString();
    const int matchPct = index.data(AssignmentTableModel::MatchPctRole).toInt();
    const bool manual = index.data(AssignmentTableModel::ManualOverrideRole).toBool();
    int width = 16 + 22 + QFontMetrics(nameFont).horizontalAdvance(name);
    if (manual || matchPct > 0) {
        const QString badge = manual ? QString("Manual") : QString("%1%").arg(matchPct);
        width += QFontMetrics(boldFont(option.font, 12)).horizontalAdvance(badge) + 12;
    }
    return width + 8;
}

// Wraps text to the width and keeps two lines at most; the second line is elided with "...".
QString twoLines(const QString& text, const QFontMetrics& fm, int width) {
    const QStringList words = text.split(' ', Qt::SkipEmptyParts);
    QString first;
    int used = 0;
    while (used < words.size()) {
        const QString candidate = first.isEmpty() ? words[used] : first + ' ' + words[used];
        if (!first.isEmpty() && fm.horizontalAdvance(candidate) > width) break;
        first = candidate;
        ++used;
    }
    const QString rest = words.mid(used).join(' ');
    if (rest.isEmpty()) return fm.elidedText(first, Qt::ElideRight, width);
    return fm.elidedText(first, Qt::ElideRight, width) + '\n' + fm.elidedText(rest, Qt::ElideRight, width);
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
    if (option.state & QStyle::State_MouseOver) return tm.color(Token::GlassFillStrong);
    return QColor(Qt::transparent);   // at rest the glass table shows through
}

SlicerTableDelegate::SlicerTableDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QSize SlicerTableDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
    switch (index.column()) {
        case AssignmentTableModel::TaskComplexity: return QSize(260, 56);
        // Pill width follows the text, so a wider body font never clips a long label such as DOCUMENTATION.
        case AssignmentTableModel::Skill:
            return QSize(std::max(110, pillWidth(option, index, AssignmentTableModel::RequiredSkillRole)), 56);
        case AssignmentTableModel::Priority:
            return QSize(std::max(95, pillWidth(option, index, AssignmentTableModel::PriorityRole)), 56);
        case AssignmentTableModel::Assignee: return QSize(std::max(180, assigneeWidth(option, index)), 56);
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
    painter->setPen(QPen(tm.color(Token::GlassEdge), 1));
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    const int col = index.column();

    // Selection is also shown by a bar on the left edge, so it does not rely on the tint alone.
    // Keyboard focus draws an outline around the current cell.
    if (col == AssignmentTableModel::TaskComplexity && (option.state & QStyle::State_Selected)) {
        painter->fillRect(QRect(rect.left(), rect.top(), 3, rect.height() - 1), tm.color(Token::Accent));
    }
    if (option.state & QStyle::State_HasFocus) {
        painter->setPen(QPen(tm.color(Token::Accent), 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(rect).adjusted(1, 1, -1, -2));
    }

    if (col == AssignmentTableModel::TaskComplexity) {
        const QFont titleFont = tm.textFont(TextStyle::LabelActive);
        painter->setFont(titleFont);
        painter->setPen(tm.color(Token::TextPrimary));
        const int titleWidth = rect.width() - 20;
        const QString title = QFontMetrics(titleFont).elidedText(
            index.data(AssignmentTableModel::TaskTitleRole).toString(), Qt::ElideRight, titleWidth);
        painter->drawText(QRect(rect.left() + 12, rect.top() + 8, titleWidth, 20),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, title);

        const int complexity = index.data(AssignmentTableModel::TaskComplexityRole).toInt();
        const int dotY = rect.top() + 36;
        const int dotX = rect.left() + 14;
        painter->setPen(Qt::NoPen);
        for (int d = 0; d < 5; ++d) {
            painter->setBrush(d < complexity ? complexityColor(complexity) : tm.color(Token::Track));
            painter->drawEllipse(QPointF(dotX + d * 10, dotY), 3, 3);
        }

        QFont levelFont = ThemeManager::instance().textFont(TextStyle::Caption);
        painter->setFont(levelFont);
        painter->setPen(tm.color(Token::TextMuted));
        painter->drawText(QRect(dotX + 54, dotY - 8, 40, 16), Qt::AlignLeft | Qt::AlignVCenter,
                          QString("C%1").arg(complexity));

    } else if (col == AssignmentTableModel::Skill) {
        const QFont font = boldFont(painter->font(), 12);
        painter->setFont(font);
        const QString skill = index.data(AssignmentTableModel::RequiredSkillRole).toString();
        drawPill(painter, pillRectFor(rect, QFontMetrics(font), skill), skill,
                 tm.color(Token::AccentSoft), tm.color(Token::Accent), tm.color(Token::Accent), 4);

    } else if (col == AssignmentTableModel::Priority) {
        const QFont font = boldFont(painter->font(), 12);
        painter->setFont(font);
        const QString priority = index.data(AssignmentTableModel::PriorityRole).toString();

        // As in the design: Critical is the danger badge, every other priority is the primary badge.
        // The text always names the priority, so colour is never the only signal.
        const bool critical = priority == "CRITICAL";
        const QColor background = tm.color(critical ? Token::DangerSoft : Token::AccentSoft);
        const QColor foreground = tm.color(critical ? Token::DangerText : Token::Accent);
        const QColor border = tm.color(critical ? Token::DangerSoft : Token::Accent);
        drawPill(painter, pillRectFor(rect, QFontMetrics(font), priority), priority, background, foreground,
                 border, 4);

    } else if (col == AssignmentTableModel::Assignee) {
        const QString initials = index.data(AssignmentTableModel::AssigneeInitialsRole).toString();
        const QString name = index.data(AssignmentTableModel::AssigneeNameRole).toString();
        const int matchPct = index.data(AssignmentTableModel::MatchPctRole).toInt();
        const bool manual = index.data(AssignmentTableModel::ManualOverrideRole).toBool();

        const int cy = rect.top() + rect.height() / 2;
        const int avatarX = rect.left() + 16;

        // Avatar as in the design: raised fill, thin border ring, primary-colour initials.
        painter->setPen(QPen(tm.color(Token::Border), 1));
        painter->setBrush(tm.color(Token::SurfaceRaised));
        painter->drawEllipse(QPointF(avatarX, cy), 11, 11);
        painter->setFont(boldFont(painter->font(), 12));
        painter->setPen(tm.color(Token::TextPrimary));
        painter->drawText(QRectF(avatarX - 11, cy - 11, 22, 22), Qt::AlignCenter, initials);

        const QFont nameFont = ThemeManager::instance().textFont(TextStyle::Body);
        painter->setFont(nameFont);
        const int nameWidth = QFontMetrics(nameFont).horizontalAdvance(name);
        painter->setPen(tm.color(Token::TextPrimary));
        painter->drawText(QRect(avatarX + 16, cy - 10, nameWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, name);

        if (manual || matchPct > 0) {
            const QString badge = manual ? QString("Manual") : QString("%1%").arg(matchPct);
            const QFont badgeFont = boldFont(painter->font(), 12);
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
        QFont font = ThemeManager::instance().textFont(TextStyle::Caption);
        painter->setFont(font);
        painter->setPen(tm.color(Token::TextSecondary));
        // At most two lines; the full text is in the tooltip.
        const QString text = twoLines(index.data(AssignmentTableModel::RationaleRole).toString(),
                                      QFontMetrics(font), rect.width() - 20);
        painter->drawText(rect.adjusted(10, 6, -10, -6), Qt::AlignLeft | Qt::AlignVCenter, text);

    } else if (col == AssignmentTableModel::Status) {
        const bool waiting = index.data(AssignmentTableModel::WaitingOnDepRole).toBool();
        const QString text = waiting ? QString::fromUtf8("▲ Waiting on Dep") : QString("Ready");
        const QFont font = boldFont(painter->font(), 12);
        painter->setFont(font);
        const QColor background = tm.color(waiting ? Token::WarningSoft : Token::SuccessSoft);
        const QColor foreground = tm.color(waiting ? Token::WarningText : Token::TextPrimary);
        drawPill(painter, pillRectFor(rect, QFontMetrics(font), text), text, background, foreground, background, 11);
    }

    painter->restore();
}

}
