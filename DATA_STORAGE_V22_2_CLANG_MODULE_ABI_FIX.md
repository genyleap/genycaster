# GenyCaster Data & Storage v22.2 — Clang C++ Module ABI Boundary Fix

## Trigger

A real Apple Silicon macOS build using Qt 6.11.1 and Clang 22 compiled every source file in the
`farcaster_client` target, including `remote_image_provider.cpp`, and then failed at the final link
step with this unresolved constructor:

```text
RemoteImageProvider::RemoteImageProvider(
    bool,
    std::shared_ptr<farcaster::presentation::media::storage_policy::RuntimeState>)
```

This occurred after the v22.1 ADL compile fix, so the failure is a distinct linker/ABI boundary
issue rather than another overload-resolution problem.

## Root cause

`RuntimeState` is owned by the named C++ module:

```text
farcaster.presentation.media.storage_policy
```

The Qt textual headers `data_storage_controller.hpp` and `remote_image_provider.hpp` nevertheless
forward-declared the same-spelled class outside that named module:

```cpp
namespace farcaster::presentation::media::storage_policy { class RuntimeState; }
```

That is not a safe way to expose a named-module-owned type through an ordinary textual header.
With Clang's module ownership model, the textual declaration and the declaration attached to the
named module can have different ownership/ABI identity even though their qualified C++ spelling
looks identical in diagnostics.

The consequence here was subtle:

- `main.cpp` saw the constructor declaration through the textual header;
- `remote_image_provider.cpp` imported the real storage-policy module before defining the
  constructor;
- both translation units compiled;
- the final linker could not match the constructor expected by `main.cpp.o` to the constructor
  emitted by `remote_image_provider.cpp.o`.

The issue was visible only at the final link stage because the module-owned type appeared as a
constructor parameter. A return type does not normally participate in the Itanium C++ function
mangling, which is why the analogous `DataStorageController::runtimeState()` boundary did not
produce the same immediate undefined-symbol diagnostic.

## Fix

The concrete module-owned `RuntimeState` no longer appears in either Qt/MOC textual header.
Instead, the Qt composition boundary carries shared ownership through an opaque handle:

```cpp
std::shared_ptr<void>
```

Specifically:

- `DataStorageController::runtimeState()` became
  `DataStorageController::runtimeStateHandle()` and returns `std::shared_ptr<void>`;
- `RemoteImageProvider` now accepts `std::shared_ptr<void>` in its textual public constructor;
- `main.cpp` passes `dataStorageController.runtimeStateHandle()`;
- only `remote_image_provider.cpp`, after importing
  `farcaster.presentation.media.storage_policy`, converts the opaque handle back to
  `std::shared_ptr<storage_policy::RuntimeState>` using `std::static_pointer_cast`.

This keeps the concrete named-module type entirely inside module-aware implementation units while
preserving the same shared lifetime and thread-safe runtime state. No raw owning pointer is
introduced, and no cache/data-storage behavior changes.

## Why not forward-declare or duplicate the module type

The project explicitly requires named C++ modules for project-owned production boundaries and only
permits `.hpp` at Qt/MOC or similar textual compatibility boundaries. Re-declaring a module-owned
class in such a header undermines that ownership model. The v22.2 boundary therefore type-erases
only the cross-boundary ownership token rather than creating a second classic header definition or
moving `RuntimeState` out of its module.

## Regression guard

`tools/check_data_storage_contract.py` now rejects either Qt textual header if it:

- forward-declares `storage_policy::RuntimeState`;
- mentions `storage_policy::RuntimeState` in the textual ABI;
- stops exposing `runtimeStateHandle()` as the opaque shared handle;
- stops accepting the opaque handle in `RemoteImageProvider`; or
- stops recovering the concrete type only inside `remote_image_provider.cpp`.

This protects the exact module/textual-boundary failure mode that reached the macOS linker.

## Verification performed in the packaging environment

After the v22.2 fix, all source-level release guards pass:

```text
tools/check_architecture.py             PASS
tools/check_async_resource_contract.py  PASS
tools/check_data_storage_contract.py    PASS
tools/check_media_upload_contract.py    PASS
tools/check_r5_release_contract.py       PASS
tools/check_r7_release_contract.py       PASS
```

All four available CMake configure profiles also pass:

```text
cmake --preset core-debug          PASS
cmake --preset strict-self-hosted  PASS
cmake --preset minimal-core        PASS
cmake --preset provider-hypersnap  PASS
```

The Linux packaging environment does not contain the user's Qt 6.11.1 macOS SDK or Apple linker,
so the exact final macOS link must be rerun on the reporting machine. The source-level ABI mismatch
that caused the undefined constructor is removed by construction: the constructor now has the
identical textual signature `RemoteImageProvider(bool, std::shared_ptr<void>)` at declaration,
call site, and definition.

## Scope

v22.2 is a linker/ABI boundary hotfix. It does not alter:

- cache directory layout;
- persisted Data & Storage keys;
- retention rules;
- Wi-Fi/cellular automatic-download policy;
- media-category accounting;
- Tap-to-load behavior;
- NFT/media presentation behavior; or
- QML UX.

The only runtime-semantic change is elimination of the unsafe named-module type leak through Qt
textual headers.
