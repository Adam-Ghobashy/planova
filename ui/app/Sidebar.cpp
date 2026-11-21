#include "Sidebar.h"
#include "theme/ThemeManager.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <exception>

namespace {
constexpr int kIconSize = 17;
constexpr int kLogoSize = 28;
constexpr int kItemHeight = 36;

// The qss selects on dynamic properties, so a style that was already applied has to be re-evaluated.
void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}
}

Sidebar::Sidebar(PlanovaCore& core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setObjectName("Sidebar");
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(230);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 16, 12, 16);
    layout->setSpacing(6);

    buildBrand(layout);
    addSection(layout, "Main Navigation",
               {{Page::Home, "Project Home", "home", Badge::None},
                {Page::Hub, "Projects Hub", "hub", Badge::None},
                {Page::Slicer, "Chaos Slicer (Pizza)", "slicer", Badge::None}});
    addSection(layout, "Team & Collaboration",
               {{Page::Pins, "Pins & Collab", "pin", Badge::OpenPins},
                {Page::Team, "Team & Skills", "team", Badge::None},
                {Page::Board, "Phase Board", "board", Badge::Tasks},
                {Page::Tracking, "Fair Tracking & Charts", "chart", Badge::None},
                {Page::GitHub, "GitHub Integration", "git", Badge::None}});
    layout->addStretch();
    layout->addWidget(buildGitCard());
    layout->addWidget(buildThemeToggle());

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        updateIcons();
        updateThemeToggle();
        updateGitCard();
    });
    updateIcons();
    updateThemeToggle();
    refresh();
}

void Sidebar::buildBrand(QVBoxLayout* layout)
{
    auto* row = new QHBoxLayout();
    row->setContentsMargins(10, 0, 10, 12);
    row->setSpacing(10);

    auto* logo = new QLabel(this);
    logo->setObjectName("brandLogo");
    logo->setFixedSize(kLogoSize, kLogoSize);
    // The logo is a picture with its own colours. Drawn at twice the size so it stays sharp on sharp screens.
    QPixmap pixmap = QPixmap(":/icons/logo.png").scaled(kLogoSize * 2, kLogoSize * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmap.setDevicePixelRatio(2.0);
    logo->setPixmap(pixmap);
    row->addWidget(logo);

    auto* name = new QLabel("Planova", this);
    name->setObjectName("brandName");
    QFont font = ThemeManager::instance().font(FontRole::Display, 15);
    font.setBold(true);
    name->setFont(font);
    row->addWidget(name, 1);

    layout->addLayout(row);
}

void Sidebar::addSection(QVBoxLayout* layout, const QString& title, const QList<Item>& items)
{
    auto* heading = new QLabel(title, this);
    heading->setObjectName("sidebarLabel");
    QFont font = heading->font();
    font.setPixelSize(10);
    font.setBold(true);
    font.setCapitalization(QFont::AllUppercase);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    heading->setFont(font);
    heading->setContentsMargins(10, 4, 10, 0);
    layout->addWidget(heading);

    for (Item item : items) {
        auto* button = new QPushButton(this);
        button->setObjectName("navItem");
        button->setProperty("active", false);
        button->setCursor(Qt::PointingHandCursor);
        // Keyboard focus shows the ring; a mouse click does not leave one behind.
        button->setFocusPolicy(Qt::TabFocus);
        button->setAccessibleName(item.label);
        button->setFixedHeight(kItemHeight);

        // The text is a QLabel (not the button text) so "&" is shown as written and is no mnemonic.
        auto* row = new QHBoxLayout(button);
        row->setContentsMargins(10, 7, 10, 7);
        row->setSpacing(10);

        item.icon = new QLabel(button);
        item.icon->setObjectName("navIcon");
        item.icon->setFixedSize(kIconSize, kIconSize);
        row->addWidget(item.icon);

        auto* text = new QLabel(item.label, button);
        text->setObjectName("navLabel");
        row->addWidget(text, 1);

        item.badgeLabel = new QLabel(button);
        item.badgeLabel->setObjectName("navBadge");
        item.badgeLabel->setAlignment(Qt::AlignCenter);
        item.badgeLabel->hide();
        row->addWidget(item.badgeLabel);

        // Clicks on the labels must reach the button.
        for (QLabel* label : {item.icon, text, item.badgeLabel})
            label->setAttribute(Qt::WA_TransparentForMouseEvents);

        const Page page = item.page;
        connect(button, &QPushButton::clicked, this, [this, page] { emit pageRequested(page); });

        item.button = button;
        items_.append(item);
        layout->addWidget(button);
    }
}

QWidget* Sidebar::buildGitCard()
{
    auto* gitCard = new QFrame(this);
    gitCard->setObjectName("SidebarGitCard");
    auto* gitLayout = new QHBoxLayout(gitCard);
    gitLayout->setContentsMargins(12, 10, 12, 10);
    gitLayout->setSpacing(8);

    gitDot_ = new QLabel("●", gitCard);
    gitDot_->setObjectName("SidebarGitDot");
    gitLayout->addWidget(gitDot_);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(1);
    gitTitle_ = new QLabel("Synced with Git", gitCard);
    gitTitle_->setObjectName("SidebarGitTitle");
    gitTitle_->setWordWrap(true);   // "No repository connected" is longer than the card
    gitSubtitle_ = new QLabel("Peer Event Store", gitCard);
    gitSubtitle_->setObjectName("SidebarGitSubtitle");
    gitSubtitle_->setWordWrap(true);
    textCol->addWidget(gitTitle_);
    textCol->addWidget(gitSubtitle_);
    gitLayout->addLayout(textCol, 1);

    gitSyncButton_ = new QPushButton(gitCard);
    gitSyncButton_->setFixedSize(28, 28);
    gitSyncButton_->setIconSize(QSize(16, 16));
    gitSyncButton_->setCursor(Qt::PointingHandCursor);
    gitSyncButton_->setAccessibleName("Simulate Git Sync");
    gitSyncButton_->setObjectName("SidebarSyncButton");
    connect(gitSyncButton_, &QPushButton::clicked, this, [this] {
        // The button is only enabled with a saved repository (see updateGitCard).
        try {
            core_.git().simulateSync(core_.memberProjection().all());
        } catch (const std::exception&) {
            // The GitHub page shows the current state; no exception may leave a slot.
        }
        emit pageRequested(Page::GitHub);
    });
    gitLayout->addWidget(gitSyncButton_);
    return gitCard;
}

QWidget* Sidebar::buildThemeToggle()
{
    themeToggle_ = new QPushButton(this);
    themeToggle_->setObjectName("themeToggle");
    themeToggle_->setCursor(Qt::PointingHandCursor);
    themeToggle_->setFocusPolicy(Qt::TabFocus);
    themeToggle_->setIconSize(QSize(kIconSize, kIconSize));
    themeToggle_->setFixedHeight(kItemHeight);
    connect(themeToggle_, &QPushButton::clicked, this, [] { ThemeManager::instance().toggle(); });
    return themeToggle_;
}

void Sidebar::updateThemeToggle()
{
    // The button shows the mode it would switch to.
    const bool dark = ThemeManager::instance().mode() == ThemeMode::Dark;
    themeToggle_->setIcon(ThemeManager::instance().icon(dark ? "sun" : "moon", Token::TextSecondary));
    themeToggle_->setText(dark ? "Light mode" : "Dark mode");
    const QString action = dark ? "Switch to light mode" : "Switch to dark mode";
    themeToggle_->setToolTip(action);
    themeToggle_->setAccessibleName(action);
}

void Sidebar::updateIcons()
{
    for (const Item& item : items_) {
        const bool active = item.page == activePage_;
        const Token tint = active ? Token::NavActiveText : Token::TextSecondary;
        item.icon->setPixmap(ThemeManager::instance().icon(item.iconName, tint).pixmap(kIconSize, kIconSize));
    }
}

void Sidebar::setActivePage(Page page)
{
    activePage_ = page;
    for (const Item& item : items_) {
        const bool active = item.page == page;
        item.button->setProperty("active", active);
        item.button->setAccessibleDescription(active ? "Current page" : QString());
        // The labels inside take their look from the button's property.
        repolish(item.button);
        for (QLabel* child : item.button->findChildren<QLabel*>())
            repolish(child);
    }
    updateIcons();
}

int Sidebar::badgeCount(Badge badge) const
{
    switch (badge) {
    case Badge::OpenPins:
        return core_.pinQuery().openCount();
    case Badge::Tasks:
        return static_cast<int>(core_.taskQuery().all().size());
    case Badge::None:
        break;
    }
    return 0;
}

void Sidebar::updateBadges()
{
    for (const Item& item : items_) {
        const int count = badgeCount(item.badge);
        item.badgeLabel->setText(QString::number(count));
        item.badgeLabel->setVisible(count > 0);
        // The number is text, so the meaning does not rely on the badge colour.
        QString name = item.label;
        if (count > 0)
            name += item.badge == Badge::OpenPins ? QString(", %1 open").arg(count)
                                                  : QString(", %1 tasks").arg(count);
        item.button->setAccessibleName(name);
    }
}

void Sidebar::updateGitCard()
{
    // Only the saved repository counts; whatever is typed on the GitHub page is not used here.
    const bool connected = core_.gitProjection().isConnected();
    // The colour comes from the qss; the property selects it. Re-polish so it takes effect.
    gitDot_->setProperty("state", connected ? "connected" : "disconnected");
    repolish(gitDot_);
    if (connected) {
        gitTitle_->setText("Synced with Git");
        gitSubtitle_->setText(QString::fromStdString(
            "Branch: " + core_.gitProjection().branch() + " (" +
            std::to_string(core_.gitProjection().commits().size()) + " commits)"));
    } else {
        gitTitle_->setText("No repository connected");
        gitSubtitle_->setText("Connect one on GitHub Integration");
    }
    gitSyncButton_->setEnabled(connected);
    gitSyncButton_->setToolTip(connected ? "Simulate Git Sync" : "Connect a repository first (GitHub Integration)");
    // The glyph is tinted for the state, so a disabled button is muted and the icon follows the theme.
    gitSyncButton_->setIcon(ThemeManager::instance().icon("refresh", connected ? Token::Accent : Token::TextMuted));
}

void Sidebar::refresh()
{
    updateBadges();
    updateGitCard();
}
