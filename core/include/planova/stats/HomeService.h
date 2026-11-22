#pragma once

#include <planova/common/Dtos.h>
#include <planova/common/Queries.h>

#include <utility>
#include <vector>

namespace planova::s6 {

struct DashboardData {
    int total = 0;
    int completed = 0;
    int inProgress = 0;
    int review = 0;
    int remaining = 0;
    std::vector<std::pair<Id, int>> contributionHours;
    std::vector<PinDto> pins;
};

class HomeService {
public:
    static constexpr int kMaxPins = 4;

    HomeService(ITaskQuery& tasks, IMemberQuery& members, IPinQuery& pins);

    DashboardData dashboard() const;

private:
    ITaskQuery& tasks_;
    IMemberQuery& members_;
    IPinQuery& pins_;
};

}