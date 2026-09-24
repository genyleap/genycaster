# Genycaster R5 — product-shell modernization

R5 turns the R4 visual correction pass into a stricter product contract.

## Client attribution

- Shipping UI writes no longer fall back to the signed-in user's FID as the client request FID.
- `FARCASTER_DEFAULT_APP_FID` defaults to `0` instead of guessing a client FID.
- Configure the real Genycaster App FID with `FARCASTER_DEFAULT_APP_FID`, `GENYCASTER_APP_FID`, or `--app-fid=`.
- Signer provisioning remains disabled until both the real App FID and the SignedKeyRequest attestation boundary are available.
- Existing self-attributed signer state is rechecked against the configured App FID before it can become Ready.

## Typography and icons

- Persian/Arabic UI typography is an application dependency, not a host dependency.
- Shipping builds embed pinned Vazirmatn v33.003 static weights into Qt resources.
- CMake supports an explicit offline `GENYCASTER_UI_FONT_DIR` and pinned configure-time acquisition.
- Latin/user content keeps a separate platform text family.
- Primary UI icons are vector QML Shape geometry, not OS icon fonts.

## Navigation roadmap

Desktop minimum destinations:

- Home
- Explorer
- Bookmarks
- Store
- Notifications
- Messages
- Business

Mobile minimum destinations:

- Home
- Explorer
- Store
- Notifications

Unimplemented destinations render a deliberate in-product coming-soon surface instead of disappearing from the information architecture.

## RTL and responsive behavior

- Application chrome uses explicit semantic direction rather than inherited global mirroring.
- User content direction remains independent from UI direction.
- Empty text fields use placeholder direction, so Persian compose/search placeholders align correctly before typing.
- Settings and combo-box sizing retain the R4 widest-item measurement contract.
- Reading/detail widths expand responsively on wide windows instead of leaving excessive dead space.

## Runtime noise and public-node resilience

- Invalid environment SIWF/Optimism/Auth-relay overrides fall back silently; explicit invalid CLI overrides still warn.
- Viewer reaction refresh is opportunistic, throttled, and uses a 7.5-second request budget.
- Expected timeout/unavailable/transport failures do not produce error-looking viewer-reaction logs.
