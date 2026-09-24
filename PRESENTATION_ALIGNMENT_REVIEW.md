# Stable presentation geometry and composer preview

Responsibility: center video transport icons, stabilize network status geometry,
and render composer preview text through the existing animated-inline pipeline.
Capability origin: Local.
Mandatory or optional: existing presentation surfaces; no new target.
Public contract: existing playback, retry and composer intents are unchanged.
Canonical data/invariants: status is authoritative, never delayed or falsified;
safe published preview text remains the content source, not rich editor markup.
Dependencies: existing QML layout, rich-text renderer and emoji controller/cache.
Expected failures: existing network errors remain readable; Unicode fallback
remains until animation is ready or when the renderer is unavailable.
Local-state representation: explicit status details expansion only.
Offline behavior: existing cached emoji and saved content remain available.
Provider policy and disablement: unchanged; no QML I/O.
Failover semantics: unchanged.
Tests proving the boundary: execution deferred per the user's no-test request.

## Geometry contract

- Network changes may update text/color/action availability, not sidebar height.
- Only an explicit details toggle may expand the status card; its expanded
  extent is also independent of the current error or diagnostics text.
- Footer status, retry, rendering and telemetry occupy bounded slots; telemetry
  and secondary metadata collapse according to viewport width, not network state.
- Player icon centers are the stage center. Captions have independent placement.
- Preview uses the same safe-text tokenization and inline emoji renderer as a
  plain feed cast, with the looping composer motion policy. Hidden/offscreen
  playback and reduced motion remain controlled by the existing emoji component.
- Mention spans include their visible text as required by the native renderer,
  so switching the preview pipeline does not silently drop @user tokens.

## Verification

Source review only. No configure, build, app, lint or tests executed. Runtime,
visual alignment and animation performance remain for user verification.
Existing emoji runtime test source covers composer looping, reduced motion and
hidden playback; those tests were read, not run. `git diff --check` reported no
whitespace errors in tracked changes; most edited UI files are already untracked
in this working tree and were reviewed directly.
