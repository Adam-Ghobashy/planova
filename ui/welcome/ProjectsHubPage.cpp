#include "ProjectsHubPage.h"

#include <QLabel>
#include <QListWidget>
#include <QTimer>
#include <QVBoxLayout>

ProjectsHubPage::ProjectsHubPage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent)
    : QWidget(parent), core_(core), list_(new QListWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Continue Your Projects", this));
    layout->addWidget(list_);

    connect(&bridge, &CoreBridge::changed, this, &ProjectsHubPage::refresh);
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const QString id = item->data(Qt::UserRole).toString();
        // Opening publishes ProjectOpened, which refreshes (and clears) this list.
        // Defer it so the clicked item is not deleted inside its own signal.
        const std::string projectId = id.toStdString();
        QTimer::singleShot(0, this, [this, projectId] {
            core_.openProject(projectId);
            emit projectOpened();
        });
    });
    refresh();
}

void ProjectsHubPage::refresh()
{
    list_->clear();
    for (const auto& project : core_.projects().list()) {
        auto* item = new QListWidgetItem(QString::fromStdString(project.name), list_);
        item->setData(Qt::UserRole, QString::fromStdString(project.id));
    }
}