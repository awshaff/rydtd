# OWNER: agent
include(FetchContent)
FetchContent_Declare(catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(catch2)

# One test executable per topic keeps failures easy to locate.
# Later phases add test_hamiltonian.cpp (W1), test_integrators.cpp (W2-W4), ...
add_executable(rydtd_tests
  test_version.cpp
  test_types.cpp)
target_link_libraries(rydtd_tests PRIVATE rydtd_core rydtd_warnings Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(rydtd_tests)
