# Profile Fidelity and Media Rendering

Date: 2026-08-29

This hardening slice improves Profile and Cast presentation without changing the
provider-neutral Application/Domain boundaries.

## Canonical product identity

- Shipping configuration exposes the public Genycaster App identity as FID `256`
  and display name `Genycaster`.
- The desktop client activates App-FID attribution only when a custody-backed
  signer-attestation service is configured. The App-FID custody secret never
  ships in the Qt application.
- Existing user-scoped signers provisioned before this identity must be revoked
  and re-provisioned through the deployed attestation service before live casts
  can be attributed to Genycaster by the network.

## Profile fidelity

The canonical Profile model and SQLite cache now preserve:

- PFP URL;
- banner URL;
- bio;
- profile URL;
- location;
- follower count;
- following count.

The enriched Hypersnap v2 User object is the fast path. Missing presentation
fields fall back to protocol-native `userDataByFid` reads for PFP, display name,
bio, URL, location and banner. Banner fallback uses the current protocol slot
`USER_DATA_TYPE_BANNER = 11`; older value 10 is not used. Embedded feed/social author summaries may update
identity fields but cannot overwrite profile-detail metadata or social counts.
Migration v19 clears older polluted follower/following snapshots once so a live
Profile refresh can repopulate them.

## Media boundary

QML does not perform arbitrary remote network I/O. Remote PFP/banner/image-embed
URLs are converted to an `image://remote/` source and fetched by a C++
`QQuickImageProvider` through the existing Qt Network transport. The provider:

- accepts only HTTP(S);
- preserves the configured system/direct proxy policy;
- uses asynchronous Qt Quick loading;
- bounds response bodies and decoded dimensions;
- decodes toward the QML-requested source size instead of retaining unnecessary full-resolution avatar/banner images;
- keeps a memory-costed decoded-image cache and short failure backoff;
- uses HTTP/1.x for best-effort remote media so an HTTP/2 stream/protocol failure does not poison repeated image loads while normal application transport remains protocol-auto.

Direct image Cast embeds render inline while preserving a safe external-open
action for the original URL.

## Profile surfaces

Profile tabs are semantically separate:

- Posts: root, non-quote Casts only;
- Quotes: root Casts containing a Cast reference;
- Replies: actual parented Casts only; the provider compatibility endpoint may
  include recasts, which are filtered out;
- Likes: protocol-native Like reactions by FID with target Cast hydration;
- Popular: provider-derived popular Casts.

## Presentation fixes

- authenticated desktop header no longer shows both the browsing identity and
  signed-in account as duplicate adjacent controls;
- bio text supports clickable HTTP(S) links and accent styling for mentions and
  channel-like tokens;
- avatars use an alpha mask rather than rectangular QQuickItem clipping, so PFP
  pixels cannot escape the circular visual boundary;
- durable Like/Recast overlays canonicalize Cast hashes before matching, so
  provider refresh cannot lose active reaction state solely because of `0x` or
  case differences;
- no `canLoadMore` binding remains in the QML tree.

## Verification status

Container gates:

- architecture guard: PASS;
- `git diff --check`: PASS;
- clean UI-disabled configure: PASS;
- configured product-name generation: PASS (`Genycaster`); protocol App FID
  defaults to the fail-closed unset value (`0`) until a dedicated client FID is
  explicitly configured;
- SQLite boundary implementation reached semantic compilation successfully;
- Hypersnap codec/feed/profile module interfaces and implementations: BUILD PASS;
- Hypersnap fixture modules and contract-test translation unit: BUILD PASS;
- higher-level GCC 14.2 module builds remain blocked by the known compiler ICE
  in `cp/module.cc:16297`, before actionable source diagnostics;
- Qt6/qmllint/runtime media verification is unavailable in the Linux runner.

Final acceptance therefore remains a macOS Clang 22 + Qt 6.11.x runtime gate.
