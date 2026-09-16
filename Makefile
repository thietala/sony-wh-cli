# Thin convenience wrapper around the real build system (CMake, in ./build).
# Not used by CI and not a Windows solution (see README) — just saves typing
# the same cmake invocations by hand on Linux/macOS.

BUILD_DIR ?= build
BUILD_TYPE ?= Release

.PHONY: all build configure install test clean help

all: build

# Only reconfigures when build/ doesn't exist yet, so `make`/`make build`
# stays fast on repeat runs. Force a reconfigure with `make clean configure`.
$(BUILD_DIR)/CMakeCache.txt:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DBUILD_TESTING=ON

configure: $(BUILD_DIR)/CMakeCache.txt

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
	@echo "  make [build]        configure (if needed) and build"
	@echo "  make configure      configure only (skips if already configured)"
	@echo "  sudo make install   install the built binaries (run 'make build' first)"
	@echo "  make test           build, then run the test suite"
	@echo "  make clean          remove the build directory"
