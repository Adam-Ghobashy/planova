function(planova_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test planova_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

planova_add_test(tst_Skeleton)
planova_add_test(tst_FileEventStore)
planova_add_test(tst_EventLog)
planova_add_test(tst_ProjectService)
# --- S2 ---
planova_add_test(tst_MemberService)
planova_add_test(tst_Velocity)
planova_add_test(tst_GitService)
# --- end S2 ---