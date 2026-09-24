# Phase 5G — Internationalization, Localization, and Locale UX

Date: 2026-08-29

## Purpose

Phase 5G is a shipping gate between Phase 5F and Phase 6. Genycaster must not
encode English-only presentation assumptions into the UI before derived search,
discovery, ranking, Mini Apps, messaging, or other surfaces multiply the amount
of product copy.

The phase is intentionally cross-platform. macOS is one acceptance environment,
not the localization architecture. The implementation uses Qt's portable
translation, locale, calendar, time-zone, resource, and settings APIs. Any
future operating-system-specific behavior remains behind the existing platform
adapter boundary.

## Feature gate

```text
Responsibility:
  Translate Genycaster-owned UI copy; select UI direction; format numbers,
  timestamps, calendars, time zones and already-valued fiat amounts.

Capability origin:
  Local.

Mandatory or optional:
  Mandatory for shipping graphical compositions. Optional for provider-free
  headless/core builds.

Public contract:
  LocalizationController is the QML-facing Qt presentation adapter. Core,
  domain, application and provider contracts contain no QLocale/QCalendar/
  QTranslator types.

Canonical data/invariants:
  - persisted timestamps stay canonical UTC/epoch values;
  - canonical monetary values always retain an explicit currency/asset identity;
  - formatting never performs exchange-rate or token-price conversion;
  - user-generated content is never translated or rewritten;
  - UI direction follows UI language;
  - user-generated text direction follows its own content;
  - language and regional-format selection are independent preferences.

Dependencies:
  Qt Core/Gui/Qml/LinguistTools at the presentation composition boundary;
  device-local QSettings persistence only.

Expected failures:
  Missing language pack is a packaging/test failure with English fallback;
  invalid locale/calendar/time-zone/currency input is rejected without changing
  the previous valid preference.

Local-state representation:
  QSettings keys under localization/*; no SQLite schema and no provider state.

Offline behavior:
  Complete localization continues offline. Translation catalogs are embedded.

Provider policy and disablement:
  None. Localization does not select or call a Farcaster/provider backend.

Failover semantics:
  Unsupported system UI languages use the English UI catalog while regional
  formatting can continue to follow the system locale. Calendar/time-zone/
  currency have explicit system/automatic policies.

Tests proving the boundary:
  ID/catalog coverage guard; placeholder integrity; English/Persian catalog
  parity; Persian RTL Qt smoke; strict qmllint; cross-platform GUI acceptance;
  calendar/date/number/currency presentation checks.
```

## Translation strategy

Genycaster uses Qt ID-based translations exclusively for product-owned copy:

```qml
text: qsTrId("genycaster.profile.follow")
```

```cpp
m_statusText = qtTrId("genycaster.bridge.ready_to_read");
```

Source-string `qsTr()`/`qsTranslate()` calls are rejected by the localization
catalog guard. English is an explicit language catalog instead of relying on
source-string fallback because ID-based lookups must remain readable even when
the UI language is English.

Initial shipping catalogs are:

- `en_US` — complete English baseline;
- `fa_IR` — complete Persian translation.

Additional languages are added only when their catalog is complete enough to
pass the same coverage/placeholder gate. The architecture is not limited to two
languages.

## Language versus regional format

These are deliberately separate:

- **UI language** selects translated product copy and global LTR/RTL direction.
- **Regional format** selects numbering, grouping, date/time patterns, default
  calendar policy, and automatic local currency identity.

This permits useful combinations such as an English UI with Iranian regional
formatting or a Persian UI with a different regional number/date convention.
A system language that Genycaster has not translated yet falls back to English
without discarding the user's system regional formatting.

## RTL and mixed-direction content

Global application mirroring follows the selected UI language. Persian therefore
mirrors navigation, layouts, controls, and presentation chrome.

User-generated content is different. Casts, bios, usernames, channel IDs, URLs,
and composer input can contain mixed Persian/Arabic and Latin text. Genycaster
must not force these strings to the application direction. `RichText`,
`DirectionAwareTextField`, and `DirectionAwareTextArea` inspect the first strong
script signal and align content independently from the mirrored application
chrome.

Explicit pixel/X placement is exceptional and must be audited because Qt layout
mirroring does not make arbitrary `x:` arithmetic logical. The profile avatar
overlap has an explicit RTL branch for this reason.

## Date, calendar, and time-zone model

Canonical timestamps remain epoch milliseconds/UTC in application data. Display
conversion happens only at the Qt presentation boundary.

Supported calendar policies:

- automatic;
- Gregorian;
- Solar Hijri / Jalali;
- Shahanshahi / Imperial Persian;
- Islamic Civil;
- Julian;
- Revised Julian / Milanković.

Qt owns the built-in Gregorian/Jalali/IslamicCivil/Julian/Milanković calendar
math. Shahanshahi reuses Jalali month/day/leap rules and applies the historical
`+1180` year-era offset. Both examples are therefore valid: Solar Hijri 1355 is
Shahanshahi 2535, and Solar Hijri 1405 is Shahanshahi 2585. The current-year
example is explicitly covered by acceptance so the era conversion cannot regress.
This keeps the calendar implementation tied to the established Persian solar
structure rather than inventing a second leap-year algorithm.

Time zones use IANA IDs through `QTimeZone`. This remains portable on Windows,
where Qt maps platform time-zone data to IANA semantics, as well as macOS,
Linux, iOS and Android compositions.

## Temporal presentation policy

Selecting a calendar changes the presentation of canonical timestamps everywhere
a date is rendered; it never rewrites stored cast/post timestamps. Social surfaces
use `formatTimestamp()` instead of ad-hoc JavaScript dates. The user can select:

- **Smart** (default): recent activity is localized relative time; older activity
  switches to the selected calendar/time-zone;
- **Relative**: always show localized elapsed intervals;
- **Calendar**: always show the selected calendar/time-zone, including recent casts.

This means a cast timestamp can render as Gregorian, Jalali 1405, Shahanshahi
2585, or another configured calendar without changing the canonical UTC value.

## Numbers, money and multi-currency presentation

`QLocale` owns decimal separators, grouping and local digits. Compact social
counts use translated unit IDs rather than hard-coded Latin `K/M/B` strings.

The existing `currencyCode` remains the **primary display unit** for compatibility.
Phase 5G adds optional secondary display units (`additionalCurrencyCodes`). The
selection list is built from the ISO currency identities exposed by Qt at runtime,
so it is not restricted to a hard-coded handful of countries. Nothing is added
to a price surface unless the user explicitly selects it and a trusted valuation
exists. For example a user can choose:

```text
Primary: USD
Also show: IRT, EUR
```

A valuation surface may then render independently supplied quotes such as:

```text
1 GENY = $0.00060
1 GENY = 52 toman
1 GENY = €0.00055
```

The numbers above are illustrative only; localization never invents those rates.
`LocalizedPrice.qml` consumes already-valued quotes and filters them by the user's
selected display units.

IRR remains the canonical ISO-4217 Iranian rial identity. `IRT` is modeled only as
a presentation denomination for toman. The exact denomination conversion
`10 IRR = 1 IRT` may be performed locally; it is not an FX rate and does not
change ledger/market identity.

Token-to-fiat and FX valuation remains a separate provider-neutral contract:

```text
asset/base identity
    + quote currency
    + decimal value
    + quote timestamp
    + source/provenance
    + staleness policy
        -> selected primary/secondary display units
        -> localized formatting
```

A future quote/FX service owns market-rate acquisition, triangulation, provenance
and freshness. QML must not fetch rates, and localization must not become a hidden
market-data provider. This allows USD + local currency + any user-selected foreign
currency to coexist without changing the user's primary preference globally.

## Platform boundary

No platform macro is required for Phase 5G itself. Translation catalogs,
`QLocale`, `QCalendar`, `QTimeZone`, `QSettings`, layout direction and QML
mirroring are Qt cross-platform facilities.

Platform-specific code is appropriate only when an OS supplies genuinely unique
behavior. Examples include the existing secure vault, browser/wallet handoff,
packaging, native accessibility integration, or later native settings migration.
Such code belongs behind `src/platform/*` adapters and cannot leak OS checks into
QML/domain/application logic.

## Acceptance matrix before Phase 6

Phase 5G source completion does not make one desktop run a cross-platform
release claim. Before Phase 6 is treated as the next product milestone:

1. architecture and localization catalog guards pass;
2. both English and Persian catalogs compile as ID-based `.qm` resources;
3. strict `qmllint` has zero project warnings;
4. Persian RTL smoke starts with Jalali/Iran/IRR settings and no Qt warnings;
5. LTR and RTL visual review covers compact, standard and wide layouts;
6. mixed English/Persian casts, bios and composer text align correctly;
7. Gregorian, Jalali and Shahanshahi output is checked around Nowruz/year
   boundaries and time-zone changes, including 1405 SH = 2585 Shahanshahi;
8. Smart/Relative/Calendar timestamp policy is checked on cast/detail surfaces;
9. local digits/grouping/currency formatting are checked for representative
   locales, including primary + optional secondary display units and IRR/IRT;
10. the Qt-enabled shipping composition is built/tested on each supported desktop
   OS (macOS, Windows and Linux) before that OS is called release-verified;
11. mobile platform acceptance is added when iOS/Android compositions become
    shipping targets; the shared localization architecture is already portable.

Phase 5F live write acceptance remains a parallel prerequisite. Phase 6 starts
only after the Phase 5F protocol/write gate and the Phase 5G localization gate
are closed to the level required for the intended release platforms.
