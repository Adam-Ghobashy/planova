function(planova_add_test name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE Qt6::Test planova_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

function(planova_add_ui_test name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE Qt6::Test planova_ui)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

planova_add_test(tst_FileEventStore)
planova_add_test(tst_EventLog)
planova_add_test(tst_ProjectService)
planova_add_test(tst_PlanovaCoreProjects)
# --- S2 ---
planova_add_test(tst_MemberService)
planova_add_test(tst_Velocity)
planova_add_test(tst_GitService)
# --- end S2 ---
# --- S3 ---
planova_add_test(tst_TaskFactory)
planova_add_test(tst_TaskStateMachine)
planova_add_test(tst_TaskGraph)
planova_add_test(tst_TaskService)
planova_add_test(tst_PlanovaCoreTasks)
# --- end S3 ---
# --- S4 ---
planova_add_test(tst_ScoringWeights)
planova_add_test(tst_WeightedStrategy)
planova_add_test(tst_PizzaPortions)
planova_add_test(tst_SlicerService)
planova_add_ui_test(tst_WeightRebalance)
planova_add_ui_test(tst_AssignmentTableModel)
planova_add_ui_test(tst_AssigneeDelegate)
planova_add_ui_test(tst_PizzaWidget)
planova_add_ui_test(tst_ChaosSlicerPage)
planova_add_ui_test(tst_SliceTimeline)
planova_add_ui_test(tst_WeightSlidersPanel)
planova_add_test(tst_PlanovaCoreSlicer)
# --- end S4 ---
# --- S1 ---
planova_add_ui_test(tst_ThemeManager)
planova_add_ui_test(tst_Contrast)
planova_add_ui_test(tst_ThemeRepaint)
planova_add_ui_test(tst_ModalDialog)
planova_add_ui_test(tst_SplashTimeline)
# --- end S1 ---
# --- S6 ---
planova_add_test(tst_ProgressCalculator)
planova_add_test(tst_Leaderboard)
planova_add_test(tst_HomeService)
planova_add_ui_test(tst_LeaderboardModel)
# --- end S6 ---
# --- S5 ---
planova_add_ui_test(tst_LeavePinDialog)
planova_add_ui_test(tst_RelativeTime)
# --- end S5 ---
