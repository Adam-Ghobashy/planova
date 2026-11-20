#pragma once
// Demo fakes for the standalone planova_pins_demo target. Not used by the real app.
#include <QDateTime>
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>

#include "planova/common/Queries.h"
#include "planova/pins/PinBoardApi.h"

namespace planova::s5::demo {

class DemoMemberQuery : public IMemberQuery {
public:
    DemoMemberQuery()
    {
        const char* names[] = {"Ava Reed", "Ben Cole", "Cara Diaz", "Dan Ellis", "Eva Frost", "Finn Gray"};
        const char* roles[] = {"Backend", "Frontend", "Storage", "UI/UX", "Integration", "Testing"};
        for (int i = 0; i < 6; ++i) {
            MemberDto m;
            m.id = "m" + std::to_string(i + 1);
            m.name = names[i];
            m.role = roles[i];
            m.isLead = i == 0;
            m.understanding = 70;
            m.velocity = 1.0;
            members_.push_back(m);
        }
    }
    std::vector<MemberDto> all() const override { return members_; }
    std::optional<MemberDto> byId(const Id& id) const override
    {
        for (const auto& m : members_) if (m.id == id) return m;
        return std::nullopt;
    }
private:
    std::vector<MemberDto> members_;
};

class DemoTaskQuery : public ITaskQuery {
public:
    DemoTaskQuery()
    {
        const char* titles[] = {"Event store replay", "Task card layout", "Dependency graph check",
                                "Slicer weights panel", "Pins board polish", "Leaderboard table"};
        for (int i = 0; i < 6; ++i) {
            TaskDto t;
            t.id = "t" + std::to_string(i + 1);
            t.title = titles[i];
            t.skill = SkillCategory::Backend;
            t.priority = Priority::Medium;
            t.complexity = 3;
            t.estHours = 6;
            t.status = TaskStatus::ToDo;
            t.assignee = "m" + std::to_string(i + 1);
            tasks_.push_back(t);
        }
    }
    std::vector<TaskDto> all() const override { return tasks_; }
    std::optional<TaskDto> byId(const Id& id) const override
    {
        for (const auto& t : tasks_) if (t.id == id) return t;
        return std::nullopt;
    }
private:
    std::vector<TaskDto> tasks_;
};

// In-memory implementation of the three S5 interfaces. Mirrors the rules the backend will follow.
class DemoPinBoard : public IPinBoardQuery, public IPinCommands, public IConflictCommands {
public:
    explicit DemoPinBoard(const ITaskQuery& tasks) : tasks_(tasks)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        constexpr qint64 min = 60 * 1000, hour = 60 * min;
        add(PinCategory::HandoffNote, "Front end pages are ready. The service layer can load handoff pins now.",
            {"frontend", "handoff"}, "m2", "t2", now - 10 * 1000);
        add(PinCategory::FeedbackBlocker, "Storage task clashes with the board work. Needs a team decision.",
            {"conflict", "storage"}, "m3", "t3", now - 15 * min);
        add(PinCategory::ReadyForTesting, "Event store replay is done. Please test with two machines.",
            {"testing", "events"}, "m4", "t1", now - 70 * min);
        add(PinCategory::ArchitectureDecision, "Each event is its own file named by timestamp and hash, so git never merges two.",
            {"architecture", "git"}, "m1", "t1", now - 3 * hour);
        add(PinCategory::HandoffNote, "Slicer weights default to 50, 30 and 20.",
            {"slicer"}, "m6", "t4", now - 4 * hour);
        add(PinCategory::HandoffNote, "Colour tokens checked on every screen in light mode.",
            {"design", "tokens"}, "m5", std::nullopt, now - 26 * hour);
        add(PinCategory::ReadyForTesting, "Leaderboard ranks by points, then efficiency, then name.",
            {"stats"}, "m2", "t6", now - 5 * hour);
        add(PinCategory::ArchitectureDecision, "Pages read through query interfaces and refresh on change.",
            {"architecture"}, "m3", std::nullopt, now - 2 * hour);
        pins_[1].comments.push_back({"c1", "m1", "Which task is it clashing with?", now - 10 * min});
        pins_[1].comments.push_back({"c2", "m3", "The board layout task. We both need the same hours.", now - 6 * min});
        pins_[3].comments.push_back({"c3", "m5", "Good call, keeps the sync simple.", now - 2 * hour});
    }

    // Contract: newest first.
    std::vector<PinView> openPins() const override
    {
        std::vector<PinView> out = pins_;
        std::stable_sort(out.begin(), out.end(),
                         [](const PinView& a, const PinView& b) { return a.pin.ts > b.pin.ts; });
        return out;
    }

    Id create(PinCategory c, const std::string& text, const std::vector<std::string>& tags,
              const Id& authorId, std::optional<Id> taskId) override
    {
        requireText(text);
        PinView v;
        v.pin.id = nextId();
        v.pin.category = c;
        v.pin.text = text;
        v.pin.tags = tags;
        v.pin.authorId = authorId;
        v.pin.ts = QDateTime::currentMSecsSinceEpoch();
        v.taskId = std::move(taskId);
        pins_.insert(pins_.begin(), v);
        return v.pin.id;
    }
    void comment(const Id& pinId, const Id& authorId, const std::string& text) override
    {
        requireText(text);
        find(pinId).comments.push_back({"c" + std::to_string(++seq_), authorId, text,
                                        QDateTime::currentMSecsSinceEpoch()});
    }
    void resolve(const Id& pinId, const Id&) override { erase(pinId); }
    void edit(const Id& pinId, const Id&, const std::string& text, const std::vector<std::string>& tags,
              std::optional<Id> taskId) override
    {
        requireText(text);
        PinView& v = find(pinId);
        v.pin.text = text;
        v.pin.tags = tags;
        v.taskId = std::move(taskId);
    }
    void move(const Id& pinId, const Id&, PinCategory to) override { find(pinId).pin.category = to; }
    void remove(const Id& pinId, const Id&) override { erase(pinId); }

    Id decline(const Id& taskId, const Id& memberId, const std::string& reason) override
    {
        requireText(reason);
        const auto task = tasks_.byId(taskId);
        if (!task || !task->assignee) throw std::runtime_error("Task is not assigned");
        return create(PinCategory::FeedbackBlocker, "Declined: " + reason, {"conflict"}, memberId, taskId);
    }

private:
    void add(PinCategory c, std::string text, std::vector<std::string> tags, Id author,
             std::optional<Id> task, qint64 ts)
    {
        PinView v;
        v.pin.id = nextId();
        v.pin.category = c;
        v.pin.text = std::move(text);
        v.pin.tags = std::move(tags);
        v.pin.authorId = std::move(author);
        v.pin.ts = ts;
        v.taskId = std::move(task);
        pins_.push_back(std::move(v));
    }
    static void requireText(const std::string& t)
    {
        if (std::all_of(t.begin(), t.end(), [](unsigned char ch) { return std::isspace(ch); }))
            throw std::invalid_argument("Text must not be empty");
    }
    Id nextId() { return "p" + std::to_string(++seq_); }
    PinView& find(const Id& id)
    {
        for (auto& v : pins_) if (v.pin.id == id) return v;
        throw std::invalid_argument("Unknown pin");
    }
    void erase(const Id& id)
    {
        const auto it = std::find_if(pins_.begin(), pins_.end(), [&](const PinView& v) { return v.pin.id == id; });
        if (it == pins_.end()) throw std::invalid_argument("Unknown pin");
        pins_.erase(it);
    }

    const ITaskQuery& tasks_;
    std::vector<PinView> pins_;
    int seq_ = 0;
};

}  // namespace planova::s5::demo
