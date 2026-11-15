#pragma once
#include "planova/common/Clock.h"
#include "planova/common/Types.h"
#include "planova/git/GitProjection.h"
#include "planova/git/GitService.h"
#include "planova/project/ProjectService.h"
#include "planova/store/EventLog.h"
#include "planova/store/FileEventStore.h"
#include "planova/team/MemberProjection.h"
#include "planova/team/MemberService.h"
#include <filesystem>

class SystemClock : public IClock {
public:
    int64_t nowMs() const override;
};

class PlanovaCore {
public:
    explicit PlanovaCore(std::filesystem::path workspaceDir);
    void openProject(const Id& projectId);

    ProjectService& projects();
    MemberService& members();
    const MemberProjection& memberProjection() const;
    GitService& git();
    const GitProjection& gitProjection() const;
    ChangeBus& bus();

private:
    std::filesystem::path workspaceDir_;
    FileEventStore store_;
    EventLog log_;
    SystemClock clock_;
    ProjectService projects_;
    MemberProjection memberProjection_;
    MemberService members_;
    GitProjection gitProjection_;
    GitService git_;
};