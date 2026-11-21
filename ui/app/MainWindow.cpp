#include "MainWindow.h"
#include "board/PhaseBoardPage.h"
#include "git/GitHubIntegrationPage.h"
#include "Sidebar.h"
#include "home/HomePage.h"
#include "pins/PinsPage.h"
#include "slicer/ChaosSlicerPage.h"
#include "team/TeamSkillsPage.h"
#include "tracking/TrackingPage.h"
#include "welcome/SetupPage.h"
#include "welcome/WelcomePage.h"
#include <QHBoxLayout>
#include <QStatusBar>

MainWindow::MainWindow(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QMainWindow(parent), core_(core), bridge_(bridge), sidebar_(new Sidebar(core, this)), pages_(new QStackedWidget(this))
{
    setWindowTitle("Planova");
    resize(1280, 820);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(sidebar_);
    layout->addWidget(pages_, 1);
    setCentralWidget(central);
    // Start with focus on the page area, so no sidebar item shows a focus ring before the user presses Tab.
    central->setFocusPolicy(Qt::ClickFocus);
    central->setFocus();

    buildPages();
    showPage(Page::Welcome);
    if (core_.currentProject())
        showHomeForCurrentProject(); // a project was auto-opened at start-up

    auto updateTelemetry = [this](const QString& lastEvent = QString()) {
        const size_t memberCount = core_.memberProjection().all().size();
        const QString gitState = core_.gitProjection().isConnected() ? "Synced" : "Disconnected";
        QString msg = QString("Workspace: Ready  •  Git: %1  •  Core Engine: C++17  •  %2 Team Members")
                          .arg(gitState)
                          .arg(memberCount);
        if (!lastEvent.isEmpty()) {
            msg += "  •  Last Event: " + lastEvent;
        }
        statusBar()->showMessage(msg);
    };

    connect(&bridge_, &CoreBridge::changed, sidebar_, &Sidebar::refresh);
    connect(sidebar_, &Sidebar::pageRequested, this, &MainWindow::showPage);
    connect(&bridge_, &CoreBridge::changed, this, [updateTelemetry](const QString& eventType) {
        updateTelemetry(eventType);
    });
    updateTelemetry();
}

void MainWindow::buildPages()
{
    welcome_ = new WelcomePage(core_, bridge_, pages_);
    setupPage_ = new SetupPage(core_, pages_);
    auto* slicerPage = new planova::slicer::ChaosSlicerPage(
        core_.slicer(), core_.taskQuery(), core_.memberQuery(), pages_);
    auto* pinsPage = new planova::s5::PinsPage(
        core_.pinBoard(), core_.pins(), core_.memberQuery(), core_.taskQuery(), pages_);
    auto* teamPage = new planova::team::TeamSkillsPage(
        core_.members(), core_.memberProjection(), core_.conflicts(), core_.conflictProjection(),
        core_.taskQuery(), bridge_, pages_);
    auto* boardPage = new PhaseBoardPage(core_, bridge_, pages_);
    auto* trackingPage = new planova::s6::TrackingPage(core_.stats(), core_.memberQuery(), pages_);
    auto* gitPage = new planova::git::GitHubIntegrationPage(
        core_.git(), core_.gitProjection(), core_.memberQuery(), bridge_, pages_);
    homePage_ = new planova::s6::HomePage(core_.home(), core_.memberQuery(), pages_);

    // One line per page, in Page enum order.
    pages_->addWidget(welcome_);
    pages_->addWidget(new QWidget(pages_)); // Page::Hub placeholder: showPage(Hub) shows the Welcome widget in projects mode
    pages_->addWidget(setupPage_);
    pages_->addWidget(slicerPage);
    pages_->addWidget(pinsPage);
    pages_->addWidget(teamPage);
    pages_->addWidget(boardPage);
    pages_->addWidget(trackingPage);
    pages_->addWidget(gitPage);
    pages_->addWidget(homePage_);

    // Board, Team and GitHub pages connect to the bridge themselves.
    connect(&bridge_, &CoreBridge::changed, slicerPage, &planova::slicer::ChaosSlicerPage::refreshFromBoard);
    connect(&bridge_, &CoreBridge::changed, trackingPage, &planova::s6::TrackingPage::refresh);
    connect(&bridge_, &CoreBridge::changed, homePage_, &planova::s6::HomePage::refresh);
    connect(&bridge_, &CoreBridge::changed, pinsPage, &planova::s5::PinsPage::refresh);
    // The Home footer counts every open pin, not just the four shown.
    auto showOpenPinCount = [this] { homePage_->setOpenPinCount(core_.pinQuery().openCount()); };
    connect(&bridge_, &CoreBridge::changed, homePage_, showOpenPinCount);
    showOpenPinCount();

    connect(boardPage, &PhaseBoardPage::slicerRequested, this, [this] { showPage(Page::Slicer); });
    connect(slicerPage, &planova::slicer::ChaosSlicerPage::assignmentsApplied, this,
            [this](int) { showPage(Page::Board); });

    connect(homePage_, &planova::s6::HomePage::projectsHubRequested, this, [this] { showPage(Page::Hub); });
    connect(homePage_, &planova::s6::HomePage::slicerRequested, this, [this] { showPage(Page::Slicer); });
    connect(homePage_, &planova::s6::HomePage::teamSkillsRequested, this, [this] { showPage(Page::Team); });
    connect(homePage_, &planova::s6::HomePage::githubRequested, this, [this] { showPage(Page::GitHub); });
    connect(homePage_, &planova::s6::HomePage::pinsRequested, this, [this] { showPage(Page::Pins); });

    connect(welcome_, &WelcomePage::startNewProject, this, [this] {
        setupPage_->reset();   // every visit starts with an empty setup
        showPage(Page::Setup);
    });
    // The new project is already open (setupProject opens it), so Home can show it.
    connect(setupPage_, &SetupPage::projectCreated, this, [this] { showHomeForCurrentProject(); });
    connect(setupPage_, &SetupPage::skipped, this, [this] {
        if (core_.currentProject())
            showHomeForCurrentProject();
        else
            showPage(Page::Welcome);
    });
    // openProject publishes ProjectOpened on the bridge, which also refreshes the sidebar badges.
    connect(welcome_, &WelcomePage::projectOpened, this, [this] { showHomeForCurrentProject(); });
    connect(welcome_, &WelcomePage::manageGitRequested, this, [this] { showPage(Page::GitHub); });
    // The Continue and Back buttons switch the mode inside the page; keep the sidebar in step.
    connect(welcome_, &WelcomePage::modeChanged, this, [this](WelcomePage::Mode mode) {
        sidebar_->setActivePage(mode == WelcomePage::Mode::Projects ? Page::Hub : Page::Welcome);
    });
}

void MainWindow::showPage(Page page)
{
    // Welcome and Hub are one widget in two modes; the stack holds a spacer at the Hub index.
    if (page == Page::Hub || page == Page::Welcome) {
        welcome_->setMode(page == Page::Hub ? WelcomePage::Mode::Projects : WelcomePage::Mode::Intro);
        pages_->setCurrentIndex(static_cast<int>(Page::Welcome));
    } else {
        pages_->setCurrentIndex(static_cast<int>(page));
    }
    sidebar_->setActivePage(page);
}

void MainWindow::showHomeForCurrentProject()
{
    const auto current = core_.currentProject();
    if (current) {
        for (const auto& project : core_.projects().list()) {
            if (project.id == *current) {
                homePage_->setProjectName(QString::fromStdString(project.name));
                break;
            }
        }
    }
    showPage(Page::Home);
}
