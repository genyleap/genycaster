pragma Singleton
import QtQuick

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
