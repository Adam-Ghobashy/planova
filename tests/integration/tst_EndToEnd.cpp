#include <QtTest>
#include <QTemporaryDir>
#include <algorithm>
#include <filesystem>

#include "planova/PlanovaCore.h"
#include "planova/project/ProjectSetup.h"

namespace {

// One team member. The ratings follow the design prototype (js/state.js): the order is
// UI/UX, Frontend, Backend, Storage, Integration, Testing, Documentation.
SetupMember addMember(const std::string& name, const std::string& role, bool isLead,
                      const std::vector<int>& ratings)
{
    const SkillCategory skills[] = {SkillCategory::UiUx,        SkillCategory::Frontend,
                                    SkillCategory::Backend,     SkillCategory::Storage,
                                    SkillCategory::Integration, SkillCategory::Testing,
                                    SkillCategory::Documentation};
    SetupMember member;
    member.name = name;
    member.role = role;
    member.isLead = isLead;
    for (size_t i = 0; i < ratings.size(); ++i)
        member.ratings[skills[i]] = ratings[i];
    return member;
}

ProjectSetup sixMemberSetup()
{
    ProjectSetup setup;
    setup.name = "Planova Desktop System";
    setup.team = "ECU Team";
    setup.seModel = "Scrum";
    setup.idea = "Event-sourced project planner";
    setup.members = {
        addMember("Zeyad", "Backend and Architecture Lead", true, {2, 3, 5, 4, 5, 4, 3}),
        addMember("Mai", "UI/UX and Frontend Specialist", false, {5, 5, 2, 2, 3, 3, 4}),
        addMember("Adam", "Storage and Services Engineer", false, {2, 2, 4, 5, 4, 3, 3}),
        addMember("Oliver", "QA and Integration Engineer", false, {1, 2, 3, 3, 5, 5, 4}),
        addMember("Haidy", "Documentation and QA", false, {4, 3, 2, 2, 3, 4, 5}),
        addMember("Omar", "Backend Developer", false, {1, 2, 5, 4, 4, 4, 2}),
    };
    setup.weights = {60, 20, 20};
    return setup;
}

Id makeTask(PlanovaCore& core, const std::string& title, SkillCategory skill, Priority priority,
            int complexity, int hours)
{
    return core.tasks().create(NewTask{title, skill, priority, complexity, hours, {}});
}

// Six tasks that cover six different skills.
std::vector<Id> createSixTasks(PlanovaCore& core)
{
    return {
        makeTask(core, "Layered core architecture", SkillCategory::Backend, Priority::High, 4, 32),
        makeTask(core, "Desktop views", SkillCategory::Frontend, Priority::Medium, 4, 36),
        makeTask(core, "Event storage", SkillCategory::Storage, Priority::High, 3, 20),
        makeTask(core, "Git sync", SkillCategory::Integration, Priority::Critical, 4, 28),
        makeTask(core, "Velocity tests", SkillCategory::Testing, Priority::Low, 3, 16),
        makeTask(core, "User guide", SkillCategory::Documentation, Priority::Low, 2, 10),
    };
}

void moveToDone(PlanovaCore& core, const Id& task)
{
    core.tasks().move(task, TaskStatus::InProgress);
    core.tasks().move(task, TaskStatus::Review);
    core.tasks().move(task, TaskStatus::Done);
}

// Same two calls the Chaos Slicer page makes: preview, then write the result.
void recalculateAndApply(PlanovaCore& core, const planova::slicer::ScoringWeights& weights)
{
    core.slicer().apply(core.slicer().preview(weights), weights);
}

} // namespace

class tst_EndToEnd : public QObject {
    Q_OBJECT
private slots:
    void fullFlow();
    void reopenKeepsWeightsAndMembers();
    void declineThenResolveClearsConflict();
};

void tst_EndToEnd::fullFlow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path workspace = dir.path().toStdString();
    const planova::slicer::ScoringWeights weights{60, 20, 20};

    PlanovaCore core(workspace);
    const Id project = core.setupProject(sixMemberSetup());
    QCOMPARE(core.memberQuery().all().size(), size_t(6));

    // Slicer: every task gets an assignee.
    const std::vector<Id> tasks = createSixTasks(core);
    recalculateAndApply(core, weights);
    for (const Id& task : tasks)
        QVERIFY2(core.taskQuery().byId(task)->assignee.has_value(), "every task has an assignee");

    // Two tasks walk the whole board. The clock is real, so the moves are instant; the velocity
    // calculation ignores those, which is fine here.
    moveToDone(core, tasks[0]);
    moveToDone(core, tasks[1]);
    QCOMPARE(core.taskQuery().byId(tasks[0])->status, TaskStatus::Done);

    // The assignee of a still-open task declines it.
    const Id declinedTask = tasks[2];
    const Id decliner = *core.taskQuery().byId(declinedTask)->assignee;
    core.conflicts().decline(declinedTask, decliner, "Too much on my plate");
    QVERIFY(!core.taskQuery().byId(declinedTask)->assignee.has_value());
    QVERIFY(core.conflictQuery().hasOpenConflict(decliner));

    // Recalculate: while the conflict is open the slicer must not give open tasks back to the
    // decliner (all six members are rated for every skill, so someone else can always take them).
    recalculateAndApply(core, weights);
    for (const TaskDto& task : core.taskQuery().all()) {
        QVERIFY2(task.assignee.has_value(), "every task is assigned again");
        if (task.status != TaskStatus::Done)
            QVERIFY2(*task.assignee != decliner, "the decliner holds no open task");
    }

    // Stats and the home dashboard agree on the done count.
    QCOMPARE(core.stats().summary().tasksDone, 2);
    QCOMPARE(core.home().dashboard().completed, 2);

    // A second core on the same workspace sees the same state.
    PlanovaCore again(workspace);
    again.openProject(project);
    const auto before = core.taskQuery().all();
    QCOMPARE(again.taskQuery().all().size(), before.size());
    for (const TaskDto& task : before) {
        const auto copy = again.taskQuery().byId(task.id);
        QVERIFY(copy.has_value());
        QCOMPARE(copy->assignee.value_or(""), task.assignee.value_or(""));
        QCOMPARE(copy->status, task.status);
    }
    QCOMPARE(again.pinQuery().openCount(), core.pinQuery().openCount());
    QCOMPARE(again.pinQuery().openCount(), 1); // the decline pin
}

void tst_EndToEnd::reopenKeepsWeightsAndMembers()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::filesystem::path workspace = dir.path().toStdString();

    Id project;
    {
        PlanovaCore core(workspace);
        project = core.setupProject(sixMemberSetup());
    }

    PlanovaCore again(workspace);
    again.openProject(project);
    const planova::slicer::ScoringWeights saved = again.slicer().savedWeights();
    QCOMPARE(saved.skill, 60);
    QCOMPARE(saved.workload, 20);
    QCOMPARE(saved.priority, 20);

    const auto members = again.memberQuery().all();
    QCOMPARE(members.size(), size_t(6));
    const auto leads = std::count_if(members.begin(), members.end(),
                                     [](const MemberDto& m) { return m.isLead; });
    QCOMPARE(int(leads), 1);
    for (const MemberDto& member : members)
        QCOMPARE(member.ratings.size(), size_t(7)); // all seven skills rated
}

void tst_EndToEnd::declineThenResolveClearsConflict()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PlanovaCore core(dir.path().toStdString());
    core.setupProject(sixMemberSetup());

    const Id task = makeTask(core, "Event storage", SkillCategory::Storage, Priority::High, 3, 20);
    const Id member = core.memberQuery().all().front().id;
    core.tasks().assign(task, member, "board");

    const Id conflict = core.conflicts().decline(task, member, "Blocked by another task");
    QVERIFY(core.conflictQuery().hasOpenConflict(member));

    core.conflicts().resolve(conflict, member);
    QVERIFY(!core.conflictQuery().hasOpenConflict(member));
}

QTEST_APPLESS_MAIN(tst_EndToEnd)
#include "tst_EndToEnd.moc"
