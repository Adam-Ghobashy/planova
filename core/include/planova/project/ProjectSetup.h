#pragma once
#include "planova/common/Types.h"
#include "planova/slicer/ScoringWeights.h"
#include <map>
#include <string>
#include <vector>

// Everything the setup page collects. PlanovaCore::setupProject() validates it and writes it in one go.
struct SetupMember {
    std::string name;
    std::string role;
    bool isLead = false;
    std::map<SkillCategory, int> ratings; // 1 to 5 per skill, a missing skill stays unrated
};

struct ProjectSetup {
    std::string name;
    std::string team;
    std::string seModel;                // "Agile", "Scrum", "Waterfall" or "Spiral"
    std::string idea;
    std::vector<SetupMember> members;
    planova::slicer::ScoringWeights weights;
    std::string repoUrl;                // empty means: do not connect Git now
    std::string branch = "main";
};
