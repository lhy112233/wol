# Testing

## Fast Development Loop

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Static Release Loop

```bash
cmake --preset release-static
cmake --build --preset release-static
ctest --preset release-static
```

## Coverage Areas

- GoogleTest unit tests cover parsing, formatting, magic packet layout, config roundtrips, validation, ARP parsing, subnet broadcast selection, CLI parsing, and help content.
- CTest CLI tests cover help/version, config checks, path printing, dry-run, listing, JSON modes, missing config, and unknown targets.
- `tests/CliBehavior.cmake` checks important command output content, not just exit status.

## Manual LAN Test

Use a powered-on local IPv4 device:

```bash
./wol learn desktop 192.168.1.20
./wol --dry-run desktop
./wol desktop
```

`learn` requires the target to be online and on the same local IPv4 network.
