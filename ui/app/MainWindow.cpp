#include "MainWindow.h"
#include "git/GitHubIntegrationPage.h"
#include "team/TeamSkillsPage.h"
#include "welcome/ProjectsHubPage.h"
#include "welcome/SetupWizard.h"
#include "welcome/WelcomePage.h"
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
    pages_->addWidget(welcome);
    pages_->addWidget(hub);

    connect(welcome, &WelcomePage::continueProjects, this, [this] {
        pages_->setCurrentIndex(1);
    });

    connect(welcome, &WelcomePage::startNewProject, this, [this] {
        SetupWizard wizard(core_, this);
        wizard.exec();
    });

    for (int i = 2; i < kScreens.size(); ++i) {
        if (i == 4) {
            auto* teamPage = new planova::team::TeamSkillsPage(
                core_.members(), const_cast<MemberProjection&>(core_.memberProjection()), bridge_, pages_);
            pages_->addWidget(teamPage);
        } else if (i == 7) {
            auto* gitPage = new planova::git::GitHubIntegrationPage(
                core_.git(), const_cast<GitProjection&>(core_.gitProjection()), const_cast<MemberProjection&>(core_.memberProjection()), bridge_, pages_);
            pages_->addWidget(gitPage);
        } else {
            auto* page = new QLabel(kScreens[i] + " - Coming soon", pages_);
            page->setAlignment(Qt::AlignCenter);
            pages_->addWidget(page);
        }
    }
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
            pages_->setCurrentIndex(i);
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
        if (!core_.gitProjection().isConnected()) {
            core_.git().connect("https://github.com/Zeyad-101/planova.git", "main");
        }
        core_.git().simulateSync(core_.memberProjection().all());
        pages_->setCurrentIndex(7);
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