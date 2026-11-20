#pragma once
#include <vector>
#include <QWidget>
#include "planova/common/Dtos.h"

class PlanovaCore;
class CoreBridge;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class TaskColumnWidget;

class PhaseBoardPage : public QWidget {
    Q_OBJECT
public:
    PhaseBoardPage(PlanovaCore& core, CoreBridge& bridge, QWidget* parent = nullptr);

private:
    void refresh();                                   // rebuild cards + apply search/member filter
    void rebuildMemberFilter(const std::vector<MemberDto>& members);
    void onAdvance(const Id& taskId);
    void onNewTask();

    PlanovaCore& core_;
    QLabel* modelValue_ = nullptr;
    QLabel* taskCount_ = nullptr;
    QLineEdit* search_ = nullptr;
    QComboBox* memberFilter_ = nullptr;
    QPushButton* newTask_ = nullptr;
    TaskColumnWidget* columns_[4] = {};
};