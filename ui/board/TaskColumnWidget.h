#pragma once
#include <QFrame>
#include "planova/common/Types.h"

class QLabel;
class QVBoxLayout;
class TaskCardWidget;

class TaskColumnWidget : public QFrame {
    Q_OBJECT
public:
    TaskColumnWidget(const QString& title, const QString& tone, QWidget* parent = nullptr);

    void clear();
    void addCard(TaskCardWidget* card);
    void setCount(int n);

signals:
    void advanceRequested(const Id& taskId);

private:
    QLabel* count_ = nullptr;
    QVBoxLayout* cards_ = nullptr;
};