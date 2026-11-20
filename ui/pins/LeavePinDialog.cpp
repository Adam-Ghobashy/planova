#include "LeavePinDialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QVBoxLayout>
#include <algorithm>

#include "PinUiHelpers.h"
#include "StyledComboBox.h"

namespace planova::s5 {

namespace {
const PinCategory kCategories[] = {PinCategory::HandoffNote, PinCategory::FeedbackBlocker,
                                   PinCategory::ReadyForTesting, PinCategory::ArchitectureDecision};

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName("DialogFieldLabel");
    return l;
}
}

LeavePinDialog::LeavePinDialog(const std::vector<TaskDto>& tasks, const std::vector<MemberDto>& members,
                               const Id& defaultAuthorId, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Leave a Pin"));
    build(tasks, members, false);
    const int idx = author_->findData(QString::fromStdString(defaultAuthorId));
    if (idx >= 0) author_->setCurrentIndex(idx);
    updateOk();
}

LeavePinDialog::LeavePinDialog(const PinView& pin, const std::vector<TaskDto>& tasks,
                               const std::vector<MemberDto>& members, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Pin"));
    build(tasks, members, true);
    author_->setCurrentIndex(std::max(0, author_->findData(QString::fromStdString(pin.pin.authorId))));
    category_->setCurrentIndex(std::max(0, category_->findData(static_cast<int>(pin.pin.category))));
    if (pin.taskId) task_->setCurrentIndex(std::max(0, task_->findData(QString::fromStdString(*pin.taskId))));
    note_->setPlainText(QString::fromStdString(pin.pin.text));
    QStringList tagTexts;
    for (const auto& t : pin.pin.tags) tagTexts << QString::fromStdString(t);
    tags_->setText(tagTexts.join(' '));
    updateOk();
}

void LeavePinDialog::build(const std::vector<TaskDto>& tasks, const std::vector<MemberDto>& members, bool edit)
{
    setObjectName("LeavePinDialog");
    setModal(true);
    setMinimumWidth(460);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(12);

    auto* title = new QLabel(edit ? tr("Edit Pin") : tr("Leave a Pin"), this);
    title->setObjectName("PinsTitle");
    root->addWidget(title);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setSpacing(10);

    author_ = new StyledComboBox(this);
    for (const auto& m : members)
        author_->addItem(QString::fromStdString(m.name), QString::fromStdString(m.id));
    author_->setEnabled(!edit);
    form->addRow(fieldLabel(tr("Author"), this), author_);

    category_ = new StyledComboBox(this);
    for (auto c : kCategories) category_->addItem(categoryTitle(c), static_cast<int>(c));
    form->addRow(fieldLabel(tr("Category"), this), category_);

    task_ = new StyledComboBox(this);
    task_->addItem(tr("None"), QString());
    for (const auto& t : tasks)
        task_->addItem(QString::fromStdString(t.title), QString::fromStdString(t.id));
    form->addRow(fieldLabel(tr("Related task"), this), task_);
    root->addLayout(form);

    note_ = new QPlainTextEdit(this);
    note_->setPlaceholderText(tr("Write context, a blocker or a decision for your team"));
    note_->setMinimumHeight(110);
    root->addWidget(fieldLabel(tr("Note"), this));
    root->addWidget(note_);
    hint_ = new QLabel(tr("A note is required."), this);
    hint_->setObjectName("DialogHint");
    root->addWidget(hint_);

    tags_ = new QLineEdit(this);
    tags_->setPlaceholderText(tr("Tags, separated by comma or space (optional)"));
    root->addWidget(fieldLabel(tr("Tags"), this));
    root->addWidget(tags_);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = new QPushButton(tr("Cancel"), this);
    ok_ = new QPushButton(edit ? tr("Save changes") : tr("Post pin"), this);
    ok_->setObjectName("PinPrimaryButton");
    ok_->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(ok_);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok_, &QPushButton::clicked, this, &QDialog::accept);
    connect(note_, &QPlainTextEdit::textChanged, this, &LeavePinDialog::updateOk);
}

void LeavePinDialog::showEvent(QShowEvent* e)
{
    QDialog::showEvent(e);
    if (fadedIn_) return;
    fadedIn_ = true;
    setWindowOpacity(0.0);
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(140);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void LeavePinDialog::updateOk()
{
    const bool has = !text().isEmpty();
    ok_->setEnabled(has);
    hint_->setVisible(!has);
}

PinCategory LeavePinDialog::category() const
{
    return static_cast<PinCategory>(category_->currentData().toInt());
}

QString LeavePinDialog::text() const { return note_->toPlainText().trimmed(); }

std::vector<std::string> LeavePinDialog::parseTags(const QString& raw)
{
    std::vector<std::string> out;
    const QStringList parts = raw.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
    for (QString p : parts) {
        while (p.startsWith('#')) p.remove(0, 1);
        p = p.trimmed().toLower();
        if (p.isEmpty()) continue;
        const std::string s = p.toStdString();
        if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
    }
    return out;
}

std::vector<std::string> LeavePinDialog::tags() const { return parseTags(tags_->text()); }

std::optional<Id> LeavePinDialog::taskId() const
{
    const QString id = task_->currentData().toString();
    if (id.isEmpty()) return std::nullopt;
    return id.toStdString();
}

Id LeavePinDialog::authorId() const { return author_->currentData().toString().toStdString(); }

}  // namespace planova::s5
