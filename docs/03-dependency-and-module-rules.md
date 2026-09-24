# 03 — Dependency and Module Rules

## Required dependency direction

Stable concepts do not depend on volatile mechanisms.

```text
presentation → application
capabilities → domain → foundation
application → stable contracts only
local-state ports → domain + foundation
sync → foundation
routing → capabilities + provider-neutral metadata + foundation
```

Infrastructure points inward:

```text
provider adapters → capability/domain contracts
storage adapters  → local-state/sync ports
transport adapters → provider-neutral transport ports
platform adapters → application ports
```

Concrete provider, SQLite, HTTP, JSON, Qt and OS mechanisms never become inward dependencies of domain/application contracts.

## Current top-level source layout

```text
src/
  foundation/
  domain/
  capabilities/
  provider/        # provider-neutral metadata only
  local_state/
  application/
  routing/
  sync/
  storage/
  transport/       # provider-neutral ports + concrete transport adapters
  presentation/

  providers/       # future concrete adapters only
  platform/        # future OS adapters only

ui/
  mobile/
  desktop/
  components/
  theme/
  motion/

tests/
  unit/
```

Do not create empty directories or targets solely to reserve future architecture. Optional feature modules are added when they own real behavior/contracts/tests.

## C++ module boundaries

Project-owned production boundaries use named modules.

Examples:

```text
farcaster.domain.identity        → namespace farcaster::domain::identity
farcaster.capability.model       → namespace farcaster::capability::model
farcaster.routing.registry       → namespace farcaster::routing::registry
farcaster.storage.continuation   → namespace farcaster::storage::continuation
```

Every exported namespace must mirror its dotted module identity exactly.

## Module source policy

- `.cppm`: exported declarations, concepts, lightweight templates, small compile-time values;
- `.cpp`: non-trivial algorithms, mutation, I/O and private implementation;
- `.hpp`: explicit documented Qt/C/ABI/third-party textual boundary only;
- `.h`: forbidden unless an external ecosystem requires it;
- never `import std;`;
- standard headers belong in the global module fragment;
- module interfaces are registered through `FILE_SET CXX_MODULES`.

## Forbidden dependencies

Architectural review/CI rejects shapes such as:

```text
domain → capabilities/providers/storage/routing/Qt
capabilities → providers/storage/routing/application
application → concrete providers/storage/routing/presentation
QML → provider/network/database APIs
```

## Qt boundary

```text
QML
 ↓
C++ presentation adapter
 ↓
Application use case/configuration
 ↓
Stable core contracts
```

Qt MOC may require a `.hpp` textual boundary, but this exception never authorizes classic header/source architecture elsewhere.

## CMake enforcement

Target linkage must reflect actual module imports. Avoid hidden reliance on transitive linkage and remove dependencies that are not owned by the target.
