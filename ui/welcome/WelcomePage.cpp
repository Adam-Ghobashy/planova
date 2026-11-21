#include "welcome/WelcomePage.h"
#include "app/SplashTimeline.h"
#include "theme/ThemeManager.h"
#include "welcome/HandwritingLabel.h"
#include "welcome/ProjectCardWidget.h"
#include "widgets/ModalDialog.h"

#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace {
constexpr int kColumns = 3;
constexpr int kIntroMaxWidth = 640;
constexpr int kProjectsMaxWidth = 860;
constexpr int kButtonsDelayMs = 100;   // the buttons fade in just after the subtitle, as in the prototype

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

void setOpacity(QGraphicsOpacityEffect* effect, double opacity)
{
    effect->setOpacity(opacity);
    effect->setEnabled(opacity < 1.0);   // a finished fade stops rendering through the effect
}
}

WelcomePage::WelcomePage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setObjectName("WelcomePage");
    setAttribute(Qt::WA_StyledBackground, true);

    introCard_ = buildIntro();
    projectsCard_ = buildProjects();

    // The projects list can be longer than the window, so the content scrolls.
    auto* content = new QWidget;
    content->setObjectName("welcomeContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->addStretch(1);
    contentLayout->addWidget(introCard_);
    contentLayout->addWidget(projectsCard_);
    contentLayout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("welcomeScroll");
    scroll->viewport()->setObjectName("welcomeViewport");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(&bridge, &CoreBridge::changed, this, &WelcomePage::refresh);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &WelcomePage::updateGitIcon);
    refresh();
    setMode(Mode::Intro);
}

void WelcomePage::setMode(Mode mode)
{
    const bool changed = mode != mode_;
    mode_ = mode;
    // Both cards sit in the same column and only one is shown. (A QStackedWidget would be as tall as the
    // taller card, which would stretch the short intro card.)
    introCard_->setVisible(mode == Mode::Intro);
    projectsCard_->setVisible(mode == Mode::Projects);
    if (changed)
        emit modeChanged(mode);
}

QWidget* WelcomePage::buildIntro()
{
    auto* card = new QFrame;
    card->setObjectName("welcomeCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(44, 48, 44, 44);
    layout->setSpacing(0);

    auto* hero = new HandwritingLabel(card);
    layout->addWidget(hero);
    layout->addSpacing(14);

    subtitle_ = new QLabel(tr("Plan the work · Match the team · Measure actual progress"), card);
    subtitle_->setObjectName("welcomeSubtitle");
    subtitle_->setAlignment(Qt::AlignCenter);
    subtitle_->setWordWrap(true);
    layout->addWidget(subtitle_);
    layout->addSpacing(36);

    auto* start = makeButton(tr("Start New Project"), "startNewProject", "primary", card);
    auto* cont = makeButton(tr("Continue Your Projects →"), "continueProjects", "secondary", card);
    start->setProperty("size", "large");
    cont->setProperty("size", "large");
    connect(start, &QPushButton::clicked, this, &WelcomePage::startNewProject);
    connect(cont, &QPushButton::clicked, this, [this] { setMode(Mode::Projects); });

    introButtons_ = new QWidget(card);
    introButtons_->setObjectName("welcomeButtons");
    auto* buttons = new QHBoxLayout(introButtons_);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(16);
    buttons->addStretch(1);
    buttons->addWidget(start);
    buttons->addWidget(cont);
    buttons->addStretch(1);
    layout->addWidget(introButtons_);

    // The text and buttons fade in after the word is written. The buttons work from the first moment.
    subtitleFade_ = new QGraphicsOpacityEffect(subtitle_);
    subtitle_->setGraphicsEffect(subtitleFade_);
    buttonsFade_ = new QGraphicsOpacityEffect(introButtons_);
    introButtons_->setGraphicsEffect(buttonsFade_);
    setOpacity(subtitleFade_, 0.0);
    setOpacity(buttonsFade_, 0.0);
    connect(hero, &HandwritingLabel::frameChanged, this, &WelcomePage::fadeIn);

    return centered(card, kIntroMaxWidth, this);
}

void WelcomePage::fadeIn(int elapsedMs)
{
    setOpacity(subtitleFade_, SplashTimeline::at(elapsedMs).subtitleOpacity);
    setOpacity(buttonsFade_, SplashTimeline::at(elapsedMs - kButtonsDelayMs).subtitleOpacity);
}

QWidget* WelcomePage::buildProjects()
{
    auto* card = new QFrame;
    card->setObjectName("welcomeCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(16);

    // header: back link and title on the left, Start New Project on the right
    auto* back = makeButton(tr("← Back to Welcome"), "backToWelcome", "ghost", card);
    connect(back, &QPushButton::clicked, this, [this] { setMode(Mode::Intro); });
    auto* title = new QLabel(tr("Welcome back · Pick up where you left off"), card);
    title->setObjectName("welcomeTitle");
    title->setWordWrap(true);
    auto* titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(6);
    titleColumn->addWidget(back, 0, Qt::AlignLeft);
    titleColumn->addWidget(title);

    auto* create = makeButton(tr("Start New Project"), "createProject", "primary", card);
    connect(create, &QPushButton::clicked, this, &WelcomePage::startNewProject);
    auto* header = new QHBoxLayout;
    header->setSpacing(16);
    header->addLayout(titleColumn, 1);
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

    // Tab order: back, search, Start New Project, Manage Git, then the cards (created later, so they follow).
    setTabOrder(back, search_);
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

    auto* manage = makeButton(tr("Manage Git →"), "manageGit", "ghost", banner);
    connect(manage, &QPushButton::clicked, this, &WelcomePage::manageGitRequested);
    layout->addWidget(manage);

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

    applyFilter();
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
