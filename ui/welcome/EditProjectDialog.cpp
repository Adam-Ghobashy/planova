#include "EditProjectDialog.h"

#include "pins/StyledComboBox.h"

#include <QComboBox>
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
    team_->setAccessibleName(tr("Team name"));
    root->addWidget(fieldLabel(tr("Team name"), team_, this));
    root->addWidget(team_);

    // StyledComboBox keeps its object name "PinCombo": pins.qss styles it by that name.
    model_ = new planova::s5::StyledComboBox(this);
    model_->setAccessibleName(tr("Process model"));
    for (const char* model : kModels) model_->addItem(QString::fromLatin1(model));
    // An unknown stored model shows no selection; Save stays off until one is chosen.
    model_->setPlaceholderText(tr("Choose a process model"));
    // MatchFixedString ignores case, so a stored "scrum" still selects Scrum.
    model_->setCurrentIndex(model_->findText(QString::fromStdString(project.seModel), Qt::MatchFixedString));
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
    connect(team_, &QLineEdit::textChanged, this, &EditProjectDialog::updateSaveEnabled);
    connect(model_, &QComboBox::currentIndexChanged, this, &EditProjectDialog::updateSaveEnabled);
    updateSaveEnabled();
}

void EditProjectDialog::updateSaveEnabled()
{
    saveButton_->setEnabled(!name_->text().trimmed().isEmpty() && model_->currentIndex() >= 0);
    error_->hide();   // an old message no longer matches what is entered
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
    } catch (...) {
        showError(tr("Could not save the project."));
        return;
    }
    ModalDialog::accept();
}
