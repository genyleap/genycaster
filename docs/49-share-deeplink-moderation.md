# Genycaster cast/profile share and moderation contract

## Cross-client identity and links

Cast identity is the protocol hash (0x + 40 hexadecimal digits); profile
identity is the positive, unsigned Farcaster FID. Neither is a Hypersnap HTTP
resource identifier. The **current public Genycaster application** is at
\`https://caster.genyleap.com/\`.

| Target | Example/shape | Status |
| --- | --- | --- |
| Genycaster cast | \`https://caster.genyleap.com/?cast=0x<40-hex>\` | Canonical HTTPS permalink |
| Genycaster profile | \`https://caster.genyleap.com/?profile=<fid>\` | Canonical HTTPS permalink |
| Farcaster cast | \`https://farcaster.xyz/<username>/0x<40-hex>\` | External HTTPS |
| Farcaster cast without username | \`https://farcaster.xyz/~/conversations/0x<40-hex>\` | External HTTPS fallback |
| Farcaster profile | \`https://farcaster.xyz/<username>\` | External HTTPS |
| Portable URI | \`farcaster://<fid>/0x<40-hex>\`, \`farcaster://<fid>\` | Experimental; draft FIP, not universally registered |

Link destinations are derived from the protocol IDs, not from the configured
read provider. A different Hypersnap or Snapchain node can resolve the same hash
or FID when the data exists in its selected network.

\`PresentationBridge::openSharedLink\` accepts only known HTTPS host/path
combinations and the constrained Farcaster URI shapes. It never executes
untrusted URL strings. The web boot process dispatches \`window.location.href\`
after the runtime and initial setup are ready; \`--open-url=<link>\` does the
same for native callers. While running in WebAssembly, browser history updates
are observed. Android/iOS/macOS **OS association** requires installing native
app-link/universal-link/scheme registration in that platform's package; the
CLI argument alone does not register the operating-system handler.

The share sheet uses the browser's \`navigator.share\` where supported and
falls back to copy; native Qt currently uses copy-to-clipboard. Do not describe
clipboard fallback as a native OS share sheet.

Quorum's Expo mobile manifest declares a \`quorummobile\` scheme, but its public
source does not establish a working cast/profile route for this scheme. **Do
not construct a guessed \`quorummobile://...\` URL.** The portable
\`farcaster://\` format remains a draft and must not be represented as guaranteed
to open Quorum.

## Provenance badge

Read adapters preserve \`Cast::sourceClient\` only when the upstream node
provides explicit \`app.name\` or \`app.display_name\`; they never infer source
from author username, avatar, post contents or the reader's selected node.
Unavailable/withheld attribution yields no badge. Genycaster's optimistic
self-authored casts may be labeled \`Genycaster\` until network-confirmed reads
arrive. A report of "which client authored the cast" is impossible without
reliable signer-to-app attribution, particularly for privacy-oriented clients.

## Moderation semantics

- \`mute\` and \`block\` are **viewer-scoped local display filters** stored in
  the current account's Qt settings. They do not publish Farcaster protocol
  messages and do not revoke any keys or prevent a blocked author from
  reading public casts. Switching accounts changes the filter projection.
- A Farcaster hub has no universal \`Mute\` or \`Report\` message type. Generic
  link messages do not make a portable, network-wide block guarantee.
- Quorum has an independent signed moderation report service using its own
  identity and server. Genycaster cannot submit to that backend using only a
  Farcaster signer. Consequently its current report action **copies cast/profile
  evidence** rather than falsely asserting submission. A provider-specific
  report API must implement an explicit authenticated transport and acknowledgments
  before offering "Report submitted".
- Muted or blocked authors are filtered from the primary read list, not deleted
  from cached data. Direct protocol-hash links are still resolvable.

## Verification

- \`node --test tests/wasm/deeplink_share_contract.test.mjs\`
- \`node --test tests/wasm/qml_attached_imports.test.mjs\`
- \`farcaster_presentation_icon_tests\` exercises valid/invalid URL routing,
  FID-scoped moderation, restoration and copy-link formatting.
- \`farcaster_hypersnap_contract_tests\` checks explicit app attribution versus
  missing metadata.
- Native Qt and release WASM builds must both complete before shipping; publish
  the verified \`genycaster-assets.json\` bundle atomically and smoke-test the
  HTTPS origin/CDN. Do not switch the live symlink on build failure.

References:

- Farcaster protocol: https://github.com/farcasterxyz/protocol/blob/main/docs/SPECIFICATION.md
- Farcaster URI proposal: https://github.com/farcasterxyz/protocol/discussions/123
- Quorum reporting service:
  https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/services/reporting/reportService.ts
