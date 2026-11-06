// OWNER: agent
#pragma once
#include <string>
#include <string_view>

namespace rtd {

/// Engine version as "major.minor.patch". Used in cache keys (TSD section 1) and
/// in every figure sidecar JSON (CLAUDE.md section 6). No units.
std::string_view version() noexcept;

/// Build platform tag, e.g. "linux-x86_64-gcc15" or "macos-arm64-appleclang16", from
/// compile-time macros only. Part of the cache key (TSD 1.3) and of result metadata,
/// so results from the two supported machines are never mixed by accident.
std::string platform_tag();

/// True if this build was compiled with OpenMP (TSD 1.2). Tests that need threads
/// (W21) skip when false.
constexpr bool has_openmp() noexcept {
#ifdef _OPENMP
  return true;
#else
  return false;
#endif
}

}  // namespace rtd
