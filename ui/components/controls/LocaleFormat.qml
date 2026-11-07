pragma Singleton
import QtQuick
import Farcaster.Client

/**
 * Reactive localization facade for value formatting used by QML bindings.
 *
 * Q_INVOKABLE method calls do not expose their internal locale dependency to
 * QML's binding engine. Every formatter therefore observes the controller's
 * revision explicitly so a runtime locale/language change invalidates values
 * even when the underlying model has not changed.
 */
QtObject {
    id: root

    readonly property double revision: localizationController.revision

    function observeRevision() {
        return root.revision
    }

    // Keep the provider's canonical category in model/filter keys. Translate
    // only UI presentation for known public catalogue taxonomy values.
    // Arbitrary third-party categories remain exactly as supplied by the app.
    function formatMiniAppCategory(value) {
        root.observeRevision()
        const raw = String(value || "").trim()
        const key = raw.toLowerCase().replace(/[_&-]+/g, " ").replace(/\s+/g, " ").trim()
        switch (key) {
        case "social": case "social networking": return qsTrId("genycaster.apps.category.social")
        case "games": case "gaming": case "game": return qsTrId("genycaster.apps.category.games")
        case "finance": case "financial": return qsTrId("genycaster.apps.category.finance")
        case "defi": case "decentralized finance": return qsTrId("genycaster.apps.category.defi")
        case "crypto": case "cryptocurrency": return qsTrId("genycaster.apps.category.crypto")
        case "trading": case "trade": return qsTrId("genycaster.apps.category.trading")
        case "education": case "learning": return qsTrId("genycaster.apps.category.education")
        case "entertainment": return qsTrId("genycaster.apps.category.entertainment")
        case "productivity": return qsTrId("genycaster.apps.category.productivity")
        case "utilities": case "utility": return qsTrId("genycaster.apps.category.utilities")
        case "tools": case "tool": return qsTrId("genycaster.apps.category.tools")
        case "music": return qsTrId("genycaster.apps.category.music")
        case "art": case "art & collectibles": return qsTrId("genycaster.apps.category.art")
        case "nft": case "nfts": case "collectibles": return qsTrId("genycaster.apps.category.collectibles")
        case "sports": return qsTrId("genycaster.apps.category.sports")
        case "news": return qsTrId("genycaster.apps.category.news")
        case "shopping": return qsTrId("genycaster.apps.category.shopping")
        case "health": case "health & fitness": case "fitness": return qsTrId("genycaster.apps.category.health")
        case "community": case "communities": return qsTrId("genycaster.apps.category.community")
        case "development": case "developer": case "developers": return qsTrId("genycaster.apps.category.development")
        case "art creativity": case "arts creativity": return qsTrId("genycaster.apps.category.art_creativity")
        case "developer tools": case "development tools": return qsTrId("genycaster.apps.category.developer_tools")
        case "food drink": case "food drinks": return qsTrId("genycaster.apps.category.food_drink")
        case "lifestyle": return qsTrId("genycaster.apps.category.lifestyle")
        case "photography": return qsTrId("genycaster.apps.category.photography")
        case "travel": return qsTrId("genycaster.apps.category.travel")
        case "onchain": case "on chain": return qsTrId("genycaster.apps.category.onchain")
        case "ai": case "artificial intelligence": return qsTrId("genycaster.apps.category.ai")
        case "memes": case "meme": return qsTrId("genycaster.apps.category.memes")
        case "video": case "videos": return qsTrId("genycaster.apps.category.video")
        case "business": return qsTrId("genycaster.apps.category.business")
        case "other": case "others": return qsTrId("genycaster.apps.category.other")
        default: return raw
        }
    }

    function formatIdentifier(value) {
        root.observeRevision()
        return localizationController.formatIdentifier(value)
    }

    function formatNumber(value, precision) {
        root.observeRevision()
        return precision === undefined
                ? localizationController.formatNumber(value)
                : localizationController.formatNumber(value, precision)
    }

    function formatInteger(value) {
        root.observeRevision()
        return localizationController.formatInteger(value)
    }

    // Presentation-only conversion: IDs, hashes, usernames and provider keys
    // retain ASCII digits. Dates such as ISO YYYY-MM-DD are displayed with
    // native digits without changing their canonical data representation.
    function localizeDigits(value) {
        root.observeRevision()
        const text=String(value === undefined || value === null ? "" : value)
        const language=String(localizationController.effectiveLanguageTag || "").toLowerCase()
        const alphabet=language.startsWith("fa") ? "۰۱۲۳۴۵۶۷۸۹"
                      : language.startsWith("ar") ? "٠١٢٣٤٥٦٧٨٩" : ""
        return alphabet ? text.replace(/[0-9]/g,digit=>alphabet[Number(digit)]) : text
    }

    function formatCompactNumber(value) {
        root.observeRevision()
        return localizationController.formatCompactNumber(value)
    }

    function calendarMonthModel(monthOffset) {
        root.observeRevision()
        return monthOffset === undefined
                ? localizationController.calendarMonthModel()
                : localizationController.calendarMonthModel(monthOffset)
    }

    function formatTimestamp(epochMilliseconds, style) {
        root.observeRevision()
        return style === undefined
                ? localizationController.formatTimestamp(epochMilliseconds)
                : localizationController.formatTimestamp(epochMilliseconds, style)
    }

    function formatRelativeTime(epochMilliseconds) {
        root.observeRevision()
        return localizationController.formatRelativeTime(epochMilliseconds)
    }

    function formatDateTime(epochMilliseconds, style) {
        root.observeRevision()
        return style === undefined
                ? localizationController.formatDateTime(epochMilliseconds)
                : localizationController.formatDateTime(epochMilliseconds, style)
    }

    function formatCurrency(value, isoCurrency, precision) {
        root.observeRevision()
        if (precision !== undefined)
            return localizationController.formatCurrency(value, isoCurrency || "", precision)
        if (isoCurrency !== undefined)
            return localizationController.formatCurrency(value, isoCurrency)
        return localizationController.formatCurrency(value)
    }

    function formatMonetaryUnit(value, displayUnit, precision) {
        root.observeRevision()
        return precision === undefined
                ? localizationController.formatMonetaryUnit(value, displayUnit)
                : localizationController.formatMonetaryUnit(value, displayUnit, precision)
    }

    function formatAssetQuote(assetSymbol, value, displayUnit, precision) {
        root.observeRevision()
        return precision === undefined
                ? localizationController.formatAssetQuote(assetSymbol, value, displayUnit)
                : localizationController.formatAssetQuote(assetSymbol, value, displayUnit, precision)
    }
}
