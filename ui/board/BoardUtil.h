#pragma once
#include <cstddef>
#include <string>
#include <QString>
#include <QStringList>
#include "planova/common/Types.h"
#include "theme/Tokens.h"

namespace board {

inline QString skillLabel(SkillCategory c) {
    switch (c) {
    case SkillCategory::Backend:       return "BACKEND";
    case SkillCategory::Frontend:      return "FRONTEND";
    case SkillCategory::Storage:       return "STORAGE";
    case SkillCategory::UiUx:          return "UI_UX";
    case SkillCategory::Integration:   return "INTEGRATION";
    case SkillCategory::Testing:       return "TESTING";
    case SkillCategory::Documentation: return "DOCUMENTATION";
    }
    return {};
}

inline QString priorityLabel(Priority p) {
    switch (p) {
    case Priority::Low:      return "Low";
    case Priority::Medium:   return "Medium";
    case Priority::High:     return "High";
    case Priority::Critical: return "Critical";
    }
    return {};
}

inline QString statusLabel(TaskStatus s) {
    switch (s) {
    case TaskStatus::ToDo:       return "To Do";
    case TaskStatus::InProgress: return "In Progress";
    case TaskStatus::Review:     return "Review / QA";
    case TaskStatus::Done:       return "Done";
    }
    return {};
}

// ToDo -> InProgress -> Review -> Done. Returns false for Done.
inline bool nextStatus(TaskStatus from, TaskStatus& to) {
    switch (from) {
    case TaskStatus::ToDo:       to = TaskStatus::InProgress; return true;
    case TaskStatus::InProgress: to = TaskStatus::Review;     return true;
    case TaskStatus::Review:     to = TaskStatus::Done;       return true;
    default: return false;
    }
}

inline QString initialsOf(const std::string& name) {
    const QStringList parts = QString::fromStdString(name).split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return "?";
    QString s = parts.first().left(1);
    if (parts.size() > 1) s += parts.last().left(1);
    return s.toUpper();
}

// Member i gets Member1..Member6 (wraps after 6)
inline Token memberToken(std::size_t index) {
    return static_cast<Token>(static_cast<int>(Token::Member1) + static_cast<int>(index % 6));
}

} // namespace board