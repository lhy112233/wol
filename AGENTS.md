# Agent Guide

This repository builds `wol`, a fully static Linux x86_64 Wake-on-LAN CLI.

## Core Rules

- Runtime release must contain only `wol` and `wol.toml`.
- Production code must not add third-party runtime dependencies.
- Tests may use GoogleTest.
- Use CMake presets with Ninja:
  - `cmake --preset dev`
  - `cmake --build --preset dev`
  - `ctest --preset dev`
  - `cmake --preset release-static`
  - `cmake --build --preset release-static`
  - `ctest --preset release-static`
  - `cmake --preset package`
  - `cmake --build --preset package`
- Before claiming release readiness, verify `dist/wol-linux-x86_64.tar.gz` contains exactly `wol` and `wol.toml`.

## Important Interfaces

- `wol` wakes the default target.
- `wol <name>` wakes a named target.
- `wol learn <name> <ipv4>` learns MAC and broadcast information from local ARP/interface state.
- `--json` makes supported commands stable for automation.
- `--check-config` validates config without sending packets.
- `--print-config-path` prints the config path that would be used.

## Where To Look

- `include/wol/*.hpp`: public internal module interfaces.
- `src/cli.cpp`: command routing and JSON/text output.
- `src/config.cpp`: schema-specific TOML parser/writer and validation.
- `src/learn.cpp`: ARP parsing and local subnet broadcast selection.
- `src/network.cpp`: UDP broadcast sender.
- `tests/wol_tests.cpp`: GoogleTest unit tests.
- `tests/CliBehavior.cmake`: business-level CLI output checks.
- `docs/`: architecture, testing, release, and handoff notes.
