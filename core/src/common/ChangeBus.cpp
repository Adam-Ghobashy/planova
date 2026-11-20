#include "planova/common/ChangeBus.h"

#include <algorithm>

int ChangeBus::subscribe(Fn fn)
{
    const int token = nextToken_++;
    subs_.emplace_back(token, std::move(fn));
    return token;
}

void ChangeBus::unsubscribe(int token)
{
    subs_.erase(std::remove_if(subs_.begin(), subs_.end(),
                               [token](const auto& s) { return s.first == token; }),
                subs_.end());
}

void ChangeBus::publish(const std::string& eventType)
{
    // Iterate a copy: a handler may subscribe, unsubscribe or publish again.
    const auto subs = subs_;
    for (const auto& s : subs)
        s.second(eventType);
}