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
    if (cleanName.empty())
        throw std::invalid_argument("Project name cannot be empty");

    static const char* const kModels[] = {"Agile", "Scrum", "Waterfall", "Spiral"};
    if (std::find(std::begin(kModels), std::end(kModels), seModel) == std::end(kModels))
        throw std::invalid_argument("Unknown process model: " + seModel);

    const int64_t ts = clock_.nowMs();

    QJsonObject payload;
    payload["name"] = QString::fromStdString(cleanName);
    payload["teamName"] = QString::fromStdString(team);
    payload["seModel"] = QString::fromStdString(seModel);
    payload["ideaText"] = QString::fromStdString(idea);

    const Id projectId = makeEventId(ts, team, payload);
    payload["projectId"] = QString::fromStdString(projectId);

    Event event;
    event.id = makeEventId(ts, team, payload);
    event.ts = ts;
    event.author = team;
    event.type = ProjectCreated;
    event.payload = payload;
    log_.append(event);

    return projectId;
}

std::vector<ProjectDto> ProjectService::list() const
{
    return projection_.list();
}