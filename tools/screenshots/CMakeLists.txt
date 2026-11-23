add_executable(planova_screenshots main.cpp)
target_include_directories(planova_screenshots PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/ui)
target_link_libraries(planova_screenshots PRIVATE planova_ui)
