#include "Sidebar.h"
#include "theme/ThemeManager.h"
#include <QFontMetrics>
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
constexpr int kChevronSize = 14;
constexpr int kLogoSize = 28;
constexpr int kItemHeight = 36;
constexpr int kSwitcherHeight = 52;
// Room left for text once the icons, margins and spacing of the row are taken (sidebar width 230).
constexpr int kSwitcherTextWidth = 128;
constexpr int kGitTextWidth = 124;

// The qss selects on dynamic properties, so a style that was already applied has to be re-evaluated.
void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

// Re-polishes a widget and the labels inside it (they take their look from the parent's property).
void repolishWithLabels(QWidget* widget)
{
    repolish(widget);
    for (QLabel* child : widget->findChildren<QLabel*>())
        repolish(child);
}

// Shows `text` shortened with "..." when it is wider than `width`, with the full text as tooltip.
// The font comes from the qss, so the label is polished first to measure with the font it really uses.
void setElidedText(QLabel* label, const QString& text, int width)
{
    label->ensurePolished();
    const QString shown = label->fontMetrics().elidedText(text, Qt::ElideRight, width);
    label->setText(shown);
    label->setToolTip(shown == text ? QString() : text);
}
}

QList<Sidebar::Item> Sidebar::navItems()
{
    return {{"Project", Page::Home, "Overview", "home", Badge::None},
            {"Project", Page::Board, "Phase Board", "board", Badge::Tasks},
            {"Project", Page::Slicer, "Chaos Slicer", "slicer", Badge::None},
            {"Collaborate", Page::Pins, "Pins", "pin", Badge::OpenPins},
            {"Collaborate", Page::Team, "Team & Skills", "team", Badge::None},
            {"Insights", Page::Tracking, "Tracking", "chart", Badge::None},
            {"Insights", Page::GitHub, "GitHub", "git", Badge::None}};
}

QList<Page> Sidebar::navOrder()
{
    QList<Page> pages;
    for (const Item& item : navItems())
        pages.append(item.page);
    return pages;
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
    layout->addWidget(buildProjectSwitcher());
    // A heading starts each group of items.
    QString heading;
    for (const Item& item : navItems()) {
        if (item.heading != heading) {
            heading = item.heading;
            addHeading(layout, heading);
        }
        addItem(layout, item);
    }
    layout->addStretch();
    layout->addWidget(buildGitCard());
    layout->addWidget(buildThemeToggle());

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        updateIcons();
        updateThemeToggle();
        updateGitCard();
    });
    updateThemeToggle();
    refresh();
    setActivePage(activePage_);
}

void Sidebar::buildBrand(QVBoxLayout* layout)
{
    auto* row = new QHBoxLayout();
    row->setContentsMargins(10, 0, 10, 8);
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
    QFont font = ThemeManager::instance().font(FontRole::Display, 20);
    font.setWeight(QFont::DemiBold);
    name->setFont(font);
    row->addWidget(name, 1);

    layout->addLayout(row);
}

QWidget* Sidebar::buildProjectSwitcher()
{
    switcher_ = new QPushButton(this);
    switcher_->setObjectName("projectSwitcher");
    switcher_->setProperty("active", false);
    switcher_->setCursor(Qt::PointingHandCursor);
    switcher_->setFocusPolicy(Qt::TabFocus);
    switcher_->setToolTip("Open Projects Hub (Ctrl+0)");
    switcher_->setFixedHeight(kSwitcherHeight);

    auto* row = new QHBoxLayout(switcher_);
    row->setContentsMargins(10, 6, 10, 6);
    row->setSpacing(10);

    switcherIcon_ = new QLabel(switcher_);
    switcherIcon_->setObjectName("projectSwitcherIcon");
    switcherIcon_->setFixedSize(kIconSize, kIconSize);
    row->addWidget(switcherIcon_);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(0);
    auto* caption = new QLabel("Project", switcher_);
    caption->setObjectName("projectSwitcherCaption");
    switcherName_ = new QLabel(switcher_);
    switcherName_->setObjectName("projectSwitcherName");
    switcherName_->setTextFormat(Qt::PlainText);   // a project name is never read as markup
    textCol->addWidget(caption);
    textCol->addWidget(switcherName_);
    row->addLayout(textCol, 1);

    switcherChevron_ = new QLabel(switcher_);
    switcherChevron_->setObjectName("projectSwitcherChevron");
    switcherChevron_->setFixedSize(kChevronSize, kChevronSize);
    row->addWidget(switcherChevron_);

    // Clicks on the labels must reach the button.
    for (QLabel* label : switcher_->findChildren<QLabel*>())
        label->setAttribute(Qt::WA_TransparentForMouseEvents);

    connect(switcher_, &QPushButton::clicked, this, [this] { emit pageRequested(Page::Hub); });
    return switcher_;
}

void Sidebar::addHeading(QVBoxLayout* layout, const QString& title)
{
    auto* heading = new QLabel(title, this);
    heading->setObjectName("sidebarLabel");
    QFont font = ThemeManager::instance().textFont(TextStyle::Caption);
    font.setCapitalization(QFont::AllUppercase);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    heading->setFont(font);
    heading->setContentsMargins(10, 8, 10, 0);
    layout->addWidget(heading);
}

void Sidebar::addItem(QVBoxLayout* layout, Item item)
{
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

QWidget* Sidebar::buildGitCard()
{
    auto* gitCard = new QFrame(this);
    gitCard->setObjectName("SidebarGitCard");
    auto* gitLayout = new QHBoxLayout(gitCard);
    gitLayout->setContentsMargins(12, 6, 8, 6);
    gitLayout->setSpacing(8);

    gitDot_ = new QLabel(QStringLiteral("●"), gitCard);
    gitDot_->setObjectName("SidebarGitDot");
    gitLayout->addWidget(gitDot_);

    gitText_ = new QLabel(gitCard);
    gitText_->setObjectName("SidebarGitText");
    gitText_->setTextFormat(Qt::PlainText);        // nor is a branch name
    gitLayout->addWidget(gitText_, 1);

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
    themeToggle_->setToolTip(action + " (Ctrl+Shift+L)");
    themeToggle_->setAccessibleName(action);
}

void Sidebar::updateIcons()
{
    ThemeManager& theme = ThemeManager::instance();
    for (const Item& item : items_) {
        Token tint = Token::TextSecondary;
        if (!item.button->isEnabled())
            tint = Token::TextMuted;
        else if (item.page == activePage_)
            tint = Token::NavActiveText;
        item.icon->setPixmap(theme.icon(item.iconName, tint).pixmap(kIconSize, kIconSize));
    }

    const bool hubActive = activePage_ == Page::Hub;
    switcherIcon_->setPixmap(
        theme.icon("hub", hubActive ? Token::NavActiveText : Token::TextSecondary).pixmap(kIconSize, kIconSize));
    switcherChevron_->setPixmap(
        theme.icon("chevron-right", hubActive ? Token::NavActiveText : Token::TextMuted).pixmap(kChevronSize, kChevronSize));
}

void Sidebar::setActivePage(Page page)
{
    activePage_ = page;
    for (const Item& item : items_) {
        const bool active = item.page == page;
        item.button->setProperty("active", active);
        item.button->setAccessibleDescription(active ? "Current page" : QString());
        repolishWithLabels(item.button);
    }
    const bool hubActive = page == Page::Hub;
    switcher_->setProperty("active", hubActive);
    switcher_->setAccessibleDescription(hubActive ? "Current page" : QString());
    repolishWithLabels(switcher_);
    updateIcons();
}

void Sidebar::setProjectName(const QString& name)
{
    const QString shown = name.isEmpty() ? QStringLiteral("No project open") : name;
    setElidedText(switcherName_, shown, kSwitcherTextWidth);
    // The full name, even when the label shows it shortened. The labels on the switcher let the mouse through, so
    // their own tooltips never show: the button carries the full name.
    switcher_->setAccessibleName("Projects: " + shown);
    switcher_->setToolTip(name.isEmpty() ? QStringLiteral("Open Projects Hub (Ctrl+0)")
                                         : name + QStringLiteral(": open Projects Hub (Ctrl+0)"));
}

void Sidebar::setProjectOpen(bool open)
{
    for (int i = 0; i < items_.size(); ++i) {
        const Item& item = items_[i];
        item.button->setEnabled(open);
        // Ctrl+1 to Ctrl+7 follow the order of the items (see MainWindow).
        item.button->setToolTip(open ? QString("%1 (Ctrl+%2)").arg(item.label).arg(i + 1)
                                     : QStringLiteral("Open a project first"));
        item.button->setCursor(open ? Qt::PointingHandCursor : Qt::ArrowCursor);
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
        const QString branch = QString::fromStdString(core_.gitProjection().branch());
        const int commits = static_cast<int>(core_.gitProjection().commits().size());
        setElidedText(gitText_, QString("Synced · %1 (%2)").arg(branch).arg(commits), kGitTextWidth);
    } else {
        setElidedText(gitText_, "No repository", kGitTextWidth);
    }
    gitSyncButton_->setEnabled(connected);
    gitSyncButton_->setToolTip(connected ? "Simulate Git Sync" : "Connect a repository first (GitHub page)");
    // The glyph is tinted for the state, so a disabled button is muted and the icon follows the theme.
    gitSyncButton_->setIcon(ThemeManager::instance().icon("refresh", connected ? Token::Accent : Token::TextMuted));
}

void Sidebar::refresh()
{
    const auto current = core_.currentProject();
    QString name;
    if (current) {
        for (const ProjectDto& project : core_.projects().list()) {
            if (project.id == *current)
                name = QString::fromStdString(project.name);
        }
    }
    setProjectName(name);
    setProjectOpen(current.has_value());
    updateBadges();
    updateGitCard();
}
