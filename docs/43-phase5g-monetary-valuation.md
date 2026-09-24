# Phase 5G.2 — Monetary Display and Valuation Contract

Date: 2026-08-29

## Purpose

Genycaster must be able to show an asset price in the user's preferred primary
currency and, independently, in zero or more optional secondary currencies. The
feature must remain provider-neutral, cross-platform, timestamped and explicit
about provenance. Localization formats values; it does not acquire or invent
market rates.

## Canonical versus presentation state

Canonical quote data owns:

```text
asset identity
quote ISO-4217 currency
canonical decimal text
observed-at timestamp
source/provenance
```

`farcaster.domain.valuation` preserves quote decimal values as text instead of
binary floating point. `farcaster.capability.valuation::IValuationSource` is the
blocking provider-neutral I/O port for asset-price and FX reads.

Presentation preferences own:

```text
primary display unit
0..N optional secondary display units
regional number formatting
currency/denomination formatting
```

Changing display preferences never mutates the canonical quote.

## Example

If the selected preferences are:

```text
Primary: USD
Also show: IRT, EUR
```

and the valuation layer supplies timestamped USD/IRR/EUR values, a surface may
render:

```text
1 GENY = $0.00060
1 GENY = 52 toman
1 GENY = €0.00055
```

Those values are illustrative. `LocalizedPrice.qml` displays only quote units
that actually exist. Missing secondary quotes disappear instead of being guessed.

## IRR and IRT/toman

IRR is the canonical ISO-4217 Iranian rial identity. IRT is a Genycaster display
denomination representing toman. It is not sent to market-data providers as an
ISO currency.

The exact local denomination relationship is:

```text
10 IRR = 1 IRT
```

This exact scaling is allowed in `LocalizationController` because it is not an
FX rate. All other cross-currency conversion requires timestamped valuation data.

## Currency discovery

The Qt presentation adapter discovers ISO currency identities from Qt's runtime
locale database rather than maintaining a short hard-coded country list. IRT is
added explicitly as a non-ISO display denomination.

## Staleness and provenance requirement

A live valuation adapter must define a staleness policy before a quote is shown.
Every converted value must remain traceable to a timestamp and source. QML must
never call a price API directly, and stale/failed quote acquisition must not make
core social reading unavailable.

## Cross-platform boundary

No macOS/Windows/Linux-specific market formatting exists. Domain/capability
contracts are standard C++ modules; localization uses portable Qt APIs. A future
market adapter may have deployment-specific configuration but not OS-specific
semantic behavior.

## Acceptance before using prices in product surfaces

1. quote decimal text, timestamp and provenance survive the provider boundary;
2. primary-only behavior is unchanged when no secondary units are selected;
3. arbitrary Qt-discovered ISO currencies can be selected;
4. missing secondary quotes are omitted, never fabricated;
5. IRR/IRT exact denomination conversion is verified;
6. locale digits/grouping/currency symbols are verified in LTR and RTL;
7. quote freshness and source are available to the presentation model;
8. network failure in valuation does not disable feed/profile/cast reading.
