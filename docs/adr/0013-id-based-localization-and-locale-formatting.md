# ADR 0013 — ID-Based Localization and Independent Regional Formatting

## Status

Accepted for Phase 5G.

## Context

Genycaster's first UI checkpoint was English-first and most QML copy used
source-string translation calls. Expanding into search, discovery, Mini Apps,
messaging and other product surfaces before establishing a localization
contract would multiply hard-coded language and direction assumptions.

Language is also not the same concept as locale. A user may want Persian UI
copy while keeping a non-Iranian regional number/date convention, or English UI
copy with Iranian/Jalali date presentation. Token/FX valuation is another
independent concern and must not become implicit localization behavior.

## Decision

- Product-owned UI copy uses stable ID-based `qsTrId`/`qtTrId` catalogs.
- English is an explicit catalog and Persian is the first additional complete
  catalog.
- Runtime language changes install a `QTranslator` and call
  `QQmlEngine::retranslate()`.
- UI language controls global layout direction.
- Regional locale independently controls number/date/currency formatting.
- Calendar, timestamp display policy and time-zone are independent device-local preferences.
- Shahanshahi formatting uses Jalali calendar parts plus the historical +1180
  era offset (for example 1405 SH = 2585 Shahanshahi).
- Canonical timestamps and monetary values remain unformatted outside the
  presentation boundary.
- The existing currency preference remains the primary display unit; optional
  secondary display units can coexist without globally replacing it.
- IRR is canonical fiat identity; IRT/toman is an exact presentation denomination.
- Currency formatting does not perform FX or token-price conversion.
- Market valuation uses a provider-neutral quote contract with decimal text,
  timestamp and provenance; QML/localization never owns market fetching.
- User-generated text is never translated and gets content-sensitive direction.
- Localization preferences persist in QSettings, not the canonical SQLite
  domain store.
- No OS-specific localization implementation is introduced unless a future
  platform genuinely requires an adapter.

## Consequences

- Translation coverage can fail CI before untranslated UI proliferates.
- Persian can be a first-class RTL interface without forking screens.
- Regional formatting remains useful even for system languages that do not yet
  have a Genycaster UI catalog.
- New languages add catalogs rather than conditional QML branches.
- New calendars can extend the presentation formatter without changing protocol
  or provider models.
- Market-data/FX work remains independently testable derived infrastructure
  with source provenance and staleness semantics.
- Windows/Linux/macOS/mobile all use the same localization contract; platform
  acceptance is evidence, not architecture.
