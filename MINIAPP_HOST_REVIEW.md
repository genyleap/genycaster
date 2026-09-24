# Internal Mini App host — implementation gate

Responsibility: launch Mini Apps in an application-owned native browser and implement a bounded subset of the Farcaster host RPC. Manifest resolution is separate from the browser adapter; no discovery provider is required.
Capability origin: Extension (host), Derived (public manifest metadata), Local (window/session).
Mandatory or optional: optional UI composition, existing `FARCASTER_ENABLE_MINIAPPS`; macOS WKWebView initially. Other platforms expose unavailable state without opening an external browser. Fresh macOS UI configurations default to ON; existing explicit/cached OFF values are preserved and need `-DFARCASTER_ENABLE_MINIAPPS=ON` to enable.
Public contract: QML launch/close intents and status via a Qt MOC adapter; `farcaster.runtime.miniapp` owns execution; the native browser has a documented textual Objective-C++ ABI boundary.
Canonical data/invariants: HTTPS launch origin, user FID/public display metadata, bounded JSON RPC; no provider DTOs, tokens, signer keys, or authenticated wallet identity in the page. Up to four live sessions are allowed, with exactly one foreground authority; separate-window presentation may keep peer windows visible. Context metadata is not authentication.
Dependencies: optional composition → Qt Network/GUI and macOS WebKit/AppKit. Core/domain/application do not depend on this feature.
Expected failures: disabled platform, invalid URL/manifest, transport/timeout/oversize, unsupported required capability/chain, unsupported RPC, blocked navigation, WebKit process termination.
Local-state representation: process-local launch/session state; nonpersistent WebKit website data store. No Mini App authentication/cookies shared across launches or accounts.
Offline behavior: deterministic load error with retry/close; no fabricated app data or automatic external fallback.
Provider policy and disablement: direct manifest resolution; no compatibility-provider dependency. CMake disablement removes WebKit/runtime composition.
Failover semantics: no browser/provider fallback; errors stay inside the app.
Tests proving the boundary: planned cases include OFF/non-Apple composition, exact-origin/main-frame RPC rejection, stale document replies, RPC limits, unsupported methods/manifest requirements, account-change teardown, ready timeout, renderer crash/retry, both card/button launch paths. Not executed: user explicitly requested no builds/tests.

## Compatibility scope

### User-selectable presentation gate

Responsibility: let users choose a bottom, left, or right native drawer, or a
separate resizable window. Capability origin: Local. Optional: existing Mini App
composition. Public contract: validated `presentationMode` preference on the Qt
controller; native browser creation receives a typed presentation value.
Canonical invariants: physical left/right never reverse with UI language; up to four
sessions with one foreground authority; mode changes apply on the next launch, not by reloading a
running app. Dependencies: existing Qt settings/host and native AppKit adapter.
Expected failures: invalid stored preferences fall back to window mode; absence
of a parent window falls back to a separate window. Local-state representation:
`miniapps/presentationMode` in the existing QSettings store. Offline behavior:
preference reads/writes work offline. Provider policy/failover: unchanged.
Verification cases: persist all four modes, invalid value, resize/move parent,
parent shutdown, backdrop/close dismissal, reduced motion, drawer bounds at small
sizes, preference changes without loss of active app state. No builds/tests run.

Drawers use a clipped native overlay within the main window rather than an
external frameless window. They share the same WebKit content and SDK transport
as window mode. Presentation choice lives in Settings → Appearance.

Current SDK surface includes context, capability/chain detection, `ready` (including `disableNativeGestures`), close, profile/cast navigation, `openMiniApp`, composeCast, back state, and the EIP-1193 wallet capability when a wallet bridge is available. `actions.addMiniApp` is intentionally not advertised because the Farcaster Add contract requires signed server events and notification plumbing that a local bookmark cannot substitute. Unsupported calls reject via Comlink's throw wire format instead of hanging. The client FID reuses the existing `FARCASTER_DEFAULT_APP_FID` product identity (866408 by default), never another client's FID.

The manifest now requires a structurally valid `accountAssociation` envelope, a custody/auth header, and an exact signed-domain match before execution. The cryptographic signature itself is not yet verified in this increment, so association data is still never presented as a verified author nor used to grant privileged operations. Required capabilities/chains are checked before execution. This is an internal host, not full Farcaster-host parity.

## Reference contracts inspected

- https://miniapps.farcaster.xyz/docs/specification
- https://miniapps.farcaster.xyz/docs/sdk/context
- https://github.com/farcasterxyz/miniapps/blob/main/packages/miniapp-sdk/src/endpoint.ts
- https://github.com/farcasterxyz/miniapps/blob/main/packages/miniapp-core/src/types.ts
- https://github.com/farcasterxyz/miniapps/blob/main/packages/miniapp-sdk/src/back.ts
- https://github.com/GoogleChromeLabs/comlink/blob/main/src/protocol.ts
- https://github.com/GoogleChromeLabs/comlink/blob/main/src/comlink.ts

Inspected 2026-09-07. No remote executable SDK shim is downloaded at runtime. The local adapter implements the documented wire protocol; web content remains untrusted.

## Delivery / remaining acceptance gates

### Framedl discovery correction (2026-09-07)

Responsibility: accept the existing `next` Mini App embed format as well as `1`,
and try the legacy metadata tag when the preferred tag is invalid. This is a
Derived metadata correction inside the existing resolver, not a new host feature.
Public contracts, dependencies, offline behavior and provider policy are unchanged.
Keep HTTPS/action/title/size validation; never treat legacy `vNext` POST frames as
executable Mini Apps. Canonical preview fields and the process-local cache remain
unchanged. Planned cases: `1`, `next`, invalid preferred tag plus valid fallback,
unknown version, legacy `vNext`, private/invalid launch URL. No tests executed.

Observed live: `https://framedl.com/` supplies both `fc:miniapp` and `fc:frame` with
version `next`, action `launch_miniapp`, image `/init-v2.png`, and launch URL
`https://framedl.com/app/v2`. The prior `version == "1"` check rejected both tags.
Its well-known manifest independently uses version `1`; these versions must not
be conflated. Reference: https://github.com/farcasterxyz/protocol/discussions/205

### Implementation notes

### Apple SDK / Clang 22 C++26 compatibility correction

The supplied build log reports mixed-enum arithmetic in Apple's deprecated
ColorSync constants, pulled in by AppKit. LLVM 22.1.8 declares this diagnostic as
`warn_conv_mixed_enum_types_cxx26`, `InGroup<EnumEnumConversion>, DefaultError`.
The Objective-C++ adapter locally suppresses that diagnostic only while importing
AppKit/WebKit in C++26 mode, with a diagnostic push/pop. Project-owned code remains
outside the suppression, C++26 remains required, and no SDK files or global
compiler flags are changed. This is an external-header compatibility correction,
not a change to host capabilities, state, dependencies or failure semantics.

Source reference: https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/include/clang/Basic/DiagnosticSemaKinds.td
Verification: supplied build log and source inspection only; no build/tests run.

### Runtime delivery notes

- Composition reuses the existing product App FID, including the CLI/environment override; zero/non-JavaScript-safe host IDs refuse launch.
- Card and button invoke the same internal path. The external-launch badge/icon is removed for Mini Apps; ordinary web links retain their existing behavior.
- Manifest loading has a 10-second request budget, 3 redirects and 64 KiB limit. Renderer readiness has a 20-second budget. RPC is limited to 32 KiB and 120 messages/second; malformed or unsupported operations cannot access arbitrary native members.
- Native window/renderer teardown is deferred out of RPC callbacks. Session and document generations reject delayed navigation intents. Reply delivery also checks the originating document token.
- A nonpersistent WebKit store isolates app sessions; no core wallet/provider credentials are installed. Camera/microphone, file upload and JavaScript native prompts are denied in this increment. Cross-origin top-level navigation and non-HTTPS/custom schemes are blocked, never forwarded to the OS.
- This is not complete web network isolation: URL filtering is not DNS pinning, and WebKit navigation delegates do not intercept all subresource/fetch traffic. DNS rebinding/private-network hardening and full adversarial testing remain release gates before treating this as a hardened arbitrary-app host.
- Cryptographic manifest-authorship verification, splash artwork/verified author UI, signed Add/notification server events, and non-macOS browser adapters remain incomplete. No claim of complete Farcaster compatibility.
- English and Persian UI strings are supplied; other catalogs currently carry English fallback text for these new messages.
- Final verification for this pass: architecture, async-resource, media-upload and UI-design guards pass; the isolated browser-wallet bridge configures, builds and its Mini App contract test passes. Full Qt Quick configure/runtime verification remains unavailable on the current Linux runner because Qt 6.6+ is not installed, so macOS AppKit/WebKit and QML runtime rendering remain release-machine verification gates.
