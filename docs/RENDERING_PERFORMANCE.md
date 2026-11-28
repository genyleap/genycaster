# Rendering performance and frame pacing

Genycaster retains Qt Quick / RHI and platform-default native GPU backends.
No backend is universally faster: compare on the actual device. Do not
force Vulkan, Direct3D 12, or desktop OpenGL on macOS for a public release
without evidence from representative hardware and media playback tests.

## Invariants

- A decoded media surface has at most one outstanding global presentation
  ticket. An exact render synchronization generation must cross the barrier
  before the next ticket is granted. Duplicate render callbacks do not post
  redundant GUI events. A stale callback from another window cannot grant a
  ticket. Hidden/minimized windows wait to admit new presentations.
- Media replacement withdraws queued obsolete tickets. Cancellation does
  not bypass the barrier for an already-granted ticket.
- Frame timing uses a monotonic timestamp captured at frameSwapped, not
  queued GUI delivery time. Isolated gaps longer than 250 ms reset the
  observation window; they are not counted as slow frames. This is an
  observed frame interval, not GPU execution time or a guaranteed FPS.
  Demand-driven rendering and irregular user interaction can still affect
  the sample. No continuously running diagnostics timer is introduced.
- Backend and refresh-rate telemetry alone do not invalidate resource-size
  bindings. Explicit rendering profile changes still take effect. Adaptive
  quality feedback cannot resize a backdrop texture or reload image pixels.
- Closed drawers disconnect the capture source and reduce its target to 1x1.
  Active snapshots retain the original blur and shader implementation.

## Gaussian and capture pipeline

The native Gaussian and chromatic Gaussian shaders retain 24 paired taps
per axis (49 texture samples); WebGL retains eight pairs (17 samples).
The generator computes the paired weights, offsets and normalization
offline, avoiding per-fragment exponentials and coefficient calculation
without reducing tap count or changing the kernel. All six existing native
QSB variants are rebuilt from the same source. WebGL's build selects its
short kernel before compiling the ES 100/300 variants.

```sh
python3 scripts/generate_gaussian_shaders.py --check
python3 scripts/generate_gaussian_shaders.py --qsb /path/to/Qt/6.12/bin/qsb
```

The shared reflection still uses live native captures outside motion.
On WebGL, the source capture and both downstream blur textures now share
the existing 240 ms snapshot cadence. While scrolling/resizing, the last
complete color field is retained. Disabled reflection detaches all three
sources and requests 1x1 targets; palette transitions retain the previous
extent reservation. Native modal Gaussian snapshots also update the
capture and the horizontal intermediate together, rather than leaving the
intermediate live. The final vertical shader remains a visible pass.
These changes reduce opportunities for redundant work, but GPU drivers
may already optimize some of the old arithmetic or snapshot work.

The isolated QML RHI test compares the actual native QSB and compiled
browser kernel against the original formula on a deterministic RGBA8
texture, across both axes, both shaders, and two opacity levels. The
acceptance limit is one 8-bit channel value. Reflection and modal tests
also inspect rendered pixels, allocation requests, motion freeze and
explicit refresh. The fixture's browser cadence is simulated on desktop
OpenGL; it is not a real browser execution. Requested texture extents are
not measurements of physical GPU allocation.

## QML preview ownership and shutdown

Timeline supporting-preview admission remains staggered and asynchronous,
but the outer preview Loader owns the complete dispatcher subtree.
Nested renderer and promoted-gallery creation do not start independent
incubators for that admitted subtree. Magazine and short-lived detail
routes use synchronous QML construction; network/decode remain async.
Hidden pooled cards do not start new preview admissions, and already
admitted cards retain their identity on reuse. Standalone rich previews
retain their existing independent renderer policy.

Native shutdown uses a local QQmlIncubationController, so pending work
can be advanced even after the last window detached its controller.
The existing bounded drain and Popup/Overlay destruction order remain.
This is not a guarantee that every application-specific incubator will
finish; do not suppress warnings or increase the deadline without evidence.

```sh
cmake -S tests/qml_lifecycle_fixture -B build/qml-lifecycle -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12
cmake --build build/qml-lifecycle
ctest --test-dir build/qml-lifecycle --output-on-failure
```

The isolated fixture builds the production C++26 shutdown module and
executes its existing multi-window regression, now with a detached
incubation controller. The QML fixture runs the actual panel/dispatcher
with stub leaf renderers, checking hidden admission, URL reuse, renderer
promotion, immediate Detail construction and repeated destruction.

## Shutdown and large-raster diagnostics

Enable these categories only when investigating a problem:

```sh
QT_LOGGING_RULES='genycaster.qml.teardown.debug=true;genycaster.render.raster.debug=true;genycaster.render.pacing.debug=true'
```

On native quit, `beforeDrain`, `afterDrain`, `beforeDetach` and
`afterRootCleanup` report the public incubation count. The last report
also records the number of remaining application roots. Qt's own engine
warning uses a broader internal creation counter, which may include
deferred control creation and is not equivalent to the public count.
The shutdown keeps the existing popup/overlay order and guards each root
against destruction by a sibling callback. No warning is suppressed.

The raster log reports the media category, requested decode dimensions,
implicit source dimensions, logical display dimensions and window DPR
when an image becomes ready. It records no URLs or content. It is useful
for correlating large textures with their presentation geometry, but
does not measure GPU residency, physical allocation or upload duration.
When disabled the QML path does not invoke the diagnostic method.

Hidden gallery tiles do not initiate their staggered construction until
visible again; already-admitted tiles remain resident. This does not
change the gallery's geometry or image decode quality.

## First-use utility surfaces

The shell keeps Task Manager, Rendering Settings and Feedback inactive until
the first explicit open request. A nonvisual Loader owner preserves the
actual overlay parent of each adaptive controller without imposing Loader
geometry on the dialog. The opened state still drives the existing modal input
guard and back-navigation path. Retain instances after first use, especially
feedback draft and asynchronous submission state. First-open construction
moves work out of startup, not out of the application entirely. No prewarm
timer is added.

Compact filter dropdowns attach the option delegate model only while the
popup is visible, preserving current selection and width measurements.
The isolated Qt fixture tests the actual shell Loader definitions using
stand-in dialog content and the actual filter with stub visual chrome.
The native teardown regression also verifies the new overlay-owned visual
controller/nonvisual Loader ownership combination under fatal warnings.

These changes reduce unnecessary initial creation; they do not establish
the cause of Qt's internal creation counter warning on macOS. No private
Qt APIs or warning suppression are introduced. Re-test the full application
with the shutdown logging category after opening and closing the tools.

## Small natural raster cache reuse

The decoded-image RAM cache retains its existing per-platform byte budget,
128-pixel request quantization, decode quality and network admission policy.
An original of at most 256 pixels on either axis may be retained under one
natural-size URL key, only if the decoded dimensions equal its original
dimensions and the requested bounds contain the complete source. A later
compatible size request can reuse those exact pixels. A smaller request,
transformed non-square decode or larger original uses the prior size-specific
key. The same LRU owns both entry types. There is no secondary unbounded map.
The decode worker rechecks RAM before disk I/O because a different queued
request may have completed in the meantime. In-flight request coalescing
continues to isolate network class, category and manual/automatic policy.

This is a cache reuse optimization, not a lower-resolution avatar policy.
It avoids redundant resident entries and some repeated disk reads/decodes
when small originals are used in different UI sizes. Concurrent jobs may
still decode independently, and GPU residency is not measured by this cache.

```sh
cmake -S tests/raster_cache_fixture -B build/raster-cache -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12
cmake --build build/raster-cache
ctest --test-dir build/raster-cache --output-on-failure
```

The standalone fixture compiles the provider's exact cache and decode routines
against Qt Core/Gui, checks PNG pixel equivalence, cache cost, size and URL
isolation and LRU eviction. It does not compile the complete provider's
transport or worker integration.

## Targeted media presentation grants

The shared scheduler retains one outstanding presentation ticket per render
barrier. Instead of connecting every raster (including immediate avatars) to
a global QML signal, it invokes the admitted surface's typed
`acceptMediaPresentation(int)` method on the GUI thread. The existing native
observation signal remains available for instrumentation. Identity and
pending-ticket guards reject stale or duplicate deliveries.

When a not-yet-presented remote raster leaves the admission margin, its
queued ticket is cancelled without dropping decoded pixels. Re-entry asks
for a fresh generation, while an already-presented retained image remains
ready. An already-granted ticket still crosses the render barrier before
the next grant. Prefetch/retention distances, decoder quality and cache
limits are unchanged.

The software and OpenGL pacing fixtures exercise real typed QML receivers
alongside render-barrier tests. The shared raster fixture checks withdrawal,
stale generation rejection, re-entry and warm reuse with a controlled local
decode. This reduces unnecessary GUI signal fan-out and off-viewport
presentation work; it does not directly limit GPU upload bytes or measure
physical GPU execution time. Actual Qt texture upload scheduling remains
backend-owned.

## Revision-only Magazine updates

A source change restricted to the revision role can update the existing
numeric slot and its admitted delegate without rebuilding all lane geometry.
Revisions include both mutable viewer state and content hydration, so the
current geometry is provisional, not proof that the content is unchanged.
The existing QML revision-settle and painted-height measurement paths remain
authoritative. A changed measured height, cached different height for the
returning revision, changed layout role, or pending structural reconciliation
still uses the full layout path.

Recording an exact sample equal to the committed slot height at the same
width and revision stores the sample without requesting another layout.
The optimization deliberately uses exact geometry equality; resize samples
and pending structural layout retain their previous behavior. It does not
change typography, card height calculations, scroll anchoring or lane rules.

The native model test includes 5,000 rows and verifies zero geometry
notifications for compatible on- and offscreen revisions, delivery of the
offscreen revision on entry, real height changes, returning cached revisions,
and revision updates while a reorder is pending. The QML masonry fixture
continues to exercise actual expanding/shrinking cards and resizing. This
is a structural reduction in work, not a measured macOS frame-time result.

## Feed projection work budget

Profile sorting decorates each row with its numeric time and (only in Popular)
its score once. The comparator and tie ordering remain the same, and the
original row objects are returned without mutation. Native reconciliation now
returns whether the canonical stable ID sequence changed. Profile uses that
result to decide whether to restore its viewport anchor instead of reading
a QVariantMap per row before reconciliation.

The Feed and Profile reservation estimators split source text once. Feed
only counts supporting embeds for image layouts, stopping at two because
the existing reservation already caps supporting cards at two. The
underlying height formula and asynchronous real-height measurement remain.

Feed static media classification uses a per-surface FIFO of at most 2,048
entries, keyed by the classifier's exact kind, URL/value, mediaType,
contentType and providerKind fields. The serialized key is capped at 2,048
characters, and malformed/non-string inputs use the original native path.
Resolver calls and metadata containing classification evidence always use
native classification. The cache retains strings only (no payloads or
metadata objects), is not persisted, and does not depend on the incomplete
structural revision or mutable viewer state. Full-feed scans admit only
while capacity remains, avoiding sequential-scan eviction of the entire
warm cache; on-demand height lookups can replace entries when full, and an
active-feed switch clears the cache. No NFT/developer filter rule or dynamic
link metadata behavior is changed.

The JavaScript regression compares the actual sorting functions against the
previous comparator on 5,002 rows, checks classification invocation counts,
eviction and reservation equivalence. The isolated Qt Quick runtime
exercises the actual production functions with a counting classifier stub.
The native model fixture verifies the structural return value for empty,
revision-only, duplicate, moved and removed rows. These are work-count and
functional checks, not measured real-device CPU or GPU speedups.

## Opt-in stall phase tracing

Set `genycaster.render.stalls.debug=true` before launching the app. The
controller installs direct, attachment-local scene-graph observers only
when this category is enabled at window attachment. No private Qt APIs,
graphics calls, timers, GPU readbacks, or new per-frame GUI events are used.
All event timestamps use the same process-local steady-clock origin.

A frame report is emitted only when the measured frame span is at least
25 ms or the synchronization span is at least 8 ms:

- `totalMs`: beforeFrameBegin to afterFrameEnd (CPU wall-clock span).
- `syncMs`: beforeSynchronizing to afterSynchronizing.
- `beforeRecordingMs`: beforeFrameBegin to beforeRendering; **includes**
  any synchronization within that interval, so do not add syncMs to it.
- `recordingMs`: beforeRendering to afterRendering (scene-graph command
  recording interval, including other callbacks/driver work).
- `submitTailMs`: afterRendering to afterFrameEnd. Includes submission
  and possible waits; is not proof of GPU execution or display presentation.
- A missing or reversed phase is -1. The next frame clears prior phases;
  aborted frames and duplicate end signals are not combined across frames.

Frame-swap telemetry retains up to 256 timestamps per window attachment,
with at most one pending GUI notification. The consumer drains timestamps
in order, outside the queue lock. Overflow drops the oldest samples and
restarts pacing statistics before the retained burst; visibility epochs
reject obsolete samples. This handoff is independent of media presentation
acknowledgments.

Separate records identify GUI delivery delays >= 25 ms (from the oldest
frameSwapped timestamp in a batch to its queued GUI callback; includes
sample and dropped counts), synchronous Feed and
Profile projections, Profile sorting and Magazine layout >= 8 ms, window
dimension changes and fixed-name shell navigation markers. These scopes
include synchronous callbacks and can overlap each other; they are not
exclusive CPU time or a causal attribution. No URLs, text, account IDs or
cast identities are recorded. Diagnostic logging itself may perturb timing.

Use the trace alongside Qt scene-graph logs and Instruments Time Profiler;
do not infer GPU saturation from a long CPU render span. For a normal
performance comparison, turn these categories off. The Qt fixture checks
synthetic phase delays, missing synchronization, duplicate/detached frames,
disabled logging and a real RHI OpenGL render with an injected recording
delay. This validates instrumentation, not Metal driver performance.

## Bounded telemetry, quit quiescence and batched admission

Frame-swap timestamps cross the thread boundary in the attachment-local fixed
256-element FrameObservationQueue introduced on main, with one queued drain
rather than one event per frame.
A short mutex protects only timestamp storage; the drain copies samples
before releasing the lock and never calls QML or telemetry observers under
it. The GUI consumes original ordered timestamps, not callback times.
Overflow retains the latest 256 timestamps, resets the observation burst
rather than fabricating an interval across lost samples, and logs the
number lost when stall tracing is enabled. Visibility changes invalidate
the old burst; detached attachments cannot update the new window.
`guiDeliveryDelayMs` describes the first queued timestamp in one batch
and includes `samples` and `dropped`, so event counts must not
be compared directly with pre-batching per-frame delay logs. The independent
media-presentation acknowledgement barrier remains unchanged.

Native quit calls the irreversible controller shutdown before runtime
producer cleanup (also on aboutToQuit). The controller detaches render
telemetry, rejects new presentation requests and discards its pending
queue. Major Feed/Profile projection, Magazine layout/admission, Gallery
and supporting-preview admission, raster source/decode/presentation and
optional shell utility entry points refuse new work once this flag is set.
Previously admitted objects are not forcibly deactivated by a global
binding. QML teardown still drains existing incubators, then also cancels
unfinished Loading loaders even when the public incubator count is zero
(a component can be waiting for URL data). The existing Cocoa-safe
Popup/Overlay/window destruction order is retained; no early hide,
releaseResources, private Qt APIs or warning suppression. This addresses
specific producer paths but does not prove Qt's broader deferred-creation
counter is zero on every native quit.

Magazine reconciliation evicts departed contiguous ranges before inserting
replacement rows, batches adjacent new rows, and publishes count changes
only when the final count differs. Retained identities, committed lane
assignments and persistent indexes remain. This avoids the temporary
old-plus-new viewport population and repeated model insertion notifications.
New opt-in `magazine.geometry` and `magazine.admission` scopes distinguish
the relayout call (including synchronous geometry callbacks) from viewport
reconciliation (including synchronous delegate work). They overlap the
existing `magazine.layout` scope; do not add them to that total. The scopes
do not capture asynchronous incubation or prove which binding is slow.

Tests exercise 10,000 queued frame signals with one MetaCall, original
frame timestamps, overflow, visibility/detach and irreversible quit on
software/RHI; a nonresponding loopback component URL with zero active
incubators; scheduled preview and raster suppression; and native/QML
Magazine range batching and retained indexes. No full application or
Metal benchmark is implied by these isolated fixtures.

## Frame-bounded magazine admission and feed reconciliation

Magazine admits at most three new native viewport slots per transaction, prioritizing
visible cards before nearest prefetch slots. Numeric geometry and existing resize
retention remain independent; subsequent 16 ms timer passes recompute the current
viewport instead of completing obsolete scroll requests. The cap is on work units,
not a claim that every transaction meets the display deadline. Feed reconciliation
also removes departed contiguous ranges before batching adjacent new identities.
Retained IDs keep persistent indexes. Initial feed projection is deferred and hidden
surfaces do not schedule projection work. The optional feed.classify, feed.roles
(cumulative from projection start), and feed.reconcile scopes separate filtering,
role construction and synchronous model/delegate notifications. These scopes overlap.

## Navigation incubation, renderer buffers and shutdown

Heavy route loaders incubate asynchronously with the existing GenySpinner
during loading; the retained Explorer/Profile/Cast surfaces preserve their
identity and navigation revisions. Shell scroll callbacks now belong to
object-owned timers and are route-checked. Profile sorting is performed only
when the visible surface reconciles, rather than eagerly on hidden row changes.
The optional shell.loader.* markers distinguish cold construction from a
warm route transition; elapsed time between loading and ready is wall time,
not exclusive GUI-thread CPU work.

Shutdown quiescence is registered before native media/mini-app close callbacks.
Pending loaders are discovered through both QObject and visual item trees.
After cancellation, a second bounded incubation drain runs while contexts are
alive. Global deferred deletion remains within the existing ordered native
window/overlay teardown; unrelated engines are excluded from cancellation. The afterCancellation counter is public incubation state,
not the engine's internal in-progress creation counter. Do not suppress the
engine warning or claim it is fixed until the device quit trace is clean.

Stroke and Superellipse use DynamicPattern geometry, explicitly mark modified
vertex data dirty, avoid same-count allocation calls and no longer mark an
unchanged vertex-color material dirty. Shape sampling, AA and colors are
unchanged. The frame trace includes newMediaTicket, a correlation flag for
newly synchronized media admission, not an upload duration or proof of GPU
causality. Inspect Metal with Instruments for remaining recording spikes.

## v3 follow-up: request identity and placeholder incubation

The v3 Metal device log exposed a profileFidText binding loop during cold
ProfileSurface incubation. The request FID is now an explicit decimal-string
snapshot committed by the existing detail synchronizer, rather than a
live binding to the broad bridge stateChanged signal. A visible profile
reconciles its request identity before projecting rows. The Magazine
skeleton also incubates asynchronously: the previous slot-count budget
did not bound synchronous construction of the placeholder's item tree.
Neither change establishes a measured maximum frame time. The engine's
remaining creation warning, long GUI delivery delays and recording spikes
require separate device verification; no warnings are filtered.

## Profile header-first navigation

Cold ProfileSurface construction is synchronous again, unlike the other
route loaders. Its nested cast loaders remain asynchronous and the stable
model is not populated until the active header has been constructed and
given a frame to request and present identity media. A 450 ms maximum
media/metadata priority window (after construction) prevents unavailable
covers or avatars from blocking posts. On a warm same-identity return,
existing rows are retained and the gate is not reset. A real identity change
restarts the gate before clearing the old model. The profile gate does not
change image download policy or cover/Avatar styling. A late network reply
can still arrive after the bounded window; the permanent cover slot remains.

## Two-sided native viewport work budget

The latest device trace contains an admission operation ending with zero slots
that took 36 ms. Insert limits alone cannot address destruction costs. A positive
eviction budget now limits model removal to four slots per production pass, with
three new slots admitted at most. During retirement debt the old slot is hidden,
noninteractive and excluded from active source-index lookup. The model retains
physical ordering until debt is cleared and admits replacements without growing
the transient working set. Hidden routes drain by the same timer. Height
estimation is no longer performed synchronously during Repeater construction.
Native callers using zero budgets retain synchronous semantics.
Enable genycaster.render.masonry.debug for planning/eviction/publish CPU spans;
these measure model signal delivery including synchronous QML observers, not
actual GPU duration. No visual-quality settings are modified.

## RHI main pass and low-allocation model publication

The public beforeRenderPassRecording/afterRenderPassRecording signals split
recordingMs into preMainPassMs, mainPassMs and postMainPassMs. Missing hooks
remain -1 (not zero). Pre-main time can include texture preparation and
offscreen passes; all spans are CPU wall time, not GPU execution timing.
The primitive feed and masonry models skip searches for known new identities
and open contiguous insertion gaps once, rather than repeatedly shifting
the same suffix. The allocation-free retirement range primitive is tested
independently of Qt; the model fixture covers Qt signals and indexes.

## Correlating GUI event delivery

Enable genycaster.render.gui.debug to time outermost main-thread Qt notify
calls at 16 ms or more. The category logs event type, receiver class and a
monotonic start/duration in the same clock domain as frame observations.
Nested delivery is counted within its outer event, not reported twice;
the guarded receiver may disappear during DeferredDelete. No event
payload, account, URL, object name or user content is logged. This timing
does not cover all native platform callbacks and cannot by itself separate
CPU execution from waits. It is disabled by default.

The isolated rendering_pacing_fixture now includes the pure C++ retirement
test and native FeedRowsModel/MasonryWindowModel test, in addition to
render pacing. Release builds retain the retirement assertions.

## v4: cooperative retirement deadline

The v4 device trace shows planning below 0.022 ms, whereas eviction reached
28 ms for four rows. Production retirement now has a cooperative 5 ms time
budget in addition to the existing four-row cap. It checks the budget between
single-row model removals, always permits at least one unit of progress and
never interrupts a Qt model notification. One expensive observer can exceed
the deadline. The time policy is independent of diagnostic logging. If fewer
positions are freed than expected, prioritize the nearest admitted cards;
retain the existing transient working-set cap. A hidden route schedules
retirement rather than executing it inside the navigation input callback.
The existing no-time-budget API retains its contiguous-range behavior.
Tests simulate a slow observer without wall-time thresholds.

## Feed publication and duplicate placeholders

v4 showed 86.5 ms inside feed.reconcile with only 8 rows. Enable
genycaster.render.feedmodel.debug to isolate reset, insertion, removal,
move, dataChanged and countChanged observer costs. A cold feed now uses
one insertion instead of resetting an empty view; an equal-count reset
does not redundantly notify countChanged. Stable-order consecutive
revision-only changes publish a single exact range; media/presentation
roles keep their individual signals and unchanged rows are not notified.
Magazine already owns a crossfading placeholder: FeedRow no longer creates
a second hidden CastSkeleton inside the transparent magazine card.
Timeline retains the existing placeholder style and reservation.

## Owned delegate callbacks and native anchor lookup

Feed navigation and geometry restoration use indexForKey/keyAt instead
of allocating a QVariantMap and crossing into C++ for every row. Pooled
timeline delegates no longer receive scroll-settlement callbacks; the
magazine has independent measurements. SpriteEmoji interest and atlas
publication, AnimatedEmoji replay restoration and GenyTabStrip reveal
requests are coalesced in object-owned timers rather than global
Qt.callLater closures. Replay restoration validates the actual renderer
and Unicode identity and restores artwork even if motion is paused.
These changes reduce lifecycle work but do not establish the cause of
the outstanding engine creation warning. No Qt-private counter is
changed and no warning is suppressed.

Regression coverage: tests/wasm/feed_publication_lifecycle_20261128.test.mjs
executes the actual timer-handler bodies against stale identity, hidden,
shutdown and renderer-replacement cases. These are source/logic tests;
the real QML engine and Metal device remain necessary for visual and
lifecycle validation.

## v5: separate immutable media inputs from viewer updates

The v5 trace attributes 34.42 ms of a 34.51 ms reconcile to one role
notification, not the removal of 68 primitive rows (0.057 ms). Role
diagnostics now include slowRoleMs and slowRoleMask (1 = revision,
2 = presentation kind, 4 = media layout; bitwise combination). These
measure synchronous observers, not just native model work.

CastItem snapshots media inputs (cast identity, text, parent URL, art-source
flag and ordered embed records) independently of viewer state. Equal input
records retain their existing array and partition identities; nested metadata
changes and new cast identity still publish normally. The native comparator
checks exact QVariant types, ordered lists and nested map values. Unknown
types or excessive depth conservatively trigger the normal update path.
This does not defer likes/recasts, suppress metadata updates, or alter
classification policy and rendering style. The effect on device latency
must be measured rather than inferred from the v5 log.

## v6: keep one rich Cast renderer per semantic row

The v6 role mask identifies a single PresentationKind notification at 57.55 ms.
ConversationRow, VisualPost and DeveloperPost all ultimately contained CastItem.
SemanticFeedItem now owns one CastItem across classification changes instead of
switching the entire nested Loader. Only the small developer label is lazy;
visual insets, compact policy, magazine height and shared media/neon policy
are preserved. This eliminates a known subtree replacement path, not every
possible binding or compact-layout cost. No render quality setting is changed.

## Profile cold construction

The v6 first profile click lasted 171.55 ms and enclosed the synchronous
profile loader's Ready marker. Both reading modes previously constructed
the full cover/bio header on cold entry. Each now constructs its header
only when selected and retains it after first use. This preserves the
header-first post gate and repeated-switch retention. The first-ever
alternate-layout switch may pay its one-time construction cost instead.
The actual navigation latency and visual behavior need device validation.

## v7: playback callback lifetime and timer diagnosis

The v7 trace has a 1975.69 ms QTimer event after native video startup
and a separate 209.96 ms render recording (207.54 ms pre-main-pass).
This does not identify the timer implementation or prove a decoder cause.
GUI traces now report the timer ID and receiver's parent class (no names
or payloads). The video player writes the existing muted Q_PROPERTY,
rather than calling an unregistered C++ setter. Transport start, reload
and seek restoration use object-owned timers, canceled on new requests,
reset and destruction. Source preparation request IDs are invalidated
on replacement. The native decoder stays on its supported Qt thread;
these changes do not claim to eliminate platform backend blocking.

## Preserve sparse feed intersections

For snapshots at most 512 rows, the model retains every surviving stable
identity even when fewer than 25% survive. A disjoint replacement still
resets; large replacements retain the existing escape hatch. The test
checks persistent indexes and exact final order. This reduces avoidable
incubation invalidation but does not eliminate cancellation of obsolete
delegates. Native tests require the real Qt SDK.

The video source setter is also guarded by a transport epoch because a
synchronous backend error can queue recovery before setSource returns.
An older request must not re-arm deferred transport after replacement.

## v8: throttle publication after a slow retirement

The v8 trace records one 17.88 ms admission with 9.76 ms eviction and
8.07 ms publication. After retirement exhausts its cooperative deadline,
admit at most one nearest new slot, preserving forward progress; defer
other new slots to the next pass. Existing visible geometry is not delayed.
Adjacent changed retirement flags are published in exact ranges. The
admissionThrottled trace distinguishes this branch. This is not a hard
real-time bound: synchronous observers may still overrun it.

Network event diagnostics add serviceClass (the receiver's grandparent
class). The cast views, browser reactions, browser preview, stats and market
network managers use their owning QObject as parent. No endpoint, token,
user content or request body is logged. This helps identify v8 network
callback stalls without guessing which service is responsible.

## Coalesced derived-data interest scans

Cast views and browser reactions keep their synchronous retain/release
ownership and immediate removal of unsent requests. A single owned
zero-interval timer coalesces cache scans triggered by retain, release
and ensure during model transactions. Existing retry deadlines, network
batch limits, metadata updates and optimistic reaction notifications are
unchanged. Completion and retry may run the scan directly and cancel
the redundant scheduled scan. This reduces repeated cache traversal
during viewport changes; it does not identify the specific v8 network
callback or eliminate synchronous QML observer work.

## v9: separate cold profile incubation and skeleton visibility

v9 records a 139.74 ms profile input event enclosing the loader Ready
marker and 73.40 ms render recording shortly afterwards. The profile
route loader now uses asynchronous incubation again, preserving the
existing header-first admission gate and applying the initial scroll
reset after Ready. Total time to header may differ; validate on device.

Magazine skeleton creation is latched once needed for an admitted slot.
Visibility and animation remain viewport-bound; no self-referential
Loader.item activation or repeated cancellation on viewport crossings.
A requested skeleton is retained (hidden and non-animating after the
crossfade) until the bounded slot retires. This trades bounded resident
objects for less creation churn, and is not general delegate recycling.
v9 still reports 24 unfinished QML creations despite a zero public
incubation count. These changes do not prove that warning is resolved.

## Native notification row reconciliation

Notifications now use the existing native FeedRowsModel and stable-key
anchor lookup rather than per-row QML ListModel operations and repeated
QVariantMap lookups. Grouping, mute and mention filtering stay unchanged;
the final visible projection drives synchronization. Duplicate stable IDs
keep the first displayed row, and payloads no longer in the model are
removed after publication. The diagnostic notification.projection measures
this path. This is a structural optimization, not proof that the v9
notification navigation stalls are caused by this model.

## Native profile ordering without round-trip payload conversion

Linux Qt 6.12.0/GCC 16.2 probe, five alternating samples per size, measured
the actual QJSEngine call boundary. Passing complete row maps into C++ and
back was slower. A prototype with a native-held source and primitive output
indices measured 2.96 ms versus 6.75 ms JavaScript for 1000 rows, and
21.28 ms versus 46.53 ms for 5000 rows. These are synthetic ranking timings,
not whole application navigation or hardware-rendering results.

The production profile now captures the bridge's native row snapshot and
a token. Native ranking reads only the canonical numeric keys and returns
a stable permutation; QML reuses its existing payload objects. Every row
publication invalidates the token; topic rows and unknown numeric forms
retain local fallback. Both paths explicitly resolve ties by original
ordinal (the Qt JavaScript engine did not preserve every large equal-key
group in the probe). Profile viewport anchors use the existing primitive
native key lookup instead of building one QVariantMap per row.

The optional `GENYCASTER_FIXTURE_PRESENTATION_BRIDGE=ON` CMake switch
also compiles the actual presentation bridge and its configuration module.
It generates non-routable .invalid endpoints for the fixture, keeps
Release assertions enabled in the existing row-notification test, and
exercises native snapshot ordering via a real QML property binding.
No provider runtime or production credentials are needed.

The standalone `masonry_lifecycle` fixture instantiates the production
FeedMasonryView with the actual native models and lightweight synthetic
card/chrome components. It repeatedly jumps among 500 rows, resizes,
hides and restores the route, replaces the source and checks bounded
slot ownership, fully drained retirement, stable identity and lifecycle
warnings. It reports wall time to a settled target, not GPU frame time.
The synthetic card intentionally does not claim the cost of CastItem's
network/media hierarchy.

## Measure on a real device

For macOS, quit the existing process first and start the actual app binary:

```sh
APP="$HOME/Downloads/genycaster/build/Qt_6_12_1_macOS_Desktop_Release/bin/Genycaster.app"
QSG_INFO=1 QT_LOGGING_RULES='genycaster.render.pacing.debug=true' \
  "$APP/Contents/MacOS/Genycaster"
```

The optional log reports the backend, reported display refresh rate,
sample count, frame budget, observed P95 interval and number of samples
over 125% of that budget. Up to 120 recent intervals are used; publication
is at most every 750 ms of observed time, with at least eight samples.
A long pause restarts the window. Use QSG_RENDER_TIMING=1 or platform
profilers in a separate diagnostic run to investigate actual CPU/GPU work.

| Refresh rate | Nominal frame budget |
| --- | --- |
| 60 Hz | 16.67 ms |
| 90 Hz | 11.11 ms |
| 120 Hz | 8.33 ms |
| 144 Hz | 6.94 ms |

Use the same hardware, display settings, account/feed data, window size,
profile, and sequence before and after a change. Record cold and warm runs
separately: continuous scrolling, reverse scrolling, resizing, profile
switching, gallery zoom, opening/closing a modal, video playback and
background/restore. Record median/P95 observed intervals during motion and
CPU/GPU timing in the platform profiler, not a guessed FPS at rest.
Check memory after repeated navigation and require no visual regression.
Do not equate a synthetic Qt fixture with application-level GPU benchmarks.

## Lightweight regressions

```sh
cmake -S tests/rendering_pacing_fixture -B build/rendering-pacing \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12
cmake --build build/rendering-pacing
ctest --test-dir build/rendering-pacing --output-on-failure
```

This builds only the production rendering controller, native model and small Qt
tests. Keep the build directory across source revisions; Ninja rebuilds
only changed objects. On shared Linux test hosts run with
`cmake --build build/rendering-pacing --parallel 1` and
`ctest --test-dir build/rendering-pacing --output-on-failure --parallel 1`
to avoid saturating production services. No deployment or release is involved.
The fixture also registers an isolated engine-shutdown smoke test, the
actual Qt QML JavaScript projection test when host qmltestrunner and Python
are available, and selected source-contract suites when Node is installed.
They are separate from application-level navigation and platform GPU tests.

The software fixture stresses signal deduplication and queued timing;
on Linux, Xvfb additionally tests actual OpenGL scene-graph barriers.
The regular UI tests also register the controller and backdrop regressions.
`tests/ui/drawer_backdrop_runtime.py` tests capture ownership and resource
budget stability using the production QML (with its shader URL redirected
to the identical local QSB file for the isolated fixture).

When Python, Qt host qsb/qmltestrunner and Xvfb are available, the same
standalone CTest fixture also runs three RHI pixel/capture tests on Linux.
To test on a real Mac, use the Qt 6.12 host tool path and run the Python
fixtures with the normal Cocoa platform and Metal backend, for example:

```sh
export QML_TEST_RUNNER=/path/to/Qt/6.12/macos/bin/qmltestrunner
QSG_RHI_BACKEND=metal python3 tests/ui/gaussian_shader_equivalence_runtime.py
QSG_RHI_BACKEND=metal python3 tests/ui/reflection_capture_runtime.py
QT_QUICK_BACKEND=rhi QT_QPA_PLATFORM=cocoa QSG_RHI_BACKEND=metal \
  python3 tests/ui/drawer_backdrop_runtime.py
```

The Gaussian and reflection scripts require a hardware-capable RHI path;
a software Qt Quick backend cannot validate ShaderEffect pixels.
Compare performance separately using the application-level workload above.
