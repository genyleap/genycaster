# Media player layout refinement

Responsibility: presentation-only refinement of the existing inline/fullscreen player.
Capability origin: Local.
Mandatory or optional: existing optional media surface; no new feature target.
Public contract: unchanged GenyVideoPlayer properties, signals and playback intents.
Canonical data/invariants: the existing player/coordinator remain authoritative; one decoder and one session.
Dependencies: existing QML controls, scene-graph superellipse, icons and Theme.
Expected failures: existing buffering, unavailable and retry states remain accessible.
Local-state representation: transient focus, hover and transport feedback only.
Offline behavior: unchanged; no additional resources fetched.
Provider policy and disablement: unchanged.
Failover semantics: unchanged.
Tests proving the boundary: execution explicitly deferred at the user's request; no build, lint, app or tests run.

## Design and layout contract

- Audience: feed readers on compact and expanded desktop/mobile surfaces. Video remains primary; playback action and time are immediately legible.
- Direction: restrained dark media chrome in both themes; a single icon target and an unboxed caption beneath it, without concentric frames.
- Persistent Play/Resume/Replay, transient transport feedback and buffering share one badge geometry. Only keyboard focus draws an accent outline.
- The icon itself sits at the exact center of the video in every transport state, independent of the dock and caption. The caption is omitted when a short viewport cannot accommodate it above the dock; the accessible name remains available.
- Timeline spans the available width, on a dedicated row above transport controls. No fixed maximum seek width leaves an accidental gap on wide players.
- The lower row groups playback and elapsed/total time on the leading edge; volume, quality and fullscreen form the trailing group. All icon targets share a 40-unit box and 20-unit glyph.
- One 16-unit inset (12 on narrow/compact surfaces) aligns timeline and lower row. No inner bordered toolbar panel; a bottom contrast scrim owns readability.
- Time and timeline keep left-to-right progression, independent of localized labels. Numeric time uses tabular OpenType figures. Secondary volume detail collapses below 560 units; quality becomes icon-only on narrow surfaces.
- Keyboard activation, visible focus, accessible state names and seek arrows/Home/End remain available. Hidden chrome cannot retain interactive targets.
- Motion is short opacity/scale feedback without overshoot or perpetual pulses; reduced motion disables spatial transitions.
- Timeline and control row share one reveal animation. The contrast scrim fades independently; the quiet progress line remains inside the existing stage mask. Focus and an open quality menu keep controls available.
- Source review also corrected seek-drag initialization and removed writes to the existing read-only scrubbing property. No media backend behavior was changed.
- Outer video geometry, feed width policy, source loading, fallback and fullscreen session ownership are not redesigned here.

## Verification status

Source review only. Compact/wide, light/dark, localized labels, keyboard focus,
scrubbing, pause/resume/replay and fullscreen visual acceptance remain NOT VERIFIED
until the user runs the Qt application.
