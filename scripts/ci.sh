#!/usr/bin/env bash
# CI script: warnings-as-errors build + tests on every available compiler, then ASan.
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$PWD/.venv/bin:$PATH"
PY="-DPython_EXECUTABLE=$PWD/.venv/bin/python"

for cxx in g++ clang++; do
  if ! command -v "$cxx" >/dev/null; then echo "== skipping $cxx (not installed)"; continue; fi
  dir="build/ci-${cxx}"
  echo "== $cxx release"
  cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$cxx" "$PY"
  cmake --build "$dir"
  ctest --test-dir "$dir" --output-on-failure
done

echo "== asan/ubsan (debug)"
cmake --preset asan "$PY"
cmake --build --preset asan
if [[ "$(uname -s)" == Darwin ]]; then LEAKS=0; else LEAKS=1; fi  # no LSan on Apple Silicon
ASAN_OPTIONS=detect_leaks=$LEAKS ctest --preset asan

echo "== python"
python -m pytest
echo "== format check"
find include src bindings tests -name '*.[ch]pp' | xargs clang-format --dry-run --Werror
echo "CI OK"
