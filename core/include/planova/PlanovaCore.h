#pragma once
#include "planova/common/Clock.h"
#include "planova/common/Queries.h"
#include "planova/common/Types.h"
#include "planova/conflict/ConflictProjection.h"
#include "planova/conflict/ConflictService.h"
#include "planova/git/GitProjection.h"
#include "planova/git/GitService.h"
#include "planova/pins/PinProjection.h"
#include "planova/pins/PinService.h"
#include "planova/project/ProjectService.h"
#include "planova/project/ProjectSetup.h"
#include "planova/store/EventLog.h"
#include "planova/store/FileEventStore.h"
#include "planova/slicer/SlicerService.h"
#include "planova/slicer/WeightedScoringStrategy.h"
#include "planova/stats/HomeService.h"
#include "planova/stats/StatsService.h"
#include "planova/task/TaskProjection.h"
#include "planova/task/TaskService.h"
#include "planova/team/MemberProjection.h"
#include "planova/team/MemberService.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class SystemClock : public IClock {
public:
    int64_t nowMs() const override;

private:
    mutable int64_t last_ = 0;
};

class PlanovaCore {
public:
    explicit PlanovaCore(std::filesystem::path workspaceDir);
    // Creates the project record and its (empty) event folder.
    Id createProject(const std::string& name, const std::string& team,
                     const std::string& seModel, const std::string& idea);
    // Checks the whole setup first (std::invalid_argument, nothing written), then creates and opens the project,
    // adds and rates the members, saves the slicer weights and connects Git when a repository URL is given.
    Id setupProject(const ProjectSetup& setup);
    bool hasProjectData(const Id& projectId) const;
    void openProject(const Id& projectId);
    std::optional<Id> currentProject() const;
    // Names of event files skipped as unreadable by the latest load of the project list and the open project.
    std::vector<std::string> brokenEventFiles() const;

    ProjectService& projects();
    MemberService& members();
    const MemberProjection& memberProjection() const;
    GitService& git();
    const GitProjection& gitProjection() const;
    ChangeBus& bus();

    TaskService& tasks();
    ITaskQuery& taskQuery();
    IMemberQuery& memberQuery();
    planova::slicer::SlicerService& slicer();
    planova::s6::StatsService& stats();
    planova::s6::HomeService& home();

    planova::PinService& pins();
    planova::IPinBoardQuery& pinBoard();
    IPinQuery& pinQuery();
    planova::ConflictService& conflicts();
    // Decline-conflict state only; GitProjection also implements IConflictQuery for git merge conflicts.
    IConflictQuery& conflictQuery();
    const planova::ConflictProjection& conflictProjection() const;

private:
    std::filesystem::path projectEventsDir(const Id& projectId) const;

    // Members are initialised in this order: the workspace log must exist
    // before projects_ (ProjectService registers its projection on it).
    std::filesystem::path workspaceDir_;
    FileEventStore workspaceStore_; // project list, always at the workspace root
    EventLog workspaceLog_;
    FileEventStore store_;          // current project's events (root until a project is opened)
    EventLog log_;
    std::optional<Id> currentProject_;
    SystemClock clock_;
    ProjectService projects_;
    MemberProjection memberProjection_;
    TaskProjection taskProjection_; // before members_, which unassigns tasks when a member is removed
    MemberService members_;
    GitProjection gitProjection_;
    GitService git_;
    TaskService tasks_;
    planova::ConflictProjection conflictProjection_; // before slicer_, which skips members with open conflicts
    planova::slicer::WeightedScoringStrategy strategy_;
    planova::slicer::SlicerService slicer_;
    planova::PinProjection pinProjection_;
    planova::PinService pins_;
    planova::ConflictService conflicts_;
    planova::s6::StatsService stats_;
    planova::s6::HomeService home_;
};