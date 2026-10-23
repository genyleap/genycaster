# Genycaster cast/profile share and moderation contract

## Cross-client identity and links

Cast identity is the protocol hash (0x + 40 hexadecimal digits); profile
identity is the positive, unsigned Farcaster FID. Neither is a Hypersnap HTTP
resource identifier. The current deployment is at
`https://caster.genyleap.com/`.

| Target | Example/shape | Status |
| --- | --- | --- |
| Genycaster cast | `https://caster.genyleap.com/?cast=0x<40-hex>` | Canonical HTTPS permalink |
| Genycaster profile | `https://caster.genyleap.com/?profile=<fid>` | Canonical HTTPS permalink |
| Farcaster cast | `https://farcaster.xyz/<username>/0x<40-hex>` | External HTTPS |
| Farcaster cast without username | `https://farcaster.xyz/~/conversations/0x<40-hex>` | External HTTPS fallback |
| Farcaster profile | `https://farcaster.xyz/<username>` | External HTTPS |
| Portable URI | `farcaster://<fid>/0x<40-hex>`, `farcaster://<fid>` | Experimental; draft FIP, not universally registered |
| Quorum cast | `quorummobile://feed?castHashPrefix=0x<40-hex>` | Source-verified Expo feed route; Quorum app must be installed |

Link destinations are derived from the protocol IDs, not from the configured
read provider. A different Hypersnap or Snapchain node can resolve the same hash
or FID when the data exists in its selected network.

`PresentationBridge::openSharedLink` accepts only known HTTPS host/path
combinations and the constrained Farcaster URI shapes. It never executes
untrusted URL strings. The web boot process dispatches `window.location.href`
after the runtime and initial setup are ready; `--open-url=<link>` does the
same for native callers. While running in WebAssembly, browser history updates
are observed. Android/iOS/macOS **OS association** requires installing native
app-link/universal-link/scheme registration in that platform's package; the
CLI argument alone does not register the operating-system handler.

The share sheet uses the browser's `navigator.share` where supported and
falls back to copy; native Qt currently uses copy-to-clipboard. Do not describe
clipboard fallback as a native OS share sheet.

Quorum's Expo mobile manifest declares the `quorummobile` scheme. Its
`app/feed.tsx` forwards `castHashPrefix` to `app/(tabs)/feed/index.tsx`,
which opens the cast thread. Genycaster offers the source-grounded link
`quorummobile://feed?castHashPrefix=...`; this requires Quorum to be installed
and associated with its scheme. Opening it on real devices has not yet been
verified. The Quorum profile FID is supported in the feed tab but not forwarded
by the public legacy `/feed` route, so we do not invent a Quorum profile URL.
The `farcaster://` format is still an experimental URI proposal.


## Provenance badge

Read adapters preserve `Cast::sourceClient` only when the upstream node
provides explicit `app.name` or `app.display_name`; they never infer source
from author username, avatar, post contents or the reader's selected node.
Unavailable/withheld attribution yields no badge. Genycaster's optimistic
self-authored casts may be labeled `Genycaster` until network-confirmed reads
arrive. A report of "which client authored the cast" is impossible without
reliable signer-to-app attribution, particularly for privacy-oriented clients.

## Moderation semantics

Mute, Block and Report are hidden until network-backed operations are
implemented and tested. Historical local-only mute/block settings no longer
filter the feed. Farcaster has no universal protocol report or mute messages.

## Verification

- `node --test tests/wasm/deeplink_share_contract.test.mjs`
- `node --test tests/wasm/qml_attached_imports.test.mjs`
- `farcaster_presentation_icon_tests` exercises valid/invalid URL routing,
  domain migration, URL security and copy-link formatting.
- `farcaster_hypersnap_contract_tests` checks explicit app attribution versus
  missing metadata.
- Validate target-platform builds before shipping; publish
  the verified `genycaster-assets.json` bundle atomically and smoke-test the
  HTTPS origin/CDN. Do not switch the live symlink on build failure.

References:

- Farcaster protocol: https://github.com/farcasterxyz/protocol/blob/main/docs/SPECIFICATION.md
- Farcaster URI proposal: https://github.com/farcasterxyz/protocol/discussions/123
- Quorum reporting service:
  https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/services/reporting/reportService.ts

- Quorum cast route: https://github.com/QuilibriumNetwork/quorum-mobile/blob/master/app/feed.tsx


## Deployment-neutral Genycaster links

The current deployment host shown above is only an example, not a constant
inside the share-link implementation. Cast and profile permalinks use
the active public HTTPS origin independently of Hypersnap, Snapchain, Farcaster
account signing, and network selection.

WebAssembly resolves window.location.origin when sharing. Moving the same
WASM build to another HTTPS domain therefore generates URLs at that domain
without a client code change.

Native builds use the following independent configuration, in precedence order:
environment variable GENYCASTER_PUBLIC_BASE_URL, QSettings key
sharing/publicBaseUrl, or the optional build-time CMake cache option
GENYCASTER_DEFAULT_PUBLIC_BASE_URL. No domain is assumed if native builds
have not been configured. All URLs must be HTTPS, without credentials,
query parameters or fragments. A deployment subpath is supported.

Already shared links to retired domains require an HTTPS redirect at the
old domain preserving the query string; changing the in-app base alone cannot
magically keep expired DNS names working.

The farcaster.xyz destination and Quorum app URI are external-client links,
not Genycaster public origins. Other first-party backend, authentication,
media and image-proxy URLs must be audited separately for a complete
infrastructure migration.

## App-first client navigation

The same actions are displayed in native and WebAssembly clients.

- Open in Farcaster uses HTTPS universal/app links. On supported phones an installed Farcaster app may handle the cast/profile, otherwise the same URL opens that item in the browser. Mobile WASM uses same-window navigation so that app-link dispatch is not bypassed by creating a new tab.
- Open in Quorum first attempts the verified quorummobile://feed?castHashPrefix route. Native Qt falls back to https://app.quorummessenger.com/ when OS URL dispatch immediately fails. Mobile WASM attempts the URI on the click, watches page visibility/pagehide and falls back to Quorum web after 1.8 seconds if the page remained visible. Desktop WASM opens the web destination.
- Quorum has no confirmed cast-specific web fallback URL. Its web destination is only the app entry point; never invent a cast permalink.
- App-install detection is not available reliably to WebAssembly or Qt. The fallback is best-effort and must be device-tested on iOS and Android, with and without Quorum installed. A browser security prompt may influence focus and routing.
- Ordinary share/copy links remain HTTPS (Genycaster or Farcaster). Raw experimental app URI copy menu actions have been removed; user-facing labels are platform-independent.
