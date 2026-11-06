# Entry points from TSD section 1. Tools come from the project venv (.venv).
VENV := $(CURDIR)/.venv/bin
export PATH := $(VENV):$(PATH)
PRESET ?= release
# LeakSanitizer is unsupported on Apple Silicon (docs/build_macos.md).
LEAKS := $(if $(filter Darwin,$(shell uname -s)),detect_leaks=0,detect_leaks=1)

.PHONY: build test validate figures bench all format tidy asan clean

build:
	cmake --preset $(PRESET) -DPython_EXECUTABLE=$(VENV)/python
	cmake --build --preset $(PRESET)

test: build
	ctest --preset $(PRESET)
	python -m pytest

asan:
	cmake --preset asan -DPython_EXECUTABLE=$(VENV)/python
	cmake --build --preset asan
	ASAN_OPTIONS=$(LEAKS) ctest --preset asan

format:
	find include src bindings tests apps -name '*.[ch]pp' 2>/dev/null | xargs clang-format -i

tidy: build
	find src -name '*.cpp' | xargs clang-tidy -p build/$(PRESET)

# Filled in by later phases:
validate figures bench all:
	@echo "target '$@' not implemented yet (see CLAUDE.md work order)"; exit 1

clean:
	rm -rf build
