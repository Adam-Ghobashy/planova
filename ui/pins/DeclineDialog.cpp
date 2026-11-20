#include "DeclineDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <exception>

namespace planova::s5 {

DeclineDialog::DeclineDialog(const Id& taskId, const Id& memberId, const QString& taskTitle,
                             const QString& memberName, IConflictCommands& conflicts, QWidget* parent)
    : QDialog(parent), taskId_(taskId), memberId_(memberId), conflicts_(conflicts)
{
    setObjectName("DeclineDialog");
    setWindowTitle(tr("Decline task"));
    setModal(true);
    setMinimumWidth(440);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(12);

    auto* title = new QLabel(tr("Decline task"), this);
    title->setObjectName("PinsTitle");
    auto* line = new QLabel(tr("%1 declines %2").arg(memberName, taskTitle), this);
    line->setObjectName("PinsSubtitle");
    line->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(line);

    root->addWidget(new QLabel(tr("Reason"), this));
    reason_ = new QPlainTextEdit(this);
    reason_->setPlaceholderText(tr("Why can this task not be done by this member?"));
    reason_->setMinimumHeight(100);
    root->addWidget(reason_);
    auto* hint = new QLabel(tr("A reason is required."), this);
    hint->setObjectName("DialogHint");
    root->addWidget(hint);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"), this);
    ok_ = new QPushButton(tr("Decline task"), this);
    ok_->setObjectName("DangerButton");
    ok_->setEnabled(false);
    buttons->addWidget(cancel);
    buttons->addWidget(ok_);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok_, &QPushButton::clicked, this, &DeclineDialog::accept);
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
        QMessageBox::warning(this, tr("Decline task"), QString::fromUtf8(e.what()));
        return;  // stay open
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Task declined"));
    box.setText(tr("Task declined. A blocker pin was added. To reassign, open the Slicer, "
                   "press Recalculate, then Apply."));
    QPushButton* openSlicer = box.addButton(tr("Open Slicer"), QMessageBox::AcceptRole);
    box.addButton(tr("Close"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == openSlicer) emit openSlicerRequested();
    QDialog::accept();
}

}  // namespace planova::s5
