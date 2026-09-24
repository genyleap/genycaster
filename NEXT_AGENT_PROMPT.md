# Continuation Prompt — Current Consolidation

`STATUS.md` is the canonical current-state snapshot. Read it together with `AGENTS.md`, `DESIGN.md`, and the relevant ADR/feature documents before changing production code.

The source tree has advanced beyond the old Phase 5F/5G gate: public-data adapters, Mini Apps, data-storage UI, media lifecycle, later visual-system work, and additional reliability iterations are already present. Do **not** use the previous “do not begin Phase 6” wording as a project-state lock. Historical phase verification files remain evidence for those increments.

## Current priorities

1. Keep all architecture/localization/storage/media/release guards green.
2. Preserve provider-neutral Domain/Application and local-first write/read semantics.
3. Continue behavior-preserving presentation decomposition; do not redesign core to reduce GUI complexity.
4. Run clean Clang 22 + Qt 6.11.x build/test and GUI smoke/lint before claiming release acceptance.
5. Close Mini App untrusted-content hardening and per-platform live signer/write acceptance before production release.
6. Update `STATUS.md` whenever current scope, supported toolchains, schema level, or release blockers change.

## Non-negotiable boundaries

Localization stays in the Qt/presentation boundary. Market valuation/provider semantics stay outside QML. Provider identities stay in adapters/composition. Protocol signer, custody wallet, App-FID attestation, and authenticated session remain distinct authorities. Recoverable C++ failures use the project Result/`std::expected` path; resource ownership follows RAII and the C++ Core Guidelines.
