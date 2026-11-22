#include "welcome/WelcomePage.h"
#include "theme/ThemeManager.h"
#include "welcome/ProjectCardWidget.h"
#include "widgets/ModalDialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QDebug>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace {
constexpr int kColumns = 3;
constexpr int kProjectsMaxWidth = 860;

QPushButton* makeButton(const QString& text, const char* objectName, const char* kind, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    button->setProperty("kind", kind);   // "primary", "secondary" or "ghost", styled in welcome.qss
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

// Puts a card in the middle of the row, at most maxWidth wide.
QWidget* centered(QFrame* card, int maxWidth, QWidget* parent)
{
    card->setMaximumWidth(maxWidth);
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch(1);
    layout->addWidget(card, 100);
    layout->addStretch(1);
    return row;
}
}

WelcomePage::WelcomePage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setObjectName("WelcomePage");
    setAttribute(Qt::WA_StyledBackground, true);

    projectsCard_ = buildProjects();

    // The projects list can be longer than the window, so the content scrolls.
    auto* content = new QWidget;
    content->setObjectName("welcomeContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->addStretch(1);
    contentLayout->addWidget(projectsCard_);
    contentLayout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("welcomeScroll");
    scroll->viewport()->setObjectName("welcomeViewport");
    scroll->setWidgetResizable(true);
    scroll->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(&bridge, &CoreBridge::changed, this, &WelcomePage::refresh);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &WelcomePage::updateGitIcon);
    refresh();
}

QWidget* WelcomePage::buildProjects()
{
    auto* card = new QFrame;
    card->setObjectName("welcomeCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(16);

    // header: title on the left, Start New Project on the right
    auto* title = new QLabel(tr("Pick up where you left off"), card);
    title->setObjectName("welcomeTitle");
    title->setWordWrap(true);
    auto* create = makeButton(tr("Start New Project"), "createProject", "primary", card);
    connect(create, &QPushButton::clicked, this, &WelcomePage::startNewProject);
    auto* header = new QHBoxLayout;
    header->setSpacing(16);
    header->addWidget(title, 1);
    header->addWidget(create, 0, Qt::AlignTop);
    layout->addLayout(header);

    auto* divider = new QFrame(card);
    divider->setObjectName("welcomeDivider");
    divider->setFixedHeight(1);
    layout->addWidget(divider);

    search_ = new QLineEdit(card);
    search_->setObjectName("projectSearch");
    search_->setPlaceholderText(tr("Search projects by name or team…"));
    search_->setAccessibleName(tr("Search projects"));
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this, &WelcomePage::applyFilter);
    search_->installEventFilter(this);   // Esc empties the box (a line edit does nothing with it by default)
    layout->addWidget(search_);

    layout->addWidget(buildGitBanner());

    auto* workspacesTitle = new QLabel(tr("Your Workspaces"), card);
    workspacesTitle->setObjectName("workspacesTitle");
    count_ = new QLabel(card);
    count_->setObjectName("workspacesCount");
    auto* workspacesHeader = new QHBoxLayout;
    workspacesHeader->addWidget(workspacesTitle);
    workspacesHeader->addStretch(1);
    workspacesHeader->addWidget(count_);
    layout->addLayout(workspacesHeader);

    gridHost_ = new QWidget(card);
    gridHost_->setObjectName("welcomeGrid");
    grid_ = new QGridLayout(gridHost_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(14);
    for (int column = 0; column < kColumns; ++column)
        grid_->setColumnStretch(column, 1);
    layout->addWidget(gridHost_);
    layout->addWidget(buildWorkspaceState());
    layout->addStretch(1);

    // Tab order: search, Start New Project, Manage Git, then the cards (created later, so they follow).
    setTabOrder(search_, create);
    setTabOrder(create, card->findChild<QPushButton*>("manageGit"));

    return centered(card, kProjectsMaxWidth, this);
}

QWidget* WelcomePage::buildGitBanner()
{
    auto* banner = new QFrame;
    banner->setObjectName("gitBanner");
    auto* layout = new QHBoxLayout(banner);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    gitIcon_ = new QLabel(banner);
    gitIcon_->setObjectName("gitBannerIcon");
    gitIcon_->setFixedSize(20, 20);
    layout->addWidget(gitIcon_);

    auto* caption = new QLabel(tr("GitHub repository"), banner);
    caption->setObjectName("gitBannerCaption");
    gitRepo_ = new QLabel(banner);
    gitRepo_->setObjectName("gitBannerRepo");
    gitRepo_->setTextFormat(Qt::PlainText);
    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(caption);
    text->addWidget(gitRepo_);
    layout->addLayout(text, 1);

    manageGit_ = makeButton(tr("Manage Git →"), "manageGit", "ghost", banner);
    connect(manageGit_, &QPushButton::clicked, this, &WelcomePage::manageGitRequested);
    layout->addWidget(manageGit_);

    updateGitIcon();
    return banner;
}

QWidget* WelcomePage::buildWorkspaceState()
{
    statePanel_ = new QWidget;
    statePanel_->setObjectName("workspaceStatePanel");
    auto* layout = new QVBoxLayout(statePanel_);
    layout->setContentsMargins(0, 24, 0, 24);
    layout->setSpacing(12);

    stateMessage_ = new QLabel(statePanel_);
    stateMessage_->setObjectName("workspaceState");
    stateMessage_->setAlignment(Qt::AlignCenter);
    stateMessage_->setWordWrap(true);
    stateMessage_->setTextFormat(Qt::PlainText);   // the text can contain what the user typed
    layout->addWidget(stateMessage_);

    stateStart_ = makeButton(tr("Start New Project"), "emptyStartNew", "primary", statePanel_);
    connect(stateStart_, &QPushButton::clicked, this, &WelcomePage::startNewProject);
    layout->addWidget(stateStart_, 0, Qt::AlignHCenter);

    stateClear_ = makeButton(tr("Clear search"), "clearSearch", "secondary", statePanel_);
    connect(stateClear_, &QPushButton::clicked, search_, &QLineEdit::clear);
    layout->addWidget(stateClear_, 0, Qt::AlignHCenter);
    return statePanel_;
}

void WelcomePage::refresh()
{
    try {
        // Cards are replaced, not edited. The old ones are deleted later because this can run inside
        // the click of one of their own buttons.
        for (ProjectCardWidget* card : cards_) {
            card->hide();
            card->setParent(nullptr);
            card->deleteLater();
        }
        cards_.clear();

        const auto current = core_.currentProject();
        for (const ProjectDto& project : core_.projects().list()) {
            auto* card = new ProjectCardWidget(project, current && *current == project.id, gridHost_);
            connect(card, &ProjectCardWidget::openRequested, this, &WelcomePage::openProject);
            cards_.append(card);
        }

        const bool connected = core_.gitProjection().isConnected();
        const QString repo = QString::fromStdString(core_.gitProjection().repoUrl());
        gitRepo_->setText(connected && !repo.isEmpty() ? repo : tr("No repository connected"));
        gitRepo_->setProperty("connected", connected && !repo.isEmpty());
        gitRepo_->style()->unpolish(gitRepo_);
        gitRepo_->style()->polish(gitRepo_);

        // The GitHub page shows the open project's repository, so there is nothing to manage without one.
        manageGit_->setEnabled(current.has_value());
        manageGit_->setToolTip(current ? tr("Open the GitHub page of the current project") : tr("Open a project first"));

        applyFilter();
    } catch (const std::exception& error) {
        // refresh() runs as a slot, so no exception may leave it. The list shows again after the next change.
        qWarning() << "WelcomePage::refresh failed:" << error.what();
    }
}

void WelcomePage::applyFilter()
{
    const QString text = search_->text().trimmed();

    // Take every card out of the grid, then put the matching ones back in rows of three.
    while (QLayoutItem* item = grid_->takeAt(0))
        delete item;   // deletes the layout item only, not the card
    int shown = 0;
    for (ProjectCardWidget* card : cards_) {
        const bool match = card->matches(text);
        card->setVisible(match);
        if (match) {
            grid_->addWidget(card, shown / kColumns, shown % kColumns);
            ++shown;
        }
    }
    updateState(static_cast<int>(cards_.size()), shown);
}

void WelcomePage::updateState(int total, int shown)
{
    count_->setText(total == 1 ? tr("1 project") : tr("%1 projects").arg(total));
    gridHost_->setVisible(shown > 0);

    const QString text = search_->text().trimmed();
    const bool empty = total == 0;
    const bool noMatch = !empty && shown == 0;
    statePanel_->setVisible(empty || noMatch);
    stateStart_->setVisible(empty);
    stateClear_->setVisible(noMatch);
    if (empty)
        stateMessage_->setText(tr("No projects yet. Start a new project to create your first workspace."));
    else if (noMatch)
        stateMessage_->setText(tr("No projects match '%1'").arg(text));
}

void WelcomePage::updateGitIcon()
{
    gitIcon_->setPixmap(ThemeManager::instance().icon("git", Token::TextSecondary).pixmap(20, 20));
}

void WelcomePage::openProject(const QString& projectId)
{
    try {
        core_.openProject(projectId.toStdString());
    } catch (const std::exception& error) {
        ModalDialog::info(this, tr("Could not open project"), QString::fromUtf8(error.what()));
        return;
    }
    emit projectOpened();
}

bool WelcomePage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == search_ && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        search_->clear();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
