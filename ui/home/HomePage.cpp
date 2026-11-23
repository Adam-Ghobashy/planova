#include "HomePage.h"

#include "../widgets/BarChartWidget.h"
#include "../widgets/DonutWidget.h"
#include "../widgets/StatCardWidget.h"
#include "../theme/ThemeManager.h"

#include <QColor>
#include <QDateTime>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetricsF>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

// Private sticky note painted on the Home pins card (home.qss colours via qproperty-*)

namespace planova::s6::detail {

class PinNote : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor noteColor READ noteColor WRITE setNoteColor)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)

public:

    explicit PinNote(QWidget* parent = nullptr);

    void setContent(const QString& tag, const QString& body, const QString& author,
        const QString& status, bool emphasiseStatus);

    void setTiltDegrees(double degrees);

    QColor noteColor() const { return m_noteColor; }
    QColor borderColor() const { return m_borderColor; }
    QColor textColor() const { return m_textColor; }
    QColor shadowColor() const { return m_shadowColor; }

    void setNoteColor(const QColor& color);
    void setBorderColor(const QColor& color);
    void setTextColor(const QColor& color);
    void setShadowColor(const QColor& color);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
private:
    void animateHover(bool hovered);
    QString m_tag;
    QString m_body;
    QString m_author;
    QString m_status;
    bool m_emphasiseStatus = false;
    double m_tilt = 0.0;
    double m_hover = 0.0;
    QVariantAnimation* m_animation;
    QColor m_noteColor;
    QColor m_borderColor;
    QColor m_textColor;
    QColor m_shadowColor;
};


namespace {

constexpr int kInset = 6;
constexpr int kPadding = 12;
// Fits the tag, three 12 px body lines and the footer inside the padding.
constexpr int kNoteHeight = 110;

constexpr qreal kHoverScale = 0.04;

constexpr int kMaxBodyLines = 3;

constexpr qreal kBodyLineHeight = 1.35;


QStringList wrapLines(const QString& text, const QFont& font, qreal width, int maxLines)
{
    QStringList lines;
    if (text.isEmpty() || width <= 0.0)
        return lines;
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(option);
    int consumed = 0;
    int lastStart = 0;
    layout.beginLayout();
    while (lines.size() < maxLines) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(width);
        lastStart = line.textStart();
        consumed = line.textStart() + line.textLength();
        lines << text.mid(line.textStart(), line.textLength()).trimmed();
    }

    layout.endLayout();
    if (consumed < text.size() && !lines.isEmpty()) {
        const QFontMetricsF metrics(font);
        lines.removeLast();
        lines << metrics.elidedText(text.mid(lastStart).simplified(), Qt::ElideRight, width);
    }

    return lines;
}
} // anonymous namespace


PinNote::PinNote(QWidget* parent)
    : QWidget(parent),
    m_animation(new QVariantAnimation(this))
{
    setObjectName("stickyNoteWidget");
    setAttribute(Qt::WA_StyledBackground, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kNoteHeight + 2 * kInset);

    // Fallback colors from palette.
    // home.qss overrides these properties.
    m_noteColor = palette().color(QPalette::Base);
    m_borderColor = palette().color(QPalette::Mid);
    m_textColor = palette().color(QPalette::WindowText);
    m_shadowColor = palette().color(QPalette::Shadow);
    m_shadowColor.setAlpha(24);

    // Hover animation
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_hover = value.toDouble();
        update();
    });
}


void PinNote::setContent(const QString& tag, const QString& body, const QString& author,
    const QString& status, bool emphasiseStatus)
{
    m_tag = tag;
    m_body = body;
    m_author = author;
    m_status = status;
    m_emphasiseStatus = emphasiseStatus;
    update();
}

void PinNote::setTiltDegrees(double degrees)
{
    m_tilt = degrees;
    update();
}


void PinNote::setNoteColor(const QColor& color)
{
    m_noteColor = color;
    update();
}

void PinNote::setBorderColor(const QColor& color)
{
    m_borderColor = color;
    update();
}

void PinNote::setTextColor(const QColor& color)
{
    m_textColor = color;
    update();
}

void PinNote::setShadowColor(const QColor& color)
{
    m_shadowColor = color;
    update();
}


QSize PinNote::sizeHint() const
{
    return QSize(150, kNoteHeight + 2 * kInset);
}

QSize PinNote::minimumSizeHint() const
{
    return QSize(110, kNoteHeight + 2 * kInset);
}


void PinNote::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
    animateHover(true);
}

void PinNote::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    animateHover(false);
}

void PinNote::animateHover(bool hovered)
{
    m_animation->stop();
    m_animation->setStartValue(m_hover);
    m_animation->setEndValue(hovered ? 1.0 : 0.0);
    m_animation->start();
}


void PinNote::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QRectF note = QRectF(rect()).adjusted(kInset, kInset, -kInset, -kInset);
    if (note.width() <= 0.0 || note.height() <= 0.0)
        return;

    // Tilt and hover scale
    painter.translate(note.center());
    painter.rotate(m_tilt * (1.0 - m_hover));
    painter.scale(1.0 + kHoverScale * m_hover, 1.0 + kHoverScale * m_hover);
    painter.translate(-note.center());

    // Shadow
    QColor shadow = m_shadowColor;
    shadow.setAlphaF(qMin(1.0, shadow.alphaF() * (1.0 + 1.5 * m_hover)));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadow);
    painter.drawRoundedRect(note.translated(0, 1.0 + 2.0 * m_hover), 10, 10);

    // Note body
    painter.setBrush(m_noteColor);
    painter.setPen(QPen(m_borderColor, 1.0));
    painter.drawRoundedRect(note.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    const QRectF content = note.adjusted(kPadding, kPadding, -kPadding, -kPadding);
    painter.setPen(m_textColor);

    // Tag
    QFont tagFont = ThemeManager::instance().textFont(TextStyle::Caption);
    tagFont.setWeight(QFont::DemiBold);
    tagFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    const QFontMetricsF tagMetrics(tagFont);
    painter.setFont(tagFont);
    painter.drawText(QRectF(content.left(), content.top(), content.width(), tagMetrics.height()),
        Qt::AlignLeft | Qt::AlignVCenter, m_tag);

    // Footer
    QFont footFont = ThemeManager::instance().textFont(TextStyle::Caption);
    const QFontMetricsF footMetrics(footFont);
    const QRectF footRect(content.left(), content.bottom() - footMetrics.height(), content.width(),
        footMetrics.height());
    painter.save();
    painter.setOpacity(0.85);
    painter.setFont(footFont);
    painter.drawText(footRect, Qt::AlignLeft | Qt::AlignVCenter, m_author);
    QFont statusFont = footFont;
    statusFont.setWeight(m_emphasiseStatus ? QFont::DemiBold : QFont::Medium);
    painter.setFont(statusFont);
    painter.drawText(footRect, Qt::AlignRight | Qt::AlignVCenter, m_status);
    painter.restore();

    // Body
    const QFont bodyFont = ThemeManager::instance().textFont(TextStyle::Caption);
    const QFontMetricsF bodyMetrics(bodyFont);
    const qreal lineStep = bodyFont.pixelSize() * kBodyLineHeight;
    painter.setFont(bodyFont);
    const QStringList lines = wrapLines(m_body, bodyFont, content.width(), kMaxBodyLines);
    qreal y = content.top() + tagMetrics.height() + 4.0;
    for (const QString& line : lines) {
        painter.drawText(QRectF(content.left(), y, content.width(), bodyMetrics.height()),
            Qt::AlignLeft | Qt::AlignVCenter, line);
        y += lineStep;
    }
}
} // namespace planova::s6::detail


namespace planova::s6 {

namespace {

// Shown until the shell calls setProjectName().

const QString& placeholderProjectName()
{
    static const QString name = QStringLiteral("Library Portal");
    return name;
}

constexpr int kMaxPins = 4;

// Reference tilts by position.

constexpr double kTilts[kMaxPins] = {
    -2.0,
    2.5,
    -1.2,
    1.8
};

QString toQString(const std::string& value)
{
    return QString::fromStdString(value);
}

QString relativeTime(std::int64_t timestamp)
{
    if (timestamp <= 0)
        return QString();
    const QDateTime created = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestamp));
    const qint64 seconds = created.secsTo(QDateTime::currentDateTime());
    if (seconds < 60)
        return QStringLiteral("Just now");
    if (seconds < 60 * 60)
        return QStringLiteral("%1m ago").arg(seconds / 60);
    if (seconds < 24 * 60 * 60)
        return QStringLiteral("%1h ago").arg(seconds / (60 * 60));
    return QStringLiteral("%1d ago").arg(seconds / (24 * 60 * 60));
}

// "Mai Khaled" -> "Mai K."

QString shortName(const QString& fullName)
{
    const QStringList parts = fullName.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return fullName.simplified();
    return parts.first()
           + QLatin1Char(' ')
           + parts.last().left(1)
           + QLatin1Char('.');
}

// Label on the sticky note, by pin type.
QString stickyTagText(PinCategory category)
{
    QString label;
    switch (category) {
    case PinCategory::HandoffNote:
        label = QStringLiteral("Handoff");
        break;
    case PinCategory::FeedbackBlocker:
        label = QStringLiteral("Blocker");
        break;
    case PinCategory::ReadyForTesting:
        label = QStringLiteral("Testing");
        break;
    case PinCategory::ArchitectureDecision:
        label = QStringLiteral("Architecture");
        break;
    }
    return QStringLiteral("\U0001F4CC ") + label;
}

// Pin status

QString statusText(PinCategory category)
{
    switch (category) {
    case PinCategory::HandoffNote:
        return QStringLiteral("Handoff");
    case PinCategory::FeedbackBlocker:
        return QStringLiteral("Alert \u26A0\uFE0F");
    case PinCategory::ReadyForTesting:
        return QStringLiteral("Ready");
    case PinCategory::ArchitectureDecision:
        return QStringLiteral("Decision");
    }

    return QString();
}

// Category property for QSS

QString categoryProperty(PinCategory category)
{
    switch (category) {
    case PinCategory::HandoffNote:
        return QStringLiteral("handoff");
    case PinCategory::FeedbackBlocker:
        return QStringLiteral("blocker");
    case PinCategory::ReadyForTesting:
        return QStringLiteral("testing");
    case PinCategory::ArchitectureDecision:
        return QStringLiteral("architecture");
    }

    return QStringLiteral("handoff");
}

// Clear layout

void clearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }

        if (QLayout* childLayout = item->layout())
            clearLayout(childLayout);
        delete item;
    }
}
} // anonymous namespace


HomePage::HomePage(HomeService& homeService, IMemberQuery& memberQuery, QWidget* parent)
    : QWidget(parent),
    m_homeService(homeService),
    m_memberQuery(memberQuery)
{
    setObjectName(QStringLiteral("homePage"));
    setAttribute(Qt::WA_StyledBackground, true);
    buildUi();
    refresh();
}


void HomePage::buildUi()
{
    // The page scrolls when the window is shorter than its content, so the window can still go down to
    // 1125 x 748 and nothing on the page gets squeezed.
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("homeScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus; the area itself would be a Tab stop with no ring
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("homeScrollContent"));
    outerLayout->addWidget(scrollArea);

    auto* pageLayout = new QVBoxLayout(content);
    pageLayout->setContentsMargins(32, 24, 32, 24);
    pageLayout->setSpacing(20);

    auto* headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);
    auto* brandLayout = new QVBoxLayout;
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(3);
    auto* eyebrow = new QLabel(QStringLiteral("ACTIVE WORKSPACE"), this);
    eyebrow->setObjectName(QStringLiteral("homeEyebrow"));
    QFont eyebrowFont = eyebrow->font();
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    eyebrow->setFont(eyebrowFont);
    m_projectTitle = new QLabel(placeholderProjectName(), this);
    m_projectTitle->setObjectName(QStringLiteral("homeTitle"));
    QFont titleFont = m_projectTitle->font();
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, -0.4);
    m_projectTitle->setFont(titleFont);
    brandLayout->addWidget(eyebrow);
    brandLayout->addWidget(m_projectTitle);
    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch(1);
    m_moreButton = new QToolButton(this);
    m_moreButton->setObjectName(QStringLiteral("homeMoreButton"));
    m_moreButton->setText(QStringLiteral("⋯"));
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    m_moreButton->setCursor(Qt::PointingHandCursor);
    m_moreButton->setFocusPolicy(Qt::StrongFocus);
    m_moreButton->setToolTip(tr("More actions"));
    m_moreButton->setAccessibleName(tr("More actions for %1").arg(m_projectTitle->text()));   // visible text is only dots
    m_moreButton->setMinimumSize(32, 28);
    auto* moreMenu = new QMenu(m_moreButton);
    QAction* editAction = moreMenu->addAction(tr("Edit details"));
    QAction* deleteAction = moreMenu->addAction(tr("Delete project"));
    m_moreButton->setMenu(moreMenu);
    // Queued so the dialog never opens inside the menu action (a refresh may rebuild this page).
    connect(editAction, &QAction::triggered, this, &HomePage::editProjectRequested, Qt::QueuedConnection);
    connect(deleteAction, &QAction::triggered, this, &HomePage::deleteProjectRequested, Qt::QueuedConnection);
    headerLayout->addWidget(m_moreButton, 0, Qt::AlignBottom);
    auto* hubButton = new QPushButton(QStringLiteral("\u2302  Projects Hub"), this);
    hubButton->setObjectName(QStringLiteral("homeHubButton"));
    hubButton->setToolTip(QStringLiteral("Switch workspace"));
    hubButton->setCursor(Qt::PointingHandCursor);
    connect(hubButton, &QPushButton::clicked, this, &HomePage::projectsHubRequested);
    headerLayout->addWidget(hubButton, 0, Qt::AlignBottom);
    auto* slicerButton = new QPushButton(QStringLiteral("\U0001F355 Chaos Slicer"), this);
    slicerButton->setObjectName(QStringLiteral("homeSlicerButton"));
    slicerButton->setCursor(Qt::PointingHandCursor);
    connect(slicerButton, &QPushButton::clicked, this, &HomePage::slicerRequested);
    headerLayout->addWidget(slicerButton, 0, Qt::AlignBottom);
    pageLayout->addLayout(headerLayout);

    auto* statsLayout = new QGridLayout;
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setHorizontalSpacing(14);
    statsLayout->setVerticalSpacing(14);
    m_totalCard = new StatCardWidget(QStringLiteral("Total Tasks"), this);
    m_completedCard = new StatCardWidget(QStringLiteral("Completed"), this);
    m_inProgressCard = new StatCardWidget(QStringLiteral("In Progress"), this);
    m_remainingCard = new StatCardWidget(QStringLiteral("Remaining"), this);
    m_totalCard->setValueRole(StatCardWidget::ValueRole::Default);
    m_completedCard->setValueRole(StatCardWidget::ValueRole::Success);
    m_inProgressCard->setValueRole(StatCardWidget::ValueRole::Accent);
    m_remainingCard->setValueRole(StatCardWidget::ValueRole::Warning);
    statsLayout->addWidget(m_totalCard, 0, 0);
    statsLayout->addWidget(m_completedCard, 0, 1);
    statsLayout->addWidget(m_inProgressCard, 0, 2);
    statsLayout->addWidget(m_remainingCard, 0, 3);
    for (int column = 0; column < 4; ++column)
        statsLayout->setColumnStretch(column, 1);
    pageLayout->addLayout(statsLayout);

    auto* middleLayout = new QHBoxLayout;
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(18);
    auto* chartsContent = new QWidget(this);
    chartsContent->setObjectName(QStringLiteral("homePanelContent"));
    auto* chartsLayout = new QVBoxLayout(chartsContent);
    chartsLayout->setContentsMargins(0, 0, 0, 0);
    chartsLayout->setSpacing(14);
    m_statusDonut = new DonutWidget(chartsContent);
    chartsLayout->addWidget(m_statusDonut);
    auto* divider = new QFrame(chartsContent);
    divider->setObjectName(QStringLiteral("homeDivider"));
    divider->setFixedHeight(1);
    chartsLayout->addWidget(divider);
    auto* contributionTitle = new QLabel(QStringLiteral("Team contribution (hours done)"),
        chartsContent);
    contributionTitle->setObjectName(QStringLiteral("homeSectionTitle"));
    chartsLayout->addWidget(contributionTitle);
    m_contributionChart = new BarChartWidget(chartsContent);
    m_contributionChart->setMinScale(20.0);
    chartsLayout->addWidget(m_contributionChart, 1);   // spare height goes to the bars, not to an empty band
    // Only sprint-based models (Agile, Scrum) show this badge; setProcessModel() decides.
    m_sprintBadge = new QLabel(QStringLiteral("Active Sprint"));
    m_sprintBadge->setObjectName(QStringLiteral("homeBadge"));
    QFrame* chartsPanel = createPanel(QStringLiteral("Task Status & Sprint Velocity"),
        chartsContent, m_sprintBadge);
    m_sprintBadge->hide();   // hidden until setProcessModel() says otherwise
    chartsPanel->setMinimumWidth(330);
    middleLayout->addWidget(chartsPanel, 23);
    m_pinsPanel = createPinsPanel();
    m_pinsPanel->setMinimumWidth(260);
    middleLayout->addWidget(m_pinsPanel, 17);
    pageLayout->addLayout(middleLayout);   // natural height: no empty filler under the panels

    auto* quickLinksLayout = new QGridLayout;
    quickLinksLayout->setContentsMargins(0, 0, 0, 0);
    quickLinksLayout->setHorizontalSpacing(14);
    quickLinksLayout->setVerticalSpacing(14);
    QPushButton* slicerLink = createQuickLink(QStringLiteral("\U0001F355"),
        QStringLiteral("Chaos Slicer Engine"),
            QStringLiteral("Interactive pizza physics & fair algorithmic assignment"),
                QStringLiteral("quickLinkSlicer"));
    QPushButton* teamLink = createQuickLink(QStringLiteral("\U0001F465"),
        QStringLiteral("Team & Skills Matrix"),
            QStringLiteral("Skill ratings, auto-match & clash resolver"),
                QStringLiteral("quickLinkTeam"));
    QPushButton* githubLink = createQuickLink(QStringLiteral("\U0001F419"),
        QStringLiteral("GitHub Integration"),
            QStringLiteral("Synced repo, commits log & active issues tracker"),
                QStringLiteral("quickLinkGithub"));
    connect(slicerLink, &QPushButton::clicked, this, &HomePage::slicerRequested);
    connect(teamLink, &QPushButton::clicked, this, &HomePage::teamSkillsRequested);
    connect(githubLink, &QPushButton::clicked, this, &HomePage::githubRequested);
    quickLinksLayout->addWidget(slicerLink, 0, 0);
    quickLinksLayout->addWidget(teamLink, 0, 1);
    quickLinksLayout->addWidget(githubLink, 0, 2);
    for (int column = 0; column < 3; ++column)
        quickLinksLayout->setColumnStretch(column, 1);
    pageLayout->addLayout(quickLinksLayout);
    pageLayout->addStretch(1);
    scrollArea->setWidget(content);
}


QFrame* HomePage::createPanel(const QString& title, QWidget* content, QWidget* headerExtra)
{
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("homePanel"));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);
    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    auto* titleLabel = new QLabel(title, panel);
    titleLabel->setObjectName(QStringLiteral("homePanelTitle"));
    header->addWidget(titleLabel);
    header->addStretch(1);
    if (headerExtra) {
        headerExtra->setParent(panel);
        header->addWidget(headerExtra);
    }

    layout->addLayout(header);
    layout->addWidget(content, 1);
    return panel;
}


QFrame* HomePage::createPinsPanel()
{
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("homePinsPanel"));
    panel->setCursor(Qt::PointingHandCursor);
    panel->setToolTip(QStringLiteral("Open Pins & Collab"));
    panel->installEventFilter(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // Header
    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(6);
    auto* title = new QLabel(QStringLiteral("\U0001F4CC Leave a Pin (Collab / Hand-off)"), panel);
    title->setObjectName(QStringLiteral("homeStickyTitle"));
    header->addWidget(title);
    header->addStretch(1);
    auto* more = new QPushButton(QStringLiteral("\u2022\u2022\u2022"), panel);
    more->setObjectName(QStringLiteral("homePinsMore"));
    more->setCursor(Qt::PointingHandCursor);
    more->setToolTip(QStringLiteral("Open Pins & Collab"));
    connect(more, &QPushButton::clicked, this, &HomePage::pinsRequested);
    header->addWidget(more);
    layout->addLayout(header);

    // Sticky Note Cluster
    auto* cluster = new QWidget(panel);
    cluster->setObjectName(QStringLiteral("homePanelContent"));
    m_pinsGrid = new QGridLayout(cluster);
    m_pinsGrid->setContentsMargins(0, 0, 0, 0);
    m_pinsGrid->setHorizontalSpacing(0);
    m_pinsGrid->setVerticalSpacing(0);
    layout->addWidget(cluster, 1);
    m_pinsEmptyLabel = new QLabel(QStringLiteral("No pins yet. Open Pins to leave the first one."), panel);
    m_pinsEmptyLabel->setObjectName(QStringLiteral("homeEmptyLabel"));
    m_pinsEmptyLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_pinsEmptyLabel, 1);

    // Footer
    auto* divider = new QFrame(panel);
    divider->setObjectName(QStringLiteral("homeDivider"));
    divider->setFixedHeight(1);
    layout->addWidget(divider);
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    m_pinsCountLabel = new QLabel(panel);
    m_pinsCountLabel->setObjectName(QStringLiteral("homePinsCount"));
    footer->addWidget(m_pinsCountLabel);
    footer->addStretch(1);
    auto* openPins = new QPushButton(QStringLiteral("Open Pins && Collab \u2192"), panel);  // "&&": "&" is a mnemonic marker
    openPins->setObjectName(QStringLiteral("homePinsLink"));
    openPins->setCursor(Qt::PointingHandCursor);
    connect(openPins, &QPushButton::clicked, this, &HomePage::pinsRequested);
    footer->addWidget(openPins);
    layout->addLayout(footer);
    return panel;
}


bool HomePage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_pinsPanel && event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton
            && m_pinsPanel->rect().contains(mouse->position().toPoint())) {
            emit pinsRequested();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}


QPushButton* HomePage::createQuickLink(const QString& icon, const QString& title,
    const QString& subtitle, const QString& linkType)
{
    auto* button = new QPushButton(this);
    button->setObjectName(QStringLiteral("quickLinkCard"));
    button->setProperty("linkType", linkType);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    button->setMinimumHeight(80);
    auto* layout = new QHBoxLayout(button);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(14);
    auto* iconLabel = new QLabel(icon, button);
    iconLabel->setObjectName(QStringLiteral("quickLinkIcon"));
    iconLabel->setFixedSize(42, 42);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto* textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);
    auto* titleLabel = new QLabel(title, button);
    titleLabel->setObjectName(QStringLiteral("quickLinkTitle"));
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto* subtitleLabel = new QLabel(subtitle, button);
    subtitleLabel->setObjectName(QStringLiteral("quickLinkSubtitle"));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    textLayout->addStretch(1);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(subtitleLabel);
    textLayout->addStretch(1);
    layout->addWidget(iconLabel, 0, Qt::AlignVCenter);
    layout->addLayout(textLayout, 1);
    return button;
}


void HomePage::setProjectName(const QString& name)
{
    const QString trimmed = name.simplified();
    m_projectTitle->setText(trimmed.isEmpty() ? placeholderProjectName() : trimmed);
    m_moreButton->setAccessibleName(tr("More actions for %1").arg(m_projectTitle->text()));
}


void HomePage::setProcessModel(const QString& model)
{
    m_sprintBadge->setVisible(model == QStringLiteral("Agile") || model == QStringLiteral("Scrum"));
}


void HomePage::refresh()
{
    const DashboardData data = m_homeService.dashboard();
    m_totalCard->setValue(QString::number(data.total));
    m_completedCard->setValue(QString::number(data.completed));
    m_inProgressCard->setValue(QString::number(data.inProgress));
    // Review is not counted in "In Progress", so it is named under the number. An empty caption hides the line.
    m_inProgressCard->setCaption(data.review > 0 ? QStringLiteral("+%1 in review").arg(data.review) : QString());
    m_remainingCard->setValue(QString::number(data.remaining));
    // The donut's "Active" segment is everything being worked on, Review included.
    m_statusDonut->setValues(data.completed, data.inProgress + data.review, data.remaining);
    updateContributionChart(data);
    updatePins(data.pins);
}


void HomePage::updateContributionChart(const DashboardData& data)
{
    const std::vector<MemberDto> members = m_memberQuery.all();
    std::vector<BarChartWidget::Bar> bars;
    bars.reserve(data.contributionHours.size());
    for (const auto& contribution : data.contributionHours) {
        const auto member = std::find_if(members.begin(), members.end(),
            [&contribution](const MemberDto& m) { return m.id == contribution.first; });
        if (member == members.end())
            continue;
        const int memberIndex = static_cast<int>(std::distance(members.begin(), member) % 6);
        bars.push_back({toQString(member->name), static_cast<double>(contribution.second),
            memberIndex});
    }

    m_contributionChart->setBars(bars, QStringLiteral("h"));
}


QWidget* HomePage::createPinNote(const PinDto& pin, const QString& authorName, int index)
{
    auto* note = new detail::PinNote;
    note->setProperty("category", categoryProperty(pin.category));
    note->setContent(stickyTagText(pin.category), toQString(pin.text), shortName(authorName),
        statusText(pin.category), pin.category == PinCategory::FeedbackBlocker);
    note->setTiltDegrees(kTilts[index % kMaxPins]);
    const QString time = relativeTime(pin.ts);
    // The note shows at most three lines, so the tooltip carries the whole text. Qt shows a tooltip with markup as
    // rich text, so the tip is always HTML and the user's text is escaped: a "<b>" in a pin shows as typed.
    QString tip = QStringLiteral("<p style=\"white-space:pre-wrap\">") + toQString(pin.text).toHtmlEscaped();
    if (!time.isEmpty()) {
        tip += QStringLiteral("<br>") + authorName.toHtmlEscaped() + QStringLiteral(" \u00B7 ") + time;
    }
    tip += QStringLiteral("</p>");
    note->setToolTip(tip);
    return note;
}


void HomePage::updatePins(const std::vector<PinDto>& pins)
{
    const std::size_t count = std::min<std::size_t>(pins.size(), kMaxPins);

    // Resolve authors and build signature.
    std::vector<QString> authors;
    authors.reserve(count);
    QString signature;
    for (std::size_t index = 0;
         index < count;
         ++index) {
        const PinDto& pin = pins[index];
        QString authorName = QStringLiteral("Unknown member");
        const auto author = m_memberQuery.byId(pin.authorId);
        if (author.has_value())
            authorName = toQString(author->name);
        authors.push_back(authorName);
        signature +=
            toQString(pin.id)
            + QLatin1Char('|')
            + QString::number(static_cast<int>(pin.category))
            + QLatin1Char('|')
            + toQString(pin.text)
            + QLatin1Char('|')
            + authorName
            + QLatin1Char('|')
            + QString::number(pin.ts / 60000)
            + QLatin1Char(';');
    }

    // Skip rebuilding if nothing changed.
    if (m_pinsBuilt && signature == m_pinsSignature) {
        return;
    }

    m_pinsBuilt = true;
    m_pinsSignature = signature;
    clearLayout(m_pinsGrid);
    m_pinsEmptyLabel->setVisible(count == 0);
    for (std::size_t index = 0;
         index < count;
         ++index) {
        QWidget* note = createPinNote(pins[index], authors[index], static_cast<int>(index));
        m_pinsGrid->addWidget(note, static_cast<int>(index / 2), static_cast<int>(index % 2));
    }

    for (int column = 0; column < 2; ++column)
        m_pinsGrid->setColumnStretch(column, 1);
    for (int row = 0; row < 2; ++row)
        m_pinsGrid->setRowStretch(row, 1);
    updatePinsCount();
}


void HomePage::setOpenPinCount(int count)
{
    m_openPinCount = count;
    updatePinsCount();
}

void HomePage::updatePinsCount()
{
    // The preview shows at most four pins; the footer counts every open pin when it is known.
    const int count = m_openPinCount >= 0 ? m_openPinCount : m_pinsGrid->count();
    m_pinsCountLabel->setText(QStringLiteral("%1 active team handoffs").arg(count));
}
} // namespace planova::s6

#include "HomePage.moc"
