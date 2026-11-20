#include <QtTest>

#include <string>
#include <vector>

#include "ui/tracking/LeaderboardModel.h"

using namespace planova::s6;

class tst_LeaderboardModel : public QObject {
    Q_OBJECT

private:
    static LeaderRow makeRow(const char* id, int done, int assigned, int points, double eff,
                             int rank)
    {
        LeaderRow r;
        r.memberId = id;
        r.done = done;
        r.assigned = assigned;
        r.points = points;
        r.efficiency = eff;
        r.rank = rank;
        return r;
    }

    static MemberDto makeMember(const char* id, const char* name, const char* role)
    {
        MemberDto m;
        m.id = id;
        m.name = name;
        m.role = role;
        return m;
    }

private slots:
    void keepsRowOrderAndRanks()
    {
        LeaderboardModel model;
        model.setRows({makeRow("b", 2, 3, 8, 1.0, 1), makeRow("a", 1, 2, 3, 0.5, 2)},
                      {makeMember("a", "Ada Lovelace", "Dev"), makeMember("b", "Bob Ray", "")});
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.index(0, LeaderboardModel::RankMemberColumn).data().toString(),
                 QStringLiteral("Bob Ray"));
        QCOMPARE(model.index(0, 0).data(LeaderboardModel::RankTextRole).toString(),
                 QStringLiteral("#1"));
        QCOMPARE(model.index(1, 0).data(LeaderboardModel::RankTextRole).toString(),
                 QStringLiteral("#2"));
    }

    void emptyRoleIsBlank()
    {
        LeaderboardModel model;
        model.setRows({makeRow("b", 0, 0, 0, 0.0, 1)}, {makeMember("b", "Bob Ray", "")});
        QVERIFY(model.index(0, LeaderboardModel::RoleColumn).data().toString().isEmpty());
    }

    void initialsUseFirstTwoWords()
    {
        LeaderboardModel model;
        model.setRows({makeRow("a", 0, 0, 0, 0.0, 1)},
                      {makeMember("a", "ada king lovelace", "")});
        QCOMPARE(model.index(0, 0).data(LeaderboardModel::InitialsRole).toString(),
                 QStringLiteral("AK"));
    }

    void barRatioScalesToTopMember()
    {
        LeaderboardModel model;
        model.setRows({makeRow("a", 0, 0, 10, 1.0, 1), makeRow("b", 0, 0, 5, 1.0, 2)},
                      {makeMember("a", "A", ""), makeMember("b", "B", "")});
        QCOMPARE(model.index(0, LeaderboardModel::PointsColumn)
                     .data(LeaderboardModel::BarRatioRole).toDouble(), 1.0);
        QCOMPARE(model.index(1, LeaderboardModel::PointsColumn)
                     .data(LeaderboardModel::BarRatioRole).toDouble(), 0.5);
    }

    void barRatioIsZeroWhenNobodyDelivered()
    {
        LeaderboardModel model;
        model.setRows({makeRow("a", 0, 0, 0, 0.0, 1)}, {makeMember("a", "A", "")});
        QCOMPARE(model.index(0, LeaderboardModel::PointsColumn)
                     .data(LeaderboardModel::BarRatioRole).toDouble(), 0.0);
    }

    void emptyRowsGiveEmptyModel()
    {
        LeaderboardModel model;
        model.setRows({}, {});
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.columnCount(), int(LeaderboardModel::ColumnCount));
    }

    void memberIndexFollowsMemberPositionModSix()
    {
        std::vector<MemberDto> members;
        std::vector<LeaderRow> rows;
        for (int i = 0; i < 7; ++i) {
            members.push_back(makeMember(std::to_string(i).c_str(), "M", ""));
        }
        // Leaderboard order is reversed relative to the member list.
        for (int i = 6; i >= 0; --i) {
            rows.push_back(makeRow(std::to_string(i).c_str(), 0, 0, 0, 0.0, 7 - i));
        }
        LeaderboardModel model;
        model.setRows(rows, members);
        QCOMPARE(model.memberIndexForRow(0), 0); // member position 6 -> Member1 again
        QCOMPARE(model.memberIndexForRow(6), 0); // member position 0 -> Member1
        QCOMPARE(model.memberIndexForRow(1), 5); // member position 5 -> Member6
    }

    void unknownMemberFallsBackToIdAndNeutralBadge()
    {
        LeaderboardModel model;
        model.setRows({makeRow("ghost", 0, 0, 0, 0.0, 1)}, {});
        QCOMPARE(model.index(0, LeaderboardModel::RankMemberColumn).data().toString(),
                 QStringLiteral("ghost"));
        QCOMPARE(model.memberIndexForRow(0), -1);
    }

    void doneAssignedShowsBothCounts()
    {
        LeaderboardModel model;
        model.setRows({makeRow("a", 3, 5, 0, 0.0, 1)}, {makeMember("a", "A", "")});
        QCOMPARE(model.index(0, LeaderboardModel::DoneAssignedColumn).data().toString(),
                 QStringLiteral("3 / 5"));
    }

    void efficiencyTextDropsTrailingZeros()
    {
        QCOMPARE(LeaderboardModel::efficiencyText(0.75), QStringLiteral("0.75 pts/h"));
        QCOMPARE(LeaderboardModel::efficiencyText(0.5), QStringLiteral("0.5 pts/h"));
        QCOMPARE(LeaderboardModel::efficiencyText(1.0), QStringLiteral("1 pts/h"));
        QCOMPARE(LeaderboardModel::efficiencyText(0.0), QStringLiteral("0 pts/h"));
    }

    void highEfficiencyThresholdIsStrictlyAboveHalf()
    {
        QVERIFY(!LeaderboardModel::isHighEfficiency(0.5));
        QVERIFY(LeaderboardModel::isHighEfficiency(0.51));
        QVERIFY(!LeaderboardModel::isHighEfficiency(0.0));
    }
};

QTEST_MAIN(tst_LeaderboardModel)
#include "tst_LeaderboardModel.moc"
