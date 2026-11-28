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

This builds only the production rendering controller and small Qt tests.
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
