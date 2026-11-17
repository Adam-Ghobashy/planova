#include "team/TeamSkillsPage.h"
#include "theme/ThemeManager.h"
#include "theme/Tokens.h"
#include <QFile>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPalette>
#include <QScrollArea>
#include <QSlider>
#include <exception>
#include <numeric>

using namespace std;

namespace planova::team {

namespace {

struct SkillMeta {
    SkillCategory category;
    const char* label;
};

constexpr SkillMeta kAllSkills[] = {
    {SkillCategory::Backend, "Backend"},
    {SkillCategory::Frontend, "Frontend"},
    {SkillCategory::Storage, "Storage"},
    {SkillCategory::UiUx, "UI / UX"},
    {SkillCategory::Integration, "Integration"},
    {SkillCategory::Testing, "Testing"},
    {SkillCategory::Documentation, "Documentation"}
};

constexpr Token kMemberTokens[] = {
    Token::Member1, Token::Member2, Token::Member3,
    Token::Member4, Token::Member5, Token::Member6
};

QString initialsFor(const string& name) {
    QString qname = QString::fromStdString(name).trimmed();
    if (qname.isEmpty()) return "?";
    QStringList parts = qname.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        return (parts[0].left(1) + parts[1].left(1)).toUpper();
    }
    return qname.left(2).toUpper();
}

} // namespace

TeamSkillsPage::TeamSkillsPage(MemberService& service,
                               IMemberQuery& members,
                               CoreBridge& bridge,
                               QWidget* parent)
    : QWidget(parent), service_(service), members_(members), bridge_(bridge) {
    setObjectName("ChaosSlicerPage");

    auto& tm = ThemeManager::instance();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, tm.color(Token::Background));
    pal.setColor(QPalette::WindowText, tm.color(Token::TextPrimary));
    pal.setColor(QPalette::Base, tm.color(Token::Surface));
    pal.setColor(QPalette::AlternateBase, tm.color(Token::SurfaceAlt));
    pal.setColor(QPalette::Text, tm.color(Token::TextPrimary));
    setPalette(pal);
    setAutoFillBackground(true);

    QFile qss(":/themes/light/slicer.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString style = QString::fromUtf8(qss.readAll());
        style += R"(
            QLineEdit#lightInput {
                background-color: #FAF8F5;
                color: #1A1A1F;
                border: 1px solid #E7E2D6;
                border-radius: 8px;
                padding: 7px 12px;
                font-size: 13px;
            }
            QLineEdit#lightInput:focus {
                border: 1px solid #5438DC;
                background-color: #FFFFFF;
            }
            QCheckBox#leadCheck {
                color: #1A1A1F;
                font-size: 13px;
                font-weight: 500;
                spacing: 6px;
                background: transparent;
            }
        )";
        setStyleSheet(style);
    }

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("slicerScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* contentWidget = new QWidget(scrollArea);
    contentWidget->setObjectName("slicerScrollContent");
    auto* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(24, 20, 24, 24);
    mainLayout->setSpacing(16);

    // --- Header Row ---
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 4);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);
    auto* pageTitle = new QLabel("Team & Skills Directory", contentWidget);
    pageTitle->setObjectName("pageTitle");
    QFont titleFont = pageTitle->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    pageTitle->setFont(titleFont);

    auto* pageSubtitle = new QLabel(
        "Manage team members, project understanding levels, 7-domain skill matrices, and velocity profiles.",
        contentWidget);
    pageSubtitle->setObjectName("cardSubtitle");
    titleLayout->addWidget(pageTitle);
    titleLayout->addWidget(pageSubtitle);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();

    statusLabel_ = new QLabel(contentWidget);
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->hide();
    headerLayout->addWidget(statusLabel_);

    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    statusTimer_->setInterval(4000);
    connect(statusTimer_, &QTimer::timeout, statusLabel_, &QLabel::hide);

    auto* seedBtn = new QPushButton("Seed Demo Team", contentWidget);
    seedBtn->setObjectName("secondaryButton");
    connect(seedBtn, &QPushButton::clicked, this, &TeamSkillsPage::onSeedDemoTeam);
    headerLayout->addWidget(seedBtn);

    mainLayout->addLayout(headerLayout);

    // --- Summary Telemetry Row ---
    auto* summaryCard = new QFrame(contentWidget);
    summaryCard->setObjectName("card");
    auto* summaryLayout = new QHBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(20, 14, 20, 14);
    summaryLayout->setSpacing(24);

    auto makeMetric = [&](const QString& label, QLabel*& valueOut) {
        auto* col = new QVBoxLayout();
        col->setSpacing(2);
        auto* lbl = new QLabel(label, summaryCard);
        lbl->setObjectName("cardSubtitle");
        valueOut = new QLabel("0", summaryCard);
        valueOut->setObjectName("cardTitle");
        QFont vf = valueOut->font();
        vf.setPointSize(15);
        vf.setBold(true);
        valueOut->setFont(vf);
        col->addWidget(lbl);
        col->addWidget(valueOut);
        summaryLayout->addLayout(col, 1);
    };

    makeMetric("Total Members", totalMembersValue_);
    makeMetric("Team Lead", teamLeadValue_);
    makeMetric("Avg Understanding", avgUnderstandingValue_);
    makeMetric("Avg Velocity (EMA)", avgVelocityValue_);

    mainLayout->addWidget(summaryCard);

    // --- Add Member Card ---
    auto* addCard = new QFrame(contentWidget);
    addCard->setObjectName("card");
    auto* addCardLayout = new QVBoxLayout(addCard);
    addCardLayout->setContentsMargins(20, 16, 20, 16);
    addCardLayout->setSpacing(12);

    auto* addTitle = new QLabel("Add Team Member", addCard);
    addTitle->setObjectName("cardTitle");
    auto* addSubtitle = new QLabel(
        "Register a new member into the event log to include them in Chaos Slicer and Velocity tracking.",
        addCard);
    addSubtitle->setObjectName("cardSubtitle");
    addCardLayout->addWidget(addTitle);
    addCardLayout->addWidget(addSubtitle);

    auto* formRow = new QHBoxLayout();
    formRow->setSpacing(12);

    nameInput_ = new QLineEdit(addCard);
    nameInput_->setObjectName("lightInput");
    nameInput_->setPlaceholderText("Member Name (e.g., Adam Ghobashy)");

    roleInput_ = new QLineEdit(addCard);
    roleInput_->setObjectName("lightInput");
    roleInput_->setPlaceholderText("Role (e.g., Software Engineer, Backend, UI/UX)");

    leadCheck_ = new QCheckBox("Team Lead", addCard);
    leadCheck_->setObjectName("leadCheck");

    auto* addBtn = new QPushButton("+ Add Member", addCard);
    addBtn->setObjectName("primaryButton");
    connect(addBtn, &QPushButton::clicked, this, &TeamSkillsPage::onAddMember);
    connect(nameInput_, &QLineEdit::returnPressed, this, &TeamSkillsPage::onAddMember);
    connect(roleInput_, &QLineEdit::returnPressed, this, &TeamSkillsPage::onAddMember);

    formRow->addWidget(nameInput_, 2);
    formRow->addWidget(roleInput_, 2);
    formRow->addWidget(leadCheck_);
    formRow->addWidget(addBtn);
    addCardLayout->addLayout(formRow);

    mainLayout->addWidget(addCard);

    // --- Member Profile Cards List ---
    cardsLayout_ = new QVBoxLayout();
    cardsLayout_->setSpacing(14);
    mainLayout->addLayout(cardsLayout_);

    emptyLabel_ = new QLabel(
        "No team members registered yet. Add a member above or click 'Seed Demo Team' to populate.",
        contentWidget);
    emptyLabel_->setObjectName("emptyLabel");
    emptyLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(emptyLabel_);

    mainLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);

    connect(&bridge_, &CoreBridge::changed, this, [this](const QString&) {
        if (!updating_) refresh();
    });

    refresh();
}

void TeamSkillsPage::onAddMember() {
    const string name = nameInput_->text().trimmed().toStdString();
    string role = roleInput_->text().trimmed().toStdString();
    if (name.empty()) {
        showStatus("Please enter a member name");
        return;
    }
    if (role.empty()) {
        role = "Developer";
    }

    updating_ = true;
    try {
        service_.add(name, role, leadCheck_->isChecked());
    } catch (const exception& e) {
        updating_ = false;
        showStatus(QString("Error: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    updating_ = false;

    nameInput_->clear();
    roleInput_->clear();
    leadCheck_->setChecked(false);
    refresh();
    showStatus(QString("Added %1 to the team").arg(QString::fromStdString(name)));
}

void TeamSkillsPage::onSeedDemoTeam() {
    updating_ = true;
    try {
        struct SeedMember {
            const char* name;
            const char* role;
            bool lead;
            int understanding;
            int ratings[7];
        };
        constexpr SeedMember kSeeds[] = {
            {"Adam",   "Team & Git Architect", true,  92, {5, 4, 4, 4, 5, 5, 4}},
            {"Zeyad",  "Core & Systems Lead",  false, 90, {5, 3, 5, 3, 4, 4, 4}},
            {"Hazem",  "Slicer & Algorithms",  false, 85, {4, 4, 3, 4, 4, 5, 3}},
            {"Mai",    "UI / UX & Board Eng",  false, 88, {3, 5, 3, 5, 4, 4, 5}},
            {"Basma",  "Analytics & QA Eng",   false, 86, {4, 4, 4, 4, 3, 5, 5}}
        };

        const auto existing = members_.all();
        int addedCount = 0;
        for (const auto& s : kSeeds) {
            bool found = false;
            for (const auto& m : existing) {
                if (m.name == s.name) {
                    found = true;
                    break;
                }
            }
            if (found) continue;

            Id id = service_.add(s.name, s.role, s.lead);
            service_.update(id, s.role, s.understanding);
            for (int i = 0; i < 7; ++i) {
                service_.rate(id, kAllSkills[i].category, s.ratings[i]);
            }
            ++addedCount;
        }
        updating_ = false;
        refresh();
        showStatus(addedCount > 0
                       ? QString("Seeded %1 team members").arg(addedCount)
                       : QString("Demo team already present"));
    } catch (const exception& e) {
        updating_ = false;
        showStatus(QString("Seed failed: %1").arg(QString::fromUtf8(e.what())));
    }
}

QFrame* TeamSkillsPage::buildMemberCard(const MemberDto& member, int colorIndex) {
    auto* card = new QFrame(this);
    card->setObjectName("card");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(14);

    // --- Top Row: Avatar + Name/Role + Badges + Understanding Slider ---
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    auto& tm = ThemeManager::instance();
    const QColor avatarColor = tm.color(kMemberTokens[colorIndex % 6]);

    auto* avatar = new QLabel(initialsFor(member.name), card);
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(QString(
                              "background-color: %1; color: #FFFFFF; border-radius: 20px; font-size: 14px; font-weight: 700;"
                              ).arg(avatarColor.name()));
    topRow->addWidget(avatar);

    auto* infoCol = new QVBoxLayout();
    infoCol->setSpacing(2);

    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);
    auto* nameLbl = new QLabel(QString::fromStdString(member.name), card);
    nameLbl->setObjectName("cardTitle");
    nameRow->addWidget(nameLbl);

    if (member.isLead) {
        auto* leadBadge = new QLabel("TEAM LEAD", card);
        leadBadge->setStyleSheet(
            "background-color: #F3F0FF; color: #5438DC; border-radius: 8px; "
            "padding: 2px 8px; font-size: 11px; font-weight: 700;");
        nameRow->addWidget(leadBadge);
    }

    auto* velBadge = new QLabel(
        QString("Velocity: %1").arg(member.velocity, 0, 'f', 2), card);
    velBadge->setStyleSheet(
        "background-color: #FAF8F5; color: #1A1A1F; border: 1px solid #E7E2D6; "
        "border-radius: 8px; padding: 2px 8px; font-size: 11px; font-weight: 600;");
    nameRow->addWidget(velBadge);
    nameRow->addStretch();

    auto* roleLbl = new QLabel(QString::fromStdString(member.role), card);
    roleLbl->setObjectName("cardSubtitle");

    infoCol->addLayout(nameRow);
    infoCol->addWidget(roleLbl);
    topRow->addLayout(infoCol, 1);

    // Understanding Slider (0 - 100%)
    auto* undCol = new QVBoxLayout();
    undCol->setSpacing(4);
    auto* undHeader = new QHBoxLayout();
    auto* undTitle = new QLabel("Project Understanding", card);
    undTitle->setObjectName("cardSubtitle");
    auto* undVal = new QLabel(QString("%1%").arg(member.understanding), card);
    undVal->setObjectName("sliderValueLabel");
    undHeader->addWidget(undTitle);
    undHeader->addStretch();
    undHeader->addWidget(undVal);

    auto* undSlider = new QSlider(Qt::Horizontal, card);
    undSlider->setRange(0, 100);
    undSlider->setValue(member.understanding);
    undSlider->setFixedWidth(180);

    connect(undSlider, &QSlider::valueChanged, this, [undVal](int v) {
        undVal->setText(QString("%1%").arg(v));
    });
    connect(undSlider, &QSlider::sliderReleased, this,
            [this, id = member.id, role = member.role, undSlider]() {
                updating_ = true;
                try {
                    service_.update(id, role, undSlider->value());
                } catch (const exception& e) {
                    updating_ = false;
                    showStatus(QString::fromUtf8(e.what()));
                    return;
                }
                updating_ = false;
                refresh();
                showStatus("Updated project understanding");
            });

    undCol->addLayout(undHeader);
    undCol->addWidget(undSlider);
    topRow->addLayout(undCol);

    cardLayout->addLayout(topRow);

    // --- Divider ---
    auto* divider = new QFrame(card);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("background-color: #F5F2EA; border: none; max-height: 1px;");
    cardLayout->addWidget(divider);

    // --- 7-Domain Skill Rating Matrix ---
    auto* skillsGrid = new QGridLayout();
    skillsGrid->setHorizontalSpacing(18);
    skillsGrid->setVerticalSpacing(8);

    for (int i = 0; i < 7; ++i) {
        const auto& meta = kAllSkills[i];
        int currentRating = 3;
        auto it = member.ratings.find(meta.category);
        if (it != member.ratings.end()) {
            currentRating = it->second;
        }

        const int row = i / 4;
        const int col = i % 4;

        auto* cellWidget = new QWidget(card);
        cellWidget->setStyleSheet("background: transparent;");
        auto* cellLayout = new QHBoxLayout(cellWidget);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(4);

        auto* skillLbl = new QLabel(meta.label, cellWidget);
        skillLbl->setFixedWidth(92);
        skillLbl->setStyleSheet("color: #6B6B73; font-size: 12px; font-weight: 500;");
        cellLayout->addWidget(skillLbl);

        for (int r = 1; r <= 5; ++r) {
            auto* rateBtn = new QPushButton(QString::number(r), cellWidget);
            rateBtn->setFixedSize(24, 24);
            rateBtn->setCursor(Qt::PointingHandCursor);
            const bool active = (r <= currentRating);
            rateBtn->setStyleSheet(active
                                       ? "QPushButton { background-color: #5438DC; color: #FFFFFF; border: none; "
                                         "border-radius: 6px; padding: 0px; font-size: 11px; font-weight: 700; }"
                                         "QPushButton:hover { background-color: #4526CB; }"
                                       : "QPushButton { background-color: #FAF8F5; color: #6B6B73; border: 1px solid #E7E2D6; "
                                         "border-radius: 6px; padding: 0px; font-size: 11px; font-weight: 600; }"
                                         "QPushButton:hover { background-color: #F3F0FF; color: #5438DC; border-color: #5438DC; }");

            connect(rateBtn, &QPushButton::clicked, this,
                    [this, id = member.id, cat = meta.category, r, label = QString(meta.label)]() {
                        updating_ = true;
                        try {
                            service_.rate(id, cat, r);
                        } catch (const exception& e) {
                            updating_ = false;
                            showStatus(QString::fromUtf8(e.what()));
                            return;
                        }
                        updating_ = false;
                        refresh();
                        showStatus(QString("Rated %1: %2/5").arg(label).arg(r));
                    });
            cellLayout->addWidget(rateBtn);
        }
        cellLayout->addStretch();
        skillsGrid->addWidget(cellWidget, row, col);
    }

    cardLayout->addLayout(skillsGrid);
    return card;
}

void TeamSkillsPage::refresh() {
    while (QLayoutItem* item = cardsLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const vector<MemberDto> allMembers = members_.all();
    emptyLabel_->setVisible(allMembers.empty());

    totalMembersValue_->setText(QString::number(allMembers.size()));

    if (allMembers.empty()) {
        teamLeadValue_->setText("Unassigned");
        avgUnderstandingValue_->setText("0%");
        avgVelocityValue_->setText("1.00");
        return;
    }

    QString leadName = "Unassigned";
    double sumUnderstanding = 0.0;
    double sumVelocity = 0.0;

    for (size_t i = 0; i < allMembers.size(); ++i) {
        const auto& m = allMembers[i];
        if (m.isLead) {
            leadName = QString::fromStdString(m.name);
        }
        sumUnderstanding += m.understanding;
        sumVelocity += m.velocity;
        cardsLayout_->addWidget(buildMemberCard(m, static_cast<int>(i)));
    }

    const double count = static_cast<double>(allMembers.size());
    teamLeadValue_->setText(leadName);
    avgUnderstandingValue_->setText(
        QString("%1%").arg( qRound(sumUnderstanding / count) ));
    avgVelocityValue_->setText(
        QString::number(sumVelocity / count, 'f', 2));
}

void TeamSkillsPage::showStatus(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setVisible(!text.isEmpty());
    statusTimer_->start();
}

} // namespace planova::team