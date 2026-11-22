#include "MainWindow.h"
#include "EventText.h"
#include "board/PhaseBoardPage.h"
#include "git/GitHubIntegrationPage.h"
#include "Sidebar.h"
#include "home/HomePage.h"
#include "pins/PinsPage.h"
#include "slicer/ChaosSlicerPage.h"
#include "team/TeamSkillsPage.h"
#include "tracking/TrackingPage.h"
#include "welcome/SetupPage.h"
#include "welcome/StartDialog.h"
#include "welcome/WelcomePage.h"
#include "widgets/ModalDialog.h"
#include "theme/GlassBackdrop.h"
#include "theme/ThemeManager.h"
#include <QHBoxLayout>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <exception>
#include <functional>

MainWindow::MainWindow(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QMainWindow(parent), core_(core), bridge_(bridge), sidebar_(new Sidebar(core, this)), pages_(new QStackedWidget(this))
{
    setWindowTitle("Planova");
    resize(1280, 820);

    // The painted glass backdrop sits behind the sidebar and the pages.
    auto* central = new GlassBackdrop(this);
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
    addShortcuts();
    // A project that was auto-opened at start-up shows its Home; otherwise the Projects Hub is behind the start popup.
    // Read the saved page first: showing Home below would overwrite it.
    const std::optional<Page> lastPage = savedPage();
    if (core_.currentProject()) {
        showHomeForCurrentProject();
        if (lastPage)
            showPage(*lastPage);
    } else {
        showPage(Page::Hub);
    }

    connect(&bridge_, &CoreBridge::changed, sidebar_, &Sidebar::refresh);
    connect(sidebar_, &Sidebar::pageRequested, this, &MainWindow::showPage);
    connect(&bridge_, &CoreBridge::changed, this, &MainWindow::updateStatusBar);
    updateStatusBar();
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
    pages_->addWidget(welcome_);   // Page::Hub
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
            showPage(Page::Hub);
    });
    // openProject publishes ProjectOpened on the bridge, which also refreshes the sidebar badges.
    connect(welcome_, &WelcomePage::projectOpened, this, [this] { showHomeForCurrentProject(); });
    connect(welcome_, &WelcomePage::manageGitRequested, this, [this] {
        if (core_.currentProject()) showPage(Page::GitHub);   // the GitHub page belongs to the open project
    });
}

void MainWindow::addShortcuts()
{
    auto add = [this](QKeySequence keys, std::function<void()> action) {
        auto* shortcut = new QShortcut(keys, this);
        shortcut->setContext(Qt::WindowShortcut);
        connect(shortcut, &QShortcut::activated, this, std::move(action));
    };

    add(QKeySequence(Qt::CTRL | Qt::Key_0), [this] { showPage(Page::Hub); });

    // Ctrl+1 to Ctrl+7 follow the sidebar order. Project pages need an open project, like the disabled items.
    const QList<Page> order = Sidebar::navOrder();
    for (int i = 0; i < order.size(); ++i) {
        const Page page = order[i];
        const QKeyCombination keys(Qt::ControlModifier, static_cast<Qt::Key>(Qt::Key_1 + i));
        add(QKeySequence(keys), [this, page] {
            if (core_.currentProject())
                showPage(page);
        });
    }

    // A Setup page that is already open may be half filled, so Ctrl+N leaves it alone.
    add(QKeySequence(Qt::CTRL | Qt::Key_N), [this] {
        if (pages_->currentIndex() == static_cast<int>(Page::Setup))
            return;
        setupPage_->reset();
        showPage(Page::Setup);
    });

    add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), [] { ThemeManager::instance().toggle(); });
}

void MainWindow::showPage(Page page)
{
    pages_->setCurrentIndex(static_cast<int>(page));
    sidebar_->setActivePage(page);
    // Only the pages with a sidebar item are remembered; Setup and the Hub are not.
    if (Sidebar::navOrder().contains(page))
        QSettings().setValue("ui/lastPage", static_cast<int>(page));
}

std::optional<Page> MainWindow::savedPage() const
{
    bool isNumber = false;
    const int value = QSettings().value("ui/lastPage").toInt(&isNumber);
    if (!isNumber)
        return std::nullopt;
    for (Page page : Sidebar::navOrder()) {
        if (static_cast<int>(page) == value)
            return page;
    }
    return std::nullopt;
}

std::optional<ProjectDto> MainWindow::currentProjectInfo() const
{
    const auto current = core_.currentProject();
    if (!current)
        return std::nullopt;
    for (const auto& project : core_.projects().list()) {
        if (project.id == *current)
            return project;
    }
    return std::nullopt;
}

void MainWindow::updateStatusBar(const QString& lastEvent)
{
    const auto project = currentProjectInfo();
    const QString projectName = project ? QString::fromStdString(project->name) : tr("No project open");
    const QString gitState = core_.gitProjection().isConnected() ? tr("Synced") : tr("Not connected");
    QString message = tr("%1  •  Git: %2").arg(projectName, gitState);
    if (!lastEvent.isEmpty())
        message += tr("  •  Saved: %1").arg(friendlyEventText(lastEvent));
    statusBar()->showMessage(message);
}

void MainWindow::showHomeForCurrentProject()
{
    if (const auto project = currentProjectInfo()) {
        homePage_->setProjectName(QString::fromStdString(project->name));
        homePage_->setProcessModel(QString::fromStdString(project->seModel));
    }
    showPage(Page::Home);
}

void MainWindow::showStartDialog()
{
    StartDialog dialog(core_, this);
    dialog.exec();
    switch (dialog.choice()) {
    case StartDialog::Choice::NewProject:
        setupPage_->reset();   // every visit starts with an empty setup
        showPage(Page::Setup);
        break;
    case StartDialog::Choice::OpenProject:
        try {
            core_.openProject(dialog.projectId().toStdString());   // publishes ProjectOpened on the bridge
        } catch (const std::exception& error) {
            ModalDialog::info(this, tr("Could not open project"), QString::fromUtf8(error.what()));
            break;
        }
        showHomeForCurrentProject();
        break;
    case StartDialog::Choice::None:
        break;                 // dismissed: stay on the page behind the popup
    }
}
