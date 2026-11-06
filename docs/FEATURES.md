# GenyCaster — Feature Guide

This guide describes the product as it exists in the current source, in terms useful to both users and contributors. The [Development Roadmap](12-roadmap.md) tracks completion and acceptance separately.

## What the labels mean

| Label | Meaning |
|---|---|
| **Available in source** | The feature has an implementation in the repository. The actual app experience can depend on platform, authenticated account, network capability or service availability. |
| **In development** | A usable part exists, but key flows, portability, verification or usability work is ongoing. |
| **Planned** | The feature has defined requirements or an engineering plan, but is not an available product capability. |
| **Concept** | Design direction only. No promise of implementation or release date. |

## Everyday social experience

| Feature | Source status | What it means |
|---|---|---|
| Feeds and conversations | Available in source | Read casts, open a conversation, reply and return to the originating feed. |
| Social interactions | Available in source | Cast, quote, like, recast, follow and manage applicable content using an authorized protocol signer. |
| Discovery | Available in source | Explore feeds, topics, channels and indexed or locally derived content depending on available node capabilities. |
| Profiles and identity | Available in source | Read Farcaster profiles, follow relationships, and optional GenyCaster-specific profile information. |
| Rich media | Available in source | Image/video viewing, galleries, link previews and media upload where the required adapter and gateway are available. |
| Bookmarks, lists and preferences | Available in source | User-specific organization and settings, including app-owned data synchronized by the authenticated service when available. |
| Notifications and user status | Available in source | Notification surfaces and account profile status, subject to runtime/service availability. |
| **Internationalization & localization (i18n/l10n)** | **Available in source** | Twelve selectable UI languages, RTL/LTR layout, and independent regional, calendar, time zone, timestamp and monetary display preferences. |
| Appearance | Available in source | Light/dark styling and adaptive layouts. |

## Internationalization & localization (i18n/l10n)

GenyCaster treats **interface language** and **regional conventions** as separate,
user-controlled preferences. Someone can read the interface in one language while
displaying dates, times, numbers and monetary values according to another
country's conventions. These settings are part of the Qt presentation layer,
not assumptions imposed by the selected Farcaster node.

| Preference | Source status | Current controls |
|---|---|---|
| **Interface language (i18n)** | Available in source | Twelve embedded language catalogs: English, Persian, German, Japanese, Korean, Chinese, Hindi, Arabic, Spanish, French, Portuguese and Russian; system-language option and live retranslation. |
| **Text direction** | Available in source | RTL/LTR layout follows the chosen language, with direction-aware text presentation for mixed-script content. |
| **Regional locale (l10n)** | Available in source | System defaults or one of thirteen suggested country/region locales, independently from UI language; governs regional number/date/currency conventions. |
| **Calendar** | Available in source | Automatic regional selection, Gregorian, Jalali (Solar Hijri), Shahanshahi, Islamic Civil, Julian and Milanković calendar options. |
| **Time and dates** | Available in source | Smart, relative and calendar-first timestamp modes; system or an available IANA time zone, with locale/calendar-aware date-time formatting. |
| **Numbers and monetary display** | Available in source | Localized digits, grouping and compact counts; automatic or selected currency display, optional additional display units and support for the toman (IRT) denomination. |

Preferences are applied and persisted **on the device**. Choosing a display
currency does **not** convert balances, fetch foreign-exchange rates or change
the underlying denomination. Likewise, localized dates do not modify canonical
timestamps, and localized numerals do not rewrite Farcaster IDs or hashes.

Implementation references: `src/presentation/qt/localization_controller.hpp`,
`src/presentation/qt/localization_controller.cpp`,
`ui/components/LocaleSettingsPopup.qml` and
`ui/components/LocaleFormat.qml`. Translation resources live in `ui/i18n/`.
Some platform-specific integrations can have separate localization coverage. The twelve catalogs are present, but translation coverage is not yet complete for every UI string; ongoing localization checks currently identify missing and untranslated entries.

## Apps and widgets

| Feature | Source status | What it means |
|---|---|---|
| **Farcaster Mini Apps** | In development | Discover, save and run supported embedded Mini Apps, subject to target-specific host capabilities. |
| **Mini App wallet access** | In development | EIP-1193/EIP-6963 and WalletConnect-related integration paths where implemented; permissions must be origin-scoped and signing requires explicit authorization. |
| **Native Apps** | **Concept** | The Apps surface contains an entry point and a **Coming Soon** view; a native application catalog/host is not currently shipped. |
| **Context widgets** | Available in source | Reorderable widgets for **assets, GenyPulse activity, calendar, market information and suggested profiles**. The widget host adapts between desktop rail and mobile drawer. |
| **GenyPulse** | In development | Activity panels using available Farcaster, GitHub and on-chain signals; availability and completeness depend on the connected data source. |

## GENY ecosystem integration

GenyCaster includes product-specific integration with the **GENY token ecosystem on Base**:

- An asset widget can display GENY token holdings associated with verified Farcaster-linked wallet addresses. A token display is informational and is **not** proof of special permissions.
- **Geny Pro** is a separate service-verified entitlement. The service verifies eligible addresses and reads token balances on Base before granting any additional product capabilities. It does not accept a user-submitted balance or client-side UI badge as proof.
- Supported Pro capabilities in source include expanded Mini App concurrency, custom bookmark collections and additional publishing/media or market-interface allowances. Exact limits are returned by the current server policy and can vary; an extended text draft is not a single oversized Farcaster protocol message.
- GENY integration is **an optional product capability**, not the canonical Farcaster protocol, the account identity authority or a requirement for ordinary public reads.
- This documentation makes **no investment, pricing, staking, reward or guaranteed eligibility claim**.

Implementation references: `ui/components/ContextRail.qml`, `ui/components/GenyProDetailsDrawer.qml`, `ui/components/AppsSurface.qml`, `ui/app/live_insights_controller.cpp` and `services/account-sync/pro_entitlements.mjs`.

## Networks and platforms

The [OpenProof identity service](https://github.com/genyleap/openproof)
(`opp`) authenticates GenyCaster accounts and verifies linked Farcaster FIDs.
App-owned bookmarks, profiles, drafts and other private product features are
served by GenyCaster's **separate** authenticated account backend and PostgreSQL
the GenyCaster PostgreSQL database. Neither service replaces the selected Farcaster protocol node;
see [OpenProof & Server Services](OPENPROOF_AND_SERVICES.md).

The default protocol routing preference is **HyperSnap**; selectable Snapchain and compatible read/write routes depend on capability and deployment. Cached content may remain usable during a network outage. Native secure storage and browser wallet providers follow different platform security models.

| Target | Source support |
|---|---|
| macOS, Windows, Linux | Native Qt application and platform-specific build paths. |
| Android | Qt Android application and mobile Mini App integration, subject to device/SDK acceptance. |
| iOS | Source/build configuration; availability of signed distributable artifacts requires separate release evidence. |
| WebAssembly | Browser-hosted Qt app, Service Worker and web-specific Mini App/wallet integration. |

Source support is not equivalent to a released, tested application on every architecture. See [Build/CI](../ci/README.md), [Architecture](ARCHITECTURE.md) and [Roadmap](12-roadmap.md).

## Security and privacy

Identity, custody approval, protocol message signing and embedded-app wallet access are independent scopes. In particular, opening or pairing a Mini App does not itself authorize a signature, and one app's approval must not be silently reused for another app. See the [Security Policy](../SECURITY.md) and [Architecture](ARCHITECTURE.md).

## Useful entry points

- [README](../README.md) — an introduction to the product.
- [Architecture](ARCHITECTURE.md) — how the client, local state, provider routes and signing work.
- [Roadmap](12-roadmap.md) — phases, progress and the evidence needed to complete them.
- [Performance](PERFORMANCE.md) — benchmarks and methodology.
