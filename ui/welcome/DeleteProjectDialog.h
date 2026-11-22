#pragma once
#include "widgets/ModalDialog.h"

#include <QString>

class QLineEdit;
class QPushButton;

// Asks the user to type the project name before the Delete button works.
// Delete is enabled only while the typed text equals the project name exactly.
class DeleteProjectDialog : public ModalDialog {
    Q_OBJECT
public:
    DeleteProjectDialog(const QString& projectName, QWidget* parent);

private:
    void updateDeleteEnabled();

    QString projectName_;
    QLineEdit* confirm_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
};
