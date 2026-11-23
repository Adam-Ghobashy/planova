add_library(planova_ui STATIC
    theme/ThemeManager.cpp
    theme/GlassBackdrop.cpp
    app/CoreBridge.cpp
    app/MainWindow.cpp
    app/Sidebar.cpp
    welcome/WelcomePage.cpp
    welcome/ProjectCardWidget.cpp
    ../resources/planova.qrc
    welcome/SetupPage.cpp
    welcome/SetupMemberRow.cpp
    welcome/StepperWidget.cpp
    welcome/StartDialog.cpp
    welcome/EditProjectDialog.cpp
    welcome/DeleteProjectDialog.cpp

)
target_include_directories(planova_ui PUBLIC .)
target_link_libraries(planova_ui PUBLIC Qt6::Widgets Qt6::Svg planova_core)

# --- S1 ---
target_sources(planova_ui PRIVATE
    app/DemoData.cpp
    app/DamagedFilesMessage.cpp
)
# --- end S1 ---

# --- S2 ---
target_sources(planova_ui PRIVATE
    team/TeamSkillsPage.cpp
    git/GitHubIntegrationPage.cpp
)
# --- end S2 ---

# --- S3 ---
target_sources(planova_ui PRIVATE
    board/ComplexityDotsWidget.cpp
    board/NewTaskDialog.cpp
    board/PhaseBoardPage.cpp
    board/TaskCardWidget.cpp
    board/TaskColumnWidget.cpp
)
# --- end S3 ---

# --- S4 ---
target_sources(planova_ui PRIVATE
    slicer/WeightRebalance.cpp
    slicer/WeightSlidersPanel.cpp
    slicer/AssignmentTableModel.cpp
    slicer/AssigneeDelegate.cpp
    slicer/ChaosSlicerPage.cpp
    slicer/PizzaWidget.cpp
    slicer/SliceTimeline.cpp
    slicer/SlicerTableDelegate.cpp
)
# --- end S4 ---

# --- S5 ---
target_sources(planova_ui PRIVATE
    pins/DeclineDialog.cpp
    pins/LeavePinDialog.cpp
    pins/PinCardWidget.cpp
    pins/PinColumnWidget.cpp
    pins/PinsPage.cpp
    pins/StyledComboBox.cpp
)
# --- end S5 ---

# --- S6 ---
target_sources(planova_ui PRIVATE
    home/HomePage.cpp
    tracking/LeaderboardModel.cpp
    tracking/TrackingPage.cpp
    widgets/BarChartWidget.cpp
    widgets/DonutWidget.cpp
    widgets/ModalDialog.cpp
    widgets/StatCardWidget.cpp
)
# --- end S6 ---

# --- Splash ---
target_sources(planova_ui PRIVATE
    app/SplashTimeline.cpp
    app/SplashWindow.cpp
    widgets/HandwritingPainter.cpp
)
# --- end Splash ---

add_executable(planova WIN32 app/main.cpp)
target_link_libraries(planova PRIVATE planova_ui)

# The icon of planova.exe (Explorer, taskbar, Alt+Tab). The .rc is generated so it can hold the absolute path.
if(WIN32)
    set(PLANOVA_RC ${CMAKE_CURRENT_BINARY_DIR}/planova.rc)
    file(WRITE ${PLANOVA_RC} "IDI_ICON1 ICON \"${CMAKE_SOURCE_DIR}/resources/icons/planova.ico\"
")
    target_sources(planova PRIVATE ${PLANOVA_RC})
endif()

# Copy the Qt DLLs next to planova.exe so it opens with a double-click, outside Qt Creator.
if(WIN32)
    add_custom_command(TARGET planova POST_BUILD
        COMMAND Qt6::windeployqt --no-translations $<TARGET_FILE:planova>)
endif()