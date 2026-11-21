#include "planova/PlanovaCore.h"

#include "planova/common/EventTypes.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

int64_t SystemClock::nowMs() const
{
    // Never repeat a millisecond: event order is (ts, id), so two events written
    // in the same ms could otherwise replay in hash order instead of write order.
    last_ = std::max(QDateTime::currentMSecsSinceEpoch(), last_ + 1);
    return last_;
}

namespace {

bool isBlank(const std::string& text)
{
    return QString::fromStdString(text).trimmed().isEmpty();
}

std::string trimmed(const std::string& text)
{
    return QString::fromStdString(text).trimmed().toStdString();
}

// Checks members and weights. The name and model are checked by ProjectService::create,
// which setupProject calls before it writes anything else.
void validateSetup(const ProjectSetup& setup)
{
    if (setup.members.empty())
        throw std::invalid_argument("Add at least one team member");
    for (const SetupMember& member : setup.members) {
        if (isBlank(member.name))
            throw std::invalid_argument("Every team member needs a name");
        for (const auto& entry : member.ratings) {
            if (entry.second < 1 || entry.second > 5)
                throw std::invalid_argument("Skill ratings must be between 1 and 5");
        }
    }

    if (!setup.weights.valid())
        throw std::invalid_argument("Slicer weights must be zero or more and add up to 100");
}

} // namespace

PlanovaCore::PlanovaCore(std::filesystem::path workspaceDir)
    : workspaceDir_(std::move(workspaceDir)),
    workspaceStore_(workspaceDir_),
    workspaceLog_(workspaceStore_),
    store_(workspaceDir_),
    log_(store_),
    clock_(),
    projects_(workspaceLog_, clock_),
    memberProjection_(),
    taskProjection_(),
    members_(log_, clock_, memberProjection_, taskProjection_),
    gitProjection_(),
    git_(log_, clock_, &gitProjection_),
    tasks_(log_, taskProjection_, clock_, &memberProjection_),
    conflictProjection_(),
    strategy_(),
    slicer_(taskProjection_, memberProjection_, strategy_, log_, clock_, &conflictProjection_),
    pinProjection_(),
    pins_(log_, pinProjection_, clock_),
    conflicts_(log_, taskProjection_, conflictProjection_, clock_),
    stats_(taskProjection_, memberProjection_),
    home_(taskProjection_, memberProjection_, pinProjection_)
{
    log_.add(memberProjection_);
    log_.add(gitProjection_);
    log_.add(taskProjection_);
    log_.add(pinProjection_);
    log_.add(conflictProjection_);
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

Id PlanovaCore::setupProject(const ProjectSetup& setup)
{
    validateSetup(setup);

    const Id id = createProject(trimmed(setup.name), trimmed(setup.team), setup.seModel, trimmed(setup.idea));
    // Open first, so members, weights and Git events are saved inside the new project.
    openProject(id);

    for (const SetupMember& member : setup.members) {
        const Id memberId = members_.add(trimmed(member.name), trimmed(member.role), member.isLead);
        for (const auto& entry : member.ratings)
            members_.rate(memberId, entry.first, entry.second);
    }
    slicer_.saveWeights(setup.weights);

    const std::string repoUrl = trimmed(setup.repoUrl);
    if (!repoUrl.empty()) {
        const std::string branch = trimmed(setup.branch);
        git_.connect(repoUrl, branch.empty() ? "main" : branch);
    }
    return id;
}

bool PlanovaCore::hasProjectData(const Id& projectId) const
{
    return std::filesystem::is_directory(projectEventsDir(projectId));
}

void PlanovaCore::openProject(const Id& projectId)
{
    const auto known = projects_.list();
    const bool exists = std::any_of(known.begin(), known.end(),
                                    [&](const ProjectDto& project) { return project.id == projectId; });
    if (!exists)
        throw std::invalid_argument("Unknown project: " + projectId);

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

std::vector<std::string> PlanovaCore::brokenEventFiles() const
{
    // Both stores read the same folder until a project is opened, so list each file once.
    std::set<std::filesystem::path> paths;
    for (const auto& path : workspaceStore_.brokenFiles())
        paths.insert(path);
    for (const auto& path : store_.brokenFiles())
        paths.insert(path);

    std::vector<std::string> names;
    for (const auto& path : paths)
        names.push_back(path.filename().string());
    return names;
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

planova::PinService& PlanovaCore::pins()
{
    return pins_;
}

planova::IPinBoardQuery& PlanovaCore::pinBoard()
{
    return pinProjection_;
}

IPinQuery& PlanovaCore::pinQuery()
{
    return pinProjection_;
}

planova::ConflictService& PlanovaCore::conflicts()
{
    return conflicts_;
}

IConflictQuery& PlanovaCore::conflictQuery()
{
    return conflictProjection_;
}

const planova::ConflictProjection& PlanovaCore::conflictProjection() const
{
    return conflictProjection_;
}
