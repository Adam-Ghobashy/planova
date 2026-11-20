#include "planova/PlanovaCore.h"

#include "planova/common/EventTypes.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <utility>

int64_t SystemClock::nowMs() const
{
    // Never repeat a millisecond: event order is (ts, id), so two events written
    // in the same ms could otherwise replay in hash order instead of write order.
    last_ = std::max(QDateTime::currentMSecsSinceEpoch(), last_ + 1);
    return last_;
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

std::filesystem::path PlanovaCore::projectEventsDir(const Id& projectId) const
{
    return workspaceDir_ / projectId / "events";
}

Id PlanovaCore::createProject(const std::string& name, const std::string& team,
                              const std::string& seModel, const std::string& idea)
{
    const Id id = projects_.create(name, team, seModel, idea);
    std::filesystem::create_directories(projectEventsDir(id));
    return id;
}

bool PlanovaCore::hasProjectData(const Id& projectId) const
{
    return std::filesystem::is_directory(projectEventsDir(projectId));
}

void PlanovaCore::openProject(const Id& projectId)
{
    const auto dir = projectEventsDir(projectId);
    if (!std::filesystem::is_directory(dir)) {
        // One-time import of data written by the pre-project app (workspace root).
        std::filesystem::create_directories(dir);
        for (const auto& entry : std::filesystem::directory_iterator(workspaceDir_)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;
            QFile file(QString::fromStdString(entry.path().string()));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const auto type = QJsonDocument::fromJson(file.readAll()).object()["type"].toString();
            if (type.isEmpty() || type.startsWith("Project"))
                continue;
            std::filesystem::copy_file(entry.path(), dir / entry.path().filename());
        }
    }
    store_.setDirectory(dir);
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
