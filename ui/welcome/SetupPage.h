#pragma once
#include "planova/PlanovaCore.h"
#include <QList>
#include <QString>
#include <QWidget>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class SetupMemberRow;
class StepperWidget;

namespace planova::slicer {
class WeightSlidersPanel;
}

// Guided project setup in six steps. Nothing is written until "Create Project": Back, Skip and leaving the
// page only drop what was typed. Create hands everything to PlanovaCore::setupProject in one call.
class SetupPage : public QWidget {
    Q_OBJECT
public:
    explicit SetupPage(PlanovaCore& core, QWidget* parent = nullptr);

    // Empties every field and goes back to step 1. Call it each time the page is shown.
    void reset();

signals:
    void projectCreated();   // the project exists and is open
    void skipped();          // the user left without creating anything

private:
    QWidget* buildFoundations();
    QWidget* buildProcessModel();
    QWidget* buildTeam();
    QWidget* buildSlicerPolicy();
    QWidget* buildGit();
    QWidget* buildReview();

    void goToStep(int step);
    void focusFirstControl();
    void addMember();
    void removeMember(SetupMemberRow* row);
    void updateNavigation();
    void updateReview();
    QString problem() const;          // why Create is blocked, empty when it is allowed
    ProjectSetup collect() const;
    QString selectedModel() const;
    void create();

    PlanovaCore& core_;
    int step_ = 0;
    int nextMemberId_ = 0;
    bool nameTouched_ = false;

    QLabel* badge_ = nullptr;
    QLabel* stepTitle_ = nullptr;
    QLabel* stepDescription_ = nullptr;
    StepperWidget* stepper_ = nullptr;
    QStackedWidget* steps_ = nullptr;
    QPushButton* backButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QPushButton* createButton_ = nullptr;

    QLineEdit* nameEdit_ = nullptr;
    QLabel* nameHint_ = nullptr;
    QLineEdit* teamEdit_ = nullptr;
    QPlainTextEdit* ideaEdit_ = nullptr;
    QButtonGroup* modelGroup_ = nullptr;
    QVBoxLayout* membersLayout_ = nullptr;
    QLabel* noMembers_ = nullptr;
    QList<SetupMemberRow*> rows_;
    planova::slicer::WeightSlidersPanel* weights_ = nullptr;
    QLineEdit* repoEdit_ = nullptr;
    QLineEdit* branchEdit_ = nullptr;

    QLabel* summaryName_ = nullptr;
    QLabel* summaryTeam_ = nullptr;
    QLabel* summaryModel_ = nullptr;
    QLabel* summaryMembers_ = nullptr;
    QLabel* summaryWeights_ = nullptr;
    QLabel* summaryGit_ = nullptr;
    QLabel* createProblem_ = nullptr;
    QPushButton* fixMembersButton_ = nullptr;
};
