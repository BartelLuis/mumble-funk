# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file at the root of the Mumble source tree.

add_executable(TestVoiceDiagnostics
    TestVoiceDiagnostics.cpp
    ${CMAKE_SOURCE_DIR}/src/murmur/VoiceDiagnostics.cpp
)
set_target_properties(TestVoiceDiagnostics PROPERTIES AUTOMOC ON)
find_package(Threads REQUIRED)
target_include_directories(TestVoiceDiagnostics PRIVATE ${CMAKE_SOURCE_DIR}/src/murmur)
target_link_libraries(TestVoiceDiagnostics PRIVATE shared Qt5::Test Threads::Threads)
target_compile_definitions(TestVoiceDiagnostics PRIVATE MUMBLE_SERVER_PATH="$<TARGET_FILE:mumble-server>")
add_dependencies(TestVoiceDiagnostics mumble-server)
add_test(NAME TestVoiceDiagnostics COMMAND $<TARGET_FILE:TestVoiceDiagnostics>)
set_tests_properties(TestVoiceDiagnostics PROPERTIES TIMEOUT 90)
