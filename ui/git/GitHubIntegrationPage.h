#pragma once

#include "app/CoreBridge.h"
#include "planova/common/Queries.h"
#include "planova/git/GitProjection.h"
#include "planova/git/GitService.h"
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace planova::git {

class GitHubIntegrationPage : public QWidget {
    Q_OBJECT
public:
    GitHubIntegrationPage(GitService& gitService,
                          const GitProjection& gitProjection,
                          IMemberQuery& members,
                          CoreBridge& bridge,
                          QWidget* parent = nullptr);

    void refresh();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSimulateSyncClicked();

private:
    void showStatus(const QString& text);
    void updateButtons();          // Connect, Disconnect and Sync follow the URL field and the saved state
    void recolorIssueStates();     // issue state colours are set per item, so a theme change recolours them
    void arrangeLists();           // commits and issues side by side on a wide page, stacked on a narrow one

    GitService& gitService_;
    const GitProjection& gitProjection_;
    IMemberQuery& members_;
    CoreBridge& bridge_;

    QLabel* statusLabel_ = nullptr;
    QTimer* statusTimer_ = nullptr;

    QLineEdit* repoInput_ = nullptr;
    QLineEdit* branchInput_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QPushButton* disconnectBtn_ = nullptr;
    QPushButton* syncBtn_ = nullptr;

    QLabel* connectHint_ = nullptr;
    QLabel* connectionBadge_ = nullptr;
    QLabel* branchValue_ = nullptr;
    QLabel* commitsValue_ = nullptr;
    QLabel* openIssuesValue_ = nullptr;
    QLabel* conflictsValue_ = nullptr;

    QTableWidget* commitsTable_ = nullptr;
    QTableWidget* issuesTable_ = nullptr;
    QLabel* emptyCommits_ = nullptr;
    QLabel* emptyIssues_ = nullptr;
    QFrame* commitsCard_ = nullptr;
    QFrame* issuesCard_ = nullptr;
    QGridLayout* listsLayout_ = nullptr;
    bool listsSideBySide_ = false;

    // What refresh() last wrote into the form, so it can tell text the user typed from text it loaded itself.
    QString loadedRepo_;
    QString loadedBranch_;

    bool updating_ = false;
};

} // namespace planova::git