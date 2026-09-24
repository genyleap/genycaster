# Adaptive video playback and quality control

Engineering authority: `thecompez/ai-for-modern-cpp` at
`326cd850c2cb5e39264b7d2ccf2c29fba6627380`; repository architecture, Qt Quick,
resource/lifecycle, testing, and UI design-system rules apply.

## Feature gate

Responsibility: provide one canonical GenyCaster video player with responsive startup,
explicit quality preference, native adaptive streaming, progressive-rendition fallback,
buffering recovery, and stable inline/fullscreen presentation.

Capability origin: Local presentation over media URLs already present in Cast/embed data.
Adaptive masters or alternate renditions are optional extension metadata; the player never
manufactures qualities that the media service did not provide.

Mandatory or optional: Direct video playback remains mandatory when a supported source URL
exists. Adaptive quality and manual quality selection are optional enhancements enabled only
when an HLS master or multiple rendition URLs are available.

Public contract: `GenyVideoPlayer` accepts `source`, optional `adaptiveSource`, optional
`renditions` (`url/source`, `height`, optional `bitrate`), and `contentType`.
`MediaPlaybackCoordinator.preferredQuality` persists the app-wide preference as `auto`,
`original`, or a canonical vertical resolution such as `720p`.

Canonical data/invariants: the original media URL remains the canonical external resource.
Quality selection changes only the playback URL. A single progressive MP4/WebM/MOV is exposed
as Original only; GenyCaster does not claim ABR or downscaling without server-provided media.

Dependencies: QML presentation -> Qt Multimedia; QML session state -> the narrow Qt MOC
`MediaPlaybackCoordinator` boundary -> `QSettings`. Composition connects that coordinator to the
shared provider-neutral asynchronous HTTP transport for remote-source preflight. URL-to-playback-
hint derivation lives in the provider-neutral `farcaster.presentation.media` module. No network
or transcoding logic enters domain/application modules.

Expected failures: unavailable URL, decoder/backend error, stalled network, malformed adaptive
manifest, or a rendition becoming unavailable. Remote preflight has an 8-second operation
deadline. The failure/retry surface remains the final recovery boundary; a 12-second first-frame
watchdog and a separate no-progress watchdog prevent indefinite loading UI.

Local-state representation: only the preferred quality string is persisted. Decoder state,
buffer state, selected rendition index, playback position during a switch, and startup state are
transient per-player state.

Offline behavior: local/cached file URLs continue to play at Original quality. No unavailable
network rendition is synthesized.

Provider policy and disablement: HLS ABR is delegated to the deployed Qt Multimedia backend.
Current macOS Qt uses the Darwin/AVFoundation plugin; FFmpeg is used only on deployments that
actually package that plugin. GenyCaster does not override an explicit deployment backend choice.
Before an HTTP(S) URL reaches either backend, the shared QtNetwork reactor performs a bounded
asynchronous HEAD preflight. This moves DNS, system-proxy, TLS, and redirect waiting away from the
GUI path. With Qt 6.10+, the coordinator also applies FFmpeg-supported playback hints before
source assignment: an 8-second remote socket timeout, playback intent, and a 512 KiB probe budget
for HLS. When explicit progressive renditions are supplied instead, Auto starts at a conservative
rendition, downshifts after sustained buffering, and attempts a single-step upgrade only after a
stable playback window.

Failover semantics: explicit requested resolution -> matching supplied rendition; Auto -> HLS
master when available -> progressive rendition Auto policy -> canonical source/original.
Unsupported persisted resolutions never cause a fake selection; the current media falls back to
its real available source.

Tests proving the boundary: coordinator tests cover normalization and persistence; media source
classification continues to cover HLS; architecture/UI/localization/async-resource guards cover
the presentation boundary. Full decoder/ABR behavior requires the Qt Multimedia runtime suite on
a supported desktop/mobile target.

## Interaction contract

A user Play gesture sets the activated/loading state immediately and defers remote media-source
assignment until the asynchronous network preflight completes. This lets pressed/loading state
reach the scene graph while DNS, system-proxy, TLS, and redirects resolve on the transport reactor;
only then is the URL assigned to `QMediaPlayer`. Initial startup uses the lightweight media
shimmer and visible progress indicator. `BufferingMedia` still means playable buffered data;
`StalledMedia` remains the blocking rebuffering condition.

The `QMediaPlayer` is bound to the inline `VideoOutput` from construction, before any playback or
fullscreen transition. Hot video frames go directly to that sink; they are not routed through
`MultiEffect`, shader masking, or another offscreen texture pass. This keeps the first-frame and
steady-state render path friendly to the FFmpeg backend's platform hardware-decoding path.

The bottom chrome uses one continuous GenyCaster control surface. The quality action is shown only
when more than one real choice exists. Selecting a quality preserves the current playback position
and whether the session was playing or paused. The app-wide preference is reused by later players,
subject to each media item's actually available renditions.

For progressive Auto mode, a sustained buffering interval requests one lower supplied rendition.
After 30 seconds of stable playback, one higher supplied rendition may be attempted. Each switch
preserves position and never runs for an HLS master, where the native backend owns ABR.

## Farcaster/CDN source normalization

Cast embeds already containing a `.m3u8` URL are handed to the media backend unchanged. The
presentation-media module also recognizes stable Cloudflare Stream URL families and derives their
`/manifest/video.m3u8` decoder URL without fetching or scraping a web page. This metadata is
propagated by the feed presentation model as `adaptiveUrl`/`contentType`, so `VideoPreview` and
`EmbedCard` use the same source contract. Unknown hosts are never rewritten.

If an optional adaptive source fails and the embed also contains a genuine progressive media URL,
the player attempts that progressive source once before exposing the failure surface. Progressive
Auto mode similarly gets one lower-rendition recovery opportunity. Generic watch pages are not
passed to FFmpeg as fallback media.

## Backend deployment and diagnostics

Qt Multimedia owns HLS, buffering, codec negotiation, and hardware decoding through the backend
available on each deployment (Darwin/AVFoundation on the current macOS SDK, or FFmpeg where it is
packaged). During decoder investigations, developers may enable the matching Qt Multimedia backend
diagnostics in a development run; private backend environment knobs are diagnostics only and are
not baked into application behavior.
