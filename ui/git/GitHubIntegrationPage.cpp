#include "git/GitHubIntegrationPage.h"
#include "widgets/EmptyHint.h"
#include "theme/ThemeManager.h"
#include "theme/Tokens.h"
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <exception>

namespace planova::git {

namespace {
// The two lists go side by side from this page width on (about 1100 px of content plus the 24 px margins). Below it they
// stack, so neither table is squeezed. The page width is used, not the content width, so a scroll bar cannot flip the layout.
constexpr int kSplitMinPageWidth = 1160;
constexpr int kListMinHeight = 180;
constexpr int kListMaxVisibleRows = 8;   // longer lists scroll inside the table

// Lets a table show its rows without an inner scroll bar, up to kListMaxVisibleRows.
void fitTableHeight(QTableWidget* table) {
    const int rows = qMin(table->rowCount(), kListMaxVisibleRows);
    const int wanted = table->horizontalHeader()->sizeHint().height()
                       + rows * table->verticalHeader()->defaultSectionSize() + 2 * table->frameWidth();
    table->setMinimumHeight(qMax(kListMinHeight, wanted));
}
}

GitHubIntegrationPage::GitHubIntegrationPage(GitService& gitService,
                                             const GitProjection& gitProjection,
                                             IMemberQuery& members,
                                             CoreBridge& bridge,
                                             QWidget* parent)
    : QWidget(parent),
    gitService_(gitService),
    gitProjection_(gitProjection),
    members_(members),
    bridge_(bridge) {
    setObjectName("GitHubIntegrationPage");

    // Styles come from github.qss, loaded once by ThemeManager.

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("slicerScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* contentWidget = new QWidget(scrollArea);
    contentWidget->setObjectName("slicerScrollContent");
    auto* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(24, 20, 24, 24);
    mainLayout->setSpacing(16);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 4);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);
    auto* pageTitle = new QLabel("GitHub Integration & Peer Event Sync", contentWidget);
    pageTitle->setObjectName("pageTitle");
    QFont titleFont = pageTitle->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    pageTitle->setFont(titleFont);

    auto* pageSubtitle = new QLabel(
        "Connect your repository, track team commits and issues, and detect merge conflicts across active members.",
        contentWidget);
    pageSubtitle->setObjectName("cardSubtitle");
    titleLayout->addWidget(pageTitle);
    titleLayout->addWidget(pageSubtitle);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    statusLabel_ = new QLabel(contentWidget);
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->hide();
    headerLayout->addWidget(statusLabel_);

    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    statusTimer_->setInterval(4000);
    connect(statusTimer_, &QTimer::timeout, statusLabel_, &QLabel::hide);

    syncBtn_ = new QPushButton("Simulate Git Sync", contentWidget);
    syncBtn_->setObjectName("primaryButton");
    connect(syncBtn_, &QPushButton::clicked, this, &GitHubIntegrationPage::onSimulateSyncClicked);
    headerLayout->addWidget(syncBtn_);

    mainLayout->addLayout(headerLayout);

    // No real GitHub provider exists yet; say so where the user starts, not only in a footer.
    auto* notice = new QLabel("Simulated data: commits and issues are generated locally until a real GitHub provider exists.",
                              contentWidget);
    notice->setObjectName("simulatedNotice");
    notice->setWordWrap(true);
    mainLayout->addWidget(notice);

    auto* connCard = new QFrame(contentWidget);
    connCard->setObjectName("card");
    auto* connLayout = new QVBoxLayout(connCard);
    connLayout->setContentsMargins(20, 16, 20, 16);
    connLayout->setSpacing(12);

    auto* connHeader = new QHBoxLayout();
    auto* connTitleCol = new QVBoxLayout();
    connTitleCol->setSpacing(2);
    auto* connTitle = new QLabel("Repository Connection", connCard);
    connTitle->setObjectName("cardTitle");
    auto* connSub = new QLabel("Link a GitHub repository URL and target branch for event-log synchronization.", connCard);
    connSub->setObjectName("cardSubtitle");
    connTitleCol->addWidget(connTitle);
    connTitleCol->addWidget(connSub);
    connHeader->addLayout(connTitleCol);
    connHeader->addStretch();

    connectionBadge_ = new QLabel("Disconnected", connCard);
    connectionBadge_->setObjectName("connectionBadge");
    connHeader->addWidget(connectionBadge_);
    connLayout->addLayout(connHeader);

    auto* formRow = new QHBoxLayout();
    formRow->setSpacing(12);

    repoInput_ = new QLineEdit(connCard);
    repoInput_->setObjectName("repoInput");
    repoInput_->setAccessibleName("Repository URL");
    repoInput_->setPlaceholderText("https://github.com/<owner>/<repo>.git");   // empty until the user types or one is saved

    branchInput_ = new QLineEdit(connCard);
    branchInput_->setObjectName("branchInput");
    branchInput_->setAccessibleName("Branch");
    branchInput_->setPlaceholderText("Branch (e.g., main)");
    branchInput_->setText("main");
    branchInput_->setFixedWidth(180);

    connectBtn_ = new QPushButton("Connect Repo", connCard);
    connectBtn_->setObjectName("connectButton");
    connect(connectBtn_, &QPushButton::clicked, this, &GitHubIntegrationPage::onConnectClicked);

    disconnectBtn_ = new QPushButton("Disconnect", connCard);
    disconnectBtn_->setObjectName("secondaryButton");
    connect(disconnectBtn_, &QPushButton::clicked, this, &GitHubIntegrationPage::onDisconnectClicked);

    formRow->addWidget(repoInput_, 1);
    formRow->addWidget(branchInput_);
    formRow->addWidget(connectBtn_);
    formRow->addWidget(disconnectBtn_);
    connLayout->addLayout(formRow);

    // Says why Connect is off; hidden as soon as a URL is typed.
    connectHint_ = new QLabel("Enter a repository URL to enable Connect and Simulate Git Sync.", connCard);
    connectHint_->setObjectName("connectHint");
    connLayout->addWidget(connectHint_);
    connect(repoInput_, &QLineEdit::textChanged, this, &GitHubIntegrationPage::updateButtons);

    mainLayout->addWidget(connCard);

    auto* metricsCard = new QFrame(contentWidget);
    metricsCard->setObjectName("card");
    auto* metricsLayout = new QHBoxLayout(metricsCard);
    metricsLayout->setContentsMargins(20, 14, 20, 14);
    metricsLayout->setSpacing(24);

    auto makeMetric = [&](const QString& label, QLabel*& valueOut) {
        auto* col = new QVBoxLayout();
        col->setSpacing(2);
        auto* lbl = new QLabel(label, metricsCard);
        lbl->setObjectName("cardSubtitle");
        valueOut = new QLabel("—", metricsCard);
        valueOut->setObjectName("cardTitle");
        QFont vf = valueOut->font();
        vf.setPointSize(15);
        vf.setBold(true);
        valueOut->setFont(vf);
        col->addWidget(lbl);
        col->addWidget(valueOut);
        metricsLayout->addLayout(col, 1);
    };

    makeMetric("Active Branch", branchValue_);
    makeMetric("Synced Commits", commitsValue_);
    makeMetric("Open Issues", openIssuesValue_);
    makeMetric("Conflict Alerts", conflictsValue_);

    mainLayout->addWidget(metricsCard);

    // Commits and issues share one grid: two equal columns on a wide page, one column on a narrow one (arrangeLists).
    auto* listsHost = new QWidget(contentWidget);
    listsLayout_ = new QGridLayout(listsHost);
    listsLayout_->setContentsMargins(0, 0, 0, 0);
    listsLayout_->setSpacing(16);

    commitsCard_ = new QFrame(listsHost);
    commitsCard_->setObjectName("card");
    auto* commitsLayout = new QVBoxLayout(commitsCard_);
    commitsLayout->setContentsMargins(20, 16, 20, 16);
    commitsLayout->setSpacing(10);

    auto* commitsTitle = new QLabel("Recent commits", commitsCard_);
    commitsTitle->setObjectName("cardTitle");
    auto* commitsSub = new QLabel("Latest commits synced from the repository.", commitsCard_);
    commitsSub->setObjectName("cardSubtitle");
    commitsLayout->addWidget(commitsTitle);
    commitsLayout->addWidget(commitsSub);

    commitsTable_ = new QTableWidget(0, 4, commitsCard_);
    commitsTable_->setObjectName("assignmentsTable");
    commitsTable_->setAccessibleName("Recent commits");
    commitsTable_->setHorizontalHeaderLabels({"Hash", "Commit Message", "Author", "Timestamp"});
    commitsTable_->verticalHeader()->setVisible(false);
    commitsTable_->setShowGrid(false);
    commitsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    commitsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    commitsTable_->setTextElideMode(Qt::ElideRight);   // long text is cut with "...", the full text is in the tooltip
    commitsTable_->setWordWrap(false);
    commitsTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    commitsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    commitsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    commitsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    commitsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    commitsTable_->setMinimumHeight(kListMinHeight);
    commitsLayout->addWidget(commitsTable_, 1);
    emptyCommits_ = makeEmptyHint(QString(), commitsCard_);   // the text depends on the connection, see refresh()
    commitsLayout->addWidget(emptyCommits_, 1);

    issuesCard_ = new QFrame(listsHost);
    issuesCard_->setObjectName("card");
    auto* issuesLayout = new QVBoxLayout(issuesCard_);
    issuesLayout->setContentsMargins(20, 16, 20, 16);
    issuesLayout->setSpacing(10);

    auto* issuesTitle = new QLabel("Issues", issuesCard_);
    issuesTitle->setObjectName("cardTitle");
    auto* issuesSub = new QLabel("Tracked repository issues and open/closed status.", issuesCard_);
    issuesSub->setObjectName("cardSubtitle");
    issuesLayout->addWidget(issuesTitle);
    issuesLayout->addWidget(issuesSub);

    issuesTable_ = new QTableWidget(0, 3, issuesCard_);
    issuesTable_->setObjectName("assignmentsTable");
    issuesTable_->setAccessibleName("Issues");
    issuesTable_->setHorizontalHeaderLabels({"Issue #", "Title", "State"});
    issuesTable_->verticalHeader()->setVisible(false);
    issuesTable_->setShowGrid(false);
    issuesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    issuesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    issuesTable_->setTextElideMode(Qt::ElideRight);
    issuesTable_->setWordWrap(false);
    issuesTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    issuesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    issuesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    issuesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    issuesTable_->setMinimumHeight(kListMinHeight);
    issuesLayout->addWidget(issuesTable_, 1);
    emptyIssues_ = makeEmptyHint(QString(), issuesCard_);
    issuesLayout->addWidget(emptyIssues_, 1);

    arrangeLists();
    mainLayout->addWidget(listsHost);

    mainLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);

    connect(&bridge_, &CoreBridge::changed, this, [this](const QString&) {
        if (!updating_) refresh();
    });
    // Issue state colours are set on the items. Only they are recoloured: a full refresh() would overwrite
    // repository and branch text the user typed but has not connected yet.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &GitHubIntegrationPage::recolorIssueStates);

    refresh();
}

void GitHubIntegrationPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    arrangeLists();
}

void GitHubIntegrationPage::arrangeLists() {
    const bool sideBySide = width() >= kSplitMinPageWidth;
    // The first call finds the grid empty and places the cards whatever the mode.
    if (sideBySide == listsSideBySide_ && listsLayout_->count() > 0) return;
    listsSideBySide_ = sideBySide;

    listsLayout_->removeWidget(commitsCard_);
    listsLayout_->removeWidget(issuesCard_);
    if (sideBySide) {
        listsLayout_->addWidget(commitsCard_, 0, 0);
        listsLayout_->addWidget(issuesCard_, 0, 1);
    } else {
        listsLayout_->addWidget(commitsCard_, 0, 0);
        listsLayout_->addWidget(issuesCard_, 1, 0);
    }
    // Two equal columns when side by side; the second column and row only exist for the layout that uses them.
    listsLayout_->setColumnStretch(0, 1);
    listsLayout_->setColumnStretch(1, sideBySide ? 1 : 0);
}

void GitHubIntegrationPage::updateButtons() {
    const bool hasUrl = !repoInput_->text().trimmed().isEmpty();
    const bool connected = gitProjection_.isConnected();
    connectBtn_->setEnabled(hasUrl);
    connectHint_->setVisible(!hasUrl);
    disconnectBtn_->setEnabled(connected);
    // Sync needs a saved repository, or a typed one that it connects first.
    const bool canSync = connected || hasUrl;
    syncBtn_->setEnabled(canSync);
    // A disabled button says why, to the pointer (tooltip) and to a screen reader (description).
    const QString syncHelp = canSync ? "Generate sample commits and issues for the repository"
                                     : "Connect a repository first: enter its URL under Repository Connection.";
    syncBtn_->setToolTip(syncHelp);
    syncBtn_->setAccessibleDescription(syncHelp);
}

void GitHubIntegrationPage::recolorIssueStates() {
    for (int row = 0; row < issuesTable_->rowCount(); ++row) {
        QTableWidgetItem* item = issuesTable_->item(row, 2);
        if (!item) continue;
        const bool open = item->text() == "OPEN";
        item->setForeground(ThemeManager::instance().color(open ? Token::Accent : Token::TextMuted));
    }
}

void GitHubIntegrationPage::onConnectClicked() {
    const std::string url = repoInput_->text().trimmed().toStdString();
    std::string branch = branchInput_->text().trimmed().toStdString();
    if (url.empty()) {
        showStatus("Please enter a valid GitHub repository URL");
        return;
    }
    if (branch.empty()) branch = "main";

    updating_ = true;
    try {
        gitService_.connect(url, branch);
    } catch (const std::exception& e) {
        updating_ = false;
        showStatus(QString("Connect failed: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    updating_ = false;
    refresh();
    showStatus("Repository connected");
}

void GitHubIntegrationPage::onDisconnectClicked() {
    if (!gitProjection_.isConnected()) {
        showStatus("Repository is already disconnected");
        return;
    }
    updating_ = true;
    try {
        gitService_.disconnect();
    } catch (const std::exception& e) {
        updating_ = false;
        showStatus(QString("Disconnect failed: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    updating_ = false;
    refresh();
    showStatus("Repository disconnected");
}

void GitHubIntegrationPage::onSimulateSyncClicked() {
    updating_ = true;
    try {
        if (!gitProjection_.isConnected()) {
            const std::string url = repoInput_->text().trimmed().toStdString();
            if (url.empty()) {
                updating_ = false;
                showStatus("Enter a repository URL first");
                return;
            }
            std::string branch = branchInput_->text().trimmed().toStdString();
            if (branch.empty()) branch = "main";
            gitService_.connect(url, branch);
        }
        gitService_.simulateSync(members_.all());
    } catch (const std::exception& e) {
        updating_ = false;
        showStatus(QString("Sync failed: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    updating_ = false;
    refresh();
    showStatus("Synced commits and issues from Git");
}

void GitHubIntegrationPage::refresh() {
    const bool connected = gitProjection_.isConnected();

    if (connected) {
        connectionBadge_->setText("● Connected (simulated)");
        connectionBadge_->setProperty("state", "connected");
        // The form follows the saved repository, but text the user typed and has not connected stays. It counts as
        // typed when it differs from what was loaded last time and the saved repository did not change since.
        const QString savedRepo = QString::fromStdString(gitProjection_.repoUrl());
        const QString savedBranch = QString::fromStdString(gitProjection_.branch());
        if (repoInput_->text() == loadedRepo_ || savedRepo != loadedRepo_) repoInput_->setText(savedRepo);
        if (branchInput_->text() == loadedBranch_ || savedBranch != loadedBranch_) branchInput_->setText(savedBranch);
        loadedRepo_ = savedRepo;
        loadedBranch_ = savedBranch;
        branchValue_->setText(savedBranch);
    } else {
        connectionBadge_->setText("○ Disconnected");
        connectionBadge_->setProperty("state", "disconnected");
        branchValue_->setText("—");
    }
    // The colours come from github.qss; re-polish so the new "state" value takes effect.
    connectionBadge_->style()->unpolish(connectionBadge_);
    connectionBadge_->style()->polish(connectionBadge_);

    const auto commits = gitProjection_.commits();
    const auto issues = gitProjection_.issues();
    const auto team = members_.all();

    commitsValue_->setText(QString::number(commits.size()));
    openIssuesValue_->setText(QString::number(gitProjection_.openIssueCount()));

    int conflictCount = 0;
    for (const auto& m : team) {
        if (gitProjection_.hasOpenConflict(m.id)) {
            ++conflictCount;
        }
    }
    conflictsValue_->setText(conflictCount == 0
                                 ? QString("None")
                                 : QString("%1 Member(s)").arg(conflictCount));

    commitsTable_->setVisible(!commits.empty());
    emptyCommits_->setVisible(commits.empty());
    issuesTable_->setVisible(!issues.empty());
    emptyIssues_->setVisible(issues.empty());
    // Each empty list names the next action; it differs once a repository is connected.
    const QString nextStep = connected ? QString("Use Simulate Git Sync to load them.")
                                       : QString("Connect a repository, then use Simulate Git Sync.");
    emptyCommits_->setText("No commits yet. " + nextStep);
    emptyIssues_->setText("No issues yet. " + nextStep);

    // Long text is elided by the table; the tooltip keeps the full text readable.
    auto makeItem = [](const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setToolTip(text);
        return item;
    };

    // Populate Commits Table
    commitsTable_->setRowCount(static_cast<int>(commits.size()));
    for (int i = 0; i < static_cast<int>(commits.size()); ++i) {
        const auto& c = commits[i];
        commitsTable_->setItem(i, 0, makeItem(QString::fromStdString(c.hash)));
        commitsTable_->setItem(i, 1, makeItem(QString::fromStdString(c.message)));
        commitsTable_->setItem(i, 2, makeItem(QString::fromStdString(c.author)));
        const QString timeStr = c.ts > 0
                                    ? QDateTime::fromMSecsSinceEpoch(c.ts).toString("yyyy-MM-dd HH:mm")   // ts is in milliseconds
                                    : QString("Just now");
        commitsTable_->setItem(i, 3, makeItem(timeStr));
    }

    // Populate Issues Table
    issuesTable_->setRowCount(static_cast<int>(issues.size()));
    for (int i = 0; i < static_cast<int>(issues.size()); ++i) {
        const auto& iss = issues[i];
        issuesTable_->setItem(i, 0, makeItem(QString("#%1").arg(iss.number)));
        issuesTable_->setItem(i, 1, makeItem(QString::fromStdString(iss.title)));
        auto* stateItem = makeItem(iss.open ? "OPEN" : "CLOSED");
        stateItem->setForeground(ThemeManager::instance().color(iss.open ? Token::Accent : Token::TextMuted));
        issuesTable_->setItem(i, 2, stateItem);
    }
    fitTableHeight(commitsTable_);
    fitTableHeight(issuesTable_);
    updateButtons();
}

void GitHubIntegrationPage::showStatus(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setVisible(!text.isEmpty());
    statusTimer_->start();
}

} // namespace planova::git