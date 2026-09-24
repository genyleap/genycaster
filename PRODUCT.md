# Genycaster Product Context

## What this product is

Genycaster is a native, local-first Farcaster client for people who want social reading and writing to feel immediate even when the network is slow or unavailable. The shared C++ core owns canonical data, routing, persistence, and sync; Qt Quick owns a fast adaptive presentation for mobile and desktop.

## Product register

- Product application, not a marketing site or developer console.
- Global audience with English and Persian as first-class launch languages.
- Frequent, glanceable mobile use and focused desktop reading.
- Telegram-class interaction smoothness and personalization, without copying Telegram's visual identity.

## Brand character

Genycaster is calm, assured, tactile, and quietly futuristic. It uses a distinctive deep-green Geny palette, high-legibility typography, and superelliptic geometry. Content remains primary; color and motion clarify hierarchy and state instead of decorating every surface.

The memorable signature is the **Geny current**: a restrained accent line or pulse used to communicate selected context, live freshness, and safe forward motion. It never becomes ambient neon or a continuous distraction.

## Users and core jobs

- Open previously seen feeds and profiles immediately from local state.
- Understand whether content is cached, refreshing, live, offline, or blocked by a source issue.
- Move between Following, Explore, profiles, casts, and social lists without losing context.
- Publish or reply when account and signer state allow it.
- Personalize appearance, language, region, calendar, time zone, and currency locally.

## Platform and accessibility

- Native Qt Quick application with one custom cross-platform control language.
- Compact, standard, and wide adaptive layouts; touch, pointer, keyboard, LTR, and RTL are first-class.
- WCAG 2.2 AA-equivalent contrast, focus visibility, semantic labels, 44 px minimum primary targets, and reduced-motion support.
- System, Dark, and Light appearance modes with user-selectable color themes.

## Anti-references

- OS-native control chrome or platform-divergent styling.
- Neon-on-black crypto terminals, purple SaaS dashboards, glass blur, and equal-weight card grids.
- Decorative animation that delays reading or replays on every refresh.
- Emoji used as product iconography.
- Provider names, raw cursors, backend errors, or protocol jargon in release UI.

## Product invariants

- Cached content is never blanked during refresh.
- QML performs no network, database, signing, or settings persistence I/O.
- Theme and locale preferences are device-local presentation state.
- User-generated content keeps its own direction and is never translated.
- A visual redesign does not change provider boundaries, canonical models, or local-first behavior.
