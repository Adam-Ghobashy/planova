#include "planova/task/TaskGraph.h"
#include <algorithm>
void TaskGraph::addNode(const Id& id, int estHours) {
    hours_[id] = estHours;
    adj_.try_emplace(id);
    revAdj_.try_emplace(id);
}
bool TaskGraph::reaches(const Id& from, const Id& target, std::set<Id>& visited) const {
    if (from == target) return true;
    if (!visited.insert(from).second) return false;
    auto it = adj_.find(from);
    if (it == adj_.end()) return false;
    for (const auto& next : it->second)
        if (reaches(next, target, visited)) return true;
    return false;
}
bool TaskGraph::hasDependency(const Id& task, const Id& dependsOn) const {
    auto it = revAdj_.find(task);
    if (it == revAdj_.end()) return false;
    return std::find(it->second.begin(), it->second.end(), dependsOn) != it->second.end();
}
bool TaskGraph::addDependency(const Id& task, const Id& dependsOn) {
    if (!hours_.count(task))      addNode(task, 0);
    if (!hours_.count(dependsOn)) addNode(dependsOn, 0);
    if (hasDependency(task, dependsOn)) return true;
    // New edge is dependsOn -> task. It makes a cycle iff dependsOn is already reachable from task.
    std::set<Id> visited;
    if (reaches(task, dependsOn, visited)) return false;
    adj_[dependsOn].push_back(task);
    revAdj_[task].push_back(dependsOn);
    return true;
}
std::vector<Id> TaskGraph::blockedBy(const Id& task, const std::set<Id>& doneIds) const {
    std::vector<Id> result;
    auto it = revAdj_.find(task);
    if (it == revAdj_.end()) return result;
    for (const auto& dep : it->second)
        if (!doneIds.count(dep)) result.push_back(dep);
    return result;
}
std::vector<Id> TaskGraph::criticalPath() const {
    std::map<Id, int> indeg;
    for (const auto& [id, h] : hours_) {
        (void)h;
        indeg[id] = static_cast<int>(revAdj_.at(id).size());
    }
    std::set<Id> ready;                       // sorted -> deterministic
    for (const auto& [id, d] : indeg)
        if (d == 0) ready.insert(id);
    std::map<Id, int> best;                   // longest hours of a path ending at id
    std::map<Id, Id> prev;
    Id endId;
    int maxH = -1;
    while (!ready.empty()) {
        Id u = *ready.begin();
        ready.erase(ready.begin());
        int base = 0;
        Id from;
        bool has = false;
        for (const auto& p : revAdj_.at(u)) {
            const int b = best[p];
            if (!has || b > base || (b == base && p < from)) { base = b; from = p; has = true; }
        }
        best[u] = base + hours_.at(u);
        if (has) prev[u] = from;
        if (best[u] > maxH || (best[u] == maxH && u < endId)) { maxH = best[u]; endId = u; }
        for (const auto& v : adj_.at(u))
            if (--indeg[v] == 0) ready.insert(v);
    }
    std::vector<Id> path;
    for (Id c = endId; !c.empty();) {
        path.push_back(c);
        auto it = prev.find(c);
        if (it == prev.end()) break;
        c = it->second;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

