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
