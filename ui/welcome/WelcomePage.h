#pragma once
#include "app/CoreBridge.h"
#include "planova/PlanovaCore.h"
#include <QList>
#include <QWidget>

class ProjectCardWidget;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;

// The Projects Hub page: search, git banner and one card per workspace.
class WelcomePage : public QWidget {
    Q_OBJECT
public:
    WelcomePage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent = nullptr);

signals:
    void startNewProject();
    void projectOpened();           // the core has opened the chosen project
    void manageGitRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildProjects();
    QWidget* buildGitBanner();
    QWidget* buildWorkspaceState();
    void refresh();                 // re-reads the projects and the repository from the core
    void applyFilter();             // shows the cards that match the search box and lays them out
    void updateState(int total, int shown);
    void updateGitIcon();
    void openProject(const QString& projectId);

    PlanovaCore& core_;
    QWidget* projectsCard_ = nullptr;

    QLineEdit* search_ = nullptr;
    QLabel* count_ = nullptr;
    QLabel* gitIcon_ = nullptr;
    QLabel* gitRepo_ = nullptr;
    QWidget* gridHost_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QList<ProjectCardWidget*> cards_;
    QWidget* statePanel_ = nullptr;
    QLabel* stateMessage_ = nullptr;
    QPushButton* stateStart_ = nullptr;
    QPushButton* stateClear_ = nullptr;
};
