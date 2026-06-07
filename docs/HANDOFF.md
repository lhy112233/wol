# Handoff

## Current Product Shape

`wol` is a static Linux x86_64 Wake-on-LAN CLI. It supports human-friendly text output and stable JSON output for agents/scripts.

## Most Important Commands

```bash
wol
wol desktop
wol --dry-run --json desktop
wol --list --json
wol --check-config --json
wol --print-config-path
wol learn desktop 192.168.1.20
```

## Exit Codes

- `0`: success
- `1`: runtime failure, including invalid/missing config, unknown target, ARP learning failure, or network send failure
- `2`: command-line usage error

## Safe Change Guidance

- Keep production dependencies at zero.
- Add tests before changing parser, config, learn, or CLI output behavior.
- Preserve stable JSON keys unless intentionally changing the automation contract.
- Do not add files to the release archive beyond `wol` and `wol.toml`.
- Re-run release-static and package verification before handing off a release.

## Known Environment Notes

On Fedora, full static C++ linking requires both `glibc-static` and `libstdc++-static`. GoogleTest comes from `gtest-devel` and is used only by test targets.
