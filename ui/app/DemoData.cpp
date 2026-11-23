#include "DemoData.h"
#include <map>
#include <stdexcept>
#include <vector>
#include <QDebug>
#include <QThread>
#include "planova/PlanovaCore.h"

namespace {

// Events replay by (ts, id): keep every timestamp distinct so the order is always the same.
void tick() { QThread::msleep(2); }

std::string firstName(const std::string& full) {
    return full.substr(0, full.find(' '));
}

struct SeedTask {
    const char* title;
    SkillCategory skill;
    Priority priority;
    int complexity;
    int hours;
    const char* assignee;     // first name
    TaskStatus status;
};

const char* const kMembers[] = {
    "Zeyad Waled", "Mai Khalaf", "Adam Tamer", "Oliver Emad", "Haidy Ahmed", "Omar Ahmed"
};

const SeedTask kTasks[] = {
    // To Do
    {"Formulate SE Process Milestones & Documentation", SkillCategory::Documentation, Priority::Medium,   2,  8, "Haidy",  TaskStatus::ToDo},
    {"Integrate Critical Path Algorithm with Task Graph", SkillCategory::Backend,     Priority::High,     4, 12, "Omar",   TaskStatus::ToDo},
    // In Progress
    {"Develop Weighted Scoring Assignment Strategy",      SkillCategory::Backend,     Priority::Critical, 5, 16, "Omar",   TaskStatus::InProgress},
    {"Build Interactive Phase Board (Kanban Columns)",    SkillCategory::Frontend,    Priority::High,     4, 14, "Mai",    TaskStatus::InProgress},
    // Review / QA
    {"Implement Append-Only JSON Git Event Syncer",       SkillCategory::Integration, Priority::High,     4, 12, "Zeyad",  TaskStatus::Review},
    {"Construct GoogleTest Test Suite for Assignment Core", SkillCategory::Testing,   Priority::Medium,   3, 10, "Oliver", TaskStatus::Review},
    // Done
    {"Design Domain Entity Interfaces & Task Factory",    SkillCategory::Backend,     Priority::High,     3,  8, "Zeyad",  TaskStatus::Done},
    {"Implement Setup Wizard & Responsive Stepper",       SkillCategory::UiUx,        Priority::Medium,   3, 10, "Mai",    TaskStatus::Done},
    {"Implement File Event Store & Event Log Replay",     SkillCategory::Storage,     Priority::Medium,   3,  6, "Adam",   TaskStatus::Done},
    };

} // namespace

namespace demo {

void seedIfEmpty(PlanovaCore& core) {
    if (!core.taskQuery().all().empty()) return;      // never touch a real project

    try {
        // Members (reuse existing ones with the same name)
        std::map<std::string, Id> memberId;           // first name -> id
        const auto existing = core.memberProjection().all();
        for (const char* full : kMembers) {
            Id id;
            for (const MemberDto& m : existing)
                if (m.name == full) { id = m.id; break; }
            if (id.empty()) { id = core.members().add(full, "", false); tick(); }
            memberId[firstName(full)] = id;
        }

        // Tasks: create, assign, move to their column
        std::vector<Id> ids;
        for (const SeedTask& s : kTasks) {
            NewTask n{};
            n.title = s.title;
            n.skill = s.skill;
            n.priority = s.priority;
            n.complexity = s.complexity;
            n.estHours = s.hours;
            const Id id = core.tasks().create(n);
            tick();
            ids.push_back(id);

            core.tasks().assign(id, memberId[s.assignee], "board");
            tick();

            const TaskStatus path[] = {TaskStatus::InProgress, TaskStatus::Review, TaskStatus::Done};
            for (TaskStatus step : path) {
                if (static_cast<int>(step) > static_cast<int>(s.status)) break;
                core.tasks().move(id, step);
                tick();
            }
        }

        // Dependencies are added last so the cards can already sit in their columns
        // "Integrate Critical Path..." and "Construct GoogleTest..." wait on "Develop Weighted Scoring..."
        core.tasks().addDependency(ids[1], ids[2]); tick();
        core.tasks().addDependency(ids[5], ids[2]); tick();

        // Four sample pins: different categories, one with no task, tags on each
        core.pins().create(PinCategory::HandoffNote,
                           "Columns and drag-and-drop are in. Card context menu and keyboard moves are still open.",
                           {"ui", "board"}, memberId["Mai"], ids[3]);
        tick();
        core.pins().create(PinCategory::FeedbackBlocker,
                           "Waiting on the team's final weights before tuning the skill score.",
                           {"slicer", "scoring"}, memberId["Omar"], ids[2]);
        tick();
        core.pins().create(PinCategory::ReadyForTesting,
                           "Syncer is ready for review. Please run it against a clean clone first.",
                           {"git", "events"}, memberId["Zeyad"], ids[4]);
        tick();
        core.pins().create(PinCategory::ArchitectureDecision,
                           "Events stay append-only. Projections rebuild all state on start-up.",
                           {"architecture", "events"}, memberId["Adam"], std::nullopt);
        tick();
    } catch (const std::exception& e) {
        qWarning() << "Demo seed failed:" << e.what();
    }
}


} // namespace demo
