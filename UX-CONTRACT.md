# Genycaster UX Contract

## Product context

- Audience: global Farcaster users on mobile and desktop.
- Primary jobs: open an identity, read cached feeds immediately, navigate canonical cast/profile/social data, and refresh safely.
- Active locales: complete English and Persian UI catalogs in Phase 5G; additional languages require full catalog/placeholder acceptance.
- Language/region policy: UI language controls translated chrome and LTR/RTL direction; regional format independently controls numbers, date patterns and automatic local currency.
- Timezone/calendar policy: protocol timestamps remain UTC canonical values; presentation supports system/IANA time zones and Gregorian, Jalali, Shahanshahi, Islamic Civil, Julian and Revised Julian display without changing stored time.
- Accessibility target: WCAG 2.2 AA-equivalent Qt Quick semantics, focus, contrast, keyboard, and target sizing.

## Business-context sources

| Domain / scope | Authoritative source | Source type | Reviewed date |
|---|---|---|---|
| Local-first lifecycle | `docs/05-local-first-state-and-sync.md`, ADR 0003 | Architecture / ADR | 2026-08-27 |
| Provider routing | `docs/04-provider-boundary-and-routing.md`, ADRs 0001/0006/0007 | Architecture / ADR | 2026-08-27 |
| Domain normalization | `docs/07-domain-model-and-normalization.md` | Domain contract | 2026-08-27 |
| UI thread and performance | `docs/14-performance-and-ui-runtime.md` | Runtime contract | 2026-08-27 |
| Phase 4 flows | `NEXT_AGENT_PROMPT.md` | Product handoff | 2026-08-27 |

Phase 4 contains no permissions, billing, destructive lifecycle, legal copy, or write operations.

## Visual contract

- Project design contract: `DESIGN.md`.
- Token ownership: `DESIGN.md` is canonical; `ui/theme/Theme.qml` is its Qt adapter.
- Supported appearance modes: System, Dark, and Light. System resolves from Qt's current platform color scheme without inheriting platform control chrome.
- Supported color presets: Geny (default), Ocean, Violet, and Ember. Presets change brand roles only; semantic status meaning is stable.
- Typography: bundled IRANSansXFaNum for product text and Font Awesome Free Solid for product icons.
- Geometry: shared n=4 superellipse paths with hierarchical curve depth; circles and one-pixel rules are explicit exceptions.
- Settings is a primary destination. Appearance and Language & Region are sections of the same settings center.
- Drift gates: DESIGN.md lint, strict QML lint, warning-fatal smoke, controller tests, and representative dark/light LTR/RTL screenshots.

## Appearance flow

| Operation | Trigger | Immediate result | Persistence / recovery | Focus outcome |
|---|---|---|---|---|
| Open settings | Gear action or labeled navigation item | Settings sheet/route opens without leaving the current read context | Closing returns to the same dataset and scroll position | Settings heading |
| Change mode | System / Dark / Light selector | Semantic tokens update immediately | Device-local C++ presentation setting; invalid stored value falls back to System | Selected mode |
| Change color theme | Labeled preset swatch | Brand roles and preview update immediately | Device-local C++ presentation setting; invalid stored value falls back to Geny | Selected theme |
| Reduce motion | Accessibility toggle | Repeating and translation/scale motion stops | Device-local C++ presentation setting | Toggle |
| Change locale/region | Language & Region controls | Existing localization behavior applies immediately | Existing localization controller and offline catalogs | Changed control |

Appearance changes never rewrite account, protocol, provider, or cached domain data. QML performs no settings persistence.

## Canonical UI map

| Capability | Canonical owner | Source of truth | Allowed variants | Verification |
|---|---|---|---|---|
| Identity form | `PresentationBridge` + shared QML field composition | This contract | development identity open | presentation + smoke |
| Dataset navigation | Application continuation state + shared Load more control | ADR 0007 / this contract | explicit cursor load-more | application + UI |
| Read status | Application `ReadMetadata` + composition runtime + shared status banner | ADR 0003 / this contract | setup issue / cached / refreshing / live / offline / source/data error | application + UI |
| Lists | C++ presentation list models | This contract | feed / social | presentation + smoke |

## Dataset navigation

- Social feeds use explicit Load more in Phase 4. The current items remain visible while the next page loads.
- Application-owned continuation tokens are opaque and persisted with the local list. No page numbers or raw provider cursors are displayed.
- Following, Trending, and each Channel retain independent cached timelines.
- Empty cached data is distinct from refresh failure. A failed refresh exposes Retry; a successful empty result explains that no posts or people were returned.

## Flow ledger

| Operation | Trigger | Pending | Success | Failure recovery | Focus outcome | Source |
|---|---|---|---|---|---|---|
| Open identity | “Open identity” | Stable busy control, cached profile may appear | Profile/feed context opens | Keep input and cached data; show Retry | Identity heading | Phase 4 handoff |
| Switch feed | “Home” / “Explore” | Cached list remains, status says Refreshing | Fresh canonical list replaces/reconciles | Cached list remains stale | Selected tab/list | Phase 4.5A |
| Open channel | “Open channel” | Existing channel cache renders first | Channel list | Cached channel remains | Channel heading | Phase 4 handoff |
| Open cast/profile | Feed row author/cast action | Cached entity renders first | Fresh canonical entity | Cached entity remains stale | Detail heading | Phase 4 handoff |
| Followers/following | Labeled profile action | Cached people remain | Fresh canonical people list | Cached list remains stale | Social heading | Phase 4 handoff |
| Load more | “Load more” | Button busy; current rows fixed | One deduplicated append | Current rows and token remain; Retry | New content/list | ADR 0007 |
| Back | “Back” | None | Return to previous read context | n/a | Originating row/action | This contract |

## Navigation and responsive behavior

- Compact mode uses one stacked reading surface. Standard/wide mode bounds the reading column and exposes context without stretching feed rows.
- Home is the identity-scoped Following feed; Explore is the explicit Trending discovery surface. Both retain keyboard focus and selected state. Channel navigation is explicit and preserves its channel identifier.
- Cast, profile, and social detail views always provide a labeled Back action.
- The app window title is “Genycaster”; current context appears in the in-app heading without exposing sensitive/provider data to the OS switcher.
- Wide navigation exposes Home, Explore, Profile, and Settings with icons and labels. Compact navigation keeps the three high-frequency read destinations and exposes Settings through the header gear so the reading bar remains uncluttered.

## Async and resilience

- Every read is local-first: background executor reads SQLite, publishes cache, then performs provider work.
- Initial load, background refresh, and pagination have distinct metadata.
- Refresh failure never clears usable cached data. Only OS-reported disconnection is labeled Offline; timeout/transport/provider failures use a source/data-issue message and Retry. A composition/runtime failure before any provider binding exists is labeled Setup issue and does not expose a meaningless Retry action.
- Fresh startup without a restored identity is Ready but idle: it does not automatically request the derived Trending feed. Opening an identity enters Home (Following) after a usable FID is resolved; restored identities do the same on later launches. Explore/Trending remains explicit and cannot gate Home.
- Feed interaction uses bounded first pages (10 items by default) so a slow derived provider read cannot occupy a bounded live-read worker unnecessarily. Debug timeout detail states the last observed transport stage.
- Each presentation request has a cancel handle. Superseded callbacks are ignored; blocking provider interfaces remain unchanged and run only on the I/O executor.
- Reconnect from an OS-reported disconnected state triggers one bounded refresh; the UI claims Live only after provider success and persistence succeeds.
- No unbounded automatic retry is introduced in Phase 4.

## Validation

- Username and channel values are trimmed and must be non-empty before dispatch.
- Cast identifiers must be non-empty.
- Errors are structured in C++ and translated to stable UI messages. Debug builds may expose the diagnostic message behind an explicit Details control; release product copy remains provider-neutral.
- Identity input is preserved after failure and duplicate open/load-more activation is blocked while busy.

## Verification

- Static: architecture guard, localization catalog guard, platform-specific SDK guards where applicable, DESIGN.md lint, strict `qmllint`.
- Behavior: application local-first tests, SQLite restart integration, routing/provider continuation tests, presentation/smoke tests.
- Visual: compact 390×844, standard 760×900, and wide 1280×900; populated, empty, refreshing, and offline/error states where the local renderer permits.
- Toolchain: clean `core-debug`, `provider-hypersnap`, `strict-self-hosted`, `minimal-core`, plus the Qt-enabled shipping UI composition on every desktop OS claimed as release-verified.

## Recoverable source failures

A provider-side feed failure must not strand the user on an inert empty surface.
When another already-supported reading surface can continue after explicit user
choice, the UI offers that path without silently changing feed semantics. Identity-
scoped navigation opens identity selection instead of presenting disabled dead
controls, and a successfully resolved identity may be restored as local UI
preference on the next launch.

Provider response details remain diagnostic-only; release copy stays provider-
neutral and never equates a source HTTP 5xx with device offline state.

## Runtime composition availability

The graphical application must not silently enter a providerless live-read runtime. Fresh UI configurations compile the currently implemented live-read adapter; explicitly disabling all runnable UI providers is a configure-time error. If runtime preflight still fails because deployment policy excludes all registered providers, the UI exposes a Setup issue with diagnostic context rather than a temporary Source issue. Core-only provider-free presets remain valid and are not subject to this graphical-runtime requirement.

## Phase 5D write interaction addendum

The Phase 4 read contract remains the baseline for guest/public/cached behavior.
Phase 5D adds signer-gated root/reply authoring without changing read failure
semantics.

- Guests can read feed/detail/cached cast surfaces and inspect engagement without
  authentication.
- Root Publish and Reply require an authenticated account plus a current
  network-verified signer with `CAST_ADD` scope. Sign In alone is insufficient.
- Feed/detail Reply opens the same composer used for root Publish and shows
  parent display context without exposing protocol construction to QML.
- The C++ presentation bridge resolves the parent author FID from canonical
  presentation state; QML never passes a protocol FID through JavaScript number
  conversion.
- Root/reply text receives a 320 UTF-8 byte presentation preflight and canonical
  Protocol validation again before signing.
- Pending/retry/blocked/failed writes remain durable and understandable; they do
  not clear the authoritative parent cast or disable guest reads.
- Confirmed optimistic casts/replies deduplicate by canonical CastId when the
  authoritative read pipeline observes the same protocol entity.
- Sign-out/account switch clears prior-account optimistic presentation and any
  retained reply target. Late callbacks for another account are ignored.


## Phase 5G localization and bidirectional-text addendum

- Product-owned UI copy must come from stable ID-based translation catalogs; source-text translation calls and hard-coded visible Latin QML copy fail the localization guard.
- Persian is a first-class RTL UI. Global layout mirroring follows UI language, but user-generated casts, bios, usernames, URLs and composer input are never translated and determine their own text direction.
- Language and regional formatting are independent device-local preferences. Switching language must not rewrite protocol/account data or remote settings.
- Canonical timestamps stay epoch/UTC. Calendar, local digits, grouping and time-zone conversion happen only for display.
- Shahanshahi display keeps Jalali month/day/leap semantics and uses the historical +1180 year-era offset.
- Currency formatting accepts an already-valued fiat amount. Token/FX conversion is not a localization responsibility and requires a separate timestamped quote capability.
- Localization must remain fully usable offline because catalogs and calendar/locale behavior are local resources.
- Visual acceptance covers compact, standard and wide layouts in both LTR and RTL, including long labels and mixed Persian/English content.
