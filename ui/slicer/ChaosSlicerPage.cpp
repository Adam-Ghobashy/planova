#include "slicer/ChaosSlicerPage.h"
#include "slicer/AssigneeDelegate.h"
#include "slicer/AssignmentTableModel.h"
#include "slicer/PizzaWidget.h"
#include "slicer/SlicerTableDelegate.h"
#include "planova/common/EventTypes.h"
#include "slicer/WeightSlidersPanel.h"
#include "theme/ThemeManager.h"
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>

namespace planova::slicer {

ChaosSlicerPage::ChaosSlicerPage(planova::slicer::SlicerService& slicer,
                                 ITaskQuery& tasks,
                                 IMemberQuery& members,
                                 QWidget* parent)
    : QWidget(parent), slicer_(slicer), tasks_(tasks), members_(members) {
    setObjectName("ChaosSlicerPage");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("slicerScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFocusPolicy(Qt::NoFocus);   // the controls inside take focus and the area follows them; the area itself would be a Tab stop with no ring
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* contentWidget = new QWidget(scrollArea);
    contentWidget->setObjectName("slicerScrollContent");
    auto* mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(24, 20, 24, 24);
    mainLayout->setSpacing(16);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 4);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);
    auto* pageTitle = new QLabel("Chaos Slicer Engine", contentWidget);
    pageTitle->setObjectName("pageTitle");
    pageTitle->setFont(ThemeManager::instance().textFont(TextStyle::PageTitle));

    auto* pageSubtitle = new QLabel(
        "Automated work division using weighted skill matching, workload leveling, and dependency ordering.",
        contentWidget);
    pageSubtitle->setObjectName("cardSubtitle");
    pageSubtitle->setWordWrap(true);
    titleLayout->addWidget(pageTitle);
    titleLayout->addWidget(pageSubtitle);
    headerLayout->addLayout(titleLayout, 1);  // takes the free width, so the subtitle wraps

    statusLabel_ = new QLabel(contentWidget);
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->hide();
    headerLayout->addWidget(statusLabel_);
    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    statusTimer_->setInterval(4000);
    connect(statusTimer_, &QTimer::timeout, statusLabel_, &QLabel::hide);

    recalculateBtn_ = new QPushButton("Recalculate", contentWidget);
    recalculateBtn_->setObjectName("recalculateButton");
    connect(recalculateBtn_, &QPushButton::clicked, this, &ChaosSlicerPage::onRecalculate);
    headerLayout->addWidget(recalculateBtn_);
    updateRecalculateIcon();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &ChaosSlicerPage::updateRecalculateIcon);

    applyBtn_ = new QPushButton("Apply Assignments to Board", contentWidget);
    applyBtn_->setObjectName("applyButton");
    connect(applyBtn_, &QPushButton::clicked, this, &ChaosSlicerPage::onApply);
    headerLayout->addWidget(applyBtn_);

    mainLayout->addLayout(headerLayout);

    sliders_ = new WeightSlidersPanel(contentWidget);
    sliders_->setWeights(slicer_.savedWeights());
    connect(sliders_, &WeightSlidersPanel::weightsCommitted, this, [this] { onRecalculate(); });
    mainLayout->addWidget(sliders_);

    auto* pizzaCard = new QFrame(contentWidget);
    pizzaCard->setObjectName("card");
    auto* pizzaCardLayout = new QVBoxLayout(pizzaCard);
    pizzaCardLayout->setContentsMargins(20, 16, 20, 20);
    pizzaCardLayout->setSpacing(12);

    auto* pizzaHeader = new QHBoxLayout();
    auto* pizzaTitleLayout = new QVBoxLayout();
    pizzaTitleLayout->setSpacing(2);
    auto* pizzaTitle = new QLabel("Project Pizza Slicer", pizzaCard);
    pizzaTitle->setObjectName("cardTitle");
    auto* pizzaSubtitle = new QLabel("Slicing the project scope into proportional portions per member.", pizzaCard);
    pizzaSubtitle->setObjectName("cardSubtitle");
    pizzaTitleLayout->addWidget(pizzaTitle);
    pizzaTitleLayout->addWidget(pizzaSubtitle);
    pizzaHeader->addLayout(pizzaTitleLayout);
    pizzaHeader->addStretch();

    auto* sliceBtn = new QPushButton("Slice the Project", pizzaCard);
    sliceBtn->setObjectName("sliceButton");
    connect(sliceBtn, &QPushButton::clicked, this, [this] { pizza_->sliceAnimated(); });
    pizzaHeader->addWidget(sliceBtn);
    pizzaCardLayout->addLayout(pizzaHeader);

    pizza_ = new PizzaWidget(pizzaCard);
    pizzaCardLayout->addWidget(pizza_);
    connect(pizza_, &PizzaWidget::memberSelected, this,
            [this](const QString& memberId) { model_->setHighlightedMember(memberId.toStdString()); });
    mainLayout->addWidget(pizzaCard);

    table_ = new QTableView(contentWidget);
    table_->setObjectName("assignmentsTable");
    model_ = new AssignmentTableModel(this);
    table_->setModel(model_);
    // The delegates read token colours while painting, so the view only needs a repaint.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, table_->viewport(),
            qOverload<>(&QWidget::update));
    connect(model_, &AssignmentTableModel::assignmentEdited, this, [this] {
        pizza_->setPortions(slicer_.portions(model_->currentAssignments()));
    });
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(62);
    table_->setShowGrid(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);  // narrow windows scroll instead of crushing columns
    table_->setMouseTracking(true);
    table_->setEditTriggers(QAbstractItemView::AllEditTriggers);

    table_->setItemDelegate(new SlicerTableDelegate(table_));
    assigneeDelegate_ = new AssigneeDelegate(members_.all(), table_);
    table_->setItemDelegateForColumn(AssignmentTableModel::ActionOverride, assigneeDelegate_);

    auto* header = table_->horizontalHeader();
    header->setStretchLastSection(false);
    // The two stretch columns stop shrinking at their header text (12 px semibold in slicer.qss, plus its
    // 12 px side padding), then the table scrolls.
    QFont headerFont = ThemeManager::instance().textFont(TextStyle::Caption);
    headerFont.setWeight(QFont::DemiBold);
    const QFontMetrics headerMetrics(headerFont);
    int minimumSection = 110;
    for (int column : {int(AssignmentTableModel::TaskComplexity), int(AssignmentTableModel::Rationale)}) {
        const QString text = table_->model()->headerData(column, Qt::Horizontal).toString();
        minimumSection = std::max(minimumSection, headerMetrics.horizontalAdvance(text) + 2 * 12 + 4);
    }
    header->setMinimumSectionSize(minimumSection);
    header->setSectionResizeMode(AssignmentTableModel::TaskComplexity, QHeaderView::Stretch);
    header->setSectionResizeMode(AssignmentTableModel::Skill, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(AssignmentTableModel::Priority, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(AssignmentTableModel::Assignee, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(AssignmentTableModel::Rationale, QHeaderView::Stretch);
    header->setSectionResizeMode(AssignmentTableModel::Status, QHeaderView::Fixed);
    table_->setColumnWidth(AssignmentTableModel::Status, 140);
    header->setSectionResizeMode(AssignmentTableModel::ActionOverride, QHeaderView::Fixed);
    table_->setColumnWidth(AssignmentTableModel::ActionOverride, 140);

    mainLayout->addWidget(table_);

    emptyLabel_ = new QLabel("No unassigned tasks to slice.", contentWidget);
    emptyLabel_->setObjectName("emptyLabel");
    emptyLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(emptyLabel_);

    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);

    // The scroll bar appears or disappears when the width changes; keep the table height in step.
    connect(table_->horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this] { fitTableHeight(); });

    recalculate();
}

bool ChaosSlicerPage::recalculate() {
    try {
        preview_ = slicer_.preview(sliders_->weights());
    } catch (const std::exception& e) {
        showStatus(QString::fromUtf8(e.what()));
        return false;
    }
    const std::vector<MemberDto> members = members_.all();
    assigneeDelegate_->setMembers(members);
    model_->setAssignments(preview_, tasks_.all(), members);
    fitTableHeight();
    pizza_->setPortions(slicer_.portions(preview_));
    return true;
}

void ChaosSlicerPage::onRecalculate() {
    if (recalculate()) showStatus("Recalculated");
}

void ChaosSlicerPage::onApply() {
    int changed = 0;
    applying_ = true;
    try {
        changed = slicer_.apply(model_->currentAssignments(), sliders_->weights());
    } catch (const std::exception& e) {
        applying_ = false;
        showStatus(QString("Could not apply: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    applying_ = false;
    pizza_->setPortions(slicer_.portions());
    showStatus(changed == 1 ? QString("Applied 1 assignment") : QString("Applied %1 assignments").arg(changed));
    emit assignmentsApplied(changed);
}

void ChaosSlicerPage::showStatus(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setVisible(!text.isEmpty());
    statusTimer_->start();
}

void ChaosSlicerPage::updateRecalculateIcon() {
    recalculateBtn_->setIcon(ThemeManager::instance().icon("refresh", Token::TextPrimary));
}

void ChaosSlicerPage::fitTableHeight() {
    const int rows = model_->rowCount();
    table_->setVisible(rows > 0);
    emptyLabel_->setVisible(rows == 0);
    // One line that says what to do next.
    emptyLabel_->setText(members_.all().empty()
                             ? "No team members yet. Add members on the Team & Skills page, then slice the project."
                             : "No unassigned tasks to slice. Add tasks on the Phase Board and they appear here.");
    const int rowHeight = table_->verticalHeader()->defaultSectionSize();
    // Room for the horizontal scroll bar, only while the table is too narrow for its columns.
    const QScrollBar* bar = table_->horizontalScrollBar();
    const int barHeight = bar->maximum() > 0 ? bar->sizeHint().height() : 0;
    table_->setFixedHeight(table_->horizontalHeader()->height() + rows * rowHeight + 2 + barHeight);
}

namespace {
// Only task and team changes can alter the suggested assignments. Other events (pins, git, settings)
// must not recalculate, because that would wipe the manual choices in the table.
bool changesTheSlicePlan(const QString& eventType) {
    return eventType.isEmpty()  // empty means "just re-read the board"
           || eventType == QString::fromUtf8(ProjectOpened)
           || eventType.startsWith(QLatin1String("Task"))
           || eventType == QString::fromUtf8(DependencyAdded)
           || eventType.startsWith(QLatin1String("Member"))
           || eventType.startsWith(QLatin1String("SkillRated"))
           || eventType.startsWith(QLatin1String("Conflict"));
}
}

void ChaosSlicerPage::refreshFromBoard(const QString& eventType) {
    if (applying_) return;
    if (eventType == QString::fromUtf8(ProjectOpened) || eventType == QString::fromUtf8(SlicerWeightsChanged))
        sliders_->setWeights(slicer_.savedWeights());
    if (!changesTheSlicePlan(eventType)) return;
    if (recalculate()) showStatus("Board changed: assignments recalculated");
}

}
