# GenyCaster application bootstrap and module boundaries

This directory contains the Qt application shell. Protocol implementations
belong under `src/`, and application bootstrap is not an alternative protocol
or signer implementation.

## Directory ownership

| Directory | Responsibility |
| --- | --- |
| `bootstrap/options` | Command-line parsing and safe public URL validation |
| `bootstrap/providers` | Proxy and hosted Read endpoint precedence |
| `bootstrap/profiles` | Independent Read/Write adapter registrations and ephemeral provider keys |
| `bootstrap/routes` | Selected protocol's signed-message destination and node candidates |
| `bootstrap/auth` | Relying-party identity, app FID, explicit signer attestation, and login relay/RPC |
| `bootstrap/fonts` | Bundled typefaces, script-specific fallbacks, and language selection |
| `bootstrap/context` | Validated non-owning QML controller names, font values, and platform flags |
| `bootstrap/monitoring` | Per-FID notification cursors and reconnect retry signals scoped to presentation lifetime |
| `bootstrap/worker` | Joining native initialization thread; does not own GUI controllers |
| `bootstrap/teardown` | Release QML roots only after producers stop and bootstrap joins |
| `desktop/instance` | Native per-user single-instance and deep-link activation |
| `web/wheel` | Browser wheel routing and modal-aware Qt Quick fallback |
| `update/install` | Verified package handoff to a platform installer or post-exit helper |
| `diagnostics/` | Explicit startup/snapshot tests, not production state ownership |

`main.cpp` is the composition entry point. It retains ownership of Qt
presentation controllers because the QML engine must be destroyed **before**
the objects referenced by its bindings. Do not move these owners across that
lifetime boundary merely to reduce the number of lines in `main.cpp`.

## Source and documentation conventions

- Keep source filenames lowercase and short: `auth.cppm`, `routes.cppm`,
  `worker.cpp`. Use folders to express responsibility.
- Use an exported `.cppm` interface with a private `.cpp` implementation
  for each non-QObject subsystem. Import its named module in consumers instead
  of including a relative parent-directory header.
- Put the standard project Doxygen file banner on **every new module**:
  `@file`, `@brief`, `@details`, `@author`, `@since`,
  `@copyright`, and `@license`. The creation date is the file's actual
  introduction date, not the build date. Document exported API preconditions,
  ownership, failure modes, and returned results.
- Qt QObject headers used by MOC remain textual `.hpp` compatibility
  boundaries. A small `modules/qt_fwd.hpp` bridge contains Qt type forward
  declarations in the global module fragment; do not export full Qt headers
  as module implementation internals.
- Use `Q_OS_WASM` for Qt platform guards. CMake toolchain branches use
  `EMSCRIPTEN`; code directly targeting Emscripten compiler/runtime
  intrinsics may use the corresponding toolchain feature guards as needed.
- Mention specific Qt or C++ versions only for a genuine API or build
  compatibility requirement, not as decorative comments.

## Security and protocol invariants

1. HyperSnap is the initial **Read** adapter only where deployment policy
   permits it. An explicit user selection and strict self-hosted mode win.
2. Read preferences do not authorize writes to another node. Canonical
   signed-message submissions use the selected compatible protocol origin
   unless the user explicitly selected a separate Write adapter.
3. Signer attestation requires its own explicitly configured trusted service.
   A Read gateway or canonical message-submission endpoint is **not** an
   attestation authority.
4. Credentials are ephemeral and associated with a validated adapter and
   HTTPS destination. Do not save API keys in QSettings, public provider
   JSON, debug logs, URLs, or bundled WebAssembly assets.
5. Stop background producers, join the bootstrap worker, and release QML
   roots before destruction of the controllers they reference.
6. Installer and updater code must verify downloaded artifacts and respect
   the OS package, signing, and user-consent boundaries.

The native regression targets in `tests/ui/` exercise these invariants.
`ci/verify_ci.py` also enforces the module layout, file banners, platform
guards, and basic security contracts. Passing these focused tests does not
replace a full platform-native build and installer smoke test.
