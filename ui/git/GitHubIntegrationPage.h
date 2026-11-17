#pragma once

#include "app/CoreBridge.h"
#include "planova/common/Queries.h"
#include "planova/git/GitProjection.h"
#include "planova/git/GitService.h"
#include <QFrame>
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
                          GitProjection& gitProjection,
                          IMemberQuery& members,
                          CoreBridge& bridge,
                          QWidget* parent = nullptr);

    void refresh();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSimulateSyncClicked();

private:
    void showStatus(const QString& text);

    GitService& gitService_;
    GitProjection& gitProjection_;
    IMemberQuery& members_;
    CoreBridge& bridge_;

    QLabel* statusLabel_ = nullptr;
    QTimer* statusTimer_ = nullptr;

    QLineEdit* repoInput_ = nullptr;
    QLineEdit* branchInput_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QPushButton* disconnectBtn_ = nullptr;
    QPushButton* syncBtn_ = nullptr;

    QLabel* connectionBadge_ = nullptr;
    QLabel* branchValue_ = nullptr;
    QLabel* commitsValue_ = nullptr;
    QLabel* openIssuesValue_ = nullptr;
    QLabel* conflictsValue_ = nullptr;

    QTableWidget* commitsTable_ = nullptr;
    QTableWidget* issuesTable_ = nullptr;

    bool updating_ = false;
};

} // namespace planova::git