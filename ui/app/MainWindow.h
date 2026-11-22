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

private:
    void buildPages();
    void addShortcuts();
    void showPage(Page page);
    // The page saved by the last session, if it is one of the sidebar pages.
    std::optional<Page> savedPage() const;
    void showHomeForCurrentProject();
    std::optional<ProjectDto> currentProjectInfo() const;
    void updateStatusBar(const QString& lastEvent = QString());

    PlanovaCore& core_;
    CoreBridge& bridge_;
    Sidebar* sidebar_;
    QStackedWidget* pages_;
    planova::s6::HomePage* homePage_ = nullptr;
    WelcomePage* welcome_ = nullptr;
    SetupPage* setupPage_ = nullptr;
};
