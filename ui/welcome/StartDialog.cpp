#include "StartDialog.h"

#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kLogoSize = 72;
constexpr int kChoiceHeight = 44;
}

StartDialog::StartDialog(QWidget* parent) : ModalDialog(tr("Welcome to Planova"), parent)
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

    // The two choices are the main content, so they sit in the body. They reuse the modal button styles.
    auto* startNew = new QPushButton(tr("Start new project"));
    startNew->setObjectName("modalPrimaryButton");
    startNew->setDefault(true);
    auto* continueOld = new QPushButton(tr("Continue an old project"));
    continueOld->setObjectName("modalSecondaryButton");
    for (QPushButton* button : {startNew, continueOld}) {
        button->setMinimumHeight(kChoiceHeight);
        button->setCursor(Qt::PointingHandCursor);
    }
    connect(startNew, &QPushButton::clicked, this, [this] { choose(Choice::NewProject); });
    connect(continueOld, &QPushButton::clicked, this, [this] { choose(Choice::ContinueProject); });

    auto* body = contentLayout();
    body->addWidget(logo);
    body->addWidget(subtitle);
    body->addWidget(startNew);
    body->addWidget(continueOld);

    addButton(tr("Not now"), Role::Secondary);
}

void StartDialog::choose(Choice choice)
{
    choice_ = choice;
    accept();
}
