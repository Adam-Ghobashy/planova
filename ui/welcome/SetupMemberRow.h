#pragma once
#include "planova/project/ProjectSetup.h"
#include <QFrame>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

// One team member on the setup page: name, role, lead flag and a 1 to 5 rating for each of the 7 skills.
// Nothing is saved here; SetupPage reads member() when the project is created.
// Object names carry the row id (memberNameEdit_<id>, skillSlider_<id>_<skill index>), which never changes.
class SetupMemberRow : public QFrame {
    Q_OBJECT
public:
    SetupMemberRow(int id, QWidget* parent = nullptr);

    // Position in the list (1 based). Shown in the title and used in the accessible names.
    void setNumber(int number);
    SetupMember member() const;
    bool hasName() const;
    QLineEdit* nameEdit() const { return nameEdit_; }

signals:
    void changed();
    void removeRequested(SetupMemberRow* row);

private:
    void updateNameHint();
    void updateRoleEdit();

    bool nameTouched_ = false;   // true once the name box was left; only then an empty name is shown as an error
    QLabel* title_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLabel* nameHint_ = nullptr;
    QComboBox* roleCombo_ = nullptr;
    QLineEdit* roleEdit_ = nullptr;
    QPushButton* leadCheck_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QSlider* sliders_[7] = {};
    QLabel* values_[7] = {};
};
