// OWNER: agent
#include <string>

#include "rydtd/version.hpp"

namespace rtd {

std::string platform_tag() {
#if defined(__APPLE__)
  constexpr const char* os = "macos";
#elif defined(__linux__)
  constexpr const char* os = "linux";
#else
  constexpr const char* os = "unknown";
#endif

#if defined(__aarch64__) || defined(__arm64__)
  constexpr const char* arch = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  constexpr const char* arch = "x86_64";
#else
  constexpr const char* arch = "unknown";
#endif

  // Order matters: clang defines __GNUC__ too, and AppleClang defines __clang__.
#if defined(__clang__)
#if defined(__apple_build_version__)
  const std::string cc = "appleclang" + std::to_string(__clang_major__);
#else
  const std::string cc = "clang" + std::to_string(__clang_major__);
#endif
#elif defined(__GNUC__)
  const std::string cc = "gcc" + std::to_string(__GNUC__);
#else
  const std::string cc = "unknowncc";
#endif
  return std::string(os) + "-" + arch + "-" + cc;
}

}  // namespace rtd
