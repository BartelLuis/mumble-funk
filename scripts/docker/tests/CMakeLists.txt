find_package(Qt5 REQUIRED COMPONENTS Test)
add_executable(FunkDockerProbe "${CMAKE_SOURCE_DIR}/scripts/docker/tests/Probe.cpp")
target_link_libraries(FunkDockerProbe PRIVATE shared Qt5::Test audio-feedback)
target_compile_definitions(FunkDockerProbe PRIVATE MUMBLE_SERVER_PATH="/usr/local/bin/mumble-server")
