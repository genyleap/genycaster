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
