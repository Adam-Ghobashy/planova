#ifndef PLANOVA_HOMEPAGE_H
#define PLANOVA_HOMEPAGE_H

#include <QString>
#include <QWidget>

#include <vector>

#include <planova/stats/HomeService.h>
#include <planova/common/Queries.h>

class BarChartWidget;
class DonutWidget;
class QEvent;
class QFrame;
class QGridLayout;
class QLabel;
class QObject;
class QPushButton;
class StatCardWidget;

namespace planova::s6 {

class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(HomeService& homeService,
                      IMemberQuery& memberQuery,
                      QWidget* parent = nullptr);

public slots:
    void refresh();

    void setProjectName(const QString& name);

    // "Agile" or "Scrum" shows the Active Sprint badge; any other model hides it.
    void setProcessModel(const QString& model);

    // Total open pins (IPinQuery::openCount) for the footer of the pins preview.
    void setOpenPinCount(int count);

signals:
    void projectsHubRequested();
    void slicerRequested();
    void teamSkillsRequested();
    void githubRequested();
    void pinsRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();

    void updatePins(const std::vector<PinDto>& pins);
    void updatePinsCount();

    void updateContributionChart(const DashboardData& data);

    QFrame* createPanel(const QString& title,
                        QWidget* content,
                        QWidget* headerExtra = nullptr);

    QFrame* createPinsPanel();

    QWidget* createPinNote(const PinDto& pin,
                           const QString& authorName,
                           int index);

    QPushButton* createQuickLink(const QString& icon,
                                 const QString& title,
                                 const QString& subtitle,
                                 const QString& linkType);

    HomeService& m_homeService;
    IMemberQuery& m_memberQuery;

    QLabel* m_projectTitle = nullptr;
    QLabel* m_sprintBadge = nullptr;

    StatCardWidget* m_totalCard = nullptr;
    StatCardWidget* m_completedCard = nullptr;
    StatCardWidget* m_inProgressCard = nullptr;
    StatCardWidget* m_remainingCard = nullptr;

    DonutWidget* m_statusDonut = nullptr;
    BarChartWidget* m_contributionChart = nullptr;

    QFrame* m_pinsPanel = nullptr;
    QGridLayout* m_pinsGrid = nullptr;
    QLabel* m_pinsEmptyLabel = nullptr;
    QLabel* m_pinsCountLabel = nullptr;

    QString m_pinsSignature;
    bool m_pinsBuilt = false;
    int m_openPinCount = -1;  // -1 = unknown, count the preview
};

} // namespace planova::s6

#endif