#pragma once
#include "planova/common/Dtos.h"
#include "planova/common/IProjection.h"
#include <vector>

class ProjectProjection : public IProjection {
public:
    void reset() override;
    void apply(const Event& event) override;
    std::vector<ProjectDto> list() const;
    // nullptr when the id is unknown. The pointer is valid until the next apply or reset.
    const ProjectDto* find(const Id& id) const;

private:
    std::vector<ProjectDto> projects_;
};