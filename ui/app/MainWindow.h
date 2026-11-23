#pragma once
#include "CoreBridge.h"
#include "Page.h"
#include <QMainWindow>
#include <QStackedWidget>
#include <QWidget>
#include <optional>

namespace planova::s6 {
class HomePage;
}

class Sidebar;
class SetupPage;
class WelcomePage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(PlanovaCore& core, CoreBridge& bridge, QWidget* parent = nullptr);

    // The popup shown at start-up: start a new project or continue an old one.
    void showStartDialog();

protected:
    // Remembers whether the last user input was a key press or a mouse press, see showPage.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildPages();
    void addShortcuts();
    // `fromControl` is true when a control inside the pages asked for the switch (a link or button on a page).
    // Only then can the keyboard focus follow to the new page, see showPage.
    void showPage(Page page, bool fromControl = false);
    // The page saved by the last session, if it is one of the sidebar pages.
    std::optional<Page> savedPage() const;
    void showHomeForCurrentProject(bool fromControl = false);
    void applyCurrentProjectToHome();   // name and process model of the open project, to the Home header
    void editProject(const QString& projectId);
    void deleteProject(const QString& projectId);
    std::optional<ProjectDto> currentProjectInfo() const;
    void updateStatusBar(const QString& lastEvent = QString());

    PlanovaCore& core_;
    CoreBridge& bridge_;
    Sidebar* sidebar_;
    QStackedWidget* pages_;
    planova::s6::HomePage* homePage_ = nullptr;
    WelcomePage* welcome_ = nullptr;
    SetupPage* setupPage_ = nullptr;
    bool lastInputWasKey_ = false;
};
