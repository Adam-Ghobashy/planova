#include "TrackingPage.h"

#include "LeaderboardModel.h"
#include "../widgets/EmptyHint.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMargins>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace planova::s6 {

namespace {

// Reference --font-mono: JetBrains Mono, Consolas, monospace.
QFont monoFont()
{
    QFont f;
    f.setFamilies({QStringLiteral("JetBrains Mono"), QStringLiteral("Consolas"),
                   QStringLiteral("Menlo"), QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("monospace")});
    return f;
}

// Reference (views.css, tracking.js): .view-container padding 24px 32px, header
// margin-bottom 20px, .metrics-grid gap 14px and margin-bottom 24px, main grid gap 20px.
constexpr int kPageMarginX = 32;
constexpr int kPageMarginY = 24;
constexpr int kHeaderGap = 20;
constexpr int kCardsGap = 14;
constexpr int kCardsToGridGap = 24;
constexpr int kGridGap = 20;
constexpr int kToggleGap = 10;
constexpr int kCardPadding = 18;
constexpr int kLeaderboardStretch = 12; // reference grid 1.2fr : 0.8fr
constexpr int kChartStretch = 8;

// Card = header frame (title, optional widget on the right) + body.
//   headerMargins: padding of the header row
//   bodyMargins:   padding of the body
struct Card {
    QFrame* frame;
    QVBoxLayout* body;
    QHBoxLayout* header;
};

Card makeCard(const QString& title, const QString& headerObjectName, const QMargins& headerMargins,
              const QMargins& bodyMargins)
{
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("TrackingCard"));

    auto* outer = new QVBoxLayout(card);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* header = new QFrame(card);
    header->setObjectName(headerObjectName);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(headerMargins);
    auto* titleLabel = new QLabel(title, header);
    titleLabel->setObjectName(QStringLiteral("TrackingCardTitle"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);
    outer->addWidget(header);

    auto* body = new QWidget(card);
    body->setObjectName(QStringLiteral("TrackingCardBody"));   // transparent in tracking.qss, so the card keeps its Surface
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(bodyMargins);
    bodyLayout->setSpacing(0);
    outer->addWidget(body, 1);

    return {card, bodyLayout, headerLayout};
}

QPushButton* makeToggleButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("TrackingToggle"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

// Canvas-style bar chart reproducing the reference drawChart(): a 360 x 260 logical
// canvas scaled to the card width, 24px square bars, 14px gap, labels at x=10,
// tracks from x=100 to width-50, value text 10px after the bar end.
// Colours are Q_PROPERTYs set from tracking.qss (QWidget#ComplexityBarChart). No colour
// literals live in C++ and the chart reads nothing from other widgets.
class ComplexityBarChart : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor valueColor READ valueColor WRITE setValueColor)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor)

public:
    struct Bar {
        QString label;
        double value = 0.0;
        QString valueText;
    };

    explicit ComplexityBarChart(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("ComplexityBarChart"));
        // Needed so QSS qproperty-* is applied to a custom QWidget.
        setAttribute(Qt::WA_StyledBackground, true);

        QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);

        // Fallbacks only (palette roles, no literals). tracking.qss overrides them.
        fill_ = palette().color(QPalette::Highlight);
        track_ = palette().color(QPalette::AlternateBase);
        value_ = palette().color(QPalette::WindowText);
        label_ = palette().color(QPalette::PlaceholderText);
    }

    void setBars(std::vector<Bar> bars)
    {
        bars_ = std::move(bars);
        updateGeometry();
        update();
    }

    QColor fillColor() const { return fill_; }
    QColor trackColor() const { return track_; }
    QColor valueColor() const { return value_; }
    QColor labelColor() const { return label_; }

    void setFillColor(const QColor& c) { fill_ = c; update(); }
    void setTrackColor(const QColor& c) { track_ = c; update(); }
    void setValueColor(const QColor& c) { value_ = c; update(); }
    void setLabelColor(const QColor& c) { label_ = c; update(); }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override
    {
        return static_cast<int>(w * logicalHeight() / kCanvasWidth);
    }
    QSize sizeHint() const override
    {
        return QSize(static_cast<int>(kCanvasWidth), static_cast<int>(logicalHeight()));
    }
    QSize minimumSizeHint() const override { return QSize(200, heightForWidth(200)); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (bars_.empty()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.scale(width() / kCanvasWidth, width() / kCanvasWidth);

        double maxValue = 0.0;
        for (const Bar& bar : bars_) {
            maxValue = std::max(maxValue, bar.value);
        }

        QFont labelFont(font());
        labelFont.setPixelSize(12);
        QFont valueFont = monoFont();
        valueFont.setWeight(QFont::DemiBold);
        valueFont.setPixelSize(12);

        const double trackWidth = kCanvasWidth - 150.0;
        double y = kStartY;
        for (const Bar& bar : bars_) {
            painter.setFont(labelFont);
            painter.setPen(label_);
            painter.drawText(QRectF(kLabelX, y, kTrackLeft - kLabelX - 4.0, kBarHeight),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QFontMetricsF(labelFont).elidedText(
                                 bar.label, Qt::ElideRight, kTrackLeft - kLabelX - 4.0));

            painter.setPen(Qt::NoPen);
            painter.setBrush(track_);
            painter.drawRect(QRectF(kTrackLeft, y, trackWidth, kBarHeight));

            // A value of 0 draws no bar (the label stays); any other value keeps at least a small stub.
            const double fill = (bar.value > 0.0 && maxValue > 0.0)
                                    ? std::max(kMinFill, bar.value / maxValue * trackWidth)
                                    : 0.0;
            if (fill > 0.0) {
                painter.setBrush(fill_);
                painter.drawRect(QRectF(kTrackLeft, y, fill, kBarHeight));
            }

            painter.setFont(valueFont);
            painter.setPen(value_);
            painter.drawText(QRectF(kTrackLeft + fill + 10.0, y, 80.0, kBarHeight),
                             Qt::AlignLeft | Qt::AlignVCenter, bar.valueText);
            y += kBarHeight + kGap;
        }
    }

private:
    static constexpr double kCanvasWidth = 360.0;
    static constexpr double kCanvasHeight = 260.0;
    static constexpr double kStartY = 20.0;
    static constexpr double kBarHeight = 24.0;
    static constexpr double kGap = 14.0;
    static constexpr double kLabelX = 10.0;
    static constexpr double kTrackLeft = 100.0;
    static constexpr double kMinFill = 12.0;

    // 260 like the reference canvas; grows only when more bars would not fit.
    double logicalHeight() const
    {
        const double needed = kStartY + static_cast<double>(bars_.size()) * (kBarHeight + kGap);
        return std::max(kCanvasHeight, needed);
    }

    std::vector<Bar> bars_;
    QColor fill_;
    QColor track_;
    QColor value_;
    QColor label_;
};

TrackingPage::TrackingPage(StatsService& stats, IMemberQuery& members, QWidget* parent)
    : QWidget(parent)
    , stats_(stats)
    , members_(members)
{
    setObjectName(QStringLiteral("TrackingPage"));
    setAttribute(Qt::WA_StyledBackground, true);

    buildUi();
    refresh();
}

void TrackingPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(kPageMarginX, kPageMarginY, kPageMarginX, kPageMarginY);
    root->setSpacing(0);

    // Header: title block left, metric toggle right, vertically centred.
    auto* header = new QHBoxLayout;
    auto* titleBlock = new QVBoxLayout;
    titleBlock->setSpacing(0);

    auto* title = new QLabel(tr("Fair Performance & Tracking"), this);
    title->setObjectName(QStringLiteral("TrackingTitle"));
    auto* subtitle = new QLabel(
        tr("Effectiveness is weighted by complexity to prevent ranking inflation from "
           "trivial tasks."),
        this);
    subtitle->setObjectName(QStringLiteral("TrackingSubtitle"));
    subtitle->setWordWrap(true);
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    header->addLayout(titleBlock, 1);

    contributionButton_ = makeToggleButton(tr("Complexity Contribution"), this);
    efficiencyButton_ = makeToggleButton(tr("Hourly Efficiency"), this);
    contributionButton_->setChecked(true);

    metricGroup_ = new QButtonGroup(this);
    metricGroup_->setExclusive(true);
    metricGroup_->addButton(contributionButton_, static_cast<int>(ChartMetric::Contribution));
    metricGroup_->addButton(efficiencyButton_, static_cast<int>(ChartMetric::Efficiency));
    connect(metricGroup_, &QButtonGroup::idClicked, this,
            [this](int id) { setMetric(static_cast<ChartMetric>(id)); });

    auto* toggleRow = new QHBoxLayout;
    toggleRow->setSpacing(kToggleGap);
    toggleRow->addWidget(contributionButton_);
    toggleRow->addWidget(efficiencyButton_);
    header->addSpacing(16);
    header->addLayout(toggleRow);
    header->setAlignment(toggleRow, Qt::AlignVCenter);
    root->addLayout(header);
    root->addSpacing(kHeaderGap);

    // Four stat cards, equal width. Each number is labelled Points or Tasks.
    overallProgressCard_ = new StatCardWidget(tr("Overall Progress"));
    complexityCard_ = new StatCardWidget(tr("Complexity Delivered"));
    teamCard_ = new StatCardWidget(tr("Active Team"));
    topPerformerCard_ = new StatCardWidget(tr("Top Performer"));

    auto* cards = new QHBoxLayout;
    cards->setSpacing(kCardsGap);
    cards->addWidget(overallProgressCard_, 1);
    cards->addWidget(complexityCard_, 1);
    cards->addWidget(teamCard_, 1);
    cards->addWidget(topPerformerCard_, 1);
    root->addLayout(cards);
    root->addSpacing(kCardsToGridGap);

    // Main row: leaderboard left (1.2fr), chart right (0.8fr).
    auto* main = new QHBoxLayout;
    main->setSpacing(kGridGap);

    // Leaderboard card: header padding 14px 18px, table edge to edge.
    const Card leaderboard = makeCard(tr("Member Effectiveness Leaderboard"),
                                      QStringLiteral("TrackingCardHeader"),
                                      QMargins(kCardPadding, 14, kCardPadding, 14),
                                      QMargins(0, 0, 0, 0));
    QFrame* leaderboardCard = leaderboard.frame;
    auto* badge = new QLabel(tr("Weighted Ranking"), leaderboardCard);
    badge->setObjectName(QStringLiteral("TrackingBadge"));
    leaderboard.header->addWidget(badge);

    model_ = new LeaderboardModel(this);
    table_ = new LeaderboardTable(leaderboardCard);
    table_->setObjectName(QStringLiteral("LeaderboardTable"));
    table_->setModel(model_);
    table_->setItemDelegateForColumn(LeaderboardModel::RankMemberColumn,
                                     new MemberCellDelegate(table_));
    table_->setItemDelegateForColumn(LeaderboardModel::RoleColumn,
                                     new TextCellDelegate(false, table_));
    table_->setItemDelegateForColumn(LeaderboardModel::DoneAssignedColumn,
                                     new TextCellDelegate(true, table_));
    table_->setItemDelegateForColumn(LeaderboardModel::PointsColumn,
                                     new PointsBarDelegate(table_));
    table_->setItemDelegateForColumn(LeaderboardModel::EfficiencyColumn,
                                     new EfficiencyChipDelegate(table_));
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionsClickable(false);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(LeaderboardModel::RoleColumn,
                                                     QHeaderView::Stretch);
    leaderboard.body->addWidget(table_, 1);
    emptyLeaderboard_ = makeEmptyHint(tr("No team members yet. Add members on Team & Skills, then finish tasks to see rankings."), leaderboardCard);
    leaderboard.body->addWidget(emptyLeaderboard_, 1);
    main->addWidget(leaderboardCard, kLeaderboardStretch);

    // Chart card: padding 18, title margin-bottom 12, chart starts right under the title (top-aligned).
    const Card chartCardParts = makeCard(tr("Complexity Distribution Chart"),
                                         QStringLiteral("TrackingCardHeaderPlain"),
                                         QMargins(kCardPadding, kCardPadding, kCardPadding, 0),
                                         QMargins(kCardPadding, 12, kCardPadding, kCardPadding));
    QFrame* chartCard = chartCardParts.frame;
    chart_ = new ComplexityBarChart(chartCard);
    chartCardParts.body->addWidget(chart_);
    emptyChart_ = makeEmptyHint(tr("Nothing to chart yet."), chartCard);
    chartCardParts.body->addWidget(emptyChart_);
    chartCardParts.body->addStretch(1);
    main->addWidget(chartCard, kChartStretch);

    root->addLayout(main, 1);
}

void TrackingPage::refresh()
{
    model_->setRows(stats_.leaderboard(), members_.all());
    const bool empty = model_->rowCount() == 0;
    table_->setVisible(!empty);
    emptyLeaderboard_->setVisible(empty);
    chart_->setVisible(!empty);
    emptyChart_->setVisible(empty);
    refreshCards();
    refreshChart();
}

void TrackingPage::refreshCards()
{
    const Progress progress = stats_.summary();
    const int memberCount = static_cast<int>(members_.all().size());

    // Empty project: pointsTotal == 0 shows 0%, no division.
    const int completionPct = progress.pointsTotal > 0
                                  ? qRound(100.0 * progress.pointsDelivered / progress.pointsTotal)
                                  : 0;

    overallProgressCard_->setValue(QStringLiteral("%1%").arg(completionPct));
    overallProgressCard_->setCaption(
        tr("%1 of %2 points").arg(progress.pointsDelivered).arg(progress.pointsTotal));

    complexityCard_->setValue(
        QStringLiteral("%1 / %2").arg(progress.pointsDelivered).arg(progress.pointsTotal));
    complexityCard_->setCaption(tr("Weighted story points"));

    teamCard_->setValue(QString::number(memberCount));
    teamCard_->setCaption(tr("Team members"));

    // Rank 1 is the top performer, but only once someone has delivered points.
    const bool hasLeader = model_->rowCount() > 0 && model_->pointsForRow(0) > 0.0;
    topPerformerCard_->setValue(hasLeader ? model_->firstNameForRow(0) : QStringLiteral("\u2014"));
    topPerformerCard_->setCaption(tr("Most points delivered"));
}

void TrackingPage::refreshChart()
{
    const bool efficiency = metric_ == ChartMetric::Efficiency;

    // Bars follow table order.
    std::vector<ComplexityBarChart::Bar> bars;
    bars.reserve(static_cast<std::size_t>(model_->rowCount()));
    for (int row = 0; row < model_->rowCount(); ++row) {
        ComplexityBarChart::Bar bar;
        bar.label = model_->firstNameForRow(row);
        bar.value = efficiency ? model_->efficiencyForRow(row) : model_->pointsForRow(row);
        bar.valueText = efficiency ? LeaderboardModel::efficiencyText(bar.value)
                                   : tr("%1 pts").arg(qRound(bar.value));
        bars.push_back(std::move(bar));
    }

    chart_->setBars(std::move(bars));
}

void TrackingPage::setMetric(ChartMetric metric)
{
    if (metric_ == metric) {
        return;
    }
    metric_ = metric;
    refreshChart(); // the toggle never touches the table or its order
}

} // namespace planova::s6

#include "TrackingPage.moc"
