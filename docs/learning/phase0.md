# Phase 0 — build system, tests, bindings

Goal of the phase: a project that builds on a clean machine, runs tests and imports from Python.
There is no physics yet.

## New C++ concepts (three)

1. **Translation units, headers and linking.** Each `.cpp` is compiled alone; headers are pasted in
   by `#include`; the linker joins the results. Analogy: in OSK you had one file and the judge ran
   it. Now there are many files, so you need `#pragma once` and a rule for who defines what.
   Needed here: `version.hpp` (declaration) vs `version.cpp` (definition).
2. **Targets and `PRIVATE`/`PUBLIC`/`INTERFACE` in CMake.** A target is a library or executable
   plus the flags and includes it needs. `PUBLIC` is passed on to whoever links the target,
   `PRIVATE` is used only inside it. Needed here: `-Werror` is `PRIVATE` to our code and must not
   leak into Eigen or Catch2.
3. **A test framework (Catch2).** `TEST_CASE` registers a function; `REQUIRE` aborts that test on
   failure. CTest runs each case as its own process, which is how ASan reports stay readable.

## Exercises

1. Open `CMakeLists.txt`, add a `#` comment on every block in your own words (this is your Phase 0
   deliverable). Mark anything you cannot explain and ask.
2. Add a function `std::string_view build_type() noexcept` to `version.hpp`/`version.cpp`
   returning `"debug"` or `"release"` (hint: `NDEBUG` is defined in release). Add a Catch2 test and
   expose it in `py_rydtd.cpp`. Which file changes need a rebuild of which targets?
3. Break something on purpose: add code that triggers `-Wconversion`, e.g.
   `int x = 3.5 * 2;` in `version.cpp`. Read the error. Then switch to the `asan` preset and
   read an ASan report from `int a[2]; a[2] = 1;` in a test (delete it afterwards).

## Common mistakes in this phase

- Forgetting `PATH` (the venv holds cmake/ninja): use `make`, or `export PATH=$PWD/.venv/bin:$PATH`.
- Putting a function definition in a header without `inline` → "multiple definition" at link time.
- Using `using namespace` in a header; everything lives in `namespace rtd`.
- Editing files in `build/`: it is generated and git-ignored; `make clean` wipes it.
- Python imports the *old* `.so` after you changed C++: rebuild first.

## OSK habit → research habit (applies now)

| OSK habit | Research habit |
| --- | --- |
| `bits/stdc++.h`, `using namespace std;` | exact includes, own namespace `rtd` |
| One file, run once | headers + sources + CMake targets |
| Judge says AC | Catch2/pytest/CTest are the judge |
