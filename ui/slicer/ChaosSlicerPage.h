#pragma once
#include "planova/common/Queries.h"
#include "planova/slicer/SlicerService.h"
#include <QWidget>
#include <vector>

class QLabel;
class QPushButton;
class QTableView;

namespace planova::slicer {

class WeightSlidersPanel;
class AssignmentTableModel;
class PizzaWidget;

class ChaosSlicerPage : public QWidget {
    Q_OBJECT
public:
    ChaosSlicerPage(planova::slicer::SlicerService& slicer,
                    ITaskQuery& tasks,
                    IMemberQuery& members,
                    QWidget* parent = nullptr);

public slots:
    void refreshFromBoard();

signals:
    // Emitted after Apply with the number of tasks whose assignee changed (0 when nothing changed).
    void assignmentsApplied(int changed);

private slots:
    void onRecalculate();
    void onApply();

private:
    bool recalculate();  // preview + table + pizza; false (with a status message) if the weights are invalid
    void fitTableHeight();
    void showStatus(const QString& text);
    planova::slicer::SlicerService& slicer_;
    ITaskQuery& tasks_;
    IMemberQuery& members_;

    WeightSlidersPanel* sliders_ = nullptr;
    PizzaWidget* pizza_ = nullptr;
    QTableView* table_ = nullptr;
    AssignmentTableModel* model_ = nullptr;
    QPushButton* recalculateBtn_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    std::vector<Assignment> preview_;
};

} // namespace planova::slicer
