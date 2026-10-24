# Node management UX and live provider verification

The Settings drawer includes a dedicated Node management tab on desktop and mobile. Content & loading no longer contains the dense legacy provider form and network switches.

## Main read workflow

- Show the startup-applied provider ID and endpoint separately from saved changes pending the next refresh.
- List automatic HyperSnap and custom compatible typed read profiles, each with Test, Use and Remove actions.
- Guide users through a friendly display name, endpoint, raw-versus-indexed contract, local example and optional API key; generate provider IDs automatically.
- Provide a browser-native clipboard read action in addition to standard field context menus and keyboard editing.
- Show a one-click WebAssembly reload when the chosen connection changes.
- Keep write providers in a clearly separate section. This web build does not compile the custom validator adapter; never imply that choosing a read URL changes signed write destinations.
- Provide 32 translated UI strings across all 12 language catalogs.

## The probe really checks the API

The controller performs an asynchronous request (eight-second deadline, no automatic redirects) using the browser's own network context and CORS rules.

- snapchain-v1: GET /v1/info, verifying an object with version and dbStats.
- hypersnap-v2: GET /v2/farcaster/user?fid=3, verifying an object with user.

Responses are classified as compatible, wrong API shape, authentication/payment required (HTTP 401/402/403), HTTP failure, or network/CORS failure. A successful probe shows measured response time. It does not prove chain synchronization, complete feature coverage, node ownership or writing rights.

## API keys and privacy

- A public or locally run raw Snapchain node normally requires no API key only when its operator allows unauthenticated access.
- The browser UI accepts optional indexed provider API keys, only for HTTPS profiles. Draft tests can send an explicit key to the chosen HTTPS endpoint.
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
