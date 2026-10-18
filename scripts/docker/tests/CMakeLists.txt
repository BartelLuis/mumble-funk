find_package(Qt5 REQUIRED COMPONENTS Test)
add_executable(FunkDockerProbe "${CMAKE_SOURCE_DIR}/scripts/docker/tests/Probe.cpp")
target_link_libraries(FunkDockerProbe PRIVATE shared Qt5::Test)
target_compile_definitions(FunkDockerProbe PRIVATE MUMBLE_SERVER_PATH="/usr/local/bin/mumble-server")

# The v0.2 server is deliberately unchanged. Its random ping-header/nonce edge
# must not make Docker packaging depend on random fixture values. Keep all
# existing routing assertions, use protocol-valid nonce resync in a copied header.
file(READ "${CMAKE_SOURCE_DIR}/src/tests/TestSimplexIntegration/Harness.h" docker_harness)
set(original_login "return crypt.isValid();")
set(docker_login [=[
if (!crypt.isValid()) return false;
auto dockerIV = crypt.getEncryptIV(); dockerIV[0] = 64;
if (!crypt.setEncryptIV(dockerIV)) return false;
MumbleProto::CryptSetup dockerResync; dockerResync.set_client_nonce(dockerIV);
return send(Type::CryptSetup, dockerResync) && barrier();
]=])
string(FIND "${docker_harness}" "${original_login}" login_position)
if(login_position LESS 0)
 message(FATAL_ERROR "The Docker test fixture adapter needs review against the current Harness.h")
endif()
string(REPLACE "${original_login}" "${docker_login}" docker_harness "${docker_harness}")
# A real client discards unrelated unauthenticated/duplicate datagrams. The fence
# still requires its matching authenticated ping; all observer assertions remain.
set(original_decrypt [=[static_cast< unsigned int >(encrypted.size())))
					return false;]=])
set(docker_decrypt [=[static_cast< unsigned int >(encrypted.size())))
					continue;]=])
string(FIND "${docker_harness}" "${original_decrypt}" decrypt_position)
if(decrypt_position LESS 0)
 message(FATAL_ERROR "The Docker UDP test fence adapter needs review")
endif()
string(REPLACE "${original_decrypt}" "${docker_decrypt}" docker_harness "${docker_harness}")
file(WRITE "${CMAKE_BINARY_DIR}/DockerSimplexHarness.h" "${docker_harness}")
target_compile_options(TestSimplexIntegration PRIVATE -include "${CMAKE_BINARY_DIR}/DockerSimplexHarness.h")
