# GenyCaster application bootstrap and module boundaries

The ui/app directory contains the Qt application shell. Protocol implementations
belong under `src/`, and application bootstrap is not an alternative protocol
or signer implementation.

## Directory ownership

Only `main.cpp` remains directly under `ui/app/`; it owns the Qt
composition and the controller lifetime boundary. The following directories
contain cohesive groups of cooperating sources, not one header/source pair
per directory.

| Folder | Responsibility |
| --- | --- |
| `bootstrap/` | Startup policy, providers, signed routes, wallet, fonts and shutdown |
| `runtime/` | GUI binding, composition, shutdown and Qt forward declarations |
| `social/` | Feed rows, view counts, channels and GenyPulse activity |
| `identity/` | OpenProof connections and browser identity approval |
| `network/` | Location, link metadata and node selection |
| `browser/` | Browser reactions, signing bridge, cast payload and emoji recovery |
| `miniapp/` | Host, catalog, tasks, runtime and native browser |
| `media/` | Image provider, emojis, Android icons and media host |
| `data/` | Local storage and live market insights |
| `settings/` | Provider profiles and text-selection policy |
| `update/` | Updater, package verification and installer handoff |
| `platform/` | Desktop single-instance and browser wheel input |
| `diagnostics/` | Explicit smoke tests and snapshots |

Use concise basenames within a group, such as `social/feed_rows.cpp`
or `miniapp/runtime.cpp`. Do not derive public Qt class names or QML
context names from source basenames. The QML engine must be destroyed
**before** controllers referenced by its bindings; preserve this ownership
in `main.cpp`.

The application now prepares one owned `RuntimeConfiguration` snapshot before
starting its background worker. The named fields distinguish Read providers,
Write destinations, ephemeral API credentials, signer attestation and wallet
approval. `makeRuntime(RuntimeConfiguration)` forwards this snapshot to the
existing validated runtime factory; it is not a second implementation of
protocol routing, signing, or session authorization.

A successfully constructed runtime is attached to the existing
`runtime/bindings.cpp` Qt presentation facade using a borrowed
`RuntimeAttachment` value. It must run on the GUI thread in this order:
transfer the runtime to its owner, connect the presentation/read/write and
media controllers, publish runtime readiness, and then restore or explicitly
open the requested identity. Failed construction publishes an error without
binding a partial session. The facade keeps its MOC-compatible header;
moving QObject-facing declarations into a named module would violate the
existing Qt build boundary. Never infer write permission or signer approval
from this GUI attachment step.

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
  boundaries. A small `runtime/qt_fwd.hpp` bridge contains Qt type forward
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
