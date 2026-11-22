#include "EditProjectDialog.h"

#include "pins/StyledComboBox.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <exception>
#include <stdexcept>

namespace {
// Same four models as the setup page.
const char* const kModels[] = {"Agile", "Scrum", "Waterfall", "Spiral"};

QLabel* fieldLabel(const QString& text, QWidget* buddy, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("modalFieldLabel");
    label->setBuddy(buddy);
    return label;
}
}

EditProjectDialog::EditProjectDialog(const ProjectDto& project, SaveFn save, QWidget* parent)
    : ModalDialog(tr("Edit project details"), parent), save_(std::move(save))
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(8);

    name_ = new QLineEdit(QString::fromStdString(project.name), this);
    name_->setObjectName("editProjectName");
    name_->setAccessibleName(tr("Project name"));
    root->addWidget(fieldLabel(tr("Project name"), name_, this));
    root->addWidget(name_);

    team_ = new QLineEdit(QString::fromStdString(project.team), this);
    team_->setObjectName("editProjectTeam");
    team_->setAccessibleName(tr("Team"));
    root->addWidget(fieldLabel(tr("Team"), team_, this));
    root->addWidget(team_);

    // StyledComboBox keeps its object name "PinCombo": pins.qss styles it by that name.
    model_ = new planova::s5::StyledComboBox(this);
    model_->setAccessibleName(tr("Process model"));
    for (const char* model : kModels) model_->addItem(QString::fromLatin1(model));
    model_->setCurrentText(QString::fromStdString(project.seModel));
    root->addWidget(fieldLabel(tr("Process model"), model_, this));
    root->addWidget(model_);

    error_ = new QLabel(this);
    error_->setObjectName("editProjectError");
    error_->setTextFormat(Qt::PlainText);
    error_->setWordWrap(true);
    error_->hide();
    root->addWidget(error_);

    addButton(tr("Cancel"), Role::Secondary);
    saveButton_ = addButton(tr("Save"), Role::Primary);

    connect(name_, &QLineEdit::textChanged, this, &EditProjectDialog::updateSaveEnabled);
    updateSaveEnabled();
}

void EditProjectDialog::updateSaveEnabled()
{
    saveButton_->setEnabled(!name_->text().trimmed().isEmpty());
    error_->hide();   // an old message no longer matches what is typed
}

void EditProjectDialog::showError(const QString& message)
{
    error_->setText(message);
    error_->show();
}

// Closes only when saving worked. Nothing may escape: this runs from a button click.
void EditProjectDialog::accept()
{
    try {
        save_(name_->text().trimmed(), team_->text().trimmed(), model_->currentText());
    } catch (const std::invalid_argument& e) {
        showError(QString::fromUtf8(e.what()));
        return;
    } catch (const std::exception&) {
        showError(tr("The project could not be saved. Please try again."));
        return;
    }
    ModalDialog::accept();
}
