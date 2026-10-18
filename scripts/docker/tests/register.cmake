# Docker-only probe: add targets after the original project has loaded Qt/shared.
cmake_language(DEFER CALL include "${CMAKE_SOURCE_DIR}/scripts/docker/tests/CMakeLists.txt")
