# Architecture

`wol` is a small C++23 Linux CLI split into focused modules.

## Modules

- `types`: MAC/IPv4 parsing, formatting, host-order conversions, and magic packet construction.
- `config`: schema-specific TOML loading, validation, selection, and writing.
- `cli`: argument parsing, command dispatch, JSON output, text output, and exit-code mapping.
- `learn`: local-device learning through UDP ARP triggering, `/proc/net/arp`, and `getifaddrs`.
- `network`: UDP broadcast packet sending with `SO_BROADCAST`.
- `help`: built-in manual and version text.

## Runtime Model

The executable resolves `wol.toml` beside `/proc/self/exe` unless `--config` is passed. Wake commands read config, select a target, construct the Wake-on-LAN magic packet, and send it to the effective broadcast/port. `learn` may create a new config when the selected config path does not exist.

Default wake behavior follows Debian `wakeonlan` wire semantics: one UDP packet
to `255.255.255.255:9` with `SO_BROADCAST` enabled. The packet is the standard
6 bytes of `0xFF` followed by the target MAC address repeated 16 times.

## Dependency Policy

Production code uses only C++ standard library and Linux/POSIX APIs. GoogleTest is allowed only for tests. The release binary must be statically linked and verified before packaging.
