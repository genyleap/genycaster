# Channel directory capability (Qt/C++ composition)

The signed cast destination (`parent_url`) is always resolved by the selected
protocol read node. The mobile channel picker never constructs a URL from a
display key and never fetches a static image catalogue from QML.

## Read capabilities

- Protocol node: selected at startup by `NodeSelectionController`. The
  configured endpoint is injected into `ChannelPickerController`; the controller
  has no hardcoded HyperSnap hostname.
- Optional metadata provider: configured by
  `--channel-directory-base-url=`, `GENYCASTER_CHANNEL_DIRECTORY_BASE_URL`, or
  `network/channelDirectoryBaseUrl` (in that order). This provider supplies
  display names, `imageUrl`, `followerCount`, and channel discovery records
  through `search-channels` and `channel`. It never supplies publishable
  parent URLs.
- Verified memberships: `user/memberships/list?fid=...` on the selected node.
  Non-members stay visible but are disabled. A positive returned membership is
  actionable while further pages load. Arbitrary token/chain parent identifiers
  are excluded from the social channel picker. **This endpoint must not be
  confused with Warpcast's private followed-channel list.** The latter requires
  a separately authenticated Farcaster session token and may differ.
- Home: always the first row; choosing it publishes without `parent_url`.
- Per-FID choice frequency is cached in `QSettings`. Network results and
  actual membership eligibility are never persisted as trusted state.

Quorum Mobile uses `client.warpcast.com/v2/search-channels` and
`v2/user-following-channels` for the client-facing channel experience because
the HyperSnap index lacks channel icons and following metadata (see upstream
`hooks/useFarcasterSearch.ts` in QuilibriumNetwork/quorum-mobile).

## WebAssembly proxy

Unlike a native Qt Network client, a WebAssembly `QNetworkAccessManager`
request is bound by browser CORS. The public Farcaster directory currently does
not permit `https://caster.genyleap.com` as a cross-origin read client.
The WASM build therefore uses
`window.location.origin + "/api/channel-directory"` by default.

Production Nginx must include the two **read-only, fixed-path** rules from
`tools/nginx_channel_directory.conf`. This file is a deployment input; merely
packaging it in the WASM bundle is not sufficient. Validate and reload the
server's active Nginx configuration, and confirm that a GET to
`/api/channel-directory/search-channels?q=genyleap&limit=3` returns JSON
containing `result.channels[0].imageUrl` before claiming avatars work on WASM.

If the administrator cannot update Nginx, the C++ controller gracefully
retains the selected node's channel list without full external imagery.
Never reintroduce QML `XMLHttpRequest`, a static icon JSON, a generic public
CORS proxy or generated channel parent URLs.

This capability is intentionally separate from signer/write-provider routing
and can be replaced with another provider's implementation later.
