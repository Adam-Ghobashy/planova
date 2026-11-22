#include "StartDialog.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <exception>

namespace {
constexpr int kLogoSize = 72;
constexpr int kChoiceHeight = 44;

QPushButton* makeChoice(const QString& text, const char* objectName, QWidget* dialog)
{
    auto* button = new QPushButton(text);
    button->setObjectName(objectName);   // modalPrimaryButton / modalSecondaryButton: styled in modal.qss
    button->setMinimumHeight(kChoiceHeight);
    button->setCursor(Qt::PointingHandCursor);
    button->setParent(dialog);
    return button;
}
}

StartDialog::StartDialog(PlanovaCore& core, QWidget* parent) : ModalDialog(tr("Welcome to Planova"), parent)
{
    auto* logo = new QLabel;
    logo->setObjectName("startDialogLogo");
    logo->setAlignment(Qt::AlignCenter);
    logo->setAccessibleName(tr("Planova logo"));
    QPixmap pixmap = QPixmap(":/icons/logo.png").scaled(kLogoSize * 2, kLogoSize * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmap.setDevicePixelRatio(2.0);
    logo->setPixmap(pixmap);

    auto* subtitle = new QLabel(tr("Plan the work · Match the team · Measure actual progress"));
    subtitle->setObjectName("modalSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);

    auto* startNew = makeChoice(tr("Start new project"), "modalPrimaryButton", this);
    startNew->setDefault(true);
    connect(startNew, &QPushButton::clicked, this, [this] { choose(Choice::NewProject); });

    auto* body = contentLayout();
    body->addWidget(logo);
    body->addWidget(subtitle);
    body->addWidget(startNew);

    auto* heading = new QLabel(tr("Or continue a project"));
    heading->setObjectName("modalFieldLabel");
    body->addWidget(heading);

    // Newest project first. The dialog is built inside a start-up slot, so a failed read must not escape.
    std::vector<ProjectDto> projects;
    try {
        projects = core.projects().list();
    } catch (const std::exception& error) {
        qWarning() << "StartDialog: could not read the projects:" << error.what();
    }
    if (projects.empty()) {
        auto* none = new QLabel(tr("No projects yet. Start a new one above."));
        none->setObjectName("modalSubtitle");
        none->setWordWrap(true);
        body->addWidget(none);
    }
    for (auto it = projects.rbegin(); it != projects.rend(); ++it) {
        const QString name = QString::fromStdString(it->name);
        const QString team = QString::fromStdString(it->team);
        auto* button = makeChoice(team.isEmpty() ? name : name + QString::fromUtf16(u"  ·  ") + team, "modalSecondaryButton", this);
        button->setAccessibleName(tr("Open project %1").arg(name));
        const QString id = QString::fromStdString(it->id);
        connect(button, &QPushButton::clicked, this, [this, id] { choose(Choice::OpenProject, id); });
        if (core.currentProject() && *core.currentProject() == it->id) {
            // The open project says so in words, not only by position or colour.
            button->setAccessibleDescription(tr("Current"));
            auto* mark = new QLabel(tr("Current"), button);
            mark->setObjectName("startDialogCurrent");
            mark->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            auto* markLayout = new QHBoxLayout(button);
            markLayout->setContentsMargins(0, 0, 14, 0);
            markLayout->addStretch(1);
            markLayout->addWidget(mark, 0, Qt::AlignVCenter);
        }
        body->addWidget(button);
    }

    addButton(tr("Not now"), Role::Secondary);
}

void StartDialog::choose(Choice choice, const QString& projectId)
{
    choice_ = choice;
    projectId_ = projectId;
    accept();
}
