# Building on macOS arm64 (M2) — DRAFT, NOT YET VERIFIED

> TSD §1.2 says to record the working recipe once it works on the M2. Everything below is
> the planned recipe, written on Linux. Aan: run it, fix what breaks, then delete this banner.

## 1. Native arm64 check (TSD pitfall 15)

```sh
uname -m                                             # must print arm64
which brew                                           # must be /opt/homebrew/bin/brew
cmake --system-information | grep CMAKE_SYSTEM_PROCESSOR   # must print arm64
```
If any says `x86_64`, you are in a Rosetta shell or using Intel Homebrew (`/usr/local`).

## 2. Tools

```sh
xcode-select --install          # AppleClang
brew install cmake ninja libomp
python3 -m venv .venv && .venv/bin/pip install pybind11 pytest scikit-build-core numpy clang-format
```

## 3. Configure, build, test

```sh
export PATH="$PWD/.venv/bin:$PATH"
cmake --preset debug -DPython_EXECUTABLE="$PWD/.venv/bin/python" \
      -DOpenMP_ROOT="$(brew --prefix libomp)"
cmake --build --preset debug && ctest --preset debug && python -m pytest
```
Check the CMake output for `Building SERIAL`: if present, OpenMP was not found.

## 4. If FindOpenMP fails with AppleClang

```sh
brew install llvm
cmake --preset debug -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" ...
```
(Homebrew LLVM ships its own OpenMP runtime.) A serial build is acceptable for development.

## 5. Things that differ from the Intel PC

- `-march=x86-64-v2` is not passed (x86 only). `-ffp-contract=off` is passed on both.
- `rydtd.platform_tag()` returns `macos-arm64-appleclangNN`; cache keys differ from Linux on purpose.
- Sanitizers: `cmake --preset asan`. LeakSanitizer is not supported on Apple Silicon, so
  `ASAN_OPTIONS=detect_leaks=1` must be dropped there (see `Makefile` target `asan`).
- Profiling: Instruments → Time Profiler (TSD §9), not `perf`.
- Results from the two machines must not share a figure until W24 passes.
