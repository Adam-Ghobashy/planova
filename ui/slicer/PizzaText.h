#pragma once
#include <QString>
#include <QStringList>
#include <string>

namespace planova::slicer {

// Legend pill text: the member's first name and a correctly pluralised task count.
inline QString legendText(const std::string& memberName, int taskCount) {
    const QStringList parts = QString::fromStdString(memberName).trimmed().split(' ', Qt::SkipEmptyParts);
    const QString first = parts.isEmpty() ? QString() : parts.first();
    return QString("%1  %2 %3").arg(first).arg(taskCount).arg(taskCount == 1 ? "task" : "tasks");
}

}
