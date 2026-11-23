#include "planova/project/ProjectService.h"
#include "planova/common/EventTypes.h"
#include "planova/common/Ids.h"

#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace {

std::string trimmed(const std::string& text)
{
    return QString::fromStdString(text).trimmed().toStdString();
}

bool isKnownModel(const std::string& model)
{
    static const char* const kModels[] = {"Agile", "Scrum", "Waterfall", "Spiral"};
    return std::find(std::begin(kModels), std::end(kModels), model) != std::end(kModels);
}

} // namespace

ProjectService::ProjectService(EventLog& log, IClock& clock)
    : log_(log), clock_(clock)
{
    log_.add(projection_);
}

Id ProjectService::create(const std::string& name, const std::string& team,
                          const std::string& seModel, const std::string& idea)
{
    const std::string cleanName = trimmed(name);
    const std::string cleanTeam = trimmed(team);
    if (cleanName.empty())
        throw std::invalid_argument("Project name cannot be empty");

    if (!isKnownModel(seModel))
        throw std::invalid_argument("Unknown process model: " + seModel);

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["name"] = QString::fromStdString(cleanName);
    payload["teamName"] = QString::fromStdString(cleanTeam);
    payload["seModel"] = QString::fromStdString(seModel);
    payload["ideaText"] = QString::fromStdString(idea);

    const Id projectId = makeEventId(ts, cleanTeam, payload);
    payload["projectId"] = QString::fromStdString(projectId);

    Event event;
    event.id = makeEventId(ts, cleanTeam, payload);
    event.ts = ts;
    event.author = cleanTeam;
    event.type = ProjectCreated;
    event.payload = payload;
    log_.append(event);

    return projectId;
}

void ProjectService::update(const Id& id, const std::string& name, const std::string& team,
                            const std::string& seModel)
{
    const ProjectDto* current = projection_.find(id);
    if (!current)
        throw std::invalid_argument("Unknown project: " + id);

    const std::string cleanName = trimmed(name);
    const std::string cleanTeam = trimmed(team);
    if (cleanName.empty())
        throw std::invalid_argument("Project name cannot be empty");
    if (!isKnownModel(seModel))
        throw std::invalid_argument("Unknown process model: " + seModel);

    if (cleanName == current->name && cleanTeam == current->team && seModel == current->seModel)
        return;

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["projectId"] = QString::fromStdString(id);
    payload["name"] = QString::fromStdString(cleanName);
    payload["teamName"] = QString::fromStdString(cleanTeam);
    payload["seModel"] = QString::fromStdString(seModel);

    Event event;
    event.id = makeEventId(ts, cleanTeam, payload);
    event.ts = ts;
    event.author = cleanTeam;
    event.type = ProjectUpdated;
    event.payload = payload;
    log_.append(event);
}

void ProjectService::remove(const Id& id)
{
    const ProjectDto* current = projection_.find(id);
    if (!current)
        throw std::invalid_argument("Unknown project: " + id);
    const std::string author = current->team;

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["projectId"] = QString::fromStdString(id);

    Event event;
    event.id = makeEventId(ts, author, payload);
    event.ts = ts;
    event.author = author;
    event.type = ProjectDeleted;
    event.payload = payload;
    log_.append(event);
}

bool ProjectService::exists(const Id& id) const
{
    return projection_.find(id) != nullptr;
}

std::vector<ProjectDto> ProjectService::list() const
{
    return projection_.list();
}
