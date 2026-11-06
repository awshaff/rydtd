// OWNER: agent
#include <catch2/catch_test_macros.hpp>

#include "rydtd/types.hpp"

TEST_CASE("Mat<4> is a stack-sized 4x4 complex matrix", "[smoke]") {
  rtd::Mat<4> m = rtd::Mat<4>::Identity();
  REQUIRE(m.trace() == rtd::cplx(4.0, 0.0));
  static_assert(sizeof(rtd::Mat<4>) == 16 * sizeof(rtd::cplx), "no heap, no padding");
}
