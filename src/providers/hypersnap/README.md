# Hypersnap provider package

This package is the optional Hypersnap v2 adapter for the capability-driven native client.

Provider-specific URLs, query parameters, response JSON, raw cursors, HTTP semantics, and compatibility behavior terminate here. Application/domain code consumes only canonical capability contracts.

The implementation is intentionally split by responsibility:

- `config` — provider endpoint/descriptor configuration;
- `request` — URL/query construction and HTTP error mapping;
- `codec` — Hypersnap v2 JSON to canonical domain normalization;
- `continuation` — provider-private cursor binding/resolution;
- `registration` — capability registration metadata;
- `feed`, `profile`, `cast`, `social`, `identity` — capability adapters.

The current compatibility target is the Hypersnap v2 contract inspected at commit `2eee4c9f2a7833ce7971dfef028480abbe9c4720`.

The Qt composition uses the documented public node by default. Development and
self-hosted runs can override it without moving provider configuration into
Application or QML:

```text
GENYCASTER_HYPERSNAP_BASE_URL=http://127.0.0.1:3381 farcaster_client
farcaster_client --hypersnap-base-url=http://127.0.0.1:3381
```

The command-line value takes precedence over the environment variable.

Build with the `provider-hypersnap` CMake preset and run the provider contract test with CTest.
