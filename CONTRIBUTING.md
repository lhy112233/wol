# Contributing

Thanks for improving `wol`. Keep changes focused and preserve the core release
contract: the shipped archive contains only `wol` and `wol.toml`, and the
executable remains fully static.

## Local Verification

Run the fast loop before opening a pull request:

```bash
cmake --preset dev
cmake --build --preset format-check
cmake --build --preset tidy
cmake --build --preset dev
ctest --preset dev
```

For release-sensitive changes, also run:

```bash
cmake --preset release-static
cmake --build --preset release-static
ctest --preset release-static
cmake --preset package
cmake --build --preset package
```

## Style

- Use C++23 and the existing module boundaries.
- Keep runtime dependencies out of production code.
- Use GoogleTest for unit tests.
- Use `cmake --build --preset format` only when intentionally applying
  formatting.

## Pull Requests

Pull requests should include:

- A short summary of the behavior changed.
- The verification commands that were run.
- Notes about any impact on static linking, release packaging, or CLI output.
