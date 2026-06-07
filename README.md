# wol

`wol` is a small dependency-free Wake-on-LAN CLI for Linux x86_64. It is built
for humans and automation agents: normal commands use readable text, and
supported commands can emit stable JSON with `--json`.

The release archive contains only two runtime files:

- `wol`
- `wol.toml`

The executable looks for `wol.toml` beside itself unless `--config <path>` is
provided.

## Build

The project uses CMake presets and Ninja.

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Build the fully static release:

```bash
cmake --preset release-static
cmake --build --preset release-static
ctest --preset release-static
```

Create the release archive:

```bash
cmake --preset package
cmake --build --preset package
```

The package target writes:

```text
dist/wol-linux-x86_64.tar.gz
```

It verifies that the executable is static before creating the archive:

- `ldd wol` must report that it is not dynamic.
- `readelf -d wol` must not contain `NEEDED` entries.
- `file wol` must report a statically linked executable.

## Build Prerequisites

Debian/Ubuntu:

```bash
sudo apt install cmake ninja-build g++ libc6-dev
```

Fedora:

```bash
sudo dnf install cmake ninja-build gcc-c++ glibc-static libstdc++-static gtest-devel binutils file tar gzip
```

Some distributions split static C++ runtime files into a separate package. If
`release-static` fails because `libstdc++.a` or `libc.a` is missing, install the
distribution package that provides those static libraries.

## Usage

Wake the default target:

```bash
./wol
```

Wake a named target:

```bash
./wol desktop
```

Validate and show what would be sent:

```bash
./wol --dry-run desktop
```

List targets:

```bash
./wol --list
```

Agent-friendly JSON:

```bash
./wol --list --json
./wol --dry-run --json desktop
./wol --check-config --json
./wol --print-config-path
```

Learn an online local IPv4 target:

```bash
./wol learn desktop 192.168.1.20
```

`learn` sends a small UDP probe, reads `/proc/net/arp`, computes the local
broadcast address from the matching interface, and updates `wol.toml`.

Show the full built-in manual:

```bash
./wol --help
```

## Config

```toml
default_target = "desktop"

[network]
port = 9
send_count = 3
interval_ms = 100
broadcast = "255.255.255.255"

[targets.desktop]
mac = "AA:BB:CC:DD:EE:FF"
ip = "192.168.1.20"
broadcast = "192.168.1.255"
port = 9
```

Target-level `broadcast` and `port` override `[network]` defaults.

## Exit Codes

- `0`: success
- `1`: runtime error, such as invalid config, unknown target, or send failure
- `2`: command-line usage error

## Troubleshooting

`learn` only works when the target is powered on, reachable, and on the same
local IPv4 network. If learning fails, contact the host first with ping or
another local network operation, then retry.

Wake failures are often caused by firmware, BIOS/UEFI, NIC, switch, or subnet
broadcast settings. Confirm Wake-on-LAN is enabled on the target and run
`./wol --dry-run <target>` to verify the selected MAC address, broadcast address,
and port.

## More Docs

- [Agent guide](AGENTS.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing](docs/TESTING.md)
- [Release](docs/RELEASE.md)
- [Handoff](docs/HANDOFF.md)
