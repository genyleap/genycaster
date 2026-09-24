# Phase 5G Review — Internationalization and Locale UX

Date: 2026-08-29

## Scope delivered in source

Phase 5G establishes a cross-platform localization boundary before Phase 6:

- 406 product/presentation messages migrated to ID-based Qt translation IDs;
- complete embedded English and Persian catalogs;
- runtime language switching with QML retranslation;
- Persian RTL application mirroring;
- content-sensitive direction for casts and text inputs;
- independent regional-format selection;
- locale-aware number and compact-count formatting;
- Gregorian, Jalali, Shahanshahi, Islamic Civil, Julian and Revised Julian
  display policies;
- IANA time-zone selection;
- Smart/Relative/Calendar timestamp policy used by cast/detail timestamps;
- primary plus optional secondary monetary display units, including IRR/IRT;
- `LocalizedPrice.qml` for displaying only quotes actually supplied by a valuation source;
- provider-neutral asset-price/FX quote domain and capability contracts with decimal text, timestamp and provenance;
- QSettings persistence for device-local preferences;
- localization catalog/placeholder guard and Persian RTL smoke target.

## Architecture review

The new behavior lives under the Qt presentation boundary. Domain, application,
routing, storage and providers gain no Qt locale or OS dependency. QML still has
no network/database access. Translation resources are compiled into the GUI
composition and work offline.

No macOS-only implementation was introduced. `QLocale`, `QCalendar`,
`QTimeZone`, `QTranslator`, `QSettings`, `QGuiApplication` layout direction and
Qt Quick mirroring are shared Qt facilities. Platform-specific release evidence
remains a separate matrix.

## Currency/valuation boundary

Phase 5G formats an amount only after the caller knows its currency. It does not
invent token/FX rates. The core now defines provider-neutral `AssetPriceQuote`,
`FxQuote` and `IValuationSource` contracts; live adapters still own rate acquisition,
source provenance, stale policy and provider/failover semantics outside localization.
IRR/IRT is the only local conversion because toman is an exact display denomination
(10 IRR = 1 IRT), not an FX market rate.

## Remaining acceptance

Source delivery still needs Qt-enabled build/lint/runtime verification on the
actual shipping toolchains, LTR/RTL visual review, and date-boundary checks.
Phase 5F live write acceptance also remains open. Phase 6 must not be declared
started until these pre-Phase-6 gates are addressed for the target release
platforms.

## Runtime hardening follow-up — 2026-08-29

A Qt 6.11.1 desktop run exposed two composition defects in the initial Phase 5G delivery:

- generated `.qm` catalogs were not reliably available through the resource path consumed by `LocalizationController`, causing `qtTrId`/`qsTrId` to return raw IDs and subsequent `QString::arg` placeholder warnings;
- `PresentationBridge` and `LocalizationController` were destroyed before `QQmlApplicationEngine`, so QML teardown reevaluated bindings against null context properties and emitted a large TypeError cascade.

The follow-up makes translation resource embedding explicit at `:/i18n/qml_<locale>.qm`, validates an ID-based translation probe before the UI is allowed to start, rolls language changes back if a replacement catalog cannot be installed, and changes composition-root ownership so both context objects outlive the QML engine. No QML null-guard workaround was added because the defect was ownership, not optional state.
