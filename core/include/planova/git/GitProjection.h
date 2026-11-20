#pragma once

#include <set>
#include <string>
#include <vector>

#include "planova/common/Dtos.h"
#include "planova/common/Queries.h"
#include "planova/git/Commit.h"
#include "planova/common/IProjection.h"

class GitProjection : public IProjection, public IConflictQuery {
public:
    void reset() override;
    void apply(const Event& event) override;

    bool isConnected() const;
    std::string repoUrl() const;
    std::string branch() const;

    std::vector<CommitDto> commits() const;
    std::vector<CommitDto> commitsByAuthor(const std::string& author) const;
    std::vector<IssueDto> issues() const;
    int openIssueCount() const;

    bool hasOpenConflict(const Id& memberId) const override;

private:
    bool connected_ = false;
    std::string repoUrl_;
    std::string branch_;
    std::vector<Commit> commits_;
    std::vector<Issue> issues_;
    std::set<Id> conflictedMembers_;
};