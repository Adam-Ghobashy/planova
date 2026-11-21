#pragma once
#include <optional>
#include <vector>
#include <QString>
#include "planova/common/Dtos.h"
#include "widgets/ModalDialog.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QSpinBox;

class NewTaskDialog : public ModalDialog {
    Q_OBJECT
public:
    NewTaskDialog(const std::vector<TaskDto>& existing,
                  const std::vector<MemberDto>& members,
                  QWidget* parent = nullptr);

    NewTask taskData() const;
    std::optional<Id> assignee() const;      // empty = unassigned
    QString component() const;

public slots:
    void accept() override;   // checks the title first

private:

    QLineEdit* title_ = nullptr;
    QComboBox* component_ = nullptr;
    QComboBox* skill_ = nullptr;
    QComboBox* priority_ = nullptr;
    QComboBox* complexity_ = nullptr;
    QComboBox* assignee_ = nullptr;
    QSpinBox* hours_ = nullptr;
    std::vector<QCheckBox*> depChecks_;
    QLabel* error_ = nullptr;
};