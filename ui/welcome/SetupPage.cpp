#include "welcome/SetupPage.h"
#include "slicer/WeightSlidersPanel.h"
#include "welcome/SetupMemberRow.h"
#include "welcome/StepperWidget.h"
#include "widgets/ModalDialog.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

namespace {

constexpr int kStepCount = 6;
constexpr int kContentMaxWidth = 980;

struct StepInfo {
    const char* shortName;     // under the circle in the stepper
    const char* title;         // heading of the card
    const char* description;
};
const StepInfo kSteps[kStepCount] = {
    {"Foundations", "Project Foundations",
     "Name the project and describe the idea. Only the project name is required."},
    {"Process Model", "Process Lifecycle Model",
     "Choose the lifecycle this project follows. It is shown on the project card and in the summary."},
    {"Team & Skills", "Team & Skill Matrix",
     "Add the team and rate each skill from 1 (new to it) to 5 (expert). The Chaos Slicer uses the ratings "
     "to assign tasks. At least one member is required."},
    {"Slicer Policy", "Chaos Slicer Balancing Policy",
     "Set the default weighting formula: \xCE\xB1\xC2\xB7Skill + \xCE\xB2\xC2\xB7Workload + \xCE\xB3\xC2\xB7Priority. "
     "The three weights always add up to 100%."},
    {"Git Sync", "Git & Peer Event Synchronization",
     "Optionally connect a GitHub repository to share events with the team. "
     "You can also do this later on the GitHub page."},
    {"Review & Launch", "Launch & Final Review",
     "Check the summary, then create the project. Nothing is saved until you press Create Project."},
};

struct ModelInfo {
    const char* name;
    const char* description;
};
const ModelInfo kModels[4] = {
    {"Agile", "Short iterative cycles with continuous feedback and a flexible backlog."},
    {"Scrum", "Fixed-length sprints, defined roles, and burn-down tracking."},
    {"Waterfall", "Sequential phases: Requirements, Design, Implementation, Verification."},
    {"Spiral", "Risk-driven iterative cycles with early prototyping and evaluation."},
};

// Sets the "error" property that setup.qss styles, and re-applies the style.
void setError(QWidget* widget, bool error)
{
    widget->setProperty("error", error);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

QPushButton* makeButton(const QString& text, const char* objectName, const char* kind, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    button->setProperty("kind", kind);   // "primary", "secondary" or "ghost", styled in setup.qss
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoDefault(false);       // Enter never presses a button the user did not focus
    return button;
}

QLabel* makeLabel(const QString& text, const char* objectName, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

// A caption above a single-line field; the caption is the field's buddy.
QLineEdit* addField(QVBoxLayout* layout, const QString& caption, const char* objectName,
                    const QString& placeholder, QWidget* parent)
{
    auto* edit = new QLineEdit(parent);
    edit->setObjectName(objectName);
    edit->setPlaceholderText(placeholder);
    auto* label = new QLabel(caption, parent);
    label->setObjectName("fieldLabel");
    label->setBuddy(edit);
    layout->addWidget(label);
    layout->addWidget(edit);
    return edit;
}

// Puts the column in the middle of the row, at most kContentMaxWidth wide.
QWidget* centered(QWidget* column, QWidget* parent)
{
    column->setMaximumWidth(kContentMaxWidth);
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch(1);
    layout->addWidget(column, 100);
    layout->addStretch(1);
    return row;
}

// A selectable card: title, description and a "Selected" mark that shows only when chosen.
QPushButton* makeModelCard(const ModelInfo& model, QWidget* parent)
{
    auto* card = new QPushButton(parent);
    card->setObjectName(QString("modelCard_%1").arg(model.name));
    card->setProperty("modelName", model.name);
    card->setProperty("kind", "modelCard");   // styled in setup.qss
    card->setCheckable(true);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(104);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    card->setAccessibleName(QString::fromUtf8(model.name));
    card->setAccessibleDescription(QString::fromUtf8(model.description));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);
    auto* title = new QLabel(QString::fromUtf8(model.name), card);
    title->setObjectName("modelCardTitle");
    auto* mark = new QLabel(QObject::tr("\xE2\x9C\x93 Selected"), card);
    mark->setObjectName("modelCardMark");
    mark->setVisible(false);
    auto* top = new QHBoxLayout;
    top->addWidget(title);
    top->addStretch(1);
    top->addWidget(mark);
    layout->addLayout(top);
    auto* description = new QLabel(QString::fromUtf8(model.description), card);
    description->setObjectName("modelCardText");
    description->setWordWrap(true);
    layout->addWidget(description, 1);

    // The labels only decorate the button; clicks must reach the button itself.
    for (QLabel* label : {title, mark, description})
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    QObject::connect(card, &QPushButton::toggled, mark, &QLabel::setVisible);
    return card;
}

} // namespace

SetupPage::SetupPage(PlanovaCore& core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setObjectName("SetupPage");
    setAttribute(Qt::WA_StyledBackground, true);

    // ---- header: title and subtitle, step badge, Skip ----
    auto* title = makeLabel(tr("Guided Project Setup"), "setupTitle", this);
    auto* subtitle = makeLabel(tr("Configure lifecycle, skill matrix, slicing policy, and repository integration."),
                               "setupSubtitle", this);
    auto* titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(4);
    titleColumn->addWidget(title);
    titleColumn->addWidget(subtitle);

    badge_ = new QLabel(this);
    badge_->setObjectName("stepBadge");
    auto* skip = makeButton(tr("Skip to Dashboard"), "skipButton", "secondary", this);
    connect(skip, &QPushButton::clicked, this, &SetupPage::skipped);

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    header->addLayout(titleColumn, 1);
    header->addWidget(badge_, 0, Qt::AlignVCenter);
    header->addWidget(skip, 0, Qt::AlignVCenter);

    // ---- stepper and the card with the six steps ----
    stepper_ = new StepperWidget(this);
    QStringList shortNames;
    for (const StepInfo& step : kSteps)
        shortNames << tr(step.shortName);
    stepper_->setSteps(shortNames);

    auto* card = new QFrame(this);
    card->setObjectName("stepCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(6);
    stepTitle_ = makeLabel(QString(), "stepTitle", card);
    stepDescription_ = makeLabel(QString(), "stepDescription", card);
    cardLayout->addWidget(stepTitle_);
    cardLayout->addWidget(stepDescription_);
    cardLayout->addSpacing(14);

    steps_ = new QStackedWidget(card);
    steps_->setObjectName("steps");
    steps_->addWidget(buildFoundations());
    steps_->addWidget(buildProcessModel());
    steps_->addWidget(buildTeam());
    steps_->addWidget(buildSlicerPolicy());
    steps_->addWidget(buildGit());
    steps_->addWidget(buildReview());
    cardLayout->addWidget(steps_, 1);

    auto* column = new QWidget(this);
    column->setObjectName("setupColumn");
    auto* columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(18);
    columnLayout->addLayout(header);
    columnLayout->addWidget(stepper_);
    card->setMinimumHeight(360);   // short steps do not collapse; the card does not stretch to the window height
    columnLayout->addWidget(card);

    auto* content = new QWidget;
    content->setObjectName("setupContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 20, 24, 20);
    contentLayout->addWidget(centered(column, content));
    contentLayout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("setupScroll");
    scroll->viewport()->setObjectName("setupViewport");
    scroll->setWidgetResizable(true);
    scroll->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);

    // ---- footer: Back on the left, Next or Create Project on the right. It stays visible while the page scrolls. ----
    backButton_ = makeButton(tr("\xE2\x86\x90 Back"), "backButton", "secondary", this);
    nextButton_ = makeButton(tr("Next \xE2\x86\x92"), "nextButton", "primary", this);
    createButton_ = makeButton(tr("Create Project"), "createButton", "primary", this);
    connect(backButton_, &QPushButton::clicked, this, [this] { goToStep(step_ - 1); });
    connect(nextButton_, &QPushButton::clicked, this, [this] { goToStep(step_ + 1); });
    connect(createButton_, &QPushButton::clicked, this, &SetupPage::create);

    auto* footerColumn = new QWidget(this);
    auto* footerRow = new QHBoxLayout(footerColumn);
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->setSpacing(10);
    footerRow->addWidget(backButton_);
    footerRow->addStretch(1);
    footerRow->addWidget(nextButton_);
    footerRow->addWidget(createButton_);
    auto* footer = new QWidget(this);
    footer->setObjectName("setupFooter");
    auto* footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(24, 12, 24, 12);
    footerLayout->addWidget(centered(footerColumn, footer));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(scroll, 1);
    layout->addWidget(footer);

    // Tab order: Skip first, then the step, then the footer buttons.
    setTabOrder(skip, steps_);
    reset();
}

QWidget* SetupPage::buildFoundations()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    nameEdit_ = addField(layout, tr("Project name (required)"), "projectNameEdit",
                         tr("e.g. Library Portal"), page);
    nameHint_ = makeLabel(QString(), "nameHint", page);
    nameHint_->setTextFormat(Qt::PlainText);
    layout->addWidget(nameHint_);
    layout->addSpacing(8);

    teamEdit_ = addField(layout, tr("Team name (optional)"), "teamNameEdit", tr("e.g. Team Six"), page);
    layout->addSpacing(8);

    auto* ideaLabel = new QLabel(tr("Project idea (optional)"), page);
    ideaLabel->setObjectName("fieldLabel");
    ideaEdit_ = new QPlainTextEdit(page);
    ideaEdit_->setObjectName("ideaEdit");
    ideaEdit_->setPlaceholderText(tr("What are you building, and for whom?"));
    ideaEdit_->setTabChangesFocus(true);   // Tab moves on instead of typing a tab character
    ideaEdit_->setFixedHeight(110);
    ideaLabel->setBuddy(ideaEdit_);
    layout->addWidget(ideaLabel);
    layout->addWidget(ideaEdit_);
    layout->addStretch(1);

    connect(nameEdit_, &QLineEdit::textChanged, this, &SetupPage::updateNavigation);
    // The hint turns red only after the user left the box empty.
    connect(nameEdit_, &QLineEdit::editingFinished, this, [this] { nameTouched_ = true; updateNavigation(); });
    return page;
}

QWidget* SetupPage::buildProcessModel()
{
    auto* page = new QWidget;
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(14);

    modelGroup_ = new QButtonGroup(this);   // exclusive by default: choosing one clears the others
    for (int i = 0; i < 4; ++i) {
        QPushButton* card = makeModelCard(kModels[i], page);
        modelGroup_->addButton(card);
        grid->addWidget(card, i / 2, i % 2);
    }
    grid->setRowStretch(2, 1);
    return page;
}

QWidget* SetupPage::buildTeam()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* addButton = makeButton(tr("+ Add Member"), "addMemberButton", "secondary", page);
    connect(addButton, &QPushButton::clicked, this, &SetupPage::addMember);
    auto* top = new QHBoxLayout;
    top->addStretch(1);
    top->addWidget(addButton);
    layout->addLayout(top);

    noMembers_ = makeLabel(tr("No team members yet. Add at least one member to create the project."),
                           "noMembers", page);
    layout->addWidget(noMembers_);

    membersLayout_ = new QVBoxLayout;
    membersLayout_->setSpacing(12);
    layout->addLayout(membersLayout_);
    layout->addStretch(1);
    return page;
}

QWidget* SetupPage::buildSlicerPolicy()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    weights_ = new planova::slicer::WeightSlidersPanel(page);
    layout->addWidget(weights_);
    layout->addStretch(1);
    return page;
}

QWidget* SetupPage::buildGit()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    repoEdit_ = addField(layout, tr("Repository URL (optional)"), "repoUrlEdit",
                         tr("https://github.com/your-team/your-repo.git"), page);
    layout->addSpacing(8);
    branchEdit_ = addField(layout, tr("Default branch"), "branchEdit", tr("main"), page);
    layout->addStretch(1);
    return page;
}

QWidget* SetupPage::buildReview()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // caption and value for each setting
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);
    struct Row {
        const char* caption;
        QLabel** value;
        const char* name;
    };
    const Row rows[] = {
        {"Project", &summaryName_, "summaryName"},   {"Team", &summaryTeam_, "summaryTeam"},
        {"Process model", &summaryModel_, "summaryModel"}, {"Team members", &summaryMembers_, "summaryMembers"},
        {"Slicer weights", &summaryWeights_, "summaryWeights"}, {"Git repository", &summaryGit_, "summaryGit"},
    };
    int row = 0;
    for (const Row& entry : rows) {
        auto* caption = new QLabel(tr(entry.caption), page);
        caption->setObjectName("fieldLabel");
        *entry.value = makeLabel(QString(), entry.name, page);
        (*entry.value)->setTextFormat(Qt::PlainText);   // the text is what the user typed
        grid->addWidget(caption, row, 0, Qt::AlignTop);
        grid->addWidget((*entry.value), row, 1);
        ++row;
    }
    layout->addLayout(grid);

    createProblem_ = makeLabel(QString(), "createProblem", page);
    fixMembersButton_ = makeButton(tr("Go to Team && Skill Matrix"), "fixMembersButton", "secondary", page);
    connect(fixMembersButton_, &QPushButton::clicked, this, [this] { goToStep(2); });
    layout->addWidget(createProblem_);
    layout->addWidget(fixMembersButton_, 0, Qt::AlignLeft);
    layout->addStretch(1);
    return page;
}

void SetupPage::reset()
{
    nameEdit_->clear();
    teamEdit_->clear();
    ideaEdit_->clear();
    repoEdit_->clear();
    branchEdit_->setText("main");
    nameTouched_ = false;
    modelGroup_->buttons().first()->setChecked(true);   // Agile
    weights_->setWeights(planova::slicer::ScoringWeights{});

    // Rows are deleted later: reset() can run inside the click of a row's own button.
    for (SetupMemberRow* row : rows_) {
        row->hide();
        row->setParent(nullptr);
        row->deleteLater();
    }
    rows_.clear();
    nextMemberId_ = 0;

    goToStep(0);
    nameEdit_->setFocus();
}

void SetupPage::goToStep(int step)
{
    step_ = qBound(0, step, kStepCount - 1);
    steps_->setCurrentIndex(step_);
    // Only the current step counts for the card height, so a long team list does not stretch the short steps.
    for (int i = 0; i < steps_->count(); ++i)
        steps_->widget(i)->setSizePolicy(QSizePolicy::Preferred, i == step_ ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    stepper_->setCurrent(step_);
    stepTitle_->setText(tr(kSteps[step_].title));
    stepDescription_->setText(tr(kSteps[step_].description));
    updateNavigation();
    if (isVisible())
        focusFirstControl();
}

// After a step change the keyboard focus moves into the new step. On the review it goes to Back, so a double
// Enter on Next can never create the project.
void SetupPage::focusFirstControl()
{
    switch (step_) {
    case 0: nameEdit_->setFocus(); break;
    case 1: modelGroup_->checkedButton()->setFocus(); break;
    case 2: steps_->currentWidget()->findChild<QPushButton*>("addMemberButton")->setFocus(); break;
    case 3: weights_->findChild<QSlider*>()->setFocus(); break;
    case 4: repoEdit_->setFocus(); break;
    default: backButton_->setFocus(); break;
    }
}

void SetupPage::addMember()
{
    auto* row = new SetupMemberRow(nextMemberId_++, steps_->widget(2));
    rows_.append(row);
    membersLayout_->addWidget(row);
    connect(row, &SetupMemberRow::changed, this, &SetupPage::updateNavigation);
    connect(row, &SetupMemberRow::removeRequested, this, &SetupPage::removeMember);
    for (int i = 0; i < rows_.size(); ++i)
        rows_[i]->setNumber(i + 1);
    row->show();
    row->nameEdit()->setFocus();
    updateNavigation();
}

void SetupPage::removeMember(SetupMemberRow* row)
{
    rows_.removeOne(row);
    row->hide();
    row->setParent(nullptr);   // out of the page now; deleted later because this runs in the row's own button click
    row->deleteLater();
    for (int i = 0; i < rows_.size(); ++i)
        rows_[i]->setNumber(i + 1);
    steps_->widget(2)->findChild<QPushButton*>("addMemberButton")->setFocus();
    updateNavigation();
}

QString SetupPage::problem() const
{
    if (nameEdit_->text().trimmed().isEmpty())
        return tr("Enter a project name.");
    if (rows_.isEmpty())
        return tr("Add at least one team member before creating the project.");
    for (const SetupMemberRow* row : rows_) {
        if (!row->hasName())
            return tr("Every team member needs a name.");
    }
    return QString();
}

void SetupPage::updateNavigation()
{
    const bool hasName = !nameEdit_->text().trimmed().isEmpty();
    badge_->setText(tr("Step %1 of %2").arg(step_ + 1).arg(kStepCount));

    // Step 1: say why Next is disabled. Neutral wording first, red once the user left the box empty.
    nameHint_->setText(hasName ? QString() : nameTouched_ ? tr("A project name is required to continue.")
                                                          : tr("Enter a project name to continue."));
    nameHint_->setVisible(!hasName);
    setError(nameHint_, !hasName && nameTouched_);
    setError(nameEdit_, !hasName && nameTouched_);

    noMembers_->setVisible(rows_.isEmpty());

    const bool last = step_ == kStepCount - 1;
    backButton_->setEnabled(step_ > 0);
    nextButton_->setVisible(!last);
    nextButton_->setEnabled(hasName);
    createButton_->setVisible(last);
    createButton_->setEnabled(problem().isEmpty());
    if (last)
        updateReview();
}

void SetupPage::updateReview()
{
    const ProjectSetup setup = collect();
    summaryName_->setText(QString::fromStdString(setup.name));
    summaryTeam_->setText(setup.team.empty() ? tr("Not set") : QString::fromStdString(setup.team));
    summaryModel_->setText(QString::fromStdString(setup.seModel));

    QStringList names;
    for (const SetupMember& member : setup.members) {
        const QString name = QString::fromStdString(member.name);
        names << (member.isLead ? tr("%1 (lead)").arg(name) : name);
    }
    summaryMembers_->setText(names.isEmpty() ? tr("None yet") : tr("%1: %2").arg(names.size()).arg(names.join(", ")));
    summaryWeights_->setText(tr("Skill %1%  \xC2\xB7  Workload %2%  \xC2\xB7  Priority %3%")
                                 .arg(setup.weights.skill).arg(setup.weights.workload).arg(setup.weights.priority));
    summaryGit_->setText(setup.repoUrl.empty()
                             ? tr("Not connected. You can connect later on the GitHub page.")
                             : tr("%1 (branch %2)").arg(QString::fromStdString(setup.repoUrl),
                                                         QString::fromStdString(setup.branch)));

    const QString blocked = problem();
    createProblem_->setText(blocked);
    createProblem_->setVisible(!blocked.isEmpty());
    fixMembersButton_->setVisible(!blocked.isEmpty());
}

QString SetupPage::selectedModel() const
{
    const QAbstractButton* checked = modelGroup_->checkedButton();
    return checked ? checked->property("modelName").toString() : QString();
}

ProjectSetup SetupPage::collect() const
{
    ProjectSetup setup;
    setup.name = nameEdit_->text().trimmed().toStdString();
    setup.team = teamEdit_->text().trimmed().toStdString();
    setup.idea = ideaEdit_->toPlainText().trimmed().toStdString();
    setup.seModel = selectedModel().toStdString();
    for (const SetupMemberRow* row : rows_)
        setup.members.push_back(row->member());
    setup.weights = weights_->weights();
    setup.repoUrl = repoEdit_->text().trimmed().toStdString();
    setup.branch = branchEdit_->text().trimmed().toStdString();
    return setup;
}

void SetupPage::create()
{
    if (!problem().isEmpty())
        return;
    try {
        core_.setupProject(collect());
    } catch (const std::exception& error) {
        // Stay on the page: everything typed is still here, so the user can fix it and press Create again.
        ModalDialog::info(this, tr("Could not create project"), QString::fromUtf8(error.what()));
        return;
    }
    emit projectCreated();
}
