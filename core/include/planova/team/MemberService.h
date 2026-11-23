#pragma once

#include <string>
#include "planova/common/Clock.h"
#include "planova/common/Queries.h"
#include "planova/common/Types.h"
#include "planova/store/EventLog.h"

class MemberService {
public:
    MemberService(EventLog& log, IClock& clock, const IMemberQuery& members, const ITaskQuery& tasks);

    Id add(const std::string& name, const std::string& role, bool isLead);
    void update(const Id& id, const std::string& role, int understanding);
    void remove(const Id& id);
    void rate(const Id& id, SkillCategory category, int rating);
    void setSetting(const std::string& key, bool value);

private:
    void requireMember(const Id& id) const;

    EventLog& log_;
    IClock& clock_;
    const IMemberQuery& members_;
    const ITaskQuery& tasks_;
};