# Release

## Prerequisites

Fedora:

```bash
sudo dnf install cmake ninja-build gcc-c++ glibc-static libstdc++-static gtest-devel binutils file tar gzip
```

Debian/Ubuntu:

```bash
sudo apt install cmake ninja-build g++ libc6-dev libgtest-dev binutils file tar gzip
```

Distribution package names for static C++ runtime files may vary.

## Build And Package

```bash
cmake --preset package
cmake --build --preset package
```

The package target performs static verification before writing:

```text
dist/wol-linux-x86_64.tar.gz
```

## Required Verification

The package target fails unless:

- `ldd wol` reports it is not a dynamic executable.
- `readelf -d wol` has no `NEEDED` entries.
- `file wol` reports a statically linked executable.

The archive must contain exactly:

```text
wol
wol.toml
```

## GitHub Release Workflow

Pushing a tag named `v*` runs `.github/workflows/release.yml`.

The workflow:

1. Builds and tests the `release-static` preset.
2. Builds the `package` preset.
3. Verifies static linkage and archive contents.
4. Creates or updates the GitHub Release for the tag.
5. Uploads `dist/wol-linux-x86_64.tar.gz`.

Manual releases can still be created locally with the GitHub CLI:

```bash
gh release create v1.1 dist/wol-linux-x86_64.tar.gz --title "wol v1.1"
```
