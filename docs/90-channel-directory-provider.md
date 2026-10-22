# Channel metadata capability (Genycaster)

Genycaster's read node and optional presentation directory are **separate injected
capabilities**. The read node continues to come from the selected Hypersnap/compatible
provider in `src`. The channel picker never invents a parent URL: selecting a
channel requires the viewer's confirmed `user/memberships/list` entry and the
canonical `parent_url` supplied by that node. Home removes the parent URL.

## Channel avatars and follower counts

Quorum Mobile itself uses the Farcaster client directory for channel
`imageUrl`, `followerCount`, and user-facing channel names: those fields
are not returned by all Hypersnap v2 channel endpoints. This is a metadata
enrichment adapter **in C++**, never a static list of image URLs and never
a QML XMLHttpRequest. Failure of enrichment must not block Home or casting
through the selected node. Search and image hydration use bounded requests
and in-memory caches. Per-FID channel selection frequency is cached locally;
membership and URL authority never come from this cache.

Native builds default to `https://client.warpcast.com/v2` for presentation
enrichment only. Browser builds require a same-origin read-only proxy because
Farcaster's directory endpoint does not provide browser CORS access:

- Add `tools/nginx_channel_directory.conf` within the active TLS server block.
- Validate the active configuration with `nginx -t` and reload Nginx.
- Verify `/api/channel-directory/search-channels?q=genyleap&limit=1`
  returns a real JSON `result.channels[0].imageUrl`, **not the SPA HTML**.
- Verify `/api/channel-directory/channel?key=genyleap` returns a channel
  metadata response.

The web origin is derived from `window.location` within the WASM composition
root; it does not embed a fixed Genycaster or Hypersnap host. Deployments can
override `--channel-directory-base-url`,
`GENYCASTER_CHANNEL_DIRECTORY_BASE_URL`, or
`network/channelDirectoryBaseUrl` settings as supported.

**Important:** the immutable WASM release script does not modify
`/etc/nginx`. Without this independently managed proxy capability, the
channel picker works with node-supplied data and displays initial-letter avatar
fallbacks. Do not claim channel icon delivery is fixed until the public relay
is responding with JSON from both the origin and CDN.
