#include "planova/git/GitService.h"
#include "planova/common/EventTypes.h"
#include "planova/common/Ids.h"

#include <QJsonArray>
#include <QJsonObject>

GitService::GitService(EventLog& log, IClock& clock)
    : log_(log)
    , clock_(clock)
{
}

void GitService::connect(const std::string& repoUrl, const std::string& branch, const Id& author)
{
    QJsonObject payload;
    payload["repoUrl"] = QString::fromStdString(repoUrl);
    payload["branch"] = QString::fromStdString(branch);

    Event evt;
    evt.ts = clock_.nowMs();
    evt.author = author;
    evt.type = GitConnected;
    evt.payload = payload;
    evt.id = makeEventId(evt.ts, evt.author, evt.payload);

    log_.append(evt);
}

void GitService::disconnect(const Id& author)
{
    QJsonObject payload;

    Event evt;
    evt.ts = clock_.nowMs();
    evt.author = author;
    evt.type = GitDisconnected;
    evt.payload = payload;
    evt.id = makeEventId(evt.ts, evt.author, evt.payload);

    log_.append(evt);
}

void GitService::sync(const std::vector<CommitDto>& commits,
                      const std::vector<IssueDto>& issues,
                      const std::vector<Id>& conflictedMembers,
                      const Id& author)
{
    QJsonArray commitArray;
    for (const auto& c : commits) {
        QJsonObject obj;
        obj["hash"] = QString::fromStdString(c.hash);
        obj["message"] = QString::fromStdString(c.message);
        obj["author"] = QString::fromStdString(c.author);
        obj["ts"] = static_cast<double>(c.ts);
        commitArray.append(obj);
    }

    QJsonArray issueArray;
    for (const auto& iss : issues) {
        QJsonObject obj;
        obj["number"] = iss.number;
        obj["title"] = QString::fromStdString(iss.title);
        obj["open"] = iss.open;
        issueArray.append(obj);
    }

    QJsonArray conflictArray;
    for (const auto& memberId : conflictedMembers) {
        conflictArray.append(QString::fromStdString(memberId));
    }

    QJsonObject payload;
    payload["commits"] = commitArray;
    payload["issues"] = issueArray;
    payload["conflictedMembers"] = conflictArray;

    Event evt;
    evt.ts = clock_.nowMs();
    evt.author = author;
    evt.type = GitSynced;
    evt.payload = payload;
    evt.id = makeEventId(evt.ts, evt.author, evt.payload);

    log_.append(evt);
}

void GitService::simulateSync(const std::vector<MemberDto>& teamMembers, const Id& author)
{
    const int64_t now = clock_.nowMs();
    std::vector<CommitDto> simulatedCommits;

    if (teamMembers.empty()) {
        simulatedCommits.push_back({"a1b2c3d", "Initial repository setup", "system", now - 3600000});
        simulatedCommits.push_back({"e4f5g6h", "Implement core event-sourcing log", "system", now - 1800000});
    } else {
        const std::vector<std::string> sampleHashes = {"a1b2c3d", "e4f5g6h", "9i8j7k6", "5l4m3n2", "1o2p3q4"};
        const std::vector<std::string> sampleMessages = {
            "Implement backend event projection",
            "Design UI layout and dashboard widgets",
            "Add unit tests for service layer",
            "Refactor task scoring and velocity calculation",
            "Fix merge conflict in CMakeLists.txt"
        };

        for (std::size_t i = 0; i < teamMembers.size(); ++i) {
            CommitDto c;
            c.hash = sampleHashes[i % sampleHashes.size()];
            c.message = sampleMessages[i % sampleMessages.size()];
            c.author = teamMembers[i].name;
            c.ts = now - static_cast<int64_t>((teamMembers.size() - i) * 1800000);
            simulatedCommits.push_back(c);
        }
    }

    const std::vector<IssueDto> simulatedIssues = {
        {101, "Connect GitHub simulation panel to UI", true},
        {102, "Add EMA velocity smoothing bounds", false},
        {103, "Validate team skill coverage warnings", true}
    };

    sync(simulatedCommits, simulatedIssues, {}, author);
}