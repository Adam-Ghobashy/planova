#pragma once
#include <QFrame>
#include <QString>
#include "planova/common/Dtos.h"
#include "theme/Tokens.h"

class TaskCardWidget : public QFrame {
    Q_OBJECT
public:
    TaskCardWidget(const TaskDto& task, const QString& assigneeName,
                   const QString& initials, Token avatarColor,
                   int pinCount = 0, QWidget* parent = nullptr);

signals:
    void advanceRequested(const Id& taskId);

private:
    Id taskId_;
};