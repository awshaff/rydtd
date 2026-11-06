# OWNER: agent  (Aan: read this file top to bottom and annotate it with your own
# comments -- see docs/learning/phase0.md, exercise 1.)
#
# A CMake file is a *program that describes a build*. It does not compile anything
# itself; it generates Ninja/Make files, which then call g++/clang++.

cmake_minimum_required(VERSION 3.24)
project(rydtd VERSION 0.1.0 LANGUAGES CXX)

# ---- Language standard -------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)            # -std=c++20, not -std=gnu++20 (portable)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)    # lets clang-tidy see the real flags

option(RYDTD_WERROR "Treat warnings as errors (CI sets this ON)" ON)
option(RYDTD_BUILD_TESTS "Build Catch2 tests" ON)
option(RYDTD_BUILD_PYTHON "Build the pybind11 module" ON)
option(RYDTD_OPENMP "Use OpenMP if available (else serial build)" ON)

# TSD 1: on macOS the toolchain must be native arm64. Under Rosetta CMake reports
# x86_64 and we would silently build an emulated Intel binary (TSD pitfall 15).
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
  message(WARNING "Running under Rosetta (x86_64 on an arm64 Mac). Use native arm64 "
                  "CMake/Homebrew under /opt/homebrew (TSD pitfall 15).")
endif()

# ---- Warnings: one INTERFACE target, linked by every target we own -----------
# Third-party code (Eigen, Catch2, pybind11) must NOT get these flags, otherwise
# their warnings would break our build. That is why we attach flags to a target
# instead of setting global CMAKE_CXX_FLAGS.
add_library(rydtd_warnings INTERFACE)
target_compile_options(rydtd_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
if(RYDTD_WERROR)
  target_compile_options(rydtd_warnings INTERFACE -Werror)
endif()
# TSD section 1 / pitfall 11: never -ffast-math or -Ofast. We do not add them, and
# the guard below fails the configure step if someone passes them from outside.
if(CMAKE_CXX_FLAGS MATCHES "fast-math|Ofast")
  message(FATAL_ERROR "-ffast-math/-Ofast are forbidden (TSD pitfall 11)")
endif()

# Release: -O3. The portable-baseline flag exists only on x86 (TSD 1.1); arm64 compilers
# already default to a safe baseline, and -march=x86-64-v2 would be an error there.
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  add_compile_options("$<$<CONFIG:Release>:-march=x86-64-v2>")
endif()

# TSD 1.1 / pitfall 13: forbid fused multiply-add contraction so the M2 (always FMA) and
# the Intel PC round the same way. Applies to ALL targets and configurations.
add_compile_options(-ffp-contract=off)

# Sanitizers (the `asan` preset sets RYDTD_SANITIZE=ON).
option(RYDTD_SANITIZE "Build with AddressSanitizer + UBSan" OFF)
if(RYDTD_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ---- Dependencies (pinned) ---------------------------------------------------
include(FetchContent)

# Eigen: header-only. We download it but skip its own CMake (SOURCE_SUBDIR points
# to a folder that does not exist) and make our own INTERFACE target.
FetchContent_Declare(eigen
  URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  SOURCE_SUBDIR cmake_skip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(eigen)
add_library(rydtd_eigen INTERFACE)
# SYSTEM => compiler treats these headers as third-party and stays quiet about them.
target_include_directories(rydtd_eigen SYSTEM INTERFACE ${eigen_SOURCE_DIR})

# OpenMP is optional (TSD 1.2). Not found => serial build with identical results.
set(RYDTD_HAVE_OPENMP OFF)
if(RYDTD_OPENMP)
  find_package(OpenMP COMPONENTS CXX)
  if(OpenMP_CXX_FOUND)
    set(RYDTD_HAVE_OPENMP ON)
  endif()
endif()
if(NOT RYDTD_HAVE_OPENMP)
  message(WARNING "Building SERIAL (OpenMP off or not found). On macOS: "
                  "brew install libomp, then -DOpenMP_ROOT=$(brew --prefix libomp)")
  # Otherwise -Werror turns the ignored `#pragma omp` lines into build failures.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rydtd_warnings INTERFACE -Wno-unknown-pragmas)
  else()
    target_compile_options(rydtd_warnings INTERFACE -Wno-source-uses-openmp)
  endif()
endif()

# ---- The library -------------------------------------------------------------
add_library(rydtd_core STATIC src/version.cpp src/platform.cpp)
target_include_directories(rydtd_core PUBLIC include)
target_compile_features(rydtd_core PUBLIC cxx_std_20)
target_link_libraries(rydtd_core
  PUBLIC  rydtd_eigen
  PRIVATE rydtd_warnings)
if(RYDTD_HAVE_OPENMP)
  target_link_libraries(rydtd_core PUBLIC OpenMP::OpenMP_CXX)
endif()
set_target_properties(rydtd_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- Tests -------------------------------------------------------------------
if(RYDTD_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/cpp)
endif()

# ---- Python module -----------------------------------------------------------
if(RYDTD_BUILD_PYTHON)
  FetchContent_Declare(pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v3.0.1
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(pybind11)
  pybind11_add_module(_rydtd bindings/py_rydtd.cpp)
  target_link_libraries(_rydtd PRIVATE rydtd_core rydtd_warnings)
  # Put the .so next to the Python package so `import rydtd` works in-tree.
  set_target_properties(_rydtd PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python/rydtd)
  install(TARGETS _rydtd LIBRARY DESTINATION rydtd)  # used by scikit-build-core
endif()
