#include "slicer/ChaosSlicerPage.h"
#include "slicer/AssigneeDelegate.h"
#include "slicer/AssignmentTableModel.h"
#include "slicer/PizzaWidget.h"
#include "slicer/SlicerTableDelegate.h"
#include "slicer/WeightSlidersPanel.h"
#include "theme/ThemeManager.h"
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <stdexcept>

namespace planova::slicer {

ChaosSlicerPage::ChaosSlicerPage(planova::slicer::SlicerService& slicer,
                                 ITaskQuery& tasks,
                                 IMemberQuery& members,
                                 QWidget* parent)
    : QWidget(parent), slicer_(slicer), tasks_(tasks), members_(members) {
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
        setStyleSheet(QString::fromUtf8(qss.readAll()));
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

    // --- 1. Top action header ---
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 4);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);
    auto* pageTitle = new QLabel("Chaos Slicer Engine", contentWidget);
    pageTitle->setObjectName("pageTitle");
    QFont titleFont = pageTitle->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    pageTitle->setFont(titleFont);

    auto* pageSubtitle = new QLabel(
        "Automated work division using weighted skill matching, workload leveling, and dependency ordering.",
        contentWidget);
    pageSubtitle->setObjectName("cardSubtitle");
    titleLayout->addWidget(pageTitle);
    titleLayout->addWidget(pageSubtitle);
    headerLayout->addLayout(titleLayout);

    headerLayout->addStretch();

    // Status message as a small pill beside the buttons, so showing it never moves the page.
    statusLabel_ = new QLabel(contentWidget);
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->hide();
    headerLayout->addWidget(statusLabel_);
    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    statusTimer_->setInterval(4000);
    connect(statusTimer_, &QTimer::timeout, statusLabel_, &QLabel::hide);

    recalculateBtn_ = new QPushButton("Recalculate", contentWidget);
    recalculateBtn_->setObjectName("recalculateButton");  // styled by slicer.qss
    connect(recalculateBtn_, &QPushButton::clicked, this, &ChaosSlicerPage::onRecalculate);
    headerLayout->addWidget(recalculateBtn_);

    applyBtn_ = new QPushButton("Apply Assignments to Board", contentWidget);
    applyBtn_->setObjectName("applyButton");  // styled by slicer.qss
    connect(applyBtn_, &QPushButton::clicked, this, &ChaosSlicerPage::onApply);
    headerLayout->addWidget(applyBtn_);

    mainLayout->addLayout(headerLayout);

    // --- 2. Card 1: Assignment Strategy Weights ---
    sliders_ = new WeightSlidersPanel(contentWidget);
    connect(sliders_, &WeightSlidersPanel::weightsCommitted, this, [this] { onRecalculate(); });
    mainLayout->addWidget(sliders_);

    // --- 3. Card 2: Project Pizza Slicer ---
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
    sliceBtn->setObjectName("sliceButton");  // styled like the primary button by slicer.qss
    connect(sliceBtn, &QPushButton::clicked, this, [this] { pizza_->sliceAnimated(); });
    pizzaHeader->addWidget(sliceBtn);
    pizzaCardLayout->addLayout(pizzaHeader);

    pizza_ = new PizzaWidget(pizzaCard);
    pizzaCardLayout->addWidget(pizza_);
    // Picking a member on the pizza highlights that member's table rows.
    connect(pizza_, &PizzaWidget::memberSelected, this,
            [this](const QString& memberId) { model_->setHighlightedMember(memberId.toStdString()); });
    mainLayout->addWidget(pizzaCard);

    // --- 4. Card 3: Assignments Table ---
    table_ = new QTableView(contentWidget);
    table_->setObjectName("assignmentsTable");
    model_ = new AssignmentTableModel(this);
    table_->setModel(model_);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(62);
    table_->setShowGrid(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    // The page scrolls, not the table: its height follows its rows (see fitTableHeight).
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setMouseTracking(true);  // row hover highlight
    table_->setEditTriggers(QAbstractItemView::AllEditTriggers);  // only the override column is editable

    table_->setItemDelegate(new SlicerTableDelegate(table_));
    auto allMembers = members_.all();
    table_->setItemDelegateForColumn(AssignmentTableModel::ActionOverride,
                                     new AssigneeDelegate(allMembers, table_));

    auto* header = table_->horizontalHeader();
    header->setStretchLastSection(false);
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

    recalculate();  // first preview, without a status message
}

bool ChaosSlicerPage::recalculate() {
    try {
        preview_ = slicer_.preview(sliders_->weights());
    } catch (const std::invalid_argument& e) {
        showStatus(QString::fromUtf8(e.what()));
        return false;
    }
    model_->setAssignments(preview_, tasks_.all(), members_.all());
    fitTableHeight();
    pizza_->setPortions(slicer_.portions(preview_));
    // The pizza stays whole until the user slices it. Once sliced, new weights cut it again.
    if (pizza_->isSliced()) pizza_->sliceAnimated();
    return true;
}

void ChaosSlicerPage::onRecalculate() {
    if (recalculate()) showStatus("Recalculated");
}

void ChaosSlicerPage::onApply() {
    int changed = 0;
    try {
        changed = slicer_.apply(model_->currentAssignments(), sliders_->weights());
    } catch (const std::exception& e) {
        // Invalid weights, or the event store could not write (disk full, file locked).
        // An exception must never leave a slot, so show it instead.
        showStatus(QString("Could not apply: %1").arg(QString::fromUtf8(e.what())));
        return;
    }
    refreshFromBoard();
    showStatus(changed == 1 ? QString("Applied 1 assignment") : QString("Applied %1 assignments").arg(changed));
    emit assignmentsApplied(changed);
}

void ChaosSlicerPage::showStatus(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setVisible(!text.isEmpty());
    statusTimer_->start();  // hides the message again after a few seconds
}

void ChaosSlicerPage::fitTableHeight() {
    const int rows = model_->rowCount();
    table_->setVisible(rows > 0);
    emptyLabel_->setVisible(rows == 0);
    const int rowHeight = table_->verticalHeader()->defaultSectionSize();
    table_->setFixedHeight(table_->horizontalHeader()->height() + rows * rowHeight + 2);
}

void ChaosSlicerPage::refreshFromBoard() {
    pizza_->setPortions(slicer_.portions());
}

} // namespace planova::slicer
