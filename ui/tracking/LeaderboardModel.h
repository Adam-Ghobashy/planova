#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QString>
#include <QStyledItemDelegate>
#include <QTableView>

#include <planova/common/Dtos.h>
#include <planova/stats/LeaderboardCalculator.h>

#include <vector>

namespace planova::s6 {

// QTableView that carries the leaderboard palette as properties. The values come
// from tracking.qss (qproperty-...), so no colour literal exists in C++.
class LeaderboardTable : public QTableView {
    Q_OBJECT
    Q_PROPERTY(QColor textPrimaryColor MEMBER textPrimary_)
    Q_PROPERTY(QColor textMutedColor MEMBER textMuted_)
    Q_PROPERTY(QColor accentColor MEMBER accent_)
    Q_PROPERTY(QColor accentTextColor MEMBER accentText_)
    Q_PROPERTY(QColor accentSoftColor MEMBER accentSoft_)
    Q_PROPERTY(QColor successSoftColor MEMBER successSoft_)
    Q_PROPERTY(QColor trackColor MEMBER track_)
    Q_PROPERTY(QColor surfaceAltColor MEMBER surfaceAlt_)
    Q_PROPERTY(QColor borderColor MEMBER border_)
    Q_PROPERTY(QColor member1Color MEMBER member1_)
    Q_PROPERTY(QColor member2Color MEMBER member2_)
    Q_PROPERTY(QColor member3Color MEMBER member3_)
    Q_PROPERTY(QColor member4Color MEMBER member4_)
    Q_PROPERTY(QColor member5Color MEMBER member5_)
    Q_PROPERTY(QColor member6Color MEMBER member6_)
public:
    explicit LeaderboardTable(QWidget* parent = nullptr);

    QColor textPrimaryColor() const { return textPrimary_; }
    QColor textMutedColor() const { return textMuted_; }
    QColor accentColor() const { return accent_; }
    QColor accentTextColor() const { return accentText_; }
    QColor accentSoftColor() const { return accentSoft_; }
    QColor successSoftColor() const { return successSoft_; }
    QColor trackColor() const { return track_; }
    QColor surfaceAltColor() const { return surfaceAlt_; }
    QColor borderColor() const { return border_; }

    // index 0..5 = Member1..Member6; anything else falls back to SurfaceAlt.
    QColor memberColor(int index) const
    {
        switch (index) {
        case 0: return member1_;
        case 1: return member2_;
        case 2: return member3_;
        case 3: return member4_;
        case 4: return member5_;
        case 5: return member6_;
        default: return surfaceAlt_;
        }
    }

private:
    QColor textPrimary_, textMuted_, accent_, accentText_, accentSoft_, successSoft_, track_,
        surfaceAlt_, border_;
    QColor member1_, member2_, member3_, member4_, member5_, member6_;
};

// Read-only model. Row order is exactly the order of StatsService::leaderboard().
class LeaderboardModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        RankMemberColumn = 0, // "#1" + initials badge + name, painted by MemberCellDelegate
        RoleColumn,
        DoneAssignedColumn,
        PointsColumn,
        EfficiencyColumn,
        ColumnCount
    };

    enum Role {
        RankTextRole = Qt::UserRole + 1, // "#1"
        InitialsRole,
        BarRatioRole,    // double 0..1, points / top points
        MemberIndexRole, // int 0..5 (Member1..Member6) or -1 for an unknown member
        EfficiencyValueRole // double pts/h (drives the chip style)
    };

    // Efficiency chip turns "done" (SuccessSoft) above this value (reference: 0.5).
    static constexpr double kHighEfficiency = 0.5;

    explicit LeaderboardModel(QObject* parent = nullptr);

    // rows: StatsService::leaderboard(); members: IMemberQuery::all().
    void setRows(const std::vector<LeaderRow>& rows, const std::vector<MemberDto>& members);

    QString firstNameForRow(int row) const;
    double pointsForRow(int row) const;
    double efficiencyForRow(int row) const;
    // Member1..Member6 = 0..5: position in the member list modulo six. -1 if unknown.
    int memberIndexForRow(int row) const;

    // "0.75 pts/h", "0.5 pts/h", "0 pts/h" (trailing zeros dropped, like the reference).
    static QString efficiencyText(double efficiency);
    static bool isHighEfficiency(double efficiency) { return efficiency > kHighEfficiency; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    struct Entry {
        LeaderRow row;
        QString name;
        QString role;
        QString initials;
        int memberIndex = -1;
    };

    const Entry* entryAt(int row) const;

    std::vector<Entry> entries_;
    int maxPoints_ = 0;
};

// Draws the QSS item frame (row separator) without the item text.
class LeaderboardCellDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void paintFrame(QPainter* painter, const QStyleOptionViewItem& option,
                    const QModelIndex& index) const;
    static const LeaderboardTable* tableOf(const QStyleOptionViewItem& option);
};

// Plain 12px text cell. primary = TextPrimary (Done / Assigned), otherwise TextMuted (Role).
class TextCellDelegate : public LeaderboardCellDelegate {
    Q_OBJECT
public:
    explicit TextCellDelegate(bool primary, QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    bool primary_;
};

// "#1", a round initials badge tinted by the member colour, then the member name.
class MemberCellDelegate : public LeaderboardCellDelegate {
    Q_OBJECT
public:
    using LeaderboardCellDelegate::LeaderboardCellDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

// Points value plus a small bar (track + accent fill).
class PointsBarDelegate : public LeaderboardCellDelegate {
    Q_OBJECT
public:
    using LeaderboardCellDelegate::LeaderboardCellDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

// Rounded "x.xx pts/h" chip: SuccessSoft above kHighEfficiency, AccentSoft otherwise.
class EfficiencyChipDelegate : public LeaderboardCellDelegate {
    Q_OBJECT
public:
    using LeaderboardCellDelegate::LeaderboardCellDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace planova::s6