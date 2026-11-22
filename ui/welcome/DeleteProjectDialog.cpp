#include "DeleteProjectDialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

DeleteProjectDialog::DeleteProjectDialog(const QString& projectName, QWidget* parent)
    : ModalDialog(tr("Delete project"), parent), projectName_(projectName)
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(10);

    auto* message = new QLabel(
        tr("This permanently deletes %1 and all its tasks, pins and team data. This cannot be undone. "
           "Type the project name to confirm.")
            .arg(projectName),
        this);
    message->setObjectName("modalMessage");
    message->setTextFormat(Qt::PlainText);   // a project name is user text, never markup
    message->setWordWrap(true);
    root->addWidget(message);

    confirm_ = new QLineEdit(this);
    confirm_->setObjectName("deleteProjectConfirm");
    confirm_->setAccessibleName(tr("Type the project name to confirm"));
    confirm_->setPlaceholderText(projectName);
    root->addWidget(confirm_);

    addButton(tr("Cancel"), Role::Secondary);
    deleteButton_ = addButton(tr("Delete"), Role::Danger);

    connect(confirm_, &QLineEdit::textChanged, this, &DeleteProjectDialog::updateDeleteEnabled);
    updateDeleteEnabled();
}

void DeleteProjectDialog::updateDeleteEnabled()
{
    deleteButton_->setEnabled(confirm_->text() == projectName_);
}
