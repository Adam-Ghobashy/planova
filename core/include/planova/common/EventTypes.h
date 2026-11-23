#pragma once

// S1 block
inline constexpr const char* ProjectCreated = "ProjectCreated";
inline constexpr const char* ProjectUpdated = "ProjectUpdated";
inline constexpr const char* ProjectDeleted = "ProjectDeleted";
// Bus notification only, never stored
inline constexpr const char* ProjectOpened  = "ProjectOpened";
// S2 block
inline constexpr const char* MemberAdded     = "MemberAdded";
inline constexpr const char* MemberUpdated   = "MemberUpdated";
inline constexpr const char* MemberRemoved   = "MemberRemoved";
inline constexpr const char* SkillRated      = "SkillRated";
inline constexpr const char* SettingChanged  = "SettingChanged";
inline constexpr const char* GitConnected    = "GitConnected";
inline constexpr const char* GitDisconnected = "GitDisconnected";
inline constexpr const char* GitSynced       = "GitSynced";
// S3 block
inline constexpr const char* TaskCreated     = "TaskCreated";
inline constexpr const char* TaskMoved       = "TaskMoved";
inline constexpr const char* TaskAssigned    = "TaskAssigned";
inline constexpr const char* TaskUnassigned  = "TaskUnassigned";
inline constexpr const char* DependencyAdded = "DependencyAdded";
// S4 block
inline constexpr const char* SlicerWeightsChanged = "SlicerWeightsChanged";
// S5 block
inline constexpr const char* PinCreated       = "PinCreated";
inline constexpr const char* PinCommentAdded  = "PinCommentAdded";
inline constexpr const char* PinResolved      = "PinResolved";
inline constexpr const char* PinEdited        = "PinEdited";
inline constexpr const char* PinMoved         = "PinMoved";
inline constexpr const char* PinRemoved       = "PinRemoved";
inline constexpr const char* ConflictRaised   = "ConflictRaised";
inline constexpr const char* ConflictResolved = "ConflictResolved";
