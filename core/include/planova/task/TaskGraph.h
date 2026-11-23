#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include "planova/common/Types.h"
class TaskGraph {
public:
    void addNode(const Id&, int estHours);
    // false if it would create a cycle (self-dependency included). Adding an existing edge is a no-op returning true.
    bool addDependency(const Id& task, const Id& dependsOn);
    bool hasDependency(const Id& task, const Id& dependsOn) const;
    std::vector<Id> blockedBy(const Id&, const std::set<Id>& doneIds) const;
    std::vector<Id> criticalPath() const;   // longest path by estHours
private:
    bool reaches(const Id& from, const Id& target, std::set<Id>& visited) const;
    std::map<Id, int> hours_;
    std::map<Id, std::vector<Id>> adj_;     // prerequisite -> dependents
    std::map<Id, std::vector<Id>> revAdj_;  // task -> prerequisites
};

