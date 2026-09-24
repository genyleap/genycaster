# GenyCaster Data & Storage v22.1 — macOS Clang/Qt Build Fix

## Trigger

A real Apple Silicon macOS build using Qt 6.11.1 and Clang 22 reached
`ui/mobile/data_storage_controller.cpp` and failed with 16 overload-resolution errors.
The failure was not visible in the earlier Linux release container because that environment
could not compile the Qt UI target.

## Root cause

`data_storage_controller.cpp` declared two Qt conversion wrappers in its anonymous namespace:

- `mediaCategoryId(storage_policy::MediaCategory) -> QString`
- `networkClassId(storage_policy::NetworkClass) -> QString`

The imported C++ module `farcaster.presentation.media.storage_policy` already exports functions
with those exact names and parameter types, returning `std::string_view`.

Calls in the controller were unqualified. Because their arguments are types from the
`storage_policy` namespace, C++ argument-dependent lookup also considered the exported module
functions. The local Qt wrappers and the exported policy functions therefore formed an
ambiguous overload set. Clang 22 correctly rejected every affected call.

## Fix

The Qt-only conversion wrappers are now named:

- `qtMediaCategoryId(...)`
- `qtNetworkClassId(...)`

All controller call sites use the renamed wrappers. The canonical exported policy API remains
unchanged, so no persisted key, cache directory, provider descriptor, QML contract, or public
C++ module API changes.

The names also make the boundary explicit: the storage-policy module owns stable IDs as
`std::string_view`; the Qt presentation adapter only converts those IDs to `QString`.

## Regression guard

`tools/check_data_storage_contract.py` now rejects future `QString mediaCategoryId(...)` or
`QString networkClassId(...)` wrappers in the controller and requires the ADL-safe Qt wrapper
names. This statically protects the exact compiler failure reported by the macOS build.

## Verification performed in the release environment

The following checks pass after the fix:

- `tools/check_architecture.py`
- `tools/check_async_resource_contract.py`
- `tools/check_data_storage_contract.py`
- `tools/check_media_upload_contract.py`
- `tools/check_r5_release_contract.py`
- `tools/check_r7_release_contract.py`
- `cmake --preset core-debug`
- `cmake --preset strict-self-hosted`
- `cmake --preset minimal-core`
- `cmake --preset provider-hypersnap`
- Clang ADL regression reproducer: original same-name wrapper fails as ambiguous; renamed Qt wrapper compiles successfully.

The full Qt 6.11.1 macOS build cannot be executed in the Linux packaging container because the
matching Apple/Qt SDK is unavailable there. The reported Clang ambiguity itself is removed by
construction and protected by the new contract guard, but the macOS build must be rerun on the
reporting machine to discover any subsequent target-specific errors.

## Scope

This is a compile hotfix only. Data & Storage behavior, persistence schema, cache layout,
retention rules, download policy, usage accounting, and QML UX are unchanged from v22.
