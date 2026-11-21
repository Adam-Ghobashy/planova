#pragma once
#include "app/CoreBridge.h"
#include "planova/PlanovaCore.h"
#include <QList>
#include <QWidget>

class ProjectCardWidget;
class QGraphicsOpacityEffect;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;

// First page of the app. Intro mode: animated "Welcome!", Start New Project / Continue Your Projects.
// Projects mode (also what Page::Hub shows): search, git banner and one card per workspace.
class WelcomePage : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Intro, Projects };
    Q_ENUM(Mode)

    WelcomePage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return mode_; }

signals:
    void startNewProject();
    void projectOpened();           // the core has opened the chosen project
    void manageGitRequested();
    void modeChanged(WelcomePage::Mode mode);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildIntro();
    QWidget* buildProjects();
    QWidget* buildGitBanner();
    QWidget* buildWorkspaceState();
    void refresh();                 // re-reads the projects and the repository from the core
    void applyFilter();             // shows the cards that match the search box and lays them out
    void updateState(int total, int shown);
    void updateGitIcon();
    void fadeIn(int elapsedMs);
    void openProject(const QString& projectId);

    PlanovaCore& core_;
    Mode mode_ = Mode::Intro;
    QWidget* introCard_ = nullptr;
    QWidget* projectsCard_ = nullptr;

    // intro
    QLabel* subtitle_ = nullptr;
    QWidget* introButtons_ = nullptr;
    QGraphicsOpacityEffect* subtitleFade_ = nullptr;
    QGraphicsOpacityEffect* buttonsFade_ = nullptr;

    // projects
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
