# Phase 5G Verification

Date: 2026-08-29

## Source/static evidence

Required commands:

```text
python3 tools/check_architecture.py .
python3 tools/check_localization_catalog.py .
git diff --check
```

The localization guard proves:

- production QML/presentation code uses ID-based translation calls;
- every referenced ID exists in both English and Persian catalogs;
- catalog source strings match;
- `%1..%9/%n` placeholders are preserved;
- visible Latin QML literals do not bypass the catalog.

## Qt-enabled evidence required

On each shipping desktop platform, collect:

```text
cmake --build <ui-build> --target farcaster_client farcaster_client_qmllint_strict
ctest --test-dir <ui-build> -R 'ui\.(smoke|localization_en_smoke|localization_fa_smoke|runtime_smoke)' --output-on-failure
```

`ui.localization_fa_smoke` starts with:

```text
--language=fa
--regional-locale=fa_IR
--calendar=jalali
--timestamp-display=calendar
--time-zone=Asia/Tehran
--currency=USD
--additional-currencies=IRT,EUR
```

The test is warning-fatal and therefore catches missing translation resources,
QML type/property failures, and runtime warnings during the smoke path.

## Visual/manual acceptance

For compact, standard and wide layouts in both English/LTR and Persian/RTL:

- navigation order and alignment mirror logically;
- profile avatar/banner overlays remain correct;
- popups remain inside the window;
- long Persian labels are not clipped;
- mixed English/Persian casts and bios use content direction;
- identity/channel/composer inputs switch alignment from entered content;
- URLs, usernames and protocol IDs remain readable;
- Gregorian/Jalali/Shahanshahi date output is checked at Nowruz/year boundaries, including 1405 SH = 2585 Shahanshahi;
- Smart/Relative/Calendar modes are checked on cast and detail timestamps;
- time-zone changes cross midnight correctly;
- local digits, grouping and currency formatting are checked for representative
  regions;
- primary and optional secondary valuation displays are checked without implicit FX;
- IRR/IRT denomination conversion is checked as 10 IRR = 1 IRT;
- valuation quotes preserve decimal text, timestamp and provenance in core.

## Current environment qualification

A source/static pass is not a Windows/Linux/macOS runtime claim. If a Qt SDK or
shipping toolchain is unavailable in the current environment, that platform's
GUI/lint/runtime state must be reported as **NOT VERIFIED**, not inferred from a
different operating system.

## Runtime regression criteria added after Qt 6.11.1 evidence

The English and Persian smoke tests now both run with `QT_FATAL_WARNINGS=1`. Acceptance additionally requires:

- zero `QString::arg: Argument missing` diagnostics;
- zero raw `genycaster.*` translation IDs rendered as fallback text;
- zero `Cannot read property ... of null` / `Cannot call method ... of null` diagnostics during orderly process shutdown;
- failure to load or validate an embedded ID-based catalog aborts composition with one explicit localization error instead of continuing with a partially translated UI.

The catalog files are explicitly emitted to the UI build directory and embedded under `:/i18n`, avoiding host/toolchain-dependent implicit resource aliases.
## Runtime catalog packaging correction (2026-08-29)

A Qt 6.11.1 macOS run proved that the embedded `qml_en_US.qm` resource was
present but was not keyed for `qtTrId`/`qsTrId` lookup. The release path no
longer relies on the `qt_add_translations()` convenience wrapper. Reviewed TS
files are now compiled by an explicit generated-file rule using the imported
`Qt6::lrelease` executable with `-idbased -fail-on-invalid`, then embedded at
`:/i18n/qml_<locale>.qm`. The rule is host-path independent and therefore the
same on macOS, Windows, and Linux.

The runtime probe remains an acceptance invariant: a catalog that loads as a
resource but cannot resolve `genycaster.app.genycaster` is rejected before QML
construction.
