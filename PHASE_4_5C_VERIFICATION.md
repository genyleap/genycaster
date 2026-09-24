# Phase 4.5C Verification

## Result

**NOT VERIFIED** on the required final matrix in this environment.

## Evidence obtained

- `python3 tools/check_architecture.py .` — PASS.
- Clean `core-debug` configure — PASS with GCC 14.2.0; build reaches project modules but GCC 14 ICEs in `cp/module.cc:write_location` while compiling `farcaster.application.read.service` / `farcaster.storage.read`.
- Clean `provider-hypersnap` configure — PASS; build reaches storage modules and hits the same GCC 14 module ICE before provider completion.
- `ui-debug` configure — NOT AVAILABLE because Qt 6 is not installed in the execution environment.
- Required Homebrew Clang 22.1.8 and Qt 6.11.1 are not present, so strict QML lint and the macOS UI/runtime smoke tests cannot be executed here.

## Required final-device matrix

Re-run from a clean tree on the pinned macOS toolchain:

```text
core-debug             configure + build + ctest
provider-hypersnap     configure + build + ctest
strict-self-hosted     configure + build + ctest
minimal-core           configure + build + ctest
ui-debug               configure + build + ctest
farcaster_client_qmllint_strict
```

A VERIFIED label must not be applied until all commands pass on that toolchain.
