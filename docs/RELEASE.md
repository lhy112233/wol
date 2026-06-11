# Release

## Prerequisites

Fedora:

```bash
sudo dnf install cmake ninja-build gcc-c++ glibc-static libstdc++-static gtest-devel binutils file tar gzip curl
```

Debian/Ubuntu:

```bash
sudo apt install cmake ninja-build g++ libc6-dev libgtest-dev binutils file tar gzip curl
```

Distribution package names for static C++ runtime files may vary.

SBOM generation uses Syft. In GitHub Actions, the workflow installs Syft with
`anchore/sbom-action/download-syft@v0`. For local manual releases, install Syft
from Anchore's release packages or install script:

```bash
curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh -s -- -b ~/.local/bin
```

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
4. Generates `dist/wol-linux-x86_64.tar.gz.sha256`.
5. Generates `dist/wol-linux-x86_64.spdx.json` from the final package directory.
6. Creates a GitHub artifact attestation for the tarball.
7. Creates or updates the GitHub Release for the tag.
8. Uploads the tarball, checksum, and SBOM.

Manual releases can still be created locally with the GitHub CLI after the
checksum and SBOM files exist:

```bash
gh release create v1.1 \
  dist/wol-linux-x86_64.tar.gz \
  dist/wol-linux-x86_64.tar.gz.sha256 \
  dist/wol-linux-x86_64.spdx.json \
  --title "wol v1.1" \
  --notes-file dist/release-notes.md
```

## Release Maintainer Checklist

Before publishing a tag:

```bash
cmake --preset dev
cmake --build --preset format-check
cmake --build --preset tidy
cmake --build --preset dev
ctest --preset dev
cmake --preset release-static
cmake --build --preset release-static
ctest --preset release-static
cmake --preset package
cmake --build --preset package
```

Confirm the runtime archive remains minimal:

```bash
tar -tzf dist/wol-linux-x86_64.tar.gz
```

Expected:

```text
wol
wol.toml
```

Confirm the checksum locally:

```bash
cd dist
sha256sum wol-linux-x86_64.tar.gz > /tmp/wol-linux-x86_64.tar.gz.sha256
sha256sum -c /tmp/wol-linux-x86_64.tar.gz.sha256
```

Generate local release sidecar files before a manual upload:

```bash
cd ..
sha256sum dist/wol-linux-x86_64.tar.gz > dist/wol-linux-x86_64.tar.gz.sha256
syft scan dir:build/package/package -o spdx-json=dist/wol-linux-x86_64.spdx.json
```

After the GitHub Release workflow completes, confirm the release assets include:

```text
wol-linux-x86_64.tar.gz
wol-linux-x86_64.tar.gz.sha256
wol-linux-x86_64.spdx.json
```

Verify the published tarball:

```bash
sha256sum -c wol-linux-x86_64.tar.gz.sha256
gh attestation verify wol-linux-x86_64.tar.gz --repo lhy112233/wol
```
