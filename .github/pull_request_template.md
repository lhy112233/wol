## Summary

- 

## Verification

- [ ] `cmake --preset dev`
- [ ] `cmake --build --preset format-check`
- [ ] `cmake --build --preset tidy`
- [ ] `cmake --build --preset dev`
- [ ] `ctest --preset dev`
- [ ] `cmake --preset release-static`
- [ ] `cmake --build --preset release-static`
- [ ] `ctest --preset release-static`
- [ ] `cmake --preset package`
- [ ] `cmake --build --preset package`

## Release Impact

- [ ] Runtime archive still contains only `wol` and `wol.toml`
- [ ] Fully static binary verification still passes
