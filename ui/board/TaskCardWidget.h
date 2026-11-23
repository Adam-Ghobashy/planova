#pragma once
#include <QFrame>
#include <QString>
#include <optional>
#include "planova/common/Dtos.h"
#include "theme/Tokens.h"

class TaskCardWidget : public QFrame {
    Q_OBJECT
public:
    TaskCardWidget(const TaskDto& task, const QString& assigneeName,
                   const QString& initials, std::optional<Token> avatarColor,
                   int pinCount = 0, QWidget* parent = nullptr);

    const Id& taskId() const { return taskId_; }

signals:
    void advanceRequested(const Id& taskId);
    void declineRequested(const Id& taskId);

private:
    Id taskId_;
};
