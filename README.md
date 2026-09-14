# sony-wh-cli

A fast, scriptable command-line tool and C++20 library for controlling Sony
WH/WF headphones and earbuds — Ambient Sound, Noise Cancelling, Equalizer,
Clear Bass, DSEE, and battery status — over Bluetooth, without the mobile app.

This is a standalone extraction of the CLI and its protocol/transport/core
libraries from [Sony Device Center](https://github.com/marconvcm/sony-device-center)
(which also ships a Qt desktop app and `sonyd` background daemon). This repo
has no dependency on that one or on Qt — everything needed to talk to the
headphones lives here, so `libs/sony-core`, `libs/sony-protocol`, and
`libs/sony-transport` can be dropped into another project (a status bar
widget, a Quickshell/Wayland shell integration, etc.) without dragging in
a GUI.

## Usage

```
sony-wh-cli devices
sony-wh-cli info
sony-wh-cli battery
sony-wh-cli anc on
sony-wh-cli ambient 10
sony-wh-cli eq preset bass-boost
sony-wh-cli dsee on
```

Run `sony-wh-cli --help` for the full command list. If `sonyd` (from
sony-device-center) is running, the CLI talks to it over its local IPC
socket; otherwise it falls back to a direct Bluetooth session (`--direct`
forces this and refuses to run alongside a live `sonyd`, since both would
fight over the same connection).

## Repository layout

```
main.cpp                    the sony-wh-cli binary
libs/sony-transport/         Bluetooth transport abstraction + platform connectors
libs/sony-protocol/          Sony's binary protocol (V1 and V2), framing, device state
libs/sony-core/              device service, IPC client/server, JSON protocol
third_party/nlohmann/        vendored nlohmann/json (header-only)
tests/                       Catch2 unit tests for the three libraries
```

`libs/sony-transport/legacy/` holds the original Bluetooth connector
implementations (Linux/BlueZ+D-Bus, macOS/IOBluetooth, Windows/WinRT) this
project was built on — see the "References & Prior Art" section of the
[sony-device-center README](https://github.com/marconvcm/sony-device-center)
for where that code originally came from.

## Building from source

Requires a C++20 compiler, CMake 3.14+, and (on Linux) `libdbus-1-dev` and
`libbluetooth-dev`.

```
git clone https://github.com/thietala/sony-wh-cli.git
cd sony-wh-cli
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Running tests

```
cmake -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Supported devices

See the [device compatibility matrix](https://github.com/marconvcm/sony-device-center#-supported-devices)
in sony-device-center — this CLI supports whatever the shared protocol
libraries support.

## License

MIT — see [LICENSE](LICENSE).
