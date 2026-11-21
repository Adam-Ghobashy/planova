#include "welcome/SetupMemberRow.h"

#include "pins/StyledComboBox.h"
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

namespace {

struct SkillInfo {
    SkillCategory category;
    const char* label;
};
// Same order as the SkillCategory enum.
const SkillInfo kSkills[7] = {
    {SkillCategory::Backend, "Backend"},         {SkillCategory::Frontend, "Frontend"},
    {SkillCategory::Storage, "Storage"},         {SkillCategory::UiUx, "UI/UX"},
    {SkillCategory::Integration, "Integration"}, {SkillCategory::Testing, "Testing"},
    {SkillCategory::Documentation, "Documentation"},
};

const char* const kRoles[] = {
    "Backend Developer", "Frontend Developer", "Full Stack Developer", "UI / UX Designer",
    "Database Architect", "QA / Testing Engineer", "Integration & DevOps", "Technical Documentation",
    "Team & Systems Lead",
};
constexpr int kDefaultRating = 3;

// Sets the "error" property that setup.qss styles, and re-applies the style.
void setError(QWidget* widget, bool error)
{
    widget->setProperty("error", error);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

SetupMemberRow::SetupMemberRow(int id, QWidget* parent)
    : QFrame(parent)
{
    setObjectName("memberRow");
    const QString suffix = QString::number(id);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 14);
    layout->setSpacing(10);

    // title row: "Member 1" and the Remove button (moved last in the tab order below)
    title_ = new QLabel(this);
    title_->setObjectName("memberTitle");
    removeButton_ = new QPushButton(tr("Remove"), this);
    removeButton_->setObjectName("memberRemove_" + suffix);
    removeButton_->setProperty("kind", "ghost");
    removeButton_->setCursor(Qt::PointingHandCursor);
    connect(removeButton_, &QPushButton::clicked, this, [this] { emit removeRequested(this); });
    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(title_);
    titleRow->addStretch(1);
    titleRow->addWidget(removeButton_);
    layout->addLayout(titleRow);

    // name, role, lead
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName("memberNameEdit_" + suffix);
    nameEdit_->setPlaceholderText(tr("Full name"));
    auto* nameLabel = new QLabel(tr("Name"), this);
    nameLabel->setBuddy(nameEdit_);
    nameLabel->setObjectName("fieldLabel");
    nameHint_ = new QLabel(this);
    nameHint_->setObjectName("fieldHint");
    nameHint_->setTextFormat(Qt::PlainText);

    roleCombo_ = new planova::s5::StyledComboBox(this);
    roleCombo_->setObjectName("memberRoleCombo_" + suffix);
    for (const char* role : kRoles)
        roleCombo_->addItem(QString::fromUtf8(role));
    roleCombo_->addItem(tr("Other..."));
    auto* roleLabel = new QLabel(tr("Role"), this);
    roleLabel->setBuddy(roleCombo_);
    roleLabel->setObjectName("fieldLabel");
    roleEdit_ = new QLineEdit(this);
    roleEdit_->setObjectName("memberRoleEdit_" + suffix);
    roleEdit_->setPlaceholderText(tr("Write a custom role"));
    roleEdit_->setAccessibleName(tr("Custom role"));
    roleEdit_->hide();

    // A toggle button, not a check box: its checked state shows a check mark and a filled background.
    leadCheck_ = new QPushButton(tr("Team lead"), this);
    leadCheck_->setCheckable(true);
    leadCheck_->setProperty("kind", "chip");
    leadCheck_->setCursor(Qt::PointingHandCursor);
    leadCheck_->setObjectName("memberLead_" + suffix);

    auto* fields = new QGridLayout;
    fields->setHorizontalSpacing(12);
    fields->setVerticalSpacing(4);
    fields->addWidget(nameLabel, 0, 0);
    fields->addWidget(roleLabel, 0, 1);
    fields->addWidget(nameEdit_, 1, 0);
    fields->addWidget(roleCombo_, 1, 1);
    fields->addWidget(roleEdit_, 1, 2);
    fields->addWidget(leadCheck_, 1, 3);
    fields->addWidget(nameHint_, 2, 0);
    fields->setColumnStretch(0, 3);
    fields->setColumnStretch(1, 2);
    fields->setColumnStretch(2, 2);
    layout->addLayout(fields);

    // skills: label, slider and "3/5" in a grid of four columns
    auto* skills = new QGridLayout;
    skills->setHorizontalSpacing(18);
    skills->setVerticalSpacing(6);
    for (int i = 0; i < 7; ++i) {
        auto* label = new QLabel(tr(kSkills[i].label), this);
        label->setObjectName("skillLabel");
        sliders_[i] = new QSlider(Qt::Horizontal, this);
        sliders_[i]->setObjectName(QString("skillSlider_%1_%2").arg(id).arg(i));
        sliders_[i]->setRange(1, 5);
        sliders_[i]->setPageStep(1);
        sliders_[i]->setValue(kDefaultRating);
        sliders_[i]->setMinimumHeight(24);   // keeps the target at least 24 px
        label->setBuddy(sliders_[i]);
        values_[i] = new QLabel(QString("%1/5").arg(kDefaultRating), this);
        values_[i]->setObjectName("skillValue");
        values_[i]->setMinimumWidth(30);
        connect(sliders_[i], &QSlider::valueChanged, this, [this, i](int value) {
            values_[i]->setText(QString("%1/5").arg(value));
            emit changed();
        });

        auto* cell = new QHBoxLayout;
        cell->setSpacing(6);
        cell->addWidget(label);
        cell->addWidget(sliders_[i], 1);
        cell->addWidget(values_[i]);
        skills->addLayout(cell, i / 4, i % 4);
    }
    layout->addLayout(skills);

    connect(nameEdit_, &QLineEdit::textChanged, this, [this] { updateNameHint(); emit changed(); });
    connect(nameEdit_, &QLineEdit::editingFinished, this, [this] { nameTouched_ = true; updateNameHint(); });
    connect(roleCombo_, &QComboBox::currentIndexChanged, this, [this] { updateRoleEdit(); emit changed(); });
    connect(roleEdit_, &QLineEdit::textChanged, this, &SetupMemberRow::changed);
    connect(leadCheck_, &QPushButton::toggled, this, [this](bool lead) {
        leadCheck_->setText(lead ? tr("✓ Team lead") : tr("Team lead"));
        emit changed();
    });

    // Remove comes after the skill sliders when tabbing, although it sits at the top.
    setTabOrder(sliders_[6], removeButton_);
    setNumber(1);
    updateNameHint();
}

void SetupMemberRow::setNumber(int number)
{
    title_->setText(tr("Member %1").arg(number));
    nameEdit_->setAccessibleName(tr("Name of member %1").arg(number));
    roleCombo_->setAccessibleName(tr("Role of member %1").arg(number));
    leadCheck_->setAccessibleName(tr("Member %1 is the team lead").arg(number));
    removeButton_->setAccessibleName(tr("Remove member %1").arg(number));
    for (int i = 0; i < 7; ++i)
        sliders_[i]->setAccessibleName(tr("%1 skill rating of member %2").arg(tr(kSkills[i].label)).arg(number));
}

SetupMember SetupMemberRow::member() const
{
    SetupMember result;
    result.name = nameEdit_->text().trimmed().toStdString();
    QString role = roleCombo_->currentText();
    if (roleCombo_->currentIndex() == roleCombo_->count() - 1)   // the "Other..." entry
        role = roleEdit_->text().trimmed();
    result.role = (role.isEmpty() ? tr("Developer") : role).toStdString();
    result.isLead = leadCheck_->isChecked();
    for (int i = 0; i < 7; ++i)
        result.ratings[kSkills[i].category] = sliders_[i]->value();
    return result;
}

bool SetupMemberRow::hasName() const
{
    return !nameEdit_->text().trimmed().isEmpty();
}

void SetupMemberRow::updateNameHint()
{
    const bool empty = !hasName();
    nameHint_->setText(empty ? tr("Enter a name for this member.") : QString());
    nameHint_->setVisible(empty);
    // Neutral guidance first; red only after the user left the box empty.
    setError(nameHint_, empty && nameTouched_);
    setError(nameEdit_, empty && nameTouched_);
}

void SetupMemberRow::updateRoleEdit()
{
    const bool other = roleCombo_->currentIndex() == roleCombo_->count() - 1;
    roleEdit_->setVisible(other);
    if (other)
        roleEdit_->setFocus();
}
