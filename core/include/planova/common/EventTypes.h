#pragma once

// S1 block
inline constexpr const char* ProjectCreated = "ProjectCreated";
inline constexpr const char* ProjectUpdated = "ProjectUpdated";
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
