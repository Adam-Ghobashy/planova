#include "planova/PlanovaCore.h"

#include "planova/common/EventTypes.h"

#include <QDateTime>
#include <utility>

int64_t SystemClock::nowMs() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

PlanovaCore::PlanovaCore(std::filesystem::path workspaceDir)
    : workspaceDir_(std::move(workspaceDir)),
    workspaceStore_(workspaceDir_),
    workspaceLog_(workspaceStore_),
    store_(workspaceDir_),
    log_(store_),
    clock_(),
    projects_(workspaceLog_, clock_),
    memberProjection_(),
    members_(log_, clock_),
    gitProjection_(),
    git_(log_, clock_),
    taskProjection_(),
    tasks_(log_, taskProjection_, clock_),
    strategy_(),
    slicer_(taskProjection_, memberProjection_, strategy_, log_, clock_),
    noPins_(),
    stats_(taskProjection_, memberProjection_),
    home_(taskProjection_, memberProjection_, noPins_)
{
    log_.add(memberProjection_);
    log_.add(gitProjection_);
    log_.add(taskProjection_);
    // Forward workspace events (e.g. ProjectCreated) so bus() has one source.
    workspaceLog_.bus().subscribe([this](const std::string& type) { log_.bus().publish(type); });

    workspaceLog_.rebuild();
    log_.rebuild();
}

void PlanovaCore::openProject(const Id& projectId)
{
    store_.setDirectory(workspaceDir_ / projectId / "events");
    log_.rebuild();
    currentProject_ = projectId;
    // Bus notification only, never stored: tells every page to refresh.
    log_.bus().publish(ProjectOpened);
}

std::optional<Id> PlanovaCore::currentProject() const
{
    return currentProject_;
}

ProjectService& PlanovaCore::projects()
{
    return projects_;
}

MemberService& PlanovaCore::members()
{
    return members_;
}

const MemberProjection& PlanovaCore::memberProjection() const
{
    return memberProjection_;
}

GitService& PlanovaCore::git()
{
    return git_;
}

const GitProjection& PlanovaCore::gitProjection() const
{
    return gitProjection_;
}

ChangeBus& PlanovaCore::bus()
{
    return log_.bus();
}

TaskService& PlanovaCore::tasks()
{
    return tasks_;
}

ITaskQuery& PlanovaCore::taskQuery()
{
    return taskProjection_;
}

IMemberQuery& PlanovaCore::memberQuery()
{
    return memberProjection_;
}

planova::slicer::SlicerService& PlanovaCore::slicer()
{
    return slicer_;
}

planova::s6::StatsService& PlanovaCore::stats()
{
    return stats_;
}

planova::s6::HomeService& PlanovaCore::home()
{
    return home_;
}
