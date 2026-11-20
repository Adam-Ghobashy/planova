#include "ProjectsHubPage.h"

#include <QLabel>
#include <QListWidget>
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
        core_.openProject(id.toStdString());
        emit projectOpened();
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