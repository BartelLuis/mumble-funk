# The planner is intentionally test-only until audible ordering is resolved.
add_executable(TestSquelchFoundation TestSquelchFoundation.cpp
    ${CMAKE_SOURCE_DIR}/src/murmur/SquelchPlan.cpp
    ${CMAKE_SOURCE_DIR}/src/murmur/SimplexController.cpp)
set_target_properties(TestSquelchFoundation PROPERTIES AUTOMOC ON)
target_link_libraries(TestSquelchFoundation PRIVATE shared Qt5::Test audio-feedback)
target_compile_definitions(TestSquelchFoundation PRIVATE SQUELCH_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/audio")
add_test(NAME TestSquelchFoundation COMMAND $<TARGET_FILE:TestSquelchFoundation>)
set_tests_properties(TestSquelchFoundation PROPERTIES TIMEOUT 120)
