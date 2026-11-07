import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root

    property string locator: ""
    property string targetUrl: ""
    property bool compact: false
    // The containing virtualized delegate determines secondary read demand.
    property bool enrichmentActive: true
    property var snapshot: ({})
    property bool expanded: false
    property bool tokenLoading: false
    property string tokenError: ""
    property string requestedLocator: ""
    property bool addressCopied: false

    Timer {
        id: copiedAddressTimer
        interval: 1500
        repeat: false
        onTriggered: root.addressCopied = false
    }

    readonly property bool available: Boolean(root.snapshot.available)
    readonly property string network: String(root.snapshot.network || root.locator.split(":")[0] || "")
    readonly property string address: String(root.snapshot.address || root.locator.substring(root.locator.indexOf(":") + 1) || "")
    readonly property real change24h: Number(root.snapshot.change24h || 0)
    readonly property bool changeKnown: root.snapshot.change24h !== null
                                       && root.snapshot.change24h !== undefined
                                       && Number.isFinite(Number(root.snapshot.change24h))
    readonly property bool priceKnown: Number(root.snapshot.priceUsd || 0) > 0
    readonly property string imageSource: String(root.snapshot.imageSource || "")
    readonly property string externalTargetUrl: /^https?:\/\//i.test(root.targetUrl) ? root.targetUrl : ""
    readonly property bool ultraNarrow: root.width < 320
    readonly property bool narrow: root.width < 430
    readonly property bool medium: root.width < 580
    readonly property bool headerStacked: root.width < 390
    readonly property bool priceStacked: root.width < 560
    readonly property int metricColumns: root.ultraNarrow ? 1 : root.narrow ? 2 : 4
    readonly property string networkIcon: {
        const value = root.network.toLowerCase()
        if (value.indexOf("base") >= 0) return "crypto/base"
        if (value.indexOf("sol") >= 0) return "crypto/solana"
        if (value.indexOf("btc") >= 0 || value.indexOf("bitcoin") >= 0) return "crypto/bitcoin"
        return "crypto/ethereum"
    }

    signal openRequested(string url)

    function reload() {
        if (!liveInsights.providerAvailable) {
            if (!root.enrichmentActive)
                return
            if (root.requestedLocator !== root.locator)
                root.requestBrowserToken()
            return
        }
        root.snapshot = liveInsights.tokenPreview(root.locator)
        if (!root.available && root.locator.length > 0)
            liveInsights.requestToken(root.locator)
    }

    function requestBrowserToken() {
        const locator = String(root.locator || "").toLowerCase()
        root.requestedLocator = locator
        root.tokenError = ""
        root.snapshot = ({})
        const found = locator.match(/^(base|eth):(0x[0-9a-f]{40})$/)
        if (!found) {
            root.tokenLoading = false
            root.tokenError = "Token data unavailable"
            return
        }
        root.tokenLoading = true
        if (typeof browserTokenPreviewController === "undefined") {
            root.tokenLoading = false
            root.tokenError = "Market data unavailable"
            return
        }
        const cached = browserTokenPreviewController.tokenFor(locator)
        if (cached && cached.available) {
            root.snapshot = cached
            root.tokenLoading = false
        }
        browserTokenPreviewController.requestToken(locator)
    }

    Connections {
        target: typeof browserTokenPreviewController !== "undefined"
            ? browserTokenPreviewController : null
        function onTokenReady(locator, snapshot) {
            if (root.requestedLocator !== String(locator))
                return
            root.tokenLoading = false
            if (snapshot && snapshot.available) {
                root.snapshot = snapshot
                root.tokenError = ""
            } else {
                // A failed refresh must not erase previously verified token data.
                if (!root.snapshot.available)
                    root.tokenError = "Market data unavailable"
            }
        }
    }

    function money(value) {
        return liveInsights.formatMarketPrice(Number(value || 0))
    }

    function compactMoney(value) {
        const number = Number(value || 0)
        return number > 0 ? "$" + LocaleFormat.formatCompactNumber(number) : "—"
    }

    function ageText() {
        const created = Number(root.snapshot.poolCreatedAtMs || 0)
        if (created <= 0)
            return "—"
        const days = Math.max(0, Math.floor((Date.now() - created) / 86400000))
        if (days < 1) return "<" + LocaleFormat.formatInteger(1) + "d"
        if (days < 30) return LocaleFormat.formatInteger(days) + "d"
        if (days < 365) return LocaleFormat.formatInteger(Math.floor(days / 30)) + "mo"
        return LocaleFormat.formatNumber(days / 365, days >= 730 ? 0 : 1) + "y"
    }

    Component.onCompleted: reload()
    onEnrichmentActiveChanged: {
        if (root.enrichmentActive) reload()
    }
    onLocatorChanged: {
        root.expanded = false
        root.addressCopied = false
        root.requestedLocator = ""
        reload()
    }

    Connections {
        target: liveInsights
        function onTokenPreviewChanged(changedLocator) {
            if (String(changedLocator).toLowerCase() === root.locator.toLowerCase())
                root.reload()
        }
    }

    fillColor: Theme.surfaceRaised
    borderWidth: Theme.strokeHairline
    borderColor: Theme.borderSubtle
    curveDepth: Theme.shapeMedia
    implicitHeight: cardContent.implicitHeight + (root.narrow ? Theme.space3 : Theme.space4) * 2

    // The card's empty surface owns its tap; underlying Cast actions must
    // never receive it. Children (expand/external buttons) have priority.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        preventStealing: false
        onClicked: root.expanded = !root.expanded
    }

    ColumnLayout {
        id: cardContent
        anchors.fill: parent
        anchors.margins: root.narrow ? Theme.space3 : Theme.space4
        spacing: root.narrow ? Theme.space2 : Theme.space3

        GridLayout {
            Layout.fillWidth: true
            columns: root.headerStacked ? 1 : 2
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                MarketAssetIcon {
                    Layout.preferredWidth: root.narrow ? 44 : 52
                    Layout.preferredHeight: Layout.preferredWidth
                    asset: ({
                        symbol: String(root.snapshot.symbol || ""),
                        name: String(root.snapshot.name || "Token"),
                        imageSource: root.imageSource
                    })
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.available ? String(root.snapshot.name || root.snapshot.symbol || "Token") : "Token"
                        color: Theme.textPrimary
                        font.pixelSize: root.narrow ? Theme.typeMetaSize : Theme.typeLabelSize
                        font.weight: Theme.weightBold
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        LayoutMirroring.enabled: false
                        text: root.available ? "$" + String(root.snapshot.symbol || "TOKEN") : root.address
                        color: root.available ? Theme.accent : Theme.textTertiary
                        font.family: Theme.fontFamilyForText(text)
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideMiddle
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: root.headerStacked
                Layout.alignment: root.headerStacked ? Qt.AlignLeft : Qt.AlignRight
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection

                Item { visible: root.headerStacked; Layout.fillWidth: true }

                Superellipse {
                    implicitWidth: chainRow.implicitWidth + Theme.space2 * 2
                    implicitHeight: 28
                    fillColor: Theme.primarySurface
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: 14
                    exponent: 2.01

                    RowLayout {
                        id: chainRow
                        anchors.centerIn: parent
                        layoutDirection: Qt.LeftToRight
                        spacing: 4

                        Icon {
                            text: root.networkIcon
                            color: Theme.accent
                            preferredSize: 14
                        }

                        Label {
                            text: root.network.toUpperCase()
                            color: Theme.accent
                            font.family: Theme.latinFontFamily
                            font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                            font.weight: Theme.weightDemiBold
                        }
                    }
                }

                ActionButton {
                    objectName: "expandTokenDetails"
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: root.expanded ? Theme.iconChevronUp : Theme.iconChevronDown
                    text: root.expanded ? "Collapse token details" : "Expand token details"
                    onClicked: root.expanded = !root.expanded
                }

                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconArrowUpRight
                    text: qsTrId("genycaster.embed.open_link")
                    enabled: root.externalTargetUrl.length > 0
                    onClicked: root.openRequested(root.externalTargetUrl)
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            columns: root.priceStacked ? 1 : 2
            columnSpacing: Theme.space3
            rowSpacing: Theme.space2

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 1

                Label {
                    Layout.fillWidth: true
                    LayoutMirroring.enabled: false
                    text: root.available && root.priceKnown ? root.money(root.snapshot.priceUsd) : "—"
                    // Color only confirmed price direction; never infer a
                    // green/red market move from missing provider data.
                    color: root.changeKnown && root.priceKnown && root.change24h > 0
                           ? Theme.positive
                           : root.changeKnown && root.priceKnown && root.change24h < 0
                             ? Theme.negative : Theme.textPrimary
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: root.narrow ? Theme.typeTitle2Size : Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                    elide: Text.ElideRight
                }

                Label {
                    visible: root.available && root.changeKnown
                    Layout.fillWidth: true
                    LayoutMirroring.enabled: false
                    text: (root.change24h >= 0 ? "+" : "")
                          + LocaleFormat.formatNumber(root.change24h, 2)
                          + "% · " + LocaleFormat.formatInteger(24) + "h"
                    color: root.change24h >= 0 ? Theme.positive : Theme.negative
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }
            }

            Superellipse {
                visible: root.expanded && root.available && (root.snapshot.sparkline || []).length > 2
                Layout.fillWidth: true
                implicitHeight: root.priceStacked ? (root.narrow ? 70 : 78) : 82
                fillColor: Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveCompact

                TokenSparkline {
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    samples: root.snapshot.sparkline || []
                    strokeColor: root.change24h >= 0 ? Theme.positive : Theme.negative
                    strokeWidth: root.narrow ? 1.8 : 2.0
                    contentPadding: 2
                    fillArea: true
                }
            }
        }

        GridLayout {
            visible: root.expanded
            Layout.fillWidth: true
            layoutDirection: Qt.LeftToRight
            columns: root.metricColumns
            rowSpacing: Theme.space2
            columnSpacing: Theme.space2

            Repeater {
                model: [
                    { label: Number(root.snapshot.marketCapUsd || 0) > 0 ? "MKT CAP" : "FDV",
                      value: root.compactMoney(root.snapshot.marketCapUsd || root.snapshot.fdvUsd) },
                    { label: "LIQUIDITY", value: root.compactMoney(root.snapshot.liquidityUsd) },
                    { label: "BUYERS", value: root.snapshot.buyers24h !== null
                                          && root.snapshot.buyers24h !== undefined
                                          ? LocaleFormat.formatCompactNumber(Number(root.snapshot.buyers24h)) : "—" },
                    { label: "AGE", value: root.ageText() }
                ]

                delegate: Superellipse {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: root.narrow ? 52 : 58
                    fillColor: Theme.surfaceMuted
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: Theme.curveCompact

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: Theme.textTertiary
                            font.family: Theme.latinFontFamily
                            font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                            font.weight: Theme.weightDemiBold
                            horizontalAlignment: Text.AlignLeft
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modelData.value
                            color: Theme.textPrimary
                            font.family: Theme.latinFontFamily
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                            horizontalAlignment: Text.AlignLeft
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        DirectionAwareLabel {
            visible: root.tokenLoading || !root.available
            Layout.fillWidth: true
            text: root.tokenLoading ? "Fetching token market data…"
                 : (root.tokenError.length > 0 ? root.tokenError : "Market data unavailable")
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
        }

        // Contract and provenance intentionally occupy independent rows. The
        // contract text owns all remaining width and can middle-elide without
        // ever competing with the source label or copy button.
        Superellipse {
            visible: root.expanded
            Layout.fillWidth: true
            implicitHeight: Theme.touchTarget
            fillColor: Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderSubtle
            curveDepth: Theme.curveCompact

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space1
                layoutDirection: Theme.uiDirection
                spacing: Theme.space1

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    LayoutMirroring.enabled: false
                    text: root.address
                    color: Theme.textTertiary
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignLeft
                }

                ActionButton {
                    Layout.preferredWidth: Theme.touchTarget
                    Layout.preferredHeight: Theme.touchTarget
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: root.addressCopied ? Theme.iconCheck : Theme.iconCopy
                    text: qsTrId("genycaster.feedback.copy")
                    enabled: root.address.length > 0
                    onClicked: {
                        presentationBridge.copyText(root.address)
                        root.addressCopied = true
                        copiedAddressTimer.restart()
                    }
                }
            }
        }

        RowLayout {
            visible: root.expanded
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            RowLayout {
                visible: root.available
                Layout.fillWidth: true
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection

                Icon {
                    text: Theme.iconGlobe
                    color: Theme.textTertiary
                    preferredSize: Theme.iconSizeSmall
                }

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    LayoutMirroring.enabled: false
                    text: String(root.snapshot.source || "")
                    color: Theme.textTertiary
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                    elide: Text.ElideRight
                }
            }

            GenySpinner {
                visible: !root.available
                running: visible
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
            }
        }
    }
}
