#include "StartDialog.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <exception>

namespace {
constexpr int kLogoSize = 72;
constexpr int kChoiceHeight = 44;

QPushButton* setUpChoice(QPushButton* button, const char* objectName, QWidget* dialog)
{
    button->setObjectName(objectName);   // modalPrimaryButton / modalSecondaryButton: styled in modal.qss
    button->setMinimumHeight(kChoiceHeight);
    button->setCursor(Qt::PointingHandCursor);
    button->setParent(dialog);
    return button;
}

constexpr int kMarkMargin = 14;      // space right of the "Current" mark
constexpr int kMarkGap = 8;          // least space between the name and the mark
constexpr int kButtonPadding = 16;   // modal.qss: 15 px padding and a 1 px border on each side

// A project choice that shortens its name with "..." to fit the button. The text is centred, so the space the
// "Current" mark needs on the right is kept free on both sides, and the name never runs under the mark.
class ProjectChoiceButton : public QPushButton {
public:
    explicit ProjectChoiceButton(const QString& fullText) : QPushButton(fullText), fullText_(fullText)
    {
        // The dialog sets the width; a long name must not widen it.
        setSizePolicy(QSizePolicy::Ignored, sizePolicy().verticalPolicy());
    }

    void setMark(QWidget* mark) { mark_ = mark; }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QPushButton::resizeEvent(event);
        const int reserved = mark_ ? mark_->sizeHint().width() + kMarkMargin + kMarkGap : 0;
        const int available = std::max(0, width() - 2 * kButtonPadding - 2 * reserved);
        setText(fontMetrics().elidedText(fullText_, Qt::ElideRight, available));
        setToolTip(text() == fullText_ ? QString() : fullText_);   // the whole name when it is shortened
    }

private:
    QString fullText_;
    QWidget* mark_ = nullptr;
};
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

    auto* startNew = setUpChoice(new QPushButton(tr("Start new project")), "modalPrimaryButton", this);
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
        auto* button = new ProjectChoiceButton(team.isEmpty() ? name : name + QString::fromUtf16(u"  ·  ") + team);
        setUpChoice(button, "modalSecondaryButton", this);
        button->setAccessibleName(tr("Open project %1").arg(name));
        const QString id = QString::fromStdString(it->id);
        connect(button, &QPushButton::clicked, this, [this, id] { choose(Choice::OpenProject, id); });
        if (core.currentProject() && *core.currentProject() == it->id) {
            // The open project says so in words, not only by position or colour.
            button->setAccessibleDescription(tr("Current"));
            auto* mark = new QLabel(tr("Current"), button);
            mark->setObjectName("startDialogCurrent");
            mark->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            button->setMark(mark);
            auto* markLayout = new QHBoxLayout(button);
            markLayout->setContentsMargins(0, 0, kMarkMargin, 0);
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
