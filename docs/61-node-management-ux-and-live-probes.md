# Node management UX and live provider verification

The Settings drawer includes a dedicated Node management tab on desktop and mobile. Content & loading no longer contains the dense legacy provider form and network switches.

## Main read workflow

- Show the startup-applied friendly provider label, ID and endpoint separately from saved changes pending the next refresh. The footer must not hard-code HyperSnap when a custom read provider was selected.
- List automatic HyperSnap and custom compatible typed read profiles, each with Test, Use and Remove actions.
- Guide users through a friendly display name, endpoint, raw-versus-indexed contract, local example and optional API key; generate provider IDs automatically. Saving a newly discovered node no longer silently selects it as the active reader: Use is an explicit separate action.
- Provide a browser-native clipboard read action in addition to standard field context menus and keyboard editing.
- Show a one-click WebAssembly reload when the chosen connection changes.
- Keep write providers in a clearly separate section. This web build does not compile the custom validator adapter; never imply that choosing a read URL changes signed write destinations.
- Provide 32 translated UI strings across all 12 language catalogs.

## The probe really checks the API

The controller performs an asynchronous request (eight-second deadline, no automatic redirects) using the browser's own network context and CORS rules.

- snapchain-v1: GET /v1/castsByFid?fid=3&pageSize=1&reverse=true followed by GET /v1/userDataByFid?fid=3&pageSize=100; both must return valid message records bound to the requested FID and expected body shapes. /v1/info is NOT enough to confirm a usable read API.
- hypersnap-v2: GET /v2/farcaster/user?fid=3, verifying an object with user.

Responses are classified as verified sampled reads, wrong API shape, authentication/payment required (HTTP 401/402/403), HTTP failure, or network/CORS failure. On an initial HTTP 404, the tester also probes the alternative registered API contract on the same origin using the same per-instance credentials. It reports a confirmed contract mismatch only if the alternative responds with the expected schema (including both cast and profile reads for a raw node). The draft form then offers a one-click switch of the service type without silently saving or selecting a different provider. An unsuccessful alternative test leaves the original HTTP 404 diagnosis in place. A successful probe shows measured response time. It does not prove chain synchronization, complete feature coverage, node ownership or writing rights. The sample FID=3 assumes mainnet data and may not exist on a fresh or private test network; an empty sample is not evidence that the node is malicious.

The raw Snapchain adapter advertises UserPosts/UserQuotes/UserReplies, profile and Follow graph capabilities, and bounded Following derivation; it does NOT implement aggregated Explorer, search, or Notifications. Node Management now displays this limitation explicitly. If the user explicitly enables compatible read fallback, the router may send unsupported requests to another compatible provider; strict raw-only users should expect those surfaces to be unavailable.

**Provider-specific operational evidence (2026-10-24):** Neynar's `hub-api.neynar.com` and `snapchain-api.neynar.com` return HTTP 402 without an API key or X-PAYMENT; their preflight OPTIONS accepts `x-api-key` from `https://caster.genyleap.com`. A Key saved in this tab is not shown in plaintext again. A green Cast/Profile test does NOT assert notifications support.

## API keys and privacy

- A public or locally run raw Snapchain node normally requires no API key only when its operator allows unauthenticated access.
- The browser UI accepts optional provider API keys for indexed and raw Snapchain profiles, only for HTTPS origins. A key is sent as the `x-api-key` header only to the selected provider instance; other vendor-specific header schemes are not yet supported. Draft tests can send an explicit key to the chosen HTTPS endpoint.
- An opted-in key is stored only in this tab's browser sessionStorage and is passed on refresh into the existing per-provider x-api-key header mechanism. It is not written to QSettings, IDBFS, the URL, source files or diagnostic logs. Deleting the profile also deletes its session credential.
- Session storage is not an OS keychain or secure enclave. JavaScript on the same origin may access it, and session-restoration behavior depends on the browser. Avoid entering sensitive credentials on shared computers.
- Native Qt continues to support operator-supplied environment credentials; this is not a native keychain UI. Nonstandard authentication for raw Snapchain endpoints remains a separate protocol adapter requirement.

## Local nodes and browser constraints

The local example http://127.0.0.1:3381 refers to the user's current browser device, not the remote Genycaster web server. The node must be running there. HTTPS-hosted web apps may be blocked from private HTTP/LAN resources by CORS, mixed-content and browser private-network policies; configure approved HTTPS/CORS when necessary. Localhost is not proof that a node is synchronized or independently operated.

## Regression

    cmake --build build/ui-qt612-debug --target farcaster_provider_profiles_tests -j 2
    ctest --test-dir build/ui-qt612-debug -R ui.provider_profiles_contract --output-on-failure
    cmake --build build/ui-qt612-debug --target farcaster_client_qmllint -j 1
    node --test tests/wasm/node_management_settings.test.mjs

The native test spins up a real local HTTP server and differentiates valid Snapchain JSON from an incompatible HTTP 200 response. Actual Safari clipboard permissions, WASM tab-session credentials and UI tap/focus behavior still require browser E2E verification.
