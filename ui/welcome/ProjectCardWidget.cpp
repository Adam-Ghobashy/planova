#include "welcome/ProjectCardWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

ProjectCardWidget::ProjectCardWidget(const ProjectDto& project, bool isCurrent, QWidget* parent)
    : QFrame(parent),
      id_(QString::fromStdString(project.id)),
      name_(QString::fromStdString(project.name)),
      team_(QString::fromStdString(project.team))
{
    setObjectName("ProjectCard");
    setProperty("current", isCurrent);
    setCursor(Qt::PointingHandCursor);

    const QString model = QString::fromStdString(project.seModel);
    QStringList spoken{name_};
    if (!team_.isEmpty())
        spoken << team_;
    if (!model.isEmpty())
        spoken << model;
    setAccessibleName(spoken.join(", "));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(6);

    auto* top = new QHBoxLayout;
    top->setSpacing(8);
    auto* name = new QLabel(name_, this);
    name->setObjectName("projectName");
    name->setWordWrap(true);
    top->addWidget(name, 1, Qt::AlignTop);
    if (isCurrent) {
        auto* chip = new QLabel(tr("Current"), this);
        chip->setObjectName("projectCurrentChip");
        top->addWidget(chip, 0, Qt::AlignTop);
    }
    root->addLayout(top);

    if (!team_.isEmpty()) {
        auto* team = new QLabel(team_, this);
        team->setObjectName("projectTeam");
        team->setWordWrap(true);
        root->addWidget(team);
    }
    root->addStretch(1);

    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(8);
    if (!model.isEmpty()) {
        auto* modelChip = new QLabel(model, this);
        modelChip->setObjectName("projectModel");
        bottom->addWidget(modelChip, 0, Qt::AlignVCenter);
    }
    bottom->addStretch(1);
    auto* open = new QPushButton(tr("Open"), this);
    open->setObjectName("openProject");
    open->setAccessibleName(tr("Open %1").arg(name_));
    bottom->addWidget(open);
    root->addLayout(bottom);

    connect(open, &QPushButton::clicked, this, [this] { emit openRequested(id_); });
}

bool ProjectCardWidget::matches(const QString& text) const
{
    return name_.contains(text, Qt::CaseInsensitive) || team_.contains(text, Qt::CaseInsensitive);
}

void ProjectCardWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit openRequested(id_);
    QFrame::mouseReleaseEvent(event);
}
