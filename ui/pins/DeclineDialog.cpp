#include "DeclineDialog.h"

#include <QCoreApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <exception>

namespace planova::s5 {

DeclineDialog::DeclineDialog(const Id& taskId, const Id& memberId, const QString& taskTitle,
                             const QString& memberName, IConflictCommands& conflicts, QWidget* parent)
    : ModalDialog(QCoreApplication::translate("DeclineDialog", "Decline task"), parent),
      taskId_(taskId), memberId_(memberId), conflicts_(conflicts)
{
    QVBoxLayout* root = contentLayout();
    root->setSpacing(12);

    auto* line = new QLabel(tr("%1 declines %2").arg(memberName, taskTitle), this);
    line->setObjectName("modalSubtitle");
    line->setWordWrap(true);
    root->addWidget(line);

    auto* reasonLabel = new QLabel(tr("Reason"), this);
    reasonLabel->setObjectName("modalFieldLabel");
    root->addWidget(reasonLabel);
    reason_ = new QPlainTextEdit(this);
    reason_->setPlaceholderText(tr("Why can this task not be done by this member?"));
    reason_->setMinimumHeight(100);
    root->addWidget(reason_);
    auto* hint = new QLabel(tr("A reason is required."), this);
    hint->setObjectName("modalHint");
    root->addWidget(hint);

    addButton(tr("Cancel"), Role::Secondary);
    ok_ = addButton(tr("Decline task"), Role::Danger);
    ok_->setEnabled(false);

    connect(reason_, &QPlainTextEdit::textChanged, this, [this, hint] {
        const bool has = !reason_->toPlainText().trimmed().isEmpty();
        ok_->setEnabled(has);
        hint->setVisible(!has);
    });
}

void DeclineDialog::accept()
{
    const QString reason = reason_->toPlainText().trimmed();
    if (reason.isEmpty()) return;
    try {
        conflicts_.decline(taskId_, memberId_, reason.toStdString());
    } catch (const std::exception& e) {
        ModalDialog::info(this, tr("Decline task"), QString::fromUtf8(e.what()));
        return;  // stay open
    }

    const bool openSlicer = ModalDialog::confirm(
        this, tr("Task declined"),
        tr("Task declined. A blocker pin was added. To reassign, open the Slicer, press Recalculate, then Apply."),
        tr("Open Slicer"));
    ModalDialog::accept();
    if (openSlicer) emit openSlicerRequested();
}

}  // namespace planova::s5
