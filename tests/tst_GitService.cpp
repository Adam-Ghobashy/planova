#include <QtTest>
#include <vector>

#include "planova/common/Clock.h"
#include "planova/git/GitProjection.h"
#include "planova/git/GitService.h"
#include "planova/store/EventLog.h"
#include "planova/team/MemberProjection.h"
#include "planova/team/MemberService.h"

using namespace std;

namespace {

class MemoryStore : public IEventStore {
public:
    vector<Event> events;
    void append(const Event& e) override { events.push_back(e); }
    vector<Event> readAll() const override { return events; }
};

class FakeClock : public IClock {
public:
    explicit FakeClock(int64_t startMs = 1000) : now_(startMs) {}
    int64_t nowMs() const override { return now_; }
    void advance(int64_t deltaMs) { now_ += deltaMs; }
private:
    int64_t now_;
};

} // namespace

class tst_GitService : public QObject
{
    Q_OBJECT

private slots:
    void connectAndDisconnectRepo()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        GitProjection proj;
        log.add(proj);

        GitService svc(log, clock);

        QCOMPARE(proj.isConnected(), false);
        QCOMPARE(proj.repoUrl(), string(""));

        svc.connect("https://github.com/Zeyad-101/planova.git", "feature/s2-team-git");
        QCOMPARE(proj.isConnected(), true);
        QCOMPARE(proj.repoUrl(), string("https://github.com/Zeyad-101/planova.git"));
        QCOMPARE(proj.branch(), string("feature/s2-team-git"));

        clock.advance(10);
        svc.disconnect();
        QCOMPARE(proj.isConnected(), false);
        QCOMPARE(proj.repoUrl(), string(""));
        QCOMPARE(proj.branch(), string(""));
    }

    void syncCommitsIssuesAndConflicts()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(1000);

        GitProjection proj;
        log.add(proj);

        GitService svc(log, clock);
        svc.connect("https://github.com/Zeyad-101/planova.git", "main");

        clock.advance(500);
        vector<CommitDto> commits = {
            {"abc1234", "Implement GitProjection", "Adam", 1500},
            {"def5678", "Design GitHub sync panel", "Zeyad", 1600}
        };
        vector<IssueDto> issues = {
            {1, "Open bug in parser", true},
            {2, "Closed setup issue", false}
        };
        vector<Id> conflicts = {"member_adam"};

        svc.sync(commits, issues, conflicts);

        QCOMPARE(proj.commits().size(), size_t(2));
        QCOMPARE(proj.commitsByAuthor("Adam").size(), size_t(1));
        QCOMPARE(proj.commitsByAuthor("Adam")[0].hash, string("abc1234"));

        QCOMPARE(proj.issues().size(), size_t(2));
        QCOMPARE(proj.openIssueCount(), 1);

        QCOMPARE(proj.hasOpenConflict("member_adam"), true);
        QCOMPARE(proj.hasOpenConflict("member_zeyad"), false);

        // Rebuild log and confirm projection state persists identically
        log.rebuild();
        QCOMPARE(proj.isConnected(), true);
        QCOMPARE(proj.commits().size(), size_t(2));
        QCOMPARE(proj.openIssueCount(), 1);
        QCOMPARE(proj.hasOpenConflict("member_adam"), true);
    }

    void simulateSyncUsesTeamMembers()
    {
        MemoryStore store;
        EventLog log(store);
        FakeClock clock(5000);

        MemberProjection memberProj;
        GitProjection gitProj;
        log.add(memberProj);
        log.add(gitProj);

        MemberService memberSvc(log, clock);
        memberSvc.add("Adam", "Backend Lead", true);
        memberSvc.add("Zeyad", "UI Designer", false);

        GitService gitSvc(log, clock);
        gitSvc.connect("https://github.com/Zeyad-101/planova.git");
        clock.advance(100);
        gitSvc.simulateSync(memberProj.all());

        QCOMPARE(gitProj.commits().size(), size_t(2));
        QCOMPARE(gitProj.commits()[0].author, string("Adam"));
        QCOMPARE(gitProj.commits()[1].author, string("Zeyad"));
        QCOMPARE(gitProj.issues().size(), size_t(3));
        QCOMPARE(gitProj.openIssueCount(), 2);
    }
};

QTEST_MAIN(tst_GitService)
#include "tst_GitService.moc"