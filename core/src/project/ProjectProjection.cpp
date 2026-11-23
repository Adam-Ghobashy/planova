#include "planova/project/ProjectProjection.h"
#include "planova/common/EventTypes.h"
#include <algorithm>

namespace {

// Finds the stored project the event is about; end() when the id is unknown.
std::vector<ProjectDto>::iterator findById(std::vector<ProjectDto>& projects, const Event& event)
{
    const std::string id = event.payload["projectId"].toString().toStdString();
    return std::find_if(projects.begin(), projects.end(),
                        [&id](const ProjectDto& p) { return p.id == id; });
}

} // namespace

void ProjectProjection::reset()
{
    projects_.clear();
}

void ProjectProjection::apply(const Event& event)
{
    if (event.type == ProjectCreated) {
        ProjectDto p;
        p.id = event.payload["projectId"].toString().toStdString();
        p.name = event.payload["name"].toString().toStdString();
        p.team = event.payload["teamName"].toString().toStdString();
        p.seModel = event.payload["seModel"].toString().toStdString();
        projects_.push_back(p);
    } else if (event.type == ProjectUpdated) {
        const auto it = findById(projects_, event);
        if (it == projects_.end())
            return;
        it->name = event.payload["name"].toString().toStdString();
        it->team = event.payload["teamName"].toString().toStdString();
        it->seModel = event.payload["seModel"].toString().toStdString();
    } else if (event.type == ProjectDeleted) {
        const auto it = findById(projects_, event);
        if (it != projects_.end())
            projects_.erase(it);
    }
}

std::vector<ProjectDto> ProjectProjection::list() const
{
    return projects_;
}

const ProjectDto* ProjectProjection::find(const Id& id) const
{
    const auto it = std::find_if(projects_.begin(), projects_.end(),
                                 [&id](const ProjectDto& p) { return p.id == id; });
    return it == projects_.end() ? nullptr : &*it;
}
