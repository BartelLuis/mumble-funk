add_executable(TestSimplex TestSimplex.cpp
    ${CMAKE_SOURCE_DIR}/src/murmur/SimplexController.cpp
    ${CMAKE_SOURCE_DIR}/src/murmur/SimplexLogger.cpp)
set_target_properties(TestSimplex PROPERTIES AUTOMOC ON)
target_include_directories(TestSimplex PRIVATE ${CMAKE_SOURCE_DIR}/src/murmur)
target_link_libraries(TestSimplex PRIVATE shared Qt5::Test Threads::Threads)
add_test(NAME TestSimplex COMMAND $<TARGET_FILE:TestSimplex>)
set_tests_properties(TestSimplex PROPERTIES TIMEOUT 60)
