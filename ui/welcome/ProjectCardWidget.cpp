#include "welcome/ProjectCardWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "theme/ThemeManager.h"

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
    // The tool button takes its own mouse clicks, so using it never opens the project.
    auto* more = new QToolButton(this);
    more->setObjectName("projectMoreButton");
    more->setIconSize(QSize(18, 18));
    more->setPopupMode(QToolButton::InstantPopup);
    more->setCursor(Qt::PointingHandCursor);
    more->setFocusPolicy(Qt::StrongFocus);
    more->setToolTip(tr("More actions"));
    more->setAccessibleName(tr("More actions for %1").arg(name_));   // the button has no visible text
    more->setMinimumSize(28, 24);
    auto* menu = new QMenu(more);
    QAction* editAction = menu->addAction(tr("Edit details"));
    QAction* deleteAction = menu->addAction(tr("Delete project"));
    more->setMenu(menu);
    top->addWidget(more, 0, Qt::AlignTop);
    // Queued: a refresh may delete this card, and the dialog must not open inside its own menu action.
    connect(editAction, &QAction::triggered, this, [this] { emit editRequested(id_); }, Qt::QueuedConnection);
    connect(deleteAction, &QAction::triggered, this, [this] { emit deleteRequested(id_); }, Qt::QueuedConnection);
    // The icon is a picture in one colour, so it is drawn again when the theme changes.
    auto tintIcon = [more] { more->setIcon(ThemeManager::instance().icon("more", Token::TextPrimary)); };
    tintIcon();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, more, tintIcon);
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
