#include "LeaderboardModel.h"

#include <QApplication>
#include <QPainter>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <algorithm>
#include <iterator>

namespace planova::s6 {

namespace {

// Reference (views.css): td padding 12px 14px, avatar 26px, rank span 16px wide,
// 10px gap, points bar 60x6, row about 51px (26 + 2*12 + 1 border, estimated).
constexpr int kBadgeDiameter = 26;
constexpr int kCellPadding = 14;
constexpr int kRankWidth = 16;
constexpr int kRankGap = 10;
constexpr int kBarWidth = 60;
constexpr int kBarHeight = 6;
constexpr int kRowHeight = 51;
constexpr int kChipRadius = 6;

// Reference --font-mono: JetBrains Mono, Consolas, monospace.
QFont monoFont()
{
    QFont f;
    f.setFamilies({QStringLiteral("JetBrains Mono"), QStringLiteral("Consolas"),
                   QStringLiteral("Menlo"), QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("monospace")});
    return f;
}

QString initialsFor(const QString& name)
{
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString initials;
    for (const QString& word : words) {
        initials += word.at(0).toUpper();
        if (initials.size() == 2) {
            break;
        }
    }
    return initials;
}

QFont fixedFont(const QFont& base, bool bold)
{
    QFont font = monoFont();
    font.setBold(bold);
    font.setPixelSize(base.pixelSize() > 0 ? base.pixelSize() : 13);
    return font;
}

QFont chipFontFor(const QFont& base, bool high)
{
    QFont font(base);
    font.setPixelSize(11);
    font.setWeight(high ? QFont::Bold : QFont::DemiBold);
    return font;
}

} // namespace

// ---------------------------------------------------------------- table

LeaderboardTable::LeaderboardTable(QWidget* parent)
    : QTableView(parent)
{
}

// ---------------------------------------------------------------- model

LeaderboardModel::LeaderboardModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void LeaderboardModel::setRows(const std::vector<LeaderRow>& rows,
                               const std::vector<MemberDto>& members)
{
    beginResetModel();
    entries_.clear();
    entries_.reserve(rows.size());
    maxPoints_ = 0;

    for (const LeaderRow& row : rows) {
        Entry entry;
        entry.row = row;

        const auto it = std::find_if(members.begin(), members.end(),
                                     [&row](const MemberDto& m) { return m.id == row.memberId; });
        if (it != members.end()) {
            entry.name = QString::fromStdString(it->name);
            entry.role = QString::fromStdString(it->role);
            entry.memberIndex = static_cast<int>(std::distance(members.begin(), it) % 6);
        } else {
            entry.name = QString::fromStdString(row.memberId);
        }
        entry.initials = initialsFor(entry.name);

        maxPoints_ = std::max(maxPoints_, row.points);
        entries_.push_back(std::move(entry));
    }
    endResetModel();
}

const LeaderboardModel::Entry* LeaderboardModel::entryAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return nullptr;
    }
    return &entries_[static_cast<std::size_t>(row)];
}

QString LeaderboardModel::firstNameForRow(int row) const
{
    const Entry* entry = entryAt(row);
    return entry ? entry->name.section(QLatin1Char(' '), 0, 0) : QString();
}

double LeaderboardModel::pointsForRow(int row) const
{
    const Entry* entry = entryAt(row);
    return entry ? static_cast<double>(entry->row.points) : 0.0;
}

double LeaderboardModel::efficiencyForRow(int row) const
{
    const Entry* entry = entryAt(row);
    return entry ? entry->row.efficiency : 0.0;
}

int LeaderboardModel::memberIndexForRow(int row) const
{
    const Entry* entry = entryAt(row);
    return entry ? entry->memberIndex : -1;
}

QString LeaderboardModel::efficiencyText(double efficiency)
{
    if (efficiency <= 0.0) {
        return tr("0 pts/h");
    }
    // Same as the reference's parseFloat(toFixed(2)): "0.50" -> "0.5", "1.00" -> "1".
    QString number = QString::number(efficiency, 'f', 2);
    while (number.endsWith(QLatin1Char('0'))) {
        number.chop(1);
    }
    if (number.endsWith(QLatin1Char('.'))) {
        number.chop(1);
    }
    return tr("%1 pts/h").arg(number);
}

int LeaderboardModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

int LeaderboardModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LeaderboardModel::data(const QModelIndex& index, int role) const
{
    const Entry* found = index.isValid() ? entryAt(index.row()) : nullptr;
    if (!found) {
        return {};
    }
    const Entry& entry = *found;
    const LeaderRow& row = entry.row;

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case RankMemberColumn:
            return entry.name;
        case RoleColumn:
            return entry.role; // role is optional, may be empty
        case DoneAssignedColumn:
            return QStringLiteral("%1 / %2").arg(row.done).arg(row.assigned);
        case PointsColumn:
            return QString::number(row.points);
        case EfficiencyColumn:
            return efficiencyText(row.efficiency);
        default:
            return {};
        }
    case Qt::TextAlignmentRole:
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    case RankTextRole:
        return QStringLiteral("#%1").arg(row.rank);
    case InitialsRole:
        return entry.initials;
    case BarRatioRole:
        return maxPoints_ > 0 ? std::clamp(static_cast<double>(row.points) / maxPoints_, 0.0, 1.0)
                              : 0.0;
    case MemberIndexRole:
        return entry.memberIndex;
    case EfficiencyValueRole:
        return row.efficiency;
    default:
        return {};
    }
}

QVariant LeaderboardModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case RankMemberColumn:
        return tr("Rank & Member");
    case RoleColumn:
        return tr("Role");
    case DoneAssignedColumn:
        return tr("Done / Assigned");
    case PointsColumn:
        return tr("Complexity Pts");
    case EfficiencyColumn:
        return tr("Efficiency");
    default:
        return {};
    }
}

Qt::ItemFlags LeaderboardModel::flags(const QModelIndex& index) const
{
    return index.isValid() ? Qt::ItemIsEnabled : Qt::NoItemFlags;
}

// ------------------------------------------------------------ delegates

const LeaderboardTable* LeaderboardCellDelegate::tableOf(const QStyleOptionViewItem& option)
{
    return qobject_cast<const LeaderboardTable*>(option.widget);
}

void LeaderboardCellDelegate::paintFrame(QPainter* painter, const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    opt.text.clear();
    opt.state &= ~QStyle::State_HasFocus;
    const QWidget* widget = opt.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
}

TextCellDelegate::TextCellDelegate(bool primary, QObject* parent)
    : LeaderboardCellDelegate(parent)
    , primary_(primary)
{
}

void TextCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    paintFrame(painter, option, index);

    const LeaderboardTable* table = tableOf(option);
    if (!table) {
        return;
    }

    QFont font(option.font);
    font.setPixelSize(12); // reference: 12px for Role and Done / Assigned
    const QRect area = option.rect.adjusted(kCellPadding, 0, -kCellPadding, 0);

    painter->save();
    painter->setFont(font);
    painter->setPen(primary_ ? table->textPrimaryColor() : table->textMutedColor());
    painter->drawText(area, Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(font).elidedText(index.data(Qt::DisplayRole).toString(),
                                                    Qt::ElideRight, area.width()));
    painter->restore();
}

QSize TextCellDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    QFont font(option.font);
    font.setPixelSize(12);
    return QSize(QFontMetrics(font).horizontalAdvance(index.data(Qt::DisplayRole).toString())
                     + 2 * kCellPadding,
                 kRowHeight);
}

void MemberCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    paintFrame(painter, option, index);

    const LeaderboardTable* table = tableOf(option);
    if (!table) {
        return;
    }

    const QRect area = option.rect.adjusted(kCellPadding, 0, -kCellPadding, 0);
    // The rank text may overflow its 16px slot into the 10px gap ("#10"), like the reference.
    const QRect rankRect(area.left(), area.top(), kRankWidth + kRankGap, area.height());
    const QRect badge(area.left() + kRankWidth + kRankGap, area.center().y() - kBadgeDiameter / 2,
                      kBadgeDiameter, kBadgeDiameter);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setFont(fixedFont(option.font, true));
    painter->setPen(table->textMutedColor());
    painter->drawText(rankRect, Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(LeaderboardModel::RankTextRole).toString());

    const int memberIndex = index.data(LeaderboardModel::MemberIndexRole).toInt();
    const bool tinted = memberIndex >= 0;
    if (tinted) {
        painter->setPen(Qt::NoPen);
    } else {
        painter->setPen(table->borderColor());
    }
    painter->setBrush(table->memberColor(memberIndex));
    painter->drawEllipse(QRectF(badge).adjusted(0.5, 0.5, -0.5, -0.5));

    QFont initialsFont(option.font);
    initialsFont.setBold(true);
    initialsFont.setPixelSize(10);
    painter->setFont(initialsFont);
    painter->setPen(tinted ? table->accentTextColor() : table->textPrimaryColor());
    painter->drawText(badge, Qt::AlignCenter, index.data(LeaderboardModel::InitialsRole).toString());

    QFont nameFont(option.font);
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);
    painter->setPen(table->textPrimaryColor());
    const QRect nameRect(badge.right() + 1 + 10, area.top(),
                         std::max(0, area.right() - badge.right() - 10), area.height());
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(),
                                                        Qt::ElideRight, nameRect.width()));
    painter->restore();
}

QSize MemberCellDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    QFont nameFont(option.font);
    nameFont.setWeight(QFont::DemiBold);
    const int textWidth = QFontMetrics(nameFont).horizontalAdvance(
        index.data(Qt::DisplayRole).toString());
    return QSize(kCellPadding + kRankWidth + kRankGap + kBadgeDiameter + 10 + textWidth
                     + kCellPadding,
                 kRowHeight);
}

void PointsBarDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    paintFrame(painter, option, index);

    const LeaderboardTable* table = tableOf(option);
    if (!table) {
        return;
    }

    const QRect area = option.rect.adjusted(kCellPadding, 0, -kCellPadding, 0);
    const QFont valueFont = fixedFont(option.font, true);
    const QFontMetrics fm(valueFont);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setFont(valueFont);
    painter->setPen(table->textPrimaryColor());

    const int valueWidth = fm.horizontalAdvance(QStringLiteral("000"));
    const QRect valueRect(area.left(), area.top(), valueWidth, area.height());
    painter->drawText(valueRect, Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());

    const QRect track(valueRect.right() + 1 + 8, area.center().y() - kBarHeight / 2,
                      kBarWidth, kBarHeight);
    const double ratio = index.data(LeaderboardModel::BarRatioRole).toDouble();
    const int fillWidth = static_cast<int>(track.width() * ratio);

    painter->setPen(Qt::NoPen);
    painter->setBrush(table->trackColor());
    painter->drawRoundedRect(track, kBarHeight / 2.0, kBarHeight / 2.0);
    if (fillWidth > 0) {
        painter->setBrush(table->accentColor());
        painter->drawRoundedRect(QRect(track.left(), track.top(), fillWidth, track.height()),
                                 kBarHeight / 2.0, kBarHeight / 2.0);
    }
    painter->restore();
}

QSize PointsBarDelegate::sizeHint(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    Q_UNUSED(index)
    const QFontMetrics fm(fixedFont(option.font, true));
    return QSize(fm.horizontalAdvance(QStringLiteral("000")) + 8 + kBarWidth + 2 * kCellPadding,
                 kRowHeight);
}

void EfficiencyChipDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    paintFrame(painter, option, index);

    const LeaderboardTable* table = tableOf(option);
    if (!table) {
        return;
    }

    const QRect area = option.rect.adjusted(kCellPadding, 0, -kCellPadding, 0);
    const QString text = index.data(Qt::DisplayRole).toString();
    const bool high = LeaderboardModel::isHighEfficiency(
        index.data(LeaderboardModel::EfficiencyValueRole).toDouble());

    // Reference .badge: padding 3px 8px, 1px border, radius 6, 11px / 600 (700 when done).
    const QFont chipFont = chipFontFor(option.font, high);
    const QFontMetrics fm(chipFont);
    const int chipHeight = fm.height() + 2 * 3 + 2;
    const int chipWidth = std::min(fm.horizontalAdvance(text) + 2 * 8 + 2, area.width());
    const QRect chip(area.left(), area.center().y() - chipHeight / 2, chipWidth, chipHeight);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(high ? table->successSoftColor() : table->accentSoftColor());
    painter->setPen(high ? table->borderColor() : table->accentColor());
    painter->drawRoundedRect(QRectF(chip).adjusted(0.5, 0.5, -0.5, -0.5), kChipRadius,
                             kChipRadius);
    painter->setFont(chipFont);
    painter->setPen(high ? table->textPrimaryColor() : table->accentColor());
    painter->drawText(chip, Qt::AlignCenter, text);
    painter->restore();
}

QSize EfficiencyChipDelegate::sizeHint(const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    const QFontMetrics fm(chipFontFor(option.font, true));
    return QSize(fm.horizontalAdvance(index.data(Qt::DisplayRole).toString()) + 2 * 8 + 2
                     + 2 * kCellPadding,
                 kRowHeight);
}

} // namespace planova::s6