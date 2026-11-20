#pragma once
#include "planova/common/Clock.h"
#include "planova/common/Queries.h"
#include "planova/common/Types.h"
#include "planova/git/GitProjection.h"
#include "planova/git/GitService.h"
#include "planova/project/ProjectService.h"
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

class SystemClock : public IClock {
public:
    int64_t nowMs() const override;

private:
    mutable int64_t last_ = 0;
};

class PlanovaCore {
public:
    explicit PlanovaCore(std::filesystem::path workspaceDir);
    void openProject(const Id& projectId);
    std::optional<Id> currentProject() const;

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

private:
    // until the S5 backend provides IPinQuery
    struct NoPinQuery : IPinQuery {
        int openCount() const override { return 0; }
        std::vector<PinDto> latest(int) const override { return {}; }
    };

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
    MemberService members_;
    GitProjection gitProjection_;
    GitService git_;
    TaskProjection taskProjection_;
    TaskService tasks_;
    planova::slicer::WeightedScoringStrategy strategy_;
    planova::slicer::SlicerService slicer_;
    NoPinQuery noPins_;
    planova::s6::StatsService stats_;
    planova::s6::HomeService home_;
};