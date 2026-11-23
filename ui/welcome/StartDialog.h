#pragma once
#include "planova/PlanovaCore.h"
#include "widgets/ModalDialog.h"
#include <QString>

// The popup shown when Planova starts: start a new project, or pick one of the existing projects.
// "Not now" (and Esc) closes it without a choice, so the user stays on the page behind it.
class StartDialog : public ModalDialog {
    Q_OBJECT
public:
    enum class Choice { None, NewProject, OpenProject };

    StartDialog(PlanovaCore& core, QWidget* parent);

    Choice choice() const { return choice_; }
    QString projectId() const { return projectId_; }   // the picked project, when choice() is OpenProject

private:
    void choose(Choice choice, const QString& projectId = QString());

    Choice choice_ = Choice::None;
    QString projectId_;
};
