# Thin convenience wrapper around the real build system (CMake, in ./build).
# Not used by CI and not a Windows solution (see README) — just saves typing
# the same cmake invocations by hand on Linux/macOS.

BUILD_DIR ?= build
BUILD_TYPE ?= Release

.PHONY: all build configure install test clean help

all: build

# Phony on purpose: it runs every time, so BUILD_TYPE / BUILD_DIR given on the
# command line always take effect (`make BUILD_TYPE=Debug` on an existing Release
# tree really switches it). Re-running cmake is cheap, leaves cache settings you
# did not name alone (e.g. CMAKE_EXPORT_COMPILE_COMMANDS), and only rebuilds
# what a changed setting affects.
configure:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DBUILD_TESTING=ON

build: configure
	cmake --build $(BUILD_DIR) --parallel

# Deliberately does not depend on `build`: run `make build` as yourself
# first, then `sudo make install`. Making install depend on build would
# have `sudo` compile everything too, leaving root-owned files in build/.
install:
	cmake --install $(BUILD_DIR)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  make [build]        configure, then build"
	@echo "  make configure      (re)configure only; BUILD_TYPE=Debug|Release (default Release)"
	@echo "                      and BUILD_DIR=<dir> apply every time"
	@echo "  sudo make install   install the built binaries (run 'make build' first)"
	@echo "  make test           build, then run the test suite"
	@echo "  make clean          remove the build directory"
