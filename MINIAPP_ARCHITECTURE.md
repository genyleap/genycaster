# Genycaster Mini App Host Architecture

## Current host contract

Genycaster resolves the Farcaster manifest before the Mini App document is created. The host validates
`requiredCapabilities` and `requiredChains` against the runtime capabilities that are actually available.
Missing requirements are reported by exact identifier, and the UI explicitly explains that Reload cannot
add a capability that the host does not provide.

The Mini App runtime never reuses the Farcaster custody authorizer as an Ethereum provider. Custody
authorization remains a one-shot signer-management workflow. EVM Mini Apps use the independent wallet
bridge described below.

## EVM wallet bridge

Genycaster now has a provider-neutral `IEthereumWalletBridge` capability boundary and a browser-wallet
adapter that connects Mini Apps to a real EIP-1193 provider without moving private keys into Genycaster.
The bridge is compiled independently from the custody authorizer.

The adapter creates a per-Mini-App loopback session on `127.0.0.1` with a cryptographically random session
path. The approval page discovers injected wallets via EIP-6963 with an EIP-1193 fallback. The host
routes Farcaster `ethProviderRequestV2` requests into that session and returns JSON-RPC responses using the
SDK wire contract.

The current invariants are:

1. `wallet.getEthereumProvider` is advertised only when the bridge is compiled, available, and has at least
   one configured EVM chain.
2. `getChains()` reports only configured CAIP-2 `eip155:*` chains. Solana is not advertised by this bridge.
3. A wallet session is created lazily on the first actual Ethereum provider RPC, not merely because a Mini
   App was opened or its manifest was inspected.
4. Unprivileged chain reads may route without account consent; `eth_accounts` and
   `wallet_getPermissions` return empty arrays until the Mini App receives
   account access. `eth_requestAccounts` or a successfully approved EIP-2255
   `wallet_requestPermissions` result containing `eth_accounts` grants access
   for the origin. Wallet disconnect or *any* `accountsChanged` revokes it.
   Signing, transactions and sensitive wallet operations need account access
   and explicit wallet approval; no successful approval is fabricated.
5. `wallet_switchEthereumChain` and `wallet_addEthereumChain` are checked against the configured chain
   policy and fail closed for disallowed networks.
6. EIP-1193 errors are preserved where possible. User rejection remains `4001`; disallowed chains use
   `4901`; an unavailable/timed-out provider session is not mislabeled as user rejection.
7. Provider events (`accountsChanged`, `chainChanged`, `connect`, `disconnect`, and `message`) are forwarded
   only to the currently live Mini App document. Native delivery is scoped by the document token so stale
   events cannot cross a navigation boundary.
8. EIP-6963 provider discovery is supported: the SDK's `eip6963RequestProvider` host call triggers an
   `eip6963:announceProvider` client event describing GenyCaster as an external-wallet host.
9. No private key, seed phrase, custody secret, or raw wallet credential crosses the Mini App bridge.
10. Closing, failing, or replacing a Mini App tears down its active bridge session.

The accepted chain set defaults to the EVM CAIP-2 chains represented by the current Farcaster Mini App
manifest schema. Deployments can reduce that set through `GENYCASTER_MINIAPP_EVM_CHAINS`; unsupported or
malformed requested chains remain absent from `getChains()`.

### Platform wallet routing and consent

- **Native desktop:** An injected EIP-1193/EIP-6963 browser extension lives
  inside its browser's origin, so Qt cannot invoke the extension directly or
  launch it by extension ID with the Mini App's authority. The loopback
  approval page is intentionally an isolated relay. The chooser temporarily
  hides the native WKWebView surface during selection and restores the same
  Mini App document afterward. The approval page uses one native wallet
  selector and a compact summary, with the full RPC payload and localhost
  explanation available on demand.
- **Independent desktop wallet applications:** Launching an installed app
  alone does not establish an Ethereum provider or a signing channel.
  WalletConnect v2 pairing/session negotiation (or a wallet's explicitly
  supported, authenticated native transport) is required before offering
  such a destination. No unverified desktop app is offered as a working
  signer.
- **Android/iOS:** A platform wallet-app handoff must use authenticated
  universal/app links and a verified callback/session transport, not the
  local desktop bridge URL. The current Android Mini App WebView does not
  assert injected-wallet availability; no unsupported native-wallet button
  is exposed.
- **Web/WASM:** Use the same-page injected EIP-6963 wallet registry, with
  EIP-1193 fallback, session-scoped consent and no loopback native launcher.

Wallet account permission and signature approval remain distinct. Neither
remembering a browser nor opening an application grants signing rights.

### Native wallet-browser selection

Native desktop Mini App requests now open a GenyCaster-themed browser selector rather
than immediately opening the system default browser (which might be Safari without
an EIP-1193 provider). Detected *browsers* are candidates, not evidence that a
wallet extension exists or is unlocked. Each selection opens the local wallet
bridge in that browser through a platform process launcher. The remembered choice
is a browser preference only, never wallet authorization. Users can explicitly
choose the default browser as a fallback. The picker validates the exact random
loopback session URL and does not accept URLs from third-party Mini App content.
An explicit cancel rejects the current wallet request, but never closes the
foreground Mini App. Opening the browser retains the loopback capability token
until wallet response, cancellation, focus/document revocation or timeout. The
modal shows an explicit waiting state, and it has a non-dismissable backdrop so
Qt's popup close transition cannot accidentally tear down an active wallet
session. Passive RPCs such as initial `eth_chainId` no longer open a browser before
an explicit `eth_requestAccounts` or `wallet_requestPermissions` request.
The picker uses bundled CC0 Simple Icons SVG glyphs (Brave, Chrome, Firefox)
inside the standard GenyCaster theme; browser labels are not wallet detection.
A chosen browser is remembered only after a successful wallet response.

### External-wallet requirement

The browser approval page uses EIP-6963 to find multiple providers, with
`window.ethereum` as an EIP-1193 compatibility fallback. Requests are
summarized by intent; printable `personal_sign` content is shown to the user,
and the original JSON-RPC payload remains available for review. Results that
were signed but could not be delivered are retried without invoking the wallet
a second time. The loopback service validates exact Host, same-origin POST
Origin, and (when present) Fetch Metadata headers. No provider is fabricated.

The WASM host also supports EIP-6963 discovery and per-session provider
selection without crossing the Mini App iframe's origin-grant boundary.

**Not implemented in this iteration:** WalletConnect v2 pairing/relay, direct
native mobile deep-link return, and complete wallet-backed Mini App execution
on Windows, Linux, Android or iOS. These capabilities must not be advertised
before a real adapter, verified pairing/permissions, and platform lifecycle
tests exist. An optional future embedded wallet can implement the same
`IEthereumWalletBridge` boundary without changing the Mini App contract.

## Mini App Sign In with Farcaster (SIWF)

Native macOS Mini App sessions implement **`actions.signIn`** as an independent
identity capability. This is not a cast/reaction signer and does not select or
override the user's preferred content node: Hypersnap remains the default, while
Snapchain is independently selectable. Farcaster identity proofs are anchored to
the FID and custody/auth-address registry on **Optimism (chain ID 10)**, not to a
Snapchain write transaction.

- The capability is advertised dynamically only when the build contains the
  verified SIWF adapter, an authenticated account FID is present, a real EIP-1193
  wallet bridge is available, and a trusted HTTPS Optimism verifier is configured.
- The Mini App provides a server-generated alphanumeric nonce (8–128 chars).
  Genycaster constructs a short-lived EIP-4361 message with the *actual Mini App
  origin and launch URL*, a single `farcaster://fid/<fid>` resource, and a
  maximum 10-minute expiration. An option to disallow auth addresses is enforced.
- The selected wallet must explicitly approve `eth_requestAccounts` when not
  already granted and separately approve each `personal_sign` request.
  No phrase, native protocol Ed25519 signer, OpenProof bearer, or cookie is
  exported to the Mini App.
- Off the UI thread, the existing SIWF verifier validates the exact domain,
  URI, nonce, expiration, EIP-191/1271/6492 signature, and FID-to-custody or
  currently active auth-address mapping on Optimism. Only a verified proof is
  returned as `{result:{message,signature,authMethod}}` per the official
  Mini App wire contract. User rejection is typed as `rejected_by_user`.
- Pending authorization belongs to one foreground task, document generation
  and FID. Closing, navigating away, signing out, or moving to another account
  invalidates the result. Sensitive operations never run on a background app.
- WebAssembly and platforms without the native wallet+verification flow do
  **not** advertise `actions.signIn`. A web-host adaptation must supply its
  own trusted verification and explicit approval before enabling it; inventing
  a signed proof or routing via Snapchain is prohibited.

Relying-party Mini App servers are still responsible for nonce issuance,
one-time consumption, signature verification, and binding the result to their
own login session.

## Apps surface

`Apps` is a first-class shell route with two application families:

- **Farcaster Mini Apps**
  - **My MiniApps**: a Genycaster-local saved collection with Save/Remove and host-mediated `actions.addMiniApp`
    consent. Adding a Mini App never silently enables notifications.
  - **Explorer**: provider-routed discovery with search, refresh, normalized metadata, Share, Save/Remove, and
    Launch. Network I/O, response-size enforcement, parsing, and catalogue normalization run on the dedicated
    `miniapp-discovery-network` worker thread. QML receives normalized state only.
- **Native Apps**: reserved for Genycaster-native applications and intentionally shown as under development.

Opening Explorer performs an initial refresh when the catalogue is empty. Saved state is kept locally via
`QSettings`, deduplicated by normalized HTTPS `homeUrl`, and capped to prevent unbounded client state.

## Discovery routing

Genycaster treats discovery as independent from Mini App execution. The discovery controller is
provider-neutral and receives endpoint/format configuration from the composition root.

The current routing strategy is:

1. Neynar's structured Mini App endpoints are the primary discovery source: `/v2/farcaster/frame/catalog/`
   for catalogue discovery, `/search/` for text search, and `/relevant/?viewer_fid=<FID>` for personalized
   discovery. `GENYCASTER_NEYNAR_API_KEY` overrides the development credential; otherwise the documented
   `NEYNAR_API_DOCS` credential is used for development/testing. Credentials remain inside the C++ network
   adapter and are never exposed to QML or a Mini App document.
2. Catalogue requests use `limit=100&time_window=7d`; text search uses `limit=20&q=<query>`; relevant discovery
   is keyed by the signed-in viewer FID. Responses are bounded to 8 MiB and parsed/normalized off the GUI thread.
3. A public Farcaster developer-reward index remains only as a degradation fallback when structured discovery
   fails. A domain-only fallback row is still safe to launch because execution fetches the canonical
   `/.well-known/farcaster.json` manifest before loading the application. Farcaster Universal Links are resolved
   through `fc:miniapp:domain`.
4. `fid:<number>` and the `My FID` shortcut filter the currently loaded catalogue by author FID; they are not
   advertised as an exhaustive global "all apps by author" query because no stable public endpoint currently
   provides that contract.
5. The proposed Hypersnap `/v1/hyper/miniapps` FIP is not probed by default because deployed nodes cannot be
   assumed to implement it. It can be reintroduced as a protocol-native provider after deployment support is
   confirmed.

The execution host never guesses a Mini App identity from an image CDN or presentation URL; the resolved
manifest remains authoritative.

## Multi-session runtime

The Mini App runtime owns up to four live sessions. Each session has an independent task identifier, browser
document, manifest/origin, document generation token, host context, lifecycle state, and task telemetry.
Exactly one session is foreground at a time. Other sessions can remain minimized without being destroyed.

The current invariants are:

1. Ordinary user launches focus an existing matching session or create a new session until the four-session limit
   is reached. `actions.openMiniApp` is a navigation handoff: the target receives
   `context.location.type = "open_miniapp"` plus the source `referrerDomain`, and the source session closes only
   after the target has successfully reached `actions.ready()`. Failed target preflight restores the source and
   rejects the SDK action instead of destroying the current app. Genycaster does not silently evict an older session.
2. Foreground selection is an explicit intent. Concurrent manifest/network completion cannot steal focus: only
   the most recently selected session may become visible when its preflight finishes. Other sessions can finish
   loading in the background without flashing over the active Mini App.
3. Minimize preserves document state while suspending native WebKit media playback where supported. Hidden
   sessions cannot steal AppKit first-responder state, consume Escape intended for the foreground drawer, or
   restore the feed responder when they are terminated.
4. Duplicate launches of the same requested/canonical URL are deduplicated into the existing session. QML binds
   to task lifecycle state so Open actions enter an immediate animated loading/disabled state instead of sending
   repeated requests on slow networks. Successful manifest preflight is cached per HTTPS origin for five minutes,
   so reopening a recently validated Mini App avoids another `/.well-known/farcaster.json` round-trip while the
   canonical document/session URL remains unchanged.
5. Focus handoff is non-destructive: the target native surface is presented before the previous surface is hidden,
   and Task Manager activation is deferred until its Qt modal has completed the current input turn. This prevents
   modal-close/pointer events from being mistaken for native drawer dismissal and removes the blank handoff frame.
6. Only the foreground session may create/use the EVM Wallet Bridge. A background/minimized session receives
   EIP-1193 `4100` for wallet access.
7. Terminating a task tears down its browser, wallet state, pending compose requests, host document generation,
   and task-manager record deterministically. Account switch/sign-out tears down all Mini App sessions.

## Mini App Task Manager

The bottom status surface exposes a compact task-activity chip with a CPU-style glyph that opens the Mini App
Task Manager. The controller is authoritative for task presentation and commands; QML only renders state and
forwards Focus, Minimize, and Terminate intents. The Task Manager is an adaptive bottom drawer below the mobile
breakpoint and a compact content-driven dialog on desktop; it does not reserve a large empty modal body. Task rows
include each Mini App icon/domain/state and keep telemetry secondary to lifecycle controls. Explorer uses a virtualized `GridView` instead of eagerly
constructing every catalogue card, so large 100-item catalogues do not instantiate every delegate/image at once.

SDK/RPC activity is counted exactly per session. CPU and resident-memory fields are optional: they are shown only
when a platform backend can attribute a renderer process identifier through a public API. Public `WKWebView` on
current macOS does not expose its WebContent PID, and separate process-pool instances no longer guarantee renderer
isolation, so Genycaster deliberately leaves CPU/RAM unavailable on that backend instead of using private WebKit
selectors or guessed process ownership. Telemetry sampling never grants the Task Manager ownership of session
lifecycle; commands are routed back to the Mini App runtime.


## Media first-frame preview

Generic video embeds that do not provide a provider poster are primed lazily after a short viewport-stability delay.
Only instantiated/visible feed delegates request a muted decode, and the player pauses immediately after the first
decoded frame while keeping that frame resident in the video sink. Provider thumbnails still take precedence and
avoid decoder/network startup entirely. Passive priming never acquires the app-wide audible playback lease, so it
cannot interrupt a video the user is already watching. This balances a non-black idle preview with feed scrolling
and weak-network performance.

## Sharing and host actions

The current host supports `actions.openMiniApp` and `actions.composeCast` in addition to existing
navigation/profile/cast actions. `actions.openMiniApp` follows Farcaster navigation semantics instead of acting as
an independent launcher. `actions.composeCast` opens the visible Genycaster
Composer, supports suggested text/embeds and cast-parent replies, and resolves only after the user publishes or
cancels. Channel-targeted compose requests currently fail explicitly rather than being silently degraded.

Mini App toolbar and Explorer/My MiniApps Share actions also route through the Genycaster Composer; no share
action posts a Cast automatically.

## Drawer geometry

Side drawers use physical presentation edges rather than text direction to determine attached-corner
geometry. The edge touching the screen is square:

- left drawer: left corners square, right corners rounded;
- right drawer: right corners square, left corners rounded;
- bottom sheet: bottom corners square, top corners rounded.

The rule is applied both to QML `GenyDrawer` and the native macOS Mini App presentation surface.

Farcaster's web reference surface is 424x695 points. Genycaster therefore caps native desktop Mini App drawer
width at 424 points and uses a 424x755 default separate-window content rect (60 points of Genycaster host chrome
above the 695-point web surface). On narrower/mobile surfaces the device width remains authoritative. Native
macOS drawer corners use Core Animation continuous corner curves; QML continuous-corner surfaces render through
Qt Quick Shapes `Shape.CurveRenderer` rather than the former tessellated superellipse visual.

## SDK add/remove semantics

`actions.addMiniApp` is deliberately **not advertised** by `getCapabilities()` in this revision. Farcaster's host
contract couples Add with signed server events and, when applicable, notification details delivered to the
manifest `webhookUrl`. Genycaster does not pretend that its local My MiniApps bookmark is equivalent to that
protocol capability. Users can still Save/Remove apps through Genycaster's own Apps UI; SDK calls to `addMiniApp`
reject explicitly until the signed event/notification backend exists.

## Remaining work

The bridge is sufficient for EVM Mini Apps that can use an external EIP-1193 wallet, but the following are
separate future capabilities rather than hidden fallbacks:

- embedded/native Genycaster wallet accounts and secure key storage;
- Solana wallet provider support;
- remote notification/webhook synchronization for added Mini Apps;
- protocol-native synchronization of `My MiniApps` when a deployed discovery/provider API exposes the
  user's canonical collection;
- richer transaction review (decoded calls, token/value/fee presentation, simulation and risk signals)
  before sensitive wallet dispatch.


### Neynar structured discovery

Genycaster uses Neynar's Mini App APIs as the primary structured discovery source:

- catalogue: `GET /v2/farcaster/frame/catalog/?limit=100&time_window=7d`
- search: `GET /v2/farcaster/frame/search/?limit=20&q=<query>`
- relevant: `GET /v2/farcaster/frame/relevant/?viewer_fid=<fid>`

Requests send `x-api-key`. `GENYCASTER_NEYNAR_API_KEY` overrides the credential. When it is absent, the public documentation credential `NEYNAR_API_DOCS` is used as a development fallback so Explorer/Search/For You remain testable without local secret provisioning. Production deployments should configure their own Neynar credential or proxy discovery through a Genycaster service.

The parser accepts Neynar's `frames` and `relevant_frames` response shapes, including the `manifest.frame` / `manifest.miniapp` snake_case fields (`home_url`, `icon_url`, `image_url`, `required_chains`, and related metadata). Discovery responses are capped at 8 MiB and normalized off the GUI thread before entering QML.
