#include "SetupWizard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWizardPage>

namespace {

class FoundationsPage : public QWizardPage {
public:
    explicit FoundationsPage(QWidget* parent)
        : QWizardPage(parent)
    {
        setTitle("Step 1: Project Foundations");
        auto* name = new QLineEdit(this);
        auto* team = new QLineEdit(this);
        auto* idea = new QPlainTextEdit(this);
        auto* form = new QFormLayout(this);
        form->addRow("Project name", name);
        form->addRow("Team name", team);
        form->addRow("Description", idea);
        registerField("projectName*", name);
        registerField("teamName*", team);
        registerField("idea", idea, "plainText", "textChanged");
    }
};

class TimelinePage : public QWizardPage {
public:
    explicit TimelinePage(QWidget* parent)
        : QWizardPage(parent)
    {
        setTitle("Step 2: Timeline & Tech Stack");
        auto* stack = new QLineEdit(this);
        auto* form = new QFormLayout(this);
        form->addRow("Tech stack", stack);
    }
};

class ModelPage : public QWizardPage {
public:
    explicit ModelPage(QWidget* parent)
        : QWizardPage(parent)
    {
        setTitle("Step 3: Process Lifecycle Model");
        auto* combo = new QComboBox(this);
        combo->addItems({"Agile/Scrum", "Waterfall", "Spiral", "Prototyping", "Hybrid"});
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(combo);
        registerField("seModel", combo, "currentText", "currentTextChanged");
    }
};

// --- S2: Step 4 Team Skills Wizard Page ---
const QStringList kKnownWizardRoles = {
    "Backend Developer",
    "Frontend Developer",
    "Full Stack Developer",
    "UI / UX Designer",
    "Database Architect",
    "QA / Testing Engineer",
    "Integration & DevOps",
    "Technical Documentation",
    "Team & Systems Lead",
    "Other..."
};

class TeamSkillsWizardPage : public QWizardPage {
public:
    explicit TeamSkillsWizardPage(PlanovaCore& core, QWidget* parent)
        : QWizardPage(parent), core_(core)
    {
        setTitle("Step 4: Team & Skills Setup");
        setSubTitle("Add your initial project team members and their primary roles.");

        auto* layout = new QVBoxLayout(this);

        auto* row = new QHBoxLayout();
        nameEdit_ = new QLineEdit(this);
        nameEdit_->setPlaceholderText("Member Name (e.g., Adam)");

        roleCombo_ = new QComboBox(this);
        roleCombo_->addItems(kKnownWizardRoles);

        customRoleEdit_ = new QLineEdit(this);
        customRoleEdit_->setPlaceholderText("Write custom role...");
        customRoleEdit_->hide();

        connect(roleCombo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
            if (text == "Other..." || text == "Other") {
                customRoleEdit_->show();
                customRoleEdit_->setFocus();
            } else {
                customRoleEdit_->hide();
            }
        });

        leadCheck_ = new QCheckBox("Lead", this);
        auto* addBtn = new QPushButton("+ Add", this);

        row->addWidget(nameEdit_, 2);
        row->addWidget(roleCombo_, 2);
        row->addWidget(customRoleEdit_, 2);
        row->addWidget(leadCheck_);
        row->addWidget(addBtn);
        layout->addLayout(row);

        list_ = new QListWidget(this);
        layout->addWidget(list_);

        connect(addBtn, &QPushButton::clicked, this, [this] {
            const QString name = nameEdit_->text().trimmed();
            if (name.isEmpty()) return;
            QString role = roleCombo_->currentText();
            if (role == "Other..." || role == "Other") {
                role = customRoleEdit_->text().trimmed();
            }
            if (role.isEmpty()) role = "Developer";
            const bool isLead = leadCheck_->isChecked();

            core_.members().add(name.toStdString(), role.toStdString(), isLead);
            list_->addItem(name + " — " + role + (isLead ? " [Team Lead]" : ""));
            nameEdit_->clear();
            roleCombo_->setCurrentIndex(0);
            customRoleEdit_->clear();
            customRoleEdit_->hide();
            leadCheck_->setChecked(false);
        });
    }

private:
    PlanovaCore& core_;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* roleCombo_ = nullptr;
    QLineEdit* customRoleEdit_ = nullptr;
    QCheckBox* leadCheck_ = nullptr;
    QListWidget* list_ = nullptr;
};

// --- S2: Step 8 Git Sync Wizard Page ---
class GitSyncWizardPage : public QWizardPage {
public:
    explicit GitSyncWizardPage(QWidget* parent)
        : QWizardPage(parent)
    {
        setTitle("Step 8: Git Sync Configuration");
        setSubTitle("Connect a GitHub repository for peer event store synchronization (optional).");

        auto* repoUrl = new QLineEdit(this);
        repoUrl->setPlaceholderText("https://github.com/Zeyad-101/planova.git");
        auto* branch = new QLineEdit("main", this);

        auto* form = new QFormLayout(this);
        form->addRow("Repository URL", repoUrl);
        form->addRow("Default branch", branch);

        registerField("gitRepoUrl", repoUrl);
        registerField("gitBranch", branch);
    }
};

class ReviewPage : public QWizardPage {
public:
    using QWizardPage::QWizardPage;

    void initializePage() override
    {
        QString gitInfo = field("gitRepoUrl").toString().trimmed();
        if (gitInfo.isEmpty()) gitInfo = "Not connected";
        summary_->setText(
            "Project: " + field("projectName").toString() + "\n"
                                                            "Team: " + field("teamName").toString() + "\n"
                                             "Process model: " + field("seModel").toString() + "\n"
                                            "Git repository: " + gitInfo);
    }

    void setSummaryLabel(QLabel* label) { summary_ = label; }

private:
    QLabel* summary_ = nullptr;
};

QWizardPage* placeholderPage(const QString& title, const QString& owner, QWizard* parent)
{
    auto* page = new QWizardPage(parent);
    page->setTitle(title);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("Coming in a later slice (" + owner + ").", page));
    return page;
}

} // namespace

SetupWizard::SetupWizard(PlanovaCore& core, QWidget* parent)
    : QWizard(parent), core_(core)
{
    setWindowTitle("Guided Project Setup");

    addPage(new FoundationsPage(this));
    addPage(new TimelinePage(this));
    addPage(new ModelPage(this));

    addPage(new TeamSkillsWizardPage(core_, this));
    addPage(placeholderPage("Step 5: Components", "team decision needed", this));
    addPage(placeholderPage("Step 6: Constraints", "team decision needed", this));
    addPage(placeholderPage("Step 7: Slicer Weights", "S4", this));
    addPage(new GitSyncWizardPage(this));

    auto* review = new ReviewPage(this);
    review->setTitle("Step 9: Launch & Final Review");
    auto* summary = new QLabel(review);
    review->setSummaryLabel(summary);
    auto* layout = new QVBoxLayout(review);
    layout->addWidget(summary);
    addPage(review);

    setButtonText(QWizard::FinishButton, "Generate & Launch Workspace");
}

void SetupWizard::accept()
{
    core_.projects().create(
        field("projectName").toString().toStdString(),
        field("teamName").toString().toStdString(),
        field("seModel").toString().toStdString(),
        field("idea").toString().toStdString());

    const QString repoUrl = field("gitRepoUrl").toString().trimmed();
    if (!repoUrl.isEmpty()) {
        QString branch = field("gitBranch").toString().trimmed();
        if (branch.isEmpty()) branch = "main";
        core_.git().connect(repoUrl.toStdString(), branch.toStdString());
    }

    QWizard::accept();
}