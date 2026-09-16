# sony-wh-cli

A fast, scriptable command-line tool and C++20 library for controlling Sony
WH/WF headphones and earbuds — Ambient Sound, Noise Cancelling, Equalizer,
Clear Bass, DSEE, and battery status — over Bluetooth, without the mobile app.

`libs/sony-core`, `libs/sony-protocol`, and `libs/sony-transport` can also be
dropped into another project (a status bar widget, a Quickshell/Wayland
shell integration, etc.) independently of the CLI and daemon binaries.

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

Run `sony-wh-cli --help` for the full command list.

`sony-wh-cli` is a thin client: it requires `sonyd` to be running and talks
to it over a local IPC socket, so any number of callers (this CLI, a status
bar widget, a script) can share one Bluetooth connection without fighting
over it. Start the daemon first:

```
sonyd &
sony-wh-cli battery
```

Pass `--direct` to `sony-wh-cli` to bypass the daemon for a one-off direct
Bluetooth session instead; it refuses to run alongside a live `sonyd`, since
both would fight over the same connection. IPC is Unix-only, so on Windows
`sonyd` isn't functional and `--direct` is the only option.

`sonyd` connects to the headphones only when a command needs it, and
disconnects again after about 15 seconds of inactivity. The headphones'
proprietary control channel only accepts one companion connection at a time,
so this lets your phone's own Sony app connect whenever the CLI isn't
actively in use, instead of `sonyd` holding the link forever. The first
command after a period of inactivity pays for a fresh Bluetooth connection
and so takes a bit longer than the ones right after it.

### Running sonyd as a service (Linux)

`cmake --install` also installs a `systemd --user` unit
(`packaging/systemd/sonyd.service`) to `<prefix>/lib/systemd/user/`. If you
installed to a prefix systemd's user unit search path covers (`/usr` or
`/usr/local`), enable it with:

```
systemctl --user daemon-reload
systemctl --user enable --now sonyd
```

At any other prefix, symlink the file into `~/.config/systemd/user/` first:

```
mkdir -p ~/.config/systemd/user
ln -s <prefix>/lib/systemd/user/sonyd.service ~/.config/systemd/user/sonyd.service
systemctl --user daemon-reload
systemctl --user enable --now sonyd
```

## Repository layout

```
main.cpp                    the sony-wh-cli binary
sonyd.cpp                   the sonyd background daemon binary
packaging/systemd/           systemd --user unit for running sonyd as a service
libs/sony-transport/         Bluetooth transport abstraction + platform connectors
libs/sony-protocol/          Sony's binary protocol (V1 and V2), framing, device state
libs/sony-core/              device service, IPC client/server, JSON protocol
third_party/nlohmann/        vendored nlohmann/json (header-only)
tests/                       Catch2 unit tests for the three libraries
```

`libs/sony-transport/legacy/` holds the original Bluetooth connector
implementations (Linux/BlueZ+D-Bus, macOS/IOBluetooth, Windows/WinRT) this
project was built on, originally written for sony-device-center.

## Building from source

Requires a C++20 compiler, CMake 3.14+, and (on Linux) `libdbus-1-dev` and
`libbluetooth-dev`.

```
git clone https://github.com/thietala/sony-wh-cli.git
cd sony-wh-cli
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

Installs `sony-wh-cli` and `sonyd` to `<prefix>/bin` (default prefix
`/usr/local`; skip `sudo` if you passed
`-DCMAKE_INSTALL_PREFIX=$HOME/.local` at configure time).

## Running tests

```
cmake -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Supported devices

Any Sony WH/WF headphone or earbud model that speaks Sony's V1 or V2
Bluetooth protocol, as implemented in `libs/sony-protocol`.

## Roadmap

- Test on a wider range of hardware — only a handful of models have been
  verified so far, and neither Windows nor macOS has been tested on real
  hardware at all (see [KNOWN_ISSUES.md](KNOWN_ISSUES.md)).
- `sonyd`'s IPC is Unix-socket only; Windows needs a named-pipe equivalent
  (and a real background-service story to go with it, e.g. a Windows
  Service via the SCM). macOS already shares the Unix-socket code path but
  still needs that hardware testing pass.
- `reset` (`0xf8 0x09 0x00`, Sony's "Initialize headphone settings") and
  `factoryreset` (`0xf8 0x09 0x01` — same sub-type, wipes the pairing
  itself and requires re-pairing) are both confirmed working via packet
  capture and real-hardware testing, but only on a WH-1000XM5 — every
  other model reports both as unsupported until someone captures and
  confirms the opcodes on that device too.

## License

MIT — see [LICENSE](LICENSE).
