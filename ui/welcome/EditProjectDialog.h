#pragma once
#include "planova/common/Dtos.h"
#include "widgets/ModalDialog.h"

#include <QString>
#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

// Edit a project's name, team and process model. Save runs `save`; when it throws std::invalid_argument
// the message shows inside the dialog and the dialog stays open.
class EditProjectDialog : public ModalDialog {
    Q_OBJECT
public:
    using SaveFn = std::function<void(const QString& name, const QString& team, const QString& model)>;

    EditProjectDialog(const ProjectDto& project, SaveFn save, QWidget* parent);

    void accept() override;

private:
    void updateSaveEnabled();
    void showError(const QString& message);

    SaveFn save_;
    QLineEdit* name_ = nullptr;
    QLineEdit* team_ = nullptr;
    QComboBox* model_ = nullptr;
    QLabel* error_ = nullptr;
    QPushButton* saveButton_ = nullptr;
};
