# Known issues

## Open branches (not yet merged to `main`)

- **`fix/v1-battery-reply-timeout`** — fixes a real CI failure: the test
  fixture `ReplyTransport` didn't answer V1's battery opcode (`0x10`), so
  every connect to a V1-named device (`WH-1000XM3`) burned 3 real seconds
  of unanswered-query timeouts. Fine on Linux (9.01s, no margin) but tipped
  a test over its 10s limit on GitHub's macOS runners. Fixed at the root —
  affected tests dropped from ~9s to ~0.02s, full suite from ~56s to ~41s.

- **`ci/warnings-as-errors`** — turns on `-Wall -Wextra -Werror` (GCC/Clang)
  and `/W4 /WX` (MSVC) for this project's own targets, and fixes what that
  found on Linux (including one real bug: `to_string(SonyModel)` had no
  case for `WF1000XM6`, silently returning `"Unknown"`).
  **Only verified on Linux/GCC.** The Windows (MSVC `/W4`) and macOS
  (AppleClang) legs are untested locally and may surface more warnings
  that need fixing before this can merge cleanly — see "Testing
  Windows/macOS locally" below.

## Known flaky test

- `ProtocolV1: sets noise control with V1 packet layout` has failed once
  with "Timeout waiting for ACK" when run as part of the full suite,
  despite passing reliably (4/4) in isolation. Uses its own scripted
  `FakeTransport`, not `ReplyTransport`, so it's unrelated to the fix
  above. Likely a tight ACK-wait timing margin under load rather than a
  logic bug. Not yet root-caused — re-run failures in this test before
  assuming a real regression.

## Testing Windows/macOS locally

CI failing only on Windows/macOS almost always means a toolchain
difference (MSVC and AppleClang warn about different things than GCC), not
a Linux-specific fix. Two ways to reproduce it without waiting on
GitHub's runners:

### 1. Build it yourself on that OS (simplest)

This repo needs no GUI toolkit, so the setup is minimal:

**Windows** — install [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/)
(Desktop C++ workload) and [CMake](https://cmake.org/download/), then from
a "Developer Command Prompt for VS 2022":
```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

**macOS** — install Xcode Command Line Tools (`xcode-select --install`)
and CMake (`brew install cmake`), then the same three commands (the `-C`
flag is harmless on the single-config Makefile/Ninja generators CMake
picks by default there).

This is the exact sequence `.github/workflows/ci.yml` runs, so any
warning or failure it produces is what CI will produce too.

### 2. Register your hardware as a self-hosted GitHub Actions runner

If you want `git push` to actually run on your own Windows PC or Mac
(faster than waiting in GitHub's queue, and useful if you're iterating a
lot), install the runner from **Settings → Actions → Runners → New
self-hosted runner** on `thietala/sony-wh-cli`, then change the matrix
entry's `runs-on` from `windows-latest`/`macos-latest` to your runner's
labels (e.g. `[self-hosted, Windows]`).

Both repos here are public, so GitHub-hosted runner minutes are free and
unlimited regardless — self-hosting is about iteration speed, not cost.
The one hard rule: **never** attach a self-hosted runner to a public
repo's `pull_request` trigger. A fork can open a PR that runs arbitrary
code on your machine; keep self-hosted runners scoped to `push` /
`workflow_dispatch` only.
