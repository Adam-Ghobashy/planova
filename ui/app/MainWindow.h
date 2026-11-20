#pragma once
#include "CoreBridge.h"
#include <QMainWindow>
#include <QStackedWidget>
#include <QWidget>

namespace planova::s6 {
class HomePage;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Order matches the stacked widget and the sidebar buttons.
    enum class Page { Welcome, Hub, Slicer, Pins, Team, Board, Tracking, GitHub, Home };

    MainWindow(PlanovaCore& core, CoreBridge& bridge, QWidget* parent = nullptr);

private:
    void buildPages();
    QWidget* buildSidebar();
    void showPage(Page page);
    void showHomeForCurrentProject();

    PlanovaCore& core_;
    CoreBridge& bridge_;
    QStackedWidget* pages_;
    planova::s6::HomePage* homePage_ = nullptr;
};
