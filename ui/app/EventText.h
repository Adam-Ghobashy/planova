#pragma once
#include "planova/common/EventTypes.h"
#include <QString>

// Plain wording for the status bar: turns an event type name into text a user can read.
inline QString friendlyEventText(const QString& eventType)
{
    struct Entry {
        const char* type;
        const char* text;
    };
    static const Entry kEntries[] = {
        {ProjectCreated, "Project created"},
        {ProjectUpdated, "Project updated"},
        {ProjectDeleted, "Project deleted"},
        {ProjectOpened, "Project opened"},
        {MemberAdded, "Member added"},
        {MemberUpdated, "Member updated"},
        {MemberRemoved, "Member removed"},
        {SkillRated, "Skill rating saved"},
        {SettingChanged, "Setting saved"},
        {GitConnected, "Repository connected"},
        {GitDisconnected, "Repository disconnected"},
        {GitSynced, "Repository synced"},
        {TaskCreated, "Task added"},
        {TaskMoved, "Task moved"},
        {TaskAssigned, "Task assigned"},
        {TaskUnassigned, "Task unassigned"},
        {DependencyAdded, "Dependency added"},
        {SlicerWeightsChanged, "Slicer weights saved"},
        {PinCreated, "Pin added"},
        {PinCommentAdded, "Pin comment added"},
        {PinResolved, "Pin resolved"},
        {PinEdited, "Pin edited"},
        {PinMoved, "Pin moved"},
        {PinRemoved, "Pin removed"},
        {ConflictRaised, "Conflict flagged"},
        {ConflictResolved, "Conflict resolved"},
    };
    for (const Entry& entry : kEntries) {
        if (eventType == QLatin1String(entry.type))
            return QString::fromLatin1(entry.text);
    }
    return QStringLiteral("Changes saved");
}
