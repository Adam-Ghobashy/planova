#pragma once
#include "CoreBridge.h"
#include "Page.h"
#include <QMainWindow>
#include <QStackedWidget>
#include <QWidget>

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

private:
    void buildPages();
    void showPage(Page page);
    void showHomeForCurrentProject();

    PlanovaCore& core_;
    CoreBridge& bridge_;
    Sidebar* sidebar_;
    QStackedWidget* pages_;
    planova::s6::HomePage* homePage_ = nullptr;
    WelcomePage* welcome_ = nullptr;
    SetupPage* setupPage_ = nullptr;
};
