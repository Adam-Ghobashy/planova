#pragma once
#include "widgets/ModalDialog.h"

// The popup shown when Planova starts: start a new project or continue an old one.
// "Not now" (and Esc) closes it without a choice, so the user stays on the page behind it.
class StartDialog : public ModalDialog {
    Q_OBJECT
public:
    enum class Choice { None, NewProject, ContinueProject };

    explicit StartDialog(QWidget* parent);

    Choice choice() const { return choice_; }

private:
    void choose(Choice choice);

    Choice choice_ = Choice::None;
};
