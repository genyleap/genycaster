# Changelog

This file is the human-readable entry point for publicly documented GenyCaster changes. The canonical, localized release metadata lives in `release-notes/`, and release tags must pass the validation process described in [ci/README.md](ci/README.md).

The project is in **beta**; a `rc` suffix identifies the release packaging channel, not a guarantee of production readiness.

## Unreleased

### Documentation and community preparation

- Added a first-page architecture overview and linked technical diagrams.
- Documented a reproducible performance-comparison protocol; no comparative benchmark numbers are published without evidence.
- Added security-reporting and contribution policies and GitHub community templates.

For current changes merged after the last tagged release, consult the Git history and open pull requests. An unreleased commit should not be represented as a published app update.

## v1.0.0-rc1 — Beta release candidate

See the canonical [release metadata](release-notes/v1.0.0-rc1.json) and [generated release notes](release-notes/v1.0.0-rc1.md).

This entry does not re-state historical release metrics or platform support beyond what is recorded in the validated release metadata. Platform acceptance and availability must be verified against the corresponding release artifacts.

## Reporting a change

When proposing an externally visible change, update its owning source/tests and the appropriate public documentation. Do not manually invent a separate version or duplicate localized release-note content; version/channel identity is configured in the top-level `CMakeLists.txt` and release notes are validated by `ci/release_gate.py`.
