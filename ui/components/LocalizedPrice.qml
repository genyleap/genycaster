import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    /** Asset symbol shown on every quote line, for example GENY or ETH. */
    property string assetSymbol
    /**
     * Already-valued quotes supplied by a valuation service.
     * Each item: { unit: "USD", valueText: "0.00060", precision: 5, asOfMs: 0, source: "..." }.
     * This component never fetches prices or performs FX conversion.
     */
    property var quotes: []
    property int defaultPrecision: 2
    property color primaryColor: Theme.ink
    property color secondaryColor: Theme.inkMuted
    spacing: 2

    function directQuote(unit) {
        const target = String(unit).toUpperCase()
        for (let index = 0; index < quotes.length; ++index) {
            if (String(quotes[index].unit || "").toUpperCase() === target)
                return quotes[index]
        }
        return null
    }

    function numericValue(quote) {
        if (!quote)
            return NaN
        return Number(quote.valueText !== undefined ? quote.valueText : quote.value)
    }

    function precisionFor(quote) {
        if (!quote)
            return root.defaultPrecision
        if (quote.precision !== undefined)
            return Number(quote.precision)
        if (quote.valueText !== undefined) {
            const text = String(quote.valueText)
            const decimalPoint = text.indexOf(".")
            return decimalPoint >= 0 ? Math.min(12, text.length - decimalPoint - 1) : 0
        }
        return root.defaultPrecision
    }

    function quoteFor(unit) {
        const target = String(unit).toUpperCase()
        const direct = directQuote(target)
        if (direct)
            return direct

        // Toman is an exact display denomination of IRR, not an FX conversion.
        const sourceUnit = target === "IRT" ? "IRR" : (target === "IRR" ? "IRT" : "")
        if (sourceUnit.length === 0)
            return null
        const source = directQuote(sourceUnit)
        if (!source)
            return null
        const converted = localizationController.convertDisplayDenomination(
            root.numericValue(source), sourceUnit, target)
        if (!Number.isFinite(converted))
            return null
        return {
            unit: target,
            value: converted,
            precision: source.precision !== undefined ? source.precision : root.precisionFor(source),
            asOfMs: source.asOfMs,
            source: source.source
        }
    }

    Repeater {
        model: localizationController.effectiveDisplayCurrencyCodes

        delegate: Label {
            required property string modelData
            readonly property var quote: root.quoteFor(modelData)
            readonly property int quotePrecision: root.precisionFor(quote)
            visible: quote !== null
            Layout.fillWidth: true
            text: quote
                  ? LocaleFormat.formatAssetQuote(
                        root.assetSymbol, root.numericValue(quote), modelData, quotePrecision)
                  : ""
            color: index === 0 ? root.primaryColor : root.secondaryColor
            font.weight: index === 0 ? Font.DemiBold : Font.Normal
            wrapMode: Text.Wrap
        }
    }
}
