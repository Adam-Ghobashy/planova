#include "planova/git/GitProjection.h"
#include "planova/common/EventTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

void GitProjection::reset()
{
    connected_ = false;
    repoUrl_.clear();
    branch_.clear();
    commits_.clear();
    issues_.clear();
    conflictedMembers_.clear();
}

void GitProjection::apply(const Event& event)
{
    if (event.type == GitConnected) {
        connected_ = true;
        repoUrl_ = event.payload["repoUrl"].toString().toStdString();
        branch_ = event.payload["branch"].toString("main").toStdString();
    } else if (event.type == GitDisconnected) {
        reset();
    } else if (event.type == GitSynced) {
        if (event.payload.contains("commits") && event.payload["commits"].isArray()) {
            commits_.clear();
            const QJsonArray commitArray = event.payload["commits"].toArray();
            for (const QJsonValue& val : commitArray) {
                const QJsonObject obj = val.toObject();
                Commit c;
                c.hash = obj["hash"].toString().toStdString();
                c.message = obj["message"].toString().toStdString();
                c.author = obj["author"].toString().toStdString();
                c.ts = static_cast<int64_t>(obj["ts"].toDouble(0.0));
                commits_.push_back(c);
            }
        }

        if (event.payload.contains("issues") && event.payload["issues"].isArray()) {
            issues_.clear();
            const QJsonArray issueArray = event.payload["issues"].toArray();
            for (const QJsonValue& val : issueArray) {
                const QJsonObject obj = val.toObject();
                Issue iss;
                iss.number = obj["number"].toInt(0);
                iss.title = obj["title"].toString().toStdString();
                iss.open = obj["open"].toBool(true);
                issues_.push_back(iss);
            }
        }

        if (event.payload.contains("conflictedMembers") && event.payload["conflictedMembers"].isArray()) {
            conflictedMembers_.clear();
            const QJsonArray conflictArray = event.payload["conflictedMembers"].toArray();
            for (const QJsonValue& val : conflictArray) {
                conflictedMembers_.insert(val.toString().toStdString());
            }
        }
    }
}

bool GitProjection::isConnected() const
{
    return connected_;
}

std::string GitProjection::repoUrl() const
{
    return repoUrl_;
}

std::string GitProjection::branch() const
{
    return branch_;
}

std::vector<CommitDto> GitProjection::commits() const
{
    std::vector<CommitDto> out;
    out.reserve(commits_.size());
    for (const auto& c : commits_) {
        out.push_back(c.toDto());
    }
    return out;
}

std::vector<CommitDto> GitProjection::commitsByAuthor(const std::string& author) const
{
    std::vector<CommitDto> out;
    for (const auto& c : commits_) {
        if (c.author == author) {
            out.push_back(c.toDto());
        }
    }
    return out;
}

std::vector<IssueDto> GitProjection::issues() const
{
    std::vector<IssueDto> out;
    out.reserve(issues_.size());
    for (const auto& iss : issues_) {
        out.push_back(iss.toDto());
    }
    return out;
}

int GitProjection::openIssueCount() const
{
    int count = 0;
    for (const auto& iss : issues_) {
        if (iss.open) {
            ++count;
        }
    }
    return count;
}

bool GitProjection::hasOpenConflict(const Id& memberId) const
{
    return conflictedMembers_.find(memberId) != conflictedMembers_.end();
}