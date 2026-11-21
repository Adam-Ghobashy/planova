add_library(planova_core STATIC
    src/PlanovaCore.cpp
    src/common/ChangeBus.cpp
    src/common/Ids.cpp
    src/project/ProjectProjection.cpp
    src/project/ProjectService.cpp
    src/store/FileEventStore.cpp
    src/store/EventLog.cpp
    include/planova/team/SkillCategory.h
    include/planova/team/Member.h
    include/planova/team/MemberProjection.h
    include/planova/team/MemberService.h
    include/planova/team/VelocityCalculator.h
    src/team/MemberService.cpp
    src/team/MemberProjection.cpp
    include/planova/git/Commit.h
    include/planova/git/GitProjection.h
    include/planova/git/GitService.h
    src/git/GitProjection.cpp
    src/git/GitService.cpp
)
target_include_directories(planova_core PUBLIC include)
target_link_libraries(planova_core PUBLIC Qt6::Core)

target_sources(planova_core PRIVATE
    src/slicer/WeightedScoringStrategy.cpp
    src/slicer/PizzaPortionCalculator.cpp
    src/slicer/SlicerService.cpp
    src/slicer/SlicerWeightsProjection.cpp
)

# --- S3 ---
target_sources(planova_core PRIVATE
    src/task/TaskFactory.cpp
    src/task/TaskGraph.cpp
    src/task/TaskProjection.cpp
    src/task/TaskService.cpp
    src/task/TaskStateMachine.cpp
)
# --- end S3 ---

# --- S6 ---
target_sources(planova_core PRIVATE
    src/stats/HomeService.cpp
    src/stats/LeaderboardCalculator.cpp
    src/stats/ProgressCalculator.cpp
    src/stats/StatsService.cpp
)
# --- end S6 ---

# --- S5 ---
target_sources(planova_core PRIVATE
    src/pins/PinProjection.cpp
    src/pins/PinService.cpp
)
# --- end S5 ---
