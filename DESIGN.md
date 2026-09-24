---
name: "Genycaster"
description: "A content-first Farcaster client with messenger discipline, neutral surfaces, precise bidirectional text, and native superellipse geometry."
---

# Genycaster Design System

## Product direction

Genycaster follows **messenger discipline** rather than dashboard styling. The interaction model takes cues from Telegram Desktop: content dominates, application chrome stays quiet, surfaces are neutral, selected states are obvious without being loud, and dialogs remain compact. This is a product-specific system, not a visual clone.

The Farcaster identity comes from cast anatomy, conversation context, local-first state, profile data, and a restrained blue interaction accent. Brand color never tints the entire dark interface.

## Runtime contract

`ui/theme/Theme.qml` owns semantic color, typography, spacing, geometry, layout, and motion tokens. Screen components consume semantic roles only. Raw colors outside the theme, arbitrary font sizes, `Rectangle.radius`, native Qt visual chrome, and one-off rounded geometry are release failures.

The visual layers are:

1. Canvas — application background and workspace separation.
2. Surface — navigation panes and message/cast surfaces.
3. Muted / raised / elevated surfaces — controls, context blocks, menus, and modals.
4. Interaction accent — links, focus, selection, active icons, and primary actions.
5. Semantic status — success, warning, and danger.

## Color hierarchy

Dark mode is a neutral graphite system, not black plus a saturated brand color. Light mode uses a cool neutral canvas with white material surfaces so hierarchy reads through tone, spacing, grouping, and restrained borders rather than tinted dashboard cards:

| Role | Dark | Light |
|---|---|---|
| Canvas | `#0B0C0F` | `#F1F1F3` |
| Surface | `#121317` | `#FFFFFF` |
| Muted surface | `#17191E` | `#F7F7F8` |
| Raised surface | `#1B1D23` | `#FFFFFF` |
| Elevated surface | `#202229` | `#FFFFFF` |
| Primary text | `#F7F7F8` | `#17181B` |
| Secondary text | `#AFB2BA` | `#60636A` |
| Tertiary text | `#858A94` | `#898D95` |
| Border | `#292C33` | `#DFE1E5` |
| Divider | `#22252B` | `#ECEDEF` |
| Geny interaction accent | `#829AFF` | `#315CF5` |
| Geny primary fill | `#526FFF` | `#315CF5` |

Text/link accent and filled-action accent are separate roles. This keeps links bright enough on dark surfaces while preserving white-on-button contrast. Alternative accent presets may change interaction color; they never recolor the canvas or neutral surfaces.

## Typography

The application family is `IRANSansXFaNum`. The semantic scale is deliberately readable for Persian and Latin content:

| Role | Size | Line height |
|---|---:|---:|
| Caption | 12 | 1.45 |
| Meta | 13 | 1.45 |
| Label | 14 | 1.42 |
| Body | 16 | 1.55 |
| Title 3 | 18 | 1.38 |
| Title 2 | 22 | 1.32 |
| Title 1 | 28 | 1.28 |
| Display | 34 | 1.20 |

Components choose roles instead of inventing pixel sizes. User-generated cast text uses Body by default.

## Bidirectional text

RTL is not implemented by reversing every component. There are two independent direction systems:

- **Application chrome** follows the selected locale through root `LayoutMirroring`. Navigation order, settings layout, buttons, and shell alignment mirror naturally.
- **User-generated content** resolves direction from its first strong character. Persian/Arabic text aligns right; Latin text aligns left regardless of UI language.

URLs, handles, FIDs, currency codes, protocol identifiers, and other inherently LTR tokens are explicitly isolated. Their glyph order never reverses inside a Persian interface. Components that perform manual positioning opt out of inherited mirroring before applying explicit directional geometry.

## Superellipse geometry

All branded rounded geometry uses the Lamé superellipse:

`|x/a|^n + |y/b|^n = 1`

The canonical exponent is `n = 4.0`. `Superellipse.qml` is backed by the native `SceneGraphSuperellipse` QQuickItem. Its fill and border are emitted as retained `QSGGeometryNode` triangle geometry from `updatePaintNode()`, with device-pixel-ratio-aware adaptive tessellation and a one-physical-pixel premultiplied-alpha feather. It does not construct SVG path strings or route branded surfaces through `QtQuick.Shapes`. The renderer also implements mathematical containment, so interaction and appearance share the same geometry.

Curve roles:

- Tiny: 6 px
- Compact: 10 px
- Control: 14 px
- Container: 18 px
- Large: 22 px
- Modal: 28 px

Circles are reserved for avatars and true circular indicators. Dividers, canvas panes, and structural full-bleed surfaces remain rectangular by design. Media uses `SuperellipseImage`, which masks the image content itself rather than placing an unclipped image over a rounded background.

## Native scene-graph rendering

Qt Quick's retained scene graph is the default rendering contract. Standard `Text`, `Image`, layout and simple Qt Quick items remain native scene-graph primitives; they are not replaced by custom painting merely for abstraction. Product-owned curved visuals use `QtQuick.Shapes` with `Shape.CurveRenderer`, which preserves curve geometry on the GPU and provides high-quality analytic antialiasing without requiring window-wide multisampling. `Canvas` and `QQuickPaintedItem` remain forbidden for product UI vector drawing.

The current visual primitives are:

- `Superellipse` — stable declarative cubic paths rendered by `Shape.CurveRenderer` for all continuous-corner branded surfaces and borders.
- `VectorStroke` — `Shape.CurveRenderer` arcs, sparklines, and dashed rounded drop targets.
- `SceneGraphSuperellipse` — non-visual mathematical containment helper retained for O(1) hit testing.
- `SceneGraphStroke` — compatibility QML type retained for source/ABI stability; production QML must not instantiate it.

The recommended Adaptive profile does not force global MSAA. Qt Quick keeps its normal scene-graph antialiasing for standard primitives while `Shape.CurveRenderer` owns curve-edge quality. Fidelity explicitly requests 4× window MSAA for users who prefer the additional hardware-dependent coverage cost. The UI guard rejects visual regressions back to custom triangle-feather strokes, dynamic SVG path generation, `Canvas`, or `QQuickPaintedItem`.

`SuperellipseImage` requests decoded image dimensions at least as large as the physical display size (`logical size × devicePixelRatio`) while retaining explicit larger source requests for hero media. This prevents small previews and icons from becoming soft on Retina/HiDPI displays.

## Optical depth and elevation

General-purpose UI drop shadows remain disabled. `Superellipse.qml` keeps the existing elevation-related API properties only for source compatibility and does not instantiate a generic shadow effect. Structural depth is still communicated primarily through tone, spacing, containment, and restrained borders.

Rich media has one deliberate exception: image/video/link artwork may render a **media-only chromatic shadow** derived from the media pixels themselves. This is an art-direction treatment, not generic elevation. It follows the original CuteMobile ColorizedBox anatomy using the Qt 6 scene graph: a downsampled copy of the visible media pixels is inset slightly, shifted downward, and blurred behind the media. The shadow must stay optically attached to the media edge and must never become a full-width tinted panel or alter feed layout.

Current depth rules:

- Use tonal separation, spacing, superellipse containment, and restrained borders for ordinary hierarchy.
- Context-rail cards use white/graphite surfaces over a cool neutral rail canvas with no generic shadow.
- Popovers, drawers, controls, and floating actions do not render project-owned generic drop shadows.
- Media chromatic shadows are bounded to the painted media rectangle, use a small explicit blur padding budget, and reserve only a minimal bottom tail outside layout.
- Media effects reproduce CuteMobile ColorizedBox geometry: the blurred clone is inset by approximately 10 logical pixels per edge, offset downward by approximately 15 pixels, and rendered inside an explicitly transparent offscreen canvas. `MultiEffect.autoPaddingEnabled` remains disabled; transparent-border behavior comes from the canvas itself, never effect-item growth.
- The chromatic shadow remains visually stable while scrolling; scroll velocity must not switch the media between different art-direction states.
- Focus remains explicit through semantic border/focus treatment and never depends on shadow.
- Existing `elevation` properties remain accepted by shared components for source compatibility but are not the media-shadow API.

## Cast anatomy

A feed cast is a messenger-style reading unit:

1. Avatar outside the message surface.
2. Superellipse message surface with quiet border and hover state.
3. Author name and isolated handle in a compact header.
4. Context/reply preview as a muted inset with a narrow accent rail.
5. Direction-aware cast body.
6. Superellipse-masked embed/media blocks.
7. Quiet engagement row and timestamp.
8. Owner-only destructive action behind a contextual menu.

The entire cast opens its conversation. Repeated “view conversation” buttons are intentionally absent.

## Navigation and layout

Compact layout uses one reading column, a compact top bar, and bottom navigation. Desktop uses a dedicated navigation surface, bounded reading column, and an optional context rail only when enough space exists. Followers and Following are profile-level destinations, not permanent global navigation.

The desktop shell does not stack a global header, sidebar, and feed tabs for the same actions. One surface owns each navigation responsibility.

## Profiles and detail views

Profiles use a flat messenger pane rather than a stack of floating cards. Banner, avatar, identity, metadata, counts, follow action, and content tabs form one hierarchy. Content tabs use low-weight selection indicators. URLs and handles stay LTR-isolated; bios and locations resolve their own text direction.

Cast detail uses the same message-surface language as the feed instead of switching to a different card system.

## Dialogs and settings

All modal flows use `GenyModal` with a common elevated `PopupSurface`, semantic overlay, keyboard focus, compact spacing, and restrained opacity transitions. Compose reply context follows the same accent-rail anatomy as cast reply previews.

Settings is divided into Appearance and Language & Region. Desktop uses internal navigation; compact layouts use top-level section controls. Option groups are flat and separated by dividers. Stateful binary preferences use `GenySwitch`. Appearance mode uses a segmented surface rather than three unrelated buttons.

## Project-owned controls

Qt Quick Controls provide behavior only. Visuals are project-owned:

- `ActionButton`
- `NavItem` / `BottomNavButton`
- `GenyTabButton`
- `GenySwitch`
- `GenyComboBox`
- `GenyScrollBar`
- `GenySpinner`
- `DirectionAwareTextField` / `DirectionAwareTextArea`
- `GenyModal`

Interactive controls follow one shared anatomy: continuous superellipse shape, semantic fill, restrained border, tonal state separation, explicit focus treatment, and state-layer motion. Primary actions use saturated fill; secondary actions use neutral material; quiet actions remain flat until hover; destructive actions reserve semantic red for meaning rather than decoration; selected controls use a low-chroma accent surface. Segmented selections sit inside a muted container and distinguish only the active segment.

Desktop controls are normally 38–44 px high while touch-relevant targets preserve at least 44 px where required. Press feedback uses approximately 1–1.5% visual compression rather than large spring movement. Hover, pressed, focus, selected, disabled, and destructive states never reflow component geometry. Icon-only controls keep a compact optical plate and accessible name.

## Composer media upload

Media upload is an attachment manager rather than a single status strip. The visual anatomy is deliberately quiet and file-oriented:

1. A large continuous-corner drop target with a native scene-graph dashed outline.
2. Click-to-browse and external drag-and-drop using the same insertion position.
3. Multi-file selection with sequential native uploads so the existing single-upload lifecycle remains authoritative.
4. A current-transfer row with HiDPI local preview, filename, real local byte size, lifecycle status, progress, cancel, and retry.
5. Completed media rows with stable URL identity, media type, open action, and removal from the rich document.
6. A compact attachment strip and explicit add-more affordance.

The panel only exposes media types supported by the existing upload contract. Unsupported PDF/audio/file semantics are not fabricated from the visual reference; they require an explicit protocol/backend capability before becoming selectable UI.

## Motion

Motion communicates state only:

- Fast: 90 ms
- Standard: 150 ms
- Emphasis: 210 ms

Reduced-motion mode removes nonessential movement. Modal entrance/exit uses opacity rather than decorative scale travel. Scrollbars are thin and fade when inactive.

## Accessibility

- Normal text targets WCAG AA contrast.
- Icon-only actions expose accessible names.
- Focus is visible independently of hover.
- Superellipse controls use shape-aware containment.
- Native visual controls are not allowed to leak into the custom system.
- Persian RTL and English LTR are release-gate layouts, not post-release fixes.

## Verification gates

The project includes automated design, localization, and architecture guards. Release verification must additionally include runtime checks with the target Qt SDK and rendered passes at:

- 390×844 compact dark
- 760×900 standard dark
- 1280×900 wide dark
- 1280×900 wide light
- Persian RTL in feed, composer, profile, settings, and cast detail
- mixed Persian/English cast content
- loading, empty, offline, source-error, and write-state flows

`qmllint`, a clean Qt build, keyboard traversal, modal focus behavior, and visual screenshots remain final release gates.

## Profile and discovery rules

- Identity lookup belongs to Explore/search surfaces only. Settings, account menus, side navigation, and profile chrome must not contain username lookup controls.
- Global account chrome uses the semantic `Account` label instead of repeating the viewed profile display name.
- Profile metadata is de-duplicated after normalization: a homepage or location equal to the username/handle is rendered once, not as repeated rows.
- Profile banners use the enriched profile response first and the deployed protocol banner UserData slot as fallback. Missing/failed banners render an intentional neutral cover surface rather than an error glyph.
- Cast surfaces remain neutral in both appearances. Ownership is not encoded by a saturated bubble color; author identity and actions provide context.
- Protocol implementation details such as `chain://` parent identifiers are not shown as user-facing reply text.

## V7 interaction hierarchy amendment

- The detailed authenticated-account Trust/Profile summary belongs to the desktop navigation rail; it must not be duplicated in the right context rail.
- The right context rail uses task/context widgets. Its top analytics surface is derived from currently loaded feed engagement and must not fabricate unavailable historical metrics.
- Unavailable authenticated profile metrics render an explicit loading state; `0` is reserved for a loaded, valid zero.
- Desktop rich composition is non-modal so unrelated shell surfaces remain usable. Compact/mobile composition may remain modal where the composer is the primary task surface.
- Media attachment UI uses progressive disclosure: compact attachment metadata rows plus one dedicated horizontally scrollable thumbnail strip. The visual rich editor must not duplicate large media previews already represented by the upload/preview surfaces.
- Farcaster visual preview may suppress exact media URL-only fallback lines when it renders the corresponding protocol media preview, but canonical safe serialization and published byte accounting remain unchanged.

## V10 localization, shell-status, drawer, and rich-preview amendment

- Calendar presentation is calendar-system aware and UI-language aware. Calendar arithmetic comes from `QCalendar`; visible month titles, weekday labels, day numbers, Trust Score values, follower/following counts, and other UI-owned numeric labels must use the selected UI language's numeral glyphs. Persian UI uses Persian digits and month-first Jalali titles such as `شهریور ۱۴۰۵`.
- Persistent runtime/source/network state belongs to the navigation rail rather than a top-of-content banner. Status changes must not alter feed geometry. The sidebar status control may expand for diagnostics/retry without pushing global shell chrome around.
- Desktop rail widths are adaptive within bounded design limits. Additional desktop width is allocated to the left navigation and right context rail before creating dead gutters around the reading surface.
- The Trust/Profile card is a reversible two-face surface. Tapping the front reveals the profile detail face; tapping the back returns to the metrics face. The card sizes from both faces instead of clipping the larger face. Temporary Trust Score presentation is backed by the normalized Neynar score and is explicitly labeled as a preview.
- Drawer translucency is not used to imply depth. The drawer surface remains opaque; when context separation is required, the application environment behind it is captured and blurred through a project-owned overlay.
- Cast URLs are presentation candidates, not necessarily literal text-only output. Feed and detail surfaces infer one missing HTTP(S) link preview from cast text and render known protocol embeds through reusable preview components. Images use gallery layouts; videos, audio, files, YouTube, Apple Music, GitHub, and generic Open Graph/Twitter-card links use typed previews. Published protocol serialization remains unchanged.
- Remote preview metadata/network work is owned by C++ presentation infrastructure. QML arranges already classified preview data and does not parse HTML or perform arbitrary network requests.

## Media chromatic shadow contract

Rich media may use a media-derived chromatic drop shadow as a deliberate visual
exception to the general no-decorative-shadow rule. This effect is constrained:

- it is derived from the actual rendered media pixels, never a palette rectangle;
- the blurred copy is slightly inset and shifted downward behind the sharp media;
- blur must be strong enough that no source-image detail is legible in the shadow;
- only the falloff outside the sharp media should be perceptible;
- effect padding is bounded and must not become a card-sized coloured field;
- image, video poster, collectible and rich-link treatments share the same effect;
- scroll state must not switch the effect topology on and off;
- effect inputs are downsampled before blur to bound scene-graph cost.

## Media Chromatic Shadow

- Project-owned generic drop shadows remain disabled.
- Media may use a source-derived chromatic drop shadow modeled after CuteMobile's `ColorizedBox`.
- The blurred clone stays inset from the sharp media, is vertically offset, and remains occluded by the sharp media body.
- The offscreen surface must reserve enough transparent feather space for the blur kernel to decay to zero before the texture edge; any visible rectangular cut line is a rendering defect.
- Blur surfaces remain downsampled and bounded for feed performance.

## Social reading layout contract

- Desktop social surfaces use a bounded reading column rather than stretching casts, profiles, and media across the entire workspace.
- At sufficiently wide desktop widths, the shell uses three zones: primary navigation, a roughly 800–900 px social reading column, and a 324–372 px contextual sidebar. The sidebar never appears by squeezing the reading column below a useful media width.
- Feed, profile, and cast-detail pages share the same desktop density. Home remains its own dashboard composition.
- A cast has one primary visual surface. When a cast already renders an image/gallery, supporting web destinations collapse to compact cards instead of repeating another large poster.
- Ordinary web destinations are vertical, scan-friendly cards. The only supported two-column embed composition is an intentional destination + token pair with explicit minimum widths.
- Link-preview loading is bounded. Metadata and artwork both have presentation deadlines; after a deadline the preview falls back to stable text/domain/actions instead of retaining an indefinite spinner.
- Rounded media geometry is consistent across still images, posters, and active video. Active VideoOutput frames retain the direct scene-graph path and use a lightweight inverse-corner overlay rather than a per-frame offscreen mask.

## V21.2 sidebar and Home widget restoration

- The desktop Home surface owns the original single-column `ContextRail`; its default widget stack is My Assets, Community Pulse, Calendar, Market/Tokens, and Suggested. Redesigning Home must not silently disconnect these widgets.
- Feed, profile, and cast-detail context sidebars remain single-column vertical rails. Widening a sidebar may improve breathing room but must not switch the rail into a dashboard grid or a second information layer.
- Left navigation and right context widths remain user-adjustable and persistent. Resizing changes width only; it must not change widget hierarchy or column count.
- The reading pane fills the workspace between rails while individual social surfaces enforce their own bounded reading widths. This avoids dead strips created by fixing the pane itself to the reading-column width.
- Compact/medium layouts may collapse the right rail into the existing drawer, but desktop Home must expose the widget rail whenever the desktop shell is active.

## Continuous timeline pagination contract

Feed pagination is an append/reconcile operation. Loading page N+1 must not reset,
replace, reposition, or geometrically destabilize the already-visible page N timeline.

- `ListView` owns viewport continuity. Presentation code must not write `contentY` to
  compensate for variable-height delegate updates or tail appends. `originY` is not
  assumed to be zero when determining proximity to the end of a variable-height list.
- Pagination busy state must not change footer geometry. Loading indicators may change
  opacity/visibility, but their reserved layout height stays constant.
- The presentation model is incrementally reconciled by stable cast/profile identity. A
  continuation terminal snapshot updates existing rows in place and appends genuinely
  new rows; it never deletes the visible prefix merely because an upstream fallback
  returned only the latest provider page.
- Starting a continuation request must not republish the same cached feed snapshot. The
  cache remains available internally for merge/failure recovery, but the existing UI
  snapshot remains authoritative until the continuation reaches a terminal result.
- Async parent/quote enrichment already painted in the UI is preserved across a
  pagination reconcile. Repeated hydration of the same referenced cast is deduplicated.
- Provider cursors must make forward progress. A provider returning the same raw cursor
  used for the request terminates the sequence instead of creating an endless chain of
  opaque continuation tokens.
- The ListView keeps a bounded warm runway for rich, variable-height delegates so media
  allocation does not repeatedly destabilize content-height estimates near the viewport.

These rules are interaction invariants, not optional animation polish. A violated
continuity invariant is treated as a pagination defect.
