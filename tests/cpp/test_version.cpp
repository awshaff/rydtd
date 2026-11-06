// OWNER: agent
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "rydtd/version.hpp"

TEST_CASE("version is non-empty and dotted", "[smoke]") {
  const auto v = rtd::version();
  REQUIRE_FALSE(v.empty());
  REQUIRE(v.find('.') != std::string_view::npos);
}

TEST_CASE("platform_tag has os-arch-compiler form", "[smoke]") {
  const std::string tag = rtd::platform_tag();
  INFO(tag);
  REQUIRE(std::count(tag.begin(), tag.end(), '-') == 2);
  REQUIRE(tag.find("unknown") == std::string::npos);
}

TEST_CASE("arm64/x86_64 tag matches the compiler's target", "[smoke]") {
#if defined(__aarch64__)
  REQUIRE(rtd::platform_tag().find("arm64") != std::string::npos);
#elif defined(__x86_64__)
  REQUIRE(rtd::platform_tag().find("x86_64") != std::string::npos);
#endif
}
