#pragma once
#include <optional>
#include <vector>
#include <QDialog>
#include <QString>
#include "planova/common/Dtos.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;

class NewTaskDialog : public QDialog {
    Q_OBJECT
public:
    NewTaskDialog(const std::vector<TaskDto>& existing,
                  const std::vector<MemberDto>& members,
                  QWidget* parent = nullptr);

    NewTask taskData() const;
    std::optional<Id> assignee() const;      // empty = unassigned
    QString component() const;

private:
    void validateAndAccept();

    QLineEdit* title_ = nullptr;
    QComboBox* component_ = nullptr;
    QComboBox* skill_ = nullptr;
    QComboBox* priority_ = nullptr;
    QComboBox* complexity_ = nullptr;
    QComboBox* assignee_ = nullptr;
    QSpinBox* hours_ = nullptr;
    QListWidget* deps_ = nullptr;
    QLabel* error_ = nullptr;
};