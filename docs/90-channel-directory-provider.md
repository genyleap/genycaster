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

Native builds default to `https://client.warpcast.com/v2` for
public display metadata only. Browser builds cannot read that endpoint through
CORS. Instead, the existing Genycaster Media Gateway now exposes fixed
CORS-safe, read-only paths under
`https://storage-farcaster.genyleap.com/v1/browser/channels`:

- `/v1/browser/channels/search-channels?q=genyleap&limit=3`
- `/v1/browser/channels/channel?key=genyleap`

The gateway permits only GET/OPTIONS, bounded channel keys/searches, fixed
upstream URLs, JSON responses, small response sizes, rate limits and short
caches. Image URLs then pass through the existing bounded
`/v1/browser/images` raster gateway used by the C++ remote-image provider.
No Nginx change is required because the Media Gateway is already deployed
under a publicly accessible, CORS-enabled origin.

The directory base URL is an independently configurable presentation
capability, **not** the read-node URL. Deployments may override it with
`--channel-directory-base-url`, `GENYCASTER_CHANNEL_DIRECTORY_BASE_URL`,
or `network/channelDirectoryBaseUrl` without changing the selected provider.

**Semantic limitation:** Quorum's authenticated Farcaster
`user-following-channels` data is not equivalent to all HyperSnap
`user/memberships/list` entries. The current picker only authorizes channel
selection when verified by the selected read-node membership endpoint. A
provider-neutral, authenticated *followed channels* capability is future work;
do not claim a complete cross-provider follow list is available.

Verification must confirm both public directory endpoints return actual
JSON—not the WASM app's HTML—and that the channel `imageUrl` loads through
the image gateway. The native cast text renderer uses QTextDocument selection
with native press-and-hold and a user-activated clipboard write; read-only
labels lazily instantiate a selectable TextEdit on long press, without assigning
unsupported textInteractionFlags to QtQuick.Controls.Label.
