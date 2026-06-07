# Testing

## Fast Development Loop

```bash
cmake --preset dev
cmake --build --preset format-check
cmake --build --preset dev
cmake --build --preset tidy
ctest --preset dev
```

`format-check` is non-mutating and should be used in verification. `format`
rewrites source files and should only be run when intentionally applying
formatting:

```bash
cmake --build --preset format
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
- `clang-tidy` runs analyzer, practical bug/performance/readability checks, and C++ Core Guidelines checks. POSIX-boundary checks that are too noisy for socket and system APIs are disabled in `.clang-tidy`.

## Manual LAN Test

Use a powered-on local IPv4 device:

```bash
./wol learn desktop 192.168.1.20
./wol --dry-run desktop
./wol desktop
```

`learn` requires the target to be online and on the same local IPv4 network.
