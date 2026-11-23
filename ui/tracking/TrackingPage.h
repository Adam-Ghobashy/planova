#pragma once

#include <QWidget>

#include <planova/common/Queries.h>
#include <planova/stats/StatsService.h>

#include "../widgets/StatCardWidget.h"

class QButtonGroup;
class QLabel;
class QPushButton;

namespace planova::s6 {

class LeaderboardModel;
class LeaderboardTable;
class ComplexityBarChart;

// Fair Performance & Tracking: four stat cards, leaderboard, complexity chart and the
// Complexity Contribution / Hourly Efficiency toggle. Reads StatsService and
// IMemberQuery only; writes nothing.
class TrackingPage : public QWidget {
    Q_OBJECT
public:
    TrackingPage(StatsService& stats, IMemberQuery& members, QWidget* parent = nullptr);

    // Re-reads the services. MainWindow connects CoreBridge::changed to it.
    void refresh();

private:
    enum class ChartMetric { Contribution, Efficiency };

    void buildUi();
    void refreshCards();
    void refreshChart();
    void setMetric(ChartMetric metric);

    StatsService& stats_;
    IMemberQuery& members_;

    StatCardWidget* overallProgressCard_ = nullptr;
    StatCardWidget* complexityCard_ = nullptr;
    StatCardWidget* teamCard_ = nullptr;
    StatCardWidget* topPerformerCard_ = nullptr;

    LeaderboardTable* table_ = nullptr;
    LeaderboardModel* model_ = nullptr;

    ComplexityBarChart* chart_ = nullptr;
    QLabel* emptyLeaderboard_ = nullptr;
    QLabel* emptyChart_ = nullptr;

    QPushButton* contributionButton_ = nullptr;
    QPushButton* efficiencyButton_ = nullptr;
    QButtonGroup* metricGroup_ = nullptr;

    ChartMetric metric_ = ChartMetric::Contribution;
};

} // namespace planova::s6