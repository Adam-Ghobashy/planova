#include "MainWindow.h"
#include "board/PhaseBoardPage.h"
#include "git/GitHubIntegrationPage.h"
#include "home/HomePage.h"
#include "slicer/ChaosSlicerPage.h"
#include "team/TeamSkillsPage.h"
#include "tracking/TrackingPage.h"
#include "welcome/ProjectsHubPage.h"
#include "welcome/SetupWizard.h"
#include "welcome/WelcomePage.h"
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>
#include <QStringList>
#include <QVBoxLayout>

namespace {
const QStringList kScreens = {
    "Welcome", "Projects Hub", "Chaos Slicer (Pizza)", "Pins & Collab",
    "Team & Skills", "Phase Board", "Fair Tracking & Charts", "GitHub Integration", "Project Home"
};
}

MainWindow::MainWindow(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QMainWindow(parent), core_(core), bridge_(bridge), pages_(new QStackedWidget(this))
{
    setWindowTitle("Planova");
    resize(1280, 820);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildSidebar());
    layout->addWidget(pages_, 1);
    setCentralWidget(central);

    buildPages();

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

    connect(&bridge_, &CoreBridge::changed, this, [updateTelemetry](const QString& eventType) {
        updateTelemetry(eventType);
    });
    updateTelemetry();
}

void MainWindow::buildPages()
{
    auto* welcome = new WelcomePage(pages_);
    auto* hub = new ProjectsHubPage(core_, bridge_, pages_);
    auto* slicerPage = new planova::slicer::ChaosSlicerPage(
        core_.slicer(), core_.taskQuery(), core_.memberQuery(), pages_);
    // Pins stays a placeholder until the S5 backend exists.
    auto* pinsPage = new QLabel("Pins & Collab - Coming soon", pages_);
    pinsPage->setAlignment(Qt::AlignCenter);
    auto* teamPage = new planova::team::TeamSkillsPage(
        core_.members(), core_.memberQuery(), bridge_, pages_);
    auto* boardPage = new PhaseBoardPage(core_, bridge_, pages_);
    auto* trackingPage = new planova::s6::TrackingPage(core_.stats(), core_.memberQuery(), pages_);
    auto* gitPage = new planova::git::GitHubIntegrationPage(
        core_.git(), core_.gitProjection(), core_.memberQuery(), bridge_, pages_);
    homePage_ = new planova::s6::HomePage(core_.home(), core_.memberQuery(), pages_);

    // One line per page, in Page enum order.
    pages_->addWidget(welcome);
    pages_->addWidget(hub);
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

    connect(slicerPage, &planova::slicer::ChaosSlicerPage::assignmentsApplied, this,
            [this](int) { showPage(Page::Board); });

    connect(homePage_, &planova::s6::HomePage::projectsHubRequested, this, [this] { showPage(Page::Hub); });
    connect(homePage_, &planova::s6::HomePage::slicerRequested, this, [this] { showPage(Page::Slicer); });
    connect(homePage_, &planova::s6::HomePage::teamSkillsRequested, this, [this] { showPage(Page::Team); });
    connect(homePage_, &planova::s6::HomePage::githubRequested, this, [this] { showPage(Page::GitHub); });
    connect(homePage_, &planova::s6::HomePage::pinsRequested, this, [this] { showPage(Page::Pins); });

    connect(welcome, &WelcomePage::continueProjects, this, [this] { showPage(Page::Hub); });

    connect(welcome, &WelcomePage::startNewProject, this, [this] {
        SetupWizard wizard(core_, this);
        if (wizard.exec() == QDialog::Accepted) {
            showHomeForCurrentProject();
        }
    });

    connect(hub, &ProjectsHubPage::projectOpened, this, [this] { showHomeForCurrentProject(); });
}

void MainWindow::showPage(Page page)
{
    pages_->setCurrentIndex(static_cast<int>(page));
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

QWidget* MainWindow::buildSidebar()
{
    auto* sidebar = new QWidget(this);
    sidebar->setFixedWidth(230);
    sidebar->setObjectName("Sidebar");
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(12, 16, 12, 16);
    layout->setSpacing(6);

    for (int i = 0; i < kScreens.size(); ++i) {
        auto* button = new QPushButton(kScreens[i], sidebar);
        button->setObjectName("SidebarNavButton");
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, i] {
            // kScreens is in Page enum order, so the button index is the page index.
            showPage(static_cast<Page>(i));
        });
        layout->addWidget(button);
    }
    layout->addStretch();

    // --- S2 Sidebar Git Sync Status Card ---
    auto* gitCard = new QFrame(sidebar);
    gitCard->setObjectName("SidebarGitCard");
    auto* gitLayout = new QHBoxLayout(gitCard);
    gitLayout->setContentsMargins(12, 10, 12, 10);
    gitLayout->setSpacing(8);

    auto* dotLbl = new QLabel("●", gitCard);
    dotLbl->setObjectName("SidebarGitDot");
    gitLayout->addWidget(dotLbl);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(1);
    auto* syncTitle = new QLabel("Synced with Git", gitCard);
    syncTitle->setObjectName("SidebarGitTitle");
    auto* syncSub = new QLabel("Peer Event Store", gitCard);
    syncSub->setObjectName("SidebarGitSubtitle");
    textCol->addWidget(syncTitle);
    textCol->addWidget(syncSub);
    gitLayout->addLayout(textCol, 1);

    auto* syncBtn = new QPushButton("⟳", gitCard);
    syncBtn->setFixedSize(28, 28);
    syncBtn->setCursor(Qt::PointingHandCursor);
    syncBtn->setToolTip("Simulate Git Sync");
    syncBtn->setObjectName("SidebarSyncButton");
    connect(syncBtn, &QPushButton::clicked, this, [this] {
        // Not connected yet: the GitHub page is where the user enters the repository.
        if (!core_.gitProjection().isConnected()) {
            showPage(Page::GitHub);
            return;
        }
        core_.git().simulateSync(core_.memberProjection().all());
        showPage(Page::GitHub);
    });
    gitLayout->addWidget(syncBtn);

    auto updateGitCard = [this, dotLbl, syncTitle, syncSub]() {
        const bool connected = core_.gitProjection().isConnected();
        // The colour comes from the qss; the property selects it. Re-polish so it takes effect.
        dotLbl->setProperty("state", connected ? "connected" : "disconnected");
        dotLbl->style()->unpolish(dotLbl);
        dotLbl->style()->polish(dotLbl);
        if (connected) {
            syncTitle->setText("Synced with Git");
            syncSub->setText(QString::fromStdString(
                "Branch: " + core_.gitProjection().branch() + " (" +
                std::to_string(core_.gitProjection().commits().size()) + " commits)"));
        } else {
            syncTitle->setText("Git Disconnected");
            syncSub->setText("Click ⟳ to sync");
        }
    };
    connect(&bridge_, &CoreBridge::changed, this, [updateGitCard](const QString&) {
        updateGitCard();
    });
    updateGitCard();

    layout->addWidget(gitCard);
    return sidebar;
}