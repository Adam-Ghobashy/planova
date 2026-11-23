#pragma once

#include <string>
#include <vector>

#include "planova/common/Clock.h"
#include "planova/common/Dtos.h"
#include "planova/common/Types.h"
#include "planova/git/GitProjection.h"
#include "planova/store/EventLog.h"

class GitService {
public:
    // state may be null: the "must be connected" check is then skipped.
    GitService(EventLog& log, IClock& clock, const GitProjection* state = nullptr);

    void connect(const std::string& repoUrl, const std::string& branch = "main", const Id& author = "system");
    void disconnect(const Id& author = "system");
    void sync(const std::vector<CommitDto>& commits,
              const std::vector<IssueDto>& issues,
              const std::vector<Id>& conflictedMembers = {},
              const Id& author = "system");

    // Helper to generate realistic simulated commits & issues for demo/testing
    void simulateSync(const std::vector<MemberDto>& teamMembers, const Id& author = "system");

private:
    EventLog& log_;
    IClock& clock_;
    const GitProjection* state_;
};