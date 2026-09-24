import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string identityName: ""
    property string accountName: ""
    property string activeFeed: "following"
    property string statusText: ""
    property bool identityAvailable: false
    property bool isAuthenticated: false
    property double accountFid: 0
    property string accountUsername: ""
    property string accountAvatarUrl: ""
    property var profileDetail: ({})
    property var feedRows: []
    property bool offline: false
    property bool busy: false
    property bool stale: false
    property bool sourceUnavailable: false
    property string expandedMarketKey: ""

    signal accountRequested()
    signal refreshRequested()
    signal profileRequested(string username)
    signal ownProfileRequested()
    signal marketSettingsRequested()

    property int calendarOffset: 0
    property double selectedJulianDay: -1
    readonly property var calendarModel: {
        const revision = localizationController.revision
        return LocaleFormat.calendarMonthModel(root.calendarOffset)
    }
    readonly property var calendarWeekdays: root.calendarModel.weekdays || []
    readonly property var calendarCells: root.calendarModel.cells || []
    readonly property var recentProfiles: root.collectRecentProfiles(4)

    onProfileDetailChanged: root.syncWalletHolding()
    Component.onCompleted: root.syncWalletHolding()

    function syncWalletHolding() {
        const addresses = []
        const verified = root.profileDetail.verifiedEthAddresses || []
        for (let index = 0; index < verified.length; ++index) {
            const value = String(verified[index] || "").toLowerCase()
            if (value.length > 0 && addresses.indexOf(value) < 0) addresses.push(value)
        }
        if (addresses.length === 0) {
            const custody = String(root.profileDetail.custodyAddress || "").toLowerCase()
            if (custody.length > 0) addresses.push(custody)
        }
        liveInsights.setWalletAddresses(addresses)
    }

    function collectRecentProfiles(limit) {
        const result = []
        const seen = ({})
        const source = root.feedRows || []
        for (let index = 0; index < source.length && result.length < limit; ++index) {
            const row = source[index] || ({})
            const fid = Number(row.authorFid || 0)
            const username = String(row.author || row.username || "")
            const key = fid > 0 ? "fid:" + fid : "user:" + username.toLowerCase()
            if (key.length === 5 || seen[key]) continue
            seen[key] = true
            result.push({ fid: fid, username: username,
                          displayName: String(row.displayName || username),
                          avatarUrl: String(row.avatarUrl || "") })
        }
        return result
    }

    function marketPrice(value) {
        return liveInsights.formatMarketPrice(Number(value || 0))
    }

    function marketMoney(value) {
        const number = Number(value || 0)
        if (!Number.isFinite(number) || number <= 0) return "—"
        if (number >= 1000) return "$" + LocaleFormat.formatCompactNumber(number)
        return LocaleFormat.formatMonetaryUnit(number, "USD", 2)
    }

    function marketChange(value) {
        const number = Number(value || 0)
        if (!Number.isFinite(number)) return "—"
        return (number >= 0 ? "+" : "") + LocaleFormat.formatNumber(number, 2) + "%"
    }

    function shortWallet(value) {
        const wallet = String(value || "")
        return wallet.length > 14 ? wallet.slice(0, 7) + "…" + wallet.slice(-5) : wallet
    }

    function genyMarketRow() {
        const values = liveInsights.marketAssets || []
        for (let index = 0; index < values.length; ++index) {
            if (String(values[index].key || "") === "geny") return values[index]
        }
        return ({})
    }

    function shiftCalendarMonth(delta) {
        root.calendarOffset += delta
        root.selectedJulianDay = -1
    }

    function componentForWidget(key) {
        if (key === "assets") return assetsComponent
        if (key === "pulse") return pulseComponent
        if (key === "calendar") return calendarComponent
        if (key === "market") return marketComponent
        return suggestedComponent
    }

    component WidgetCard: Superellipse {
        id: card
        property string headerText: ""
        property string headerDetail: ""
        property string headerIcon: ""
        property color headerIconColor: Theme.accent
        property color headerIconSurface: Theme.primarySurface
        default property alias content: body.data

        fillColor: Theme.contextRailCard
        borderWidth: 0
        curveDepth: Theme.curveModal
        elevation: Theme.elevationNone
        implicitHeight: body.implicitHeight + Theme.space5

        ColumnLayout {
            id: body
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space3

            RowLayout {
                visible: card.headerText.length > 0
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Superellipse {
                    visible: card.headerIcon.length > 0
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    fillColor: card.headerIconSurface
                    curveDepth: 16
                    exponent: 2.01
                    Icon { anchors.centerIn: parent; text: card.headerIcon; color: card.headerIconColor; preferredSize: Theme.iconSizeSmall }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: card.headerText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightBold
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        visible: card.headerDetail.length > 0
                        Layout.fillWidth: true
                        text: card.headerDetail
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Rectangle { anchors.fill: parent; color: Theme.contextRailCanvas }

    ScrollView {
        id: railScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
        ScrollBar.vertical: GenyScrollBar {}

        ColumnLayout {
            id: widgetColumn
            width: railScroll.availableWidth
            spacing: Theme.space2

            Item { Layout.preferredHeight: Theme.space2 }

            Repeater {
                id: widgetRepeater
                model: liveInsights.contextWidgetOrder
                delegate: Item {
                    id: widgetDelegate
                    required property string modelData
                    required property int index
                    property string widgetId: modelData
                    property int orderIndex: index
                    property real dragOffset: 0
                    property real lastDragOffset: 0
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space3
                    Layout.rightMargin: Theme.space3
                    implicitHeight: widgetLoader.implicitHeight
                    z: reorderDrag.active ? 50 : 0
                    scale: reorderDrag.active ? 1.012 : 1.0

                    transform: Translate { y: widgetDelegate.dragOffset }

                    function commitReorder(offset) {
                        if (Math.abs(offset) < 12) return
                        const center = widgetDelegate.y + widgetDelegate.height / 2 + offset
                        let targetIndex = 0
                        for (let itemIndex = 0; itemIndex < widgetRepeater.count; ++itemIndex) {
                            const item = widgetRepeater.itemAt(itemIndex)
                            if (!item || item === widgetDelegate) continue
                            if (center > item.y + item.height / 2) ++targetIndex
                        }
                        liveInsights.moveContextWidget(widgetDelegate.widgetId, targetIndex)
                    }

                    Loader {
                        id: widgetLoader
                        anchors.left: parent.left
                        anchors.right: parent.right
                        sourceComponent: root.componentForWidget(widgetDelegate.widgetId)
                    }

                    Item {
                        id: reorderHandle
                        width: 30
                        height: 24
                        anchors.top: parent.top
                        anchors.right: Theme.rtl ? undefined : parent.right
                        anchors.left: Theme.rtl ? parent.left : undefined
                        anchors.topMargin: Theme.space2
                        anchors.rightMargin: Theme.rtl ? 0 : Theme.space2
                        anchors.leftMargin: Theme.rtl ? Theme.space2 : 0
                        z: 5
                        opacity: reorderDrag.active ? 1.0 : (dragHover.hovered ? 0.9 : 0.48)

                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconSort
                            color: reorderDrag.active ? Theme.accent : Theme.textTertiary
                            preferredSize: Theme.iconSizeSmall
                        }

                        HoverHandler {
                            id: dragHover
                            cursorShape: Qt.SizeVerCursor
                        }
                        DragHandler {
                            id: reorderDrag
                            target: null
                            xAxis.enabled: false
                            onTranslationChanged: {
                                if (!active) return
                                widgetDelegate.dragOffset = translation.y
                                widgetDelegate.lastDragOffset = translation.y
                            }
                            onActiveChanged: {
                                if (active) {
                                    widgetDelegate.dragOffset = 0
                                    widgetDelegate.lastDragOffset = 0
                                } else {
                                    const completedOffset = widgetDelegate.lastDragOffset
                                    widgetDelegate.commitReorder(completedOffset)
                                    widgetDelegate.dragOffset = 0
                                    widgetDelegate.lastDragOffset = 0
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: Theme.space4 }
        }
    }

    Component {
        id: assetsComponent
        WidgetCard {
            id: assetsCard
            property bool showingDetails: false
            headerText: qsTrId("genycaster.context.my_assets")
            headerDetail: qsTrId("genycaster.context.geny_utility")
            headerIcon: Theme.iconLeaf
            headerIconColor: Theme.accent
            headerIconSurface: Theme.primarySurface

            readonly property var holding: liveInsights.genyHolding || ({})
            readonly property var market: root.genyMarketRow()
            readonly property var addressRows: holding.addresses || []
            readonly property real balance: Number(holding.balance || 0)
            readonly property real valueUsd: balance * Number(market.priceUsd || 0)

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Item {
                    id: assetFlip
                    Layout.fillWidth: true
                    Layout.preferredHeight: assetsCard.showingDetails
                                            ? Math.min(420, 112 + Math.max(1, assetsCard.addressRows.length) * 78)
                                            : 170
                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.OutCubic }
                    }

                    Superellipse {
                        id: assetFront
                        anchors.fill: parent
                        fillColor: Theme.primarySurface
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.accent
                        curveDepth: Theme.curveContainer
                        opacity: assetsCard.showingDetails ? 0 : 1
                        enabled: !assetsCard.showingDetails
                        transform: Rotation {
                            origin.x: assetFront.width / 2
                            origin.y: assetFront.height / 2
                            axis { x: 0; y: 1; z: 0 }
                            angle: assetsCard.showingDetails ? 90 : 0
                            Behavior on angle { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic } }
                        }
                        Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space3
                                layoutDirection: Theme.uiDirection

                                SuperellipseImage {
                                    Layout.preferredWidth: 46
                                    Layout.preferredHeight: 46
                                    source: String(assetsCard.market.imageSource || "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/geny.svg")
                                    fillMode: Image.PreserveAspectFit
                                    mediaCategory: "other"
                                    requestedSourceWidth: 96
                                    requestedSourceHeight: 96
                                    fillColor: Theme.surfaceRaised
                                    curveDepth: 15
                                    exponent: 3.8
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: assetsCard.holding.loading ? qsTrId("genycaster.context.checking_geny")
                                             : (assetsCard.holding.wallets || []).length === 0 ? qsTrId("genycaster.context.wallet_needed")
                                             : assetsCard.holding.available && assetsCard.holding.holder
                                               ? LocaleFormat.formatNumber(assetsCard.balance, assetsCard.balance >= 100 ? 0 : 2) + " GENY"
                                               : assetsCard.holding.available ? qsTrId("genycaster.context.not_geny_holder")
                                               : qsTrId("genycaster.context.geny_balance_unavailable")
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.typeLabelSize
                                        font.weight: Theme.weightBold
                                        wrapMode: Text.Wrap
                                    }
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: assetsCard.holding.available && assetsCard.holding.holder && assetsCard.valueUsd > 0
                                              ? root.marketMoney(assetsCard.valueUsd)
                                              : qsTrId("genycaster.context.geny_membership_hint")
                                        color: assetsCard.holding.available && assetsCard.holding.holder ? Theme.accent : Theme.textSecondary
                                        font.pixelSize: Theme.typeCaptionSize
                                        wrapMode: Text.Wrap
                                    }
                                }

                                ActionButton {
                                    compact: true
                                    quiet: true
                                    iconOnly: true
                                    iconGlyph: Theme.iconRefresh
                                    text: qsTrId("genycaster.profile.refresh")
                                    enabled: (assetsCard.holding.wallets || []).length > 0 && !Boolean(assetsCard.holding.loading)
                                    onClicked: liveInsights.refreshGenyHolding()
                                }
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.context.geny_asset_explainer")
                                color: Theme.textTertiary
                                font.pixelSize: Theme.typeCaptionSize
                                maximumLineCount: 3
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space1
                                Icon { text: Theme.iconWallet; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: LocaleFormat.formatInteger((assetsCard.holding.wallets || []).length) + " · Base"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                                Icon { text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight; color: Theme.textTertiary; preferredSize: Theme.iconSizeSmall }
                            }
                        }

                        TapHandler { onTapped: assetsCard.showingDetails = true }
                    }

                    Superellipse {
                        id: assetBack
                        anchors.fill: parent
                        fillColor: Theme.surfaceRaised
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderStrong
                        curveDepth: Theme.curveContainer
                        opacity: assetsCard.showingDetails ? 1 : 0
                        enabled: assetsCard.showingDetails
                        transform: Rotation {
                            origin.x: assetBack.width / 2
                            origin.y: assetBack.height / 2
                            axis { x: 0; y: 1; z: 0 }
                            angle: assetsCard.showingDetails ? 0 : -90
                            Behavior on angle { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard; easing.type: Easing.OutCubic } }
                        }
                        Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space2

                            RowLayout {
                                Layout.fillWidth: true
                                layoutDirection: Theme.uiDirection
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.context.my_assets") + " · GENY"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeMetaSize
                                    font.weight: Theme.weightBold
                                }
                                ActionButton {
                                    compact: true
                                    quiet: true
                                    iconOnly: true
                                    iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
                                    text: qsTrId("genycaster.detail.back_back_to_feed")
                                    onClicked: assetsCard.showingDetails = false
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: walletList
                                    anchors.fill: parent
                                    model: assetsCard.addressRows
                                    spacing: Theme.space1
                                    clip: true
                                    boundsBehavior: Flickable.StopAtBounds

                                    delegate: Superellipse {
                                        required property var modelData
                                        width: walletList.width
                                        height: 72
                                        fillColor: Theme.surfaceMuted
                                        borderWidth: Theme.strokeHairline
                                        borderColor: Theme.borderSubtle
                                        curveDepth: Theme.curveCompact

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.space2
                                            spacing: Theme.space1

                                            Label {
                                                Layout.fillWidth: true
                                                LayoutMirroring.enabled: false
                                                text: root.shortWallet(modelData.wallet)
                                                color: Theme.textSecondary
                                                font.family: Theme.latinFontFamily
                                                font.pixelSize: Theme.typeCaptionSize
                                                horizontalAlignment: Text.AlignLeft
                                                elide: Text.ElideMiddle
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                layoutDirection: Theme.uiDirection
                                                spacing: Theme.space2
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    text: Boolean(modelData.holder)
                                                          ? qsTrId("genycaster.context.geny_holder")
                                                          : qsTrId("genycaster.context.not_geny_holder")
                                                    color: Boolean(modelData.holder) ? Theme.positive : Theme.textTertiary
                                                    font.pixelSize: Theme.typeCaptionSize
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    LayoutMirroring.enabled: false
                                                    Layout.maximumWidth: walletList.width * 0.48
                                                    text: Boolean(modelData.available)
                                                          ? LocaleFormat.formatNumber(Number(modelData.balance || 0), Number(modelData.balance || 0) >= 100 ? 0 : 2) + " GENY"
                                                          : "—"
                                                    color: Boolean(modelData.holder) ? Theme.accent : Theme.textTertiary
                                                    font.family: Theme.fontFamilyForText(text)
                                                    font.pixelSize: Theme.typeCaptionSize
                                                    font.weight: Theme.weightDemiBold
                                                    horizontalAlignment: Text.AlignRight
                                                    elide: Text.ElideLeft
                                                }
                                            }
                                        }
                                    }
                                }

                                DirectionAwareLabel {
                                    anchors.centerIn: parent
                                    width: parent.width
                                    visible: assetsCard.addressRows.length === 0
                                    text: qsTrId("genycaster.context.wallet_needed")
                                    color: Theme.textTertiary
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: pulseComponent
        FeedPulseCard {
            width: parent ? parent.width : implicitWidth
            rows: root.feedRows
            busy: root.busy
            accountFid: root.accountFid
            accountOnly: false
            stale: root.stale
            offline: root.offline
        }
    }

    Component {
        id: calendarComponent
        WidgetCard {
            headerText: qsTrId("genycaster.context.calendar")
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2
                    ActionButton { compact: true; quiet: true; iconOnly: true; iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft; text: qsTrId("genycaster.context.calendar"); onClicked: root.shiftCalendarMonth(-1) }
                    Label {
                        Layout.fillWidth: true
                        LayoutMirroring.enabled: false
                        text: String(root.calendarModel.title || "")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamilyForText(text)
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                    ActionButton { compact: true; quiet: true; iconOnly: true; iconGlyph: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight; text: qsTrId("genycaster.context.calendar"); onClicked: root.shiftCalendarMonth(1) }
                }
                GridLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    columns: 7
                    rowSpacing: 2
                    columnSpacing: 2
                    Repeater {
                        model: root.calendarWeekdays
                        delegate: Label {
                            required property var modelData
                            Layout.fillWidth: true; Layout.preferredHeight: 22; LayoutMirroring.enabled: false
                            text: String(modelData); color: Theme.textTertiary; font.family: Theme.fontFamilyForText(text)
                            font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightMedium
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                        }
                    }
                    Repeater {
                        model: root.calendarCells
                        delegate: Item {
                            required property var modelData
                            readonly property bool adjacentMonth: Boolean(modelData.adjacent)
                            readonly property bool today: Boolean(modelData.today)
                            readonly property bool selected: root.selectedJulianDay > 0 ? Number(modelData.julianDay || 0) === root.selectedJulianDay : today
                            Layout.fillWidth: true; Layout.preferredHeight: 32
                            Superellipse {
                                anchors.centerIn: parent; width: 28; height: 28
                                fillColor: parent.selected ? Theme.primaryFill : parent.today ? Theme.primarySurface : "transparent"
                                borderWidth: parent.today && !parent.selected ? 1 : 0; borderColor: Theme.accent; curveDepth: 14; exponent: 2.01
                                Label {
                                    anchors.centerIn: parent; text: String(modelData.dayText || modelData.day || "")
                                    color: parent.parent.selected ? Theme.accentInk : parent.parent.adjacentMonth ? Theme.textDisabled : parent.parent.today ? Theme.accent : Theme.textSecondary
                                    font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize
                                    font.weight: parent.parent.selected || parent.parent.today ? Theme.weightDemiBold : Theme.weightRegular
                                }
                                TapHandler { onTapped: root.selectedJulianDay = Number(modelData.julianDay || -1) }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: marketComponent
        WidgetCard {
            headerText: qsTrId("genycaster.context.token_pulse")
            headerDetail: liveInsights.marketStatus
            headerIcon: Theme.iconChart
            headerIconColor: Theme.accent
            headerIconSurface: Theme.primarySurface

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Repeater {
                    model: liveInsights.marketAssets
                    delegate: Superellipse {
                        id: tokenRow
                        required property var modelData
                        readonly property bool expanded: root.expandedMarketKey === String(modelData.key || "")
                        Layout.fillWidth: true
                        implicitHeight: tokenRow.expanded ? 188 : 62
                        fillColor: tokenTap.hovered || tokenRow.expanded ? Theme.surfaceRaised : Theme.surfaceMuted
                        borderWidth: Theme.strokeHairline
                        borderColor: tokenRow.expanded ? Theme.borderStrong : Theme.borderSubtle
                        curveDepth: Theme.curveCompact

                        Behavior on implicitHeight { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.OutCubic } }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space2
                            spacing: Theme.space2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space2
                                layoutDirection: Theme.uiDirection

                                MarketAssetIcon {
                                    Layout.preferredWidth: 38
                                    Layout.preferredHeight: 38
                                    asset: modelData
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 0
                                    DirectionAwareLabel { Layout.fillWidth: true; text: String(modelData.name || modelData.symbol || "Token"); color: Theme.textPrimary; font.pixelSize: Theme.typeMetaSize; font.weight: Theme.weightDemiBold; elide: Text.ElideRight }
                                    Label { LayoutMirroring.enabled: false; text: String(modelData.symbol || "") + (Boolean(modelData.stale) ? " · stale" : ""); color: Boolean(modelData.stale) ? Theme.warning : Theme.textTertiary; font.family: Theme.latinFontFamily; font.pixelSize: Theme.typeCaptionSize }
                                }

                                TokenSparkline {
                                    Layout.preferredWidth: 82; Layout.preferredHeight: 30
                                    samples: modelData.sparkline || []
                                    strokeColor: Number(modelData.change24h || 0) >= 0 ? Theme.positive : Theme.negative
                                }

                                ColumnLayout {
                                    spacing: 0
                                    Label { LayoutMirroring.enabled: false; text: root.marketPrice(modelData.priceUsd); color: Theme.textPrimary; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeMetaSize; font.weight: Theme.weightDemiBold; horizontalAlignment: Text.AlignRight }
                                    Label { LayoutMirroring.enabled: false; text: Boolean(modelData.available) ? root.marketChange(modelData.change24h) : qsTrId("genycaster.context.market_waiting"); color: !Boolean(modelData.available) ? Theme.textTertiary : Number(modelData.change24h || 0) >= 0 ? Theme.positive : Theme.negative; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightMedium; horizontalAlignment: Text.AlignRight }
                                }

                                Icon { text: tokenRow.expanded ? Theme.iconChevronUp : Theme.iconChevronDown; color: Theme.textTertiary; preferredSize: Theme.iconSizeSmall }
                            }

                            GridLayout {
                                visible: tokenRow.expanded
                                layoutDirection: Theme.uiDirection
                                Layout.fillWidth: true
                                columns: 3
                                uniformCellWidths: true
                                rowSpacing: Theme.space2
                                columnSpacing: Theme.space2

                                ColumnLayout { spacing: 0; DirectionAwareLabel { text: qsTrId("genycaster.context.market_cap"); color: Theme.textTertiary; font.pixelSize: Theme.typeCaptionSize }
                                Label { text: root.marketMoney(modelData.marketCapUsd); color: Theme.textPrimary; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold } }
                                ColumnLayout { spacing: 0; DirectionAwareLabel { text: qsTrId("genycaster.context.volume_24h"); color: Theme.textTertiary; font.pixelSize: Theme.typeCaptionSize }
                                Label { text: root.marketMoney(modelData.volume24hUsd); color: Theme.textPrimary; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold } }
                                ColumnLayout { spacing: 0; DirectionAwareLabel { text: qsTrId("genycaster.context.liquidity"); color: Theme.textTertiary; font.pixelSize: Theme.typeCaptionSize }
                                Label { text: root.marketMoney(modelData.liquidityUsd); color: Theme.textPrimary; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold } }
                                ColumnLayout { spacing: 0; DirectionAwareLabel { text: qsTrId("genycaster.context.fdv"); color: Theme.textTertiary; font.pixelSize: Theme.typeCaptionSize }
                                Label { text: root.marketMoney(modelData.fdvUsd); color: Theme.textPrimary; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold } }
                                ColumnLayout { spacing: 0; DirectionAwareLabel { text: qsTrId("genycaster.context.buyers_24h"); color: Theme.textTertiary; font.pixelSize: Theme.typeCaptionSize }
                                Label { text: LocaleFormat.formatInteger(Number(modelData.buyers24h || 0)); color: Theme.textPrimary; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold } }
                                ColumnLayout { spacing: 0; DirectionAwareLabel { text: qsTrId("genycaster.context.sellers_24h"); color: Theme.textTertiary; font.pixelSize: Theme.typeCaptionSize }
                                Label { text: LocaleFormat.formatInteger(Number(modelData.sellers24h || 0)); color: Theme.textPrimary; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold } }
                            }
                        }

                        HoverHandler { id: tokenTap; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: root.expandedMarketKey = tokenRow.expanded ? "" : String(modelData.key || "") }
                    }
                }

                DirectionAwareLabel { visible: liveInsights.marketAssets.length === 0; Layout.fillWidth: true; text: qsTrId("genycaster.context.market_empty"); color: Theme.textTertiary; font.pixelSize: Theme.typeCaptionSize; wrapMode: Text.Wrap }

                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.space2; layoutDirection: Theme.uiDirection
                    DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.context.market_source_live"); color: Theme.textTertiary; font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1); elide: Text.ElideRight }
                    ActionButton { compact: true; quiet: true; iconOnly: true; iconGlyph: Theme.iconRefresh; text: qsTrId("genycaster.profile.refresh"); enabled: liveInsights.providerAvailable && !liveInsights.marketRefreshing; onClicked: liveInsights.refreshMarket() }
                    ActionButton { compact: true; quiet: true; iconOnly: true; iconGlyph: Theme.iconSettings; text: qsTrId("genycaster.settings.market_pulse"); onClicked: root.marketSettingsRequested() }
                }
            }
        }
    }

    Component {
        id: suggestedComponent
        WidgetCard {
            visible: root.recentProfiles.length > 0
            headerText: qsTrId("genycaster.context.suggested_for_you")
            headerIcon: Theme.iconUser
            headerIconColor: Theme.accent
            headerIconSurface: Theme.primarySurface
            ColumnLayout {
                Layout.fillWidth: true; spacing: Theme.space2
                Repeater {
                    model: root.recentProfiles
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; layoutDirection: Theme.uiDirection; spacing: Theme.space2
                        Avatar { avatarSize: 34; sourceUrl: String(modelData.avatarUrl || ""); displayName: String(modelData.displayName || "") }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 0
                            DirectionAwareLabel { Layout.fillWidth: true; text: String(modelData.displayName || modelData.username || ""); color: Theme.textPrimary; font.pixelSize: Theme.typeMetaSize; font.weight: Theme.weightDemiBold; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; LayoutMirroring.enabled: false; text: "@" + String(modelData.username || ""); color: Theme.textTertiary; font.family: Theme.latinFontFamily; font.pixelSize: Theme.typeCaptionSize; horizontalAlignment: Text.AlignLeft; elide: Text.ElideRight }
                        }
                        ActionButton { compact: true; quiet: true; iconOnly: true; iconGlyph: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight; text: qsTrId("genycaster.home.view_profile"); onClicked: root.profileRequested(String(modelData.username || "")) }
                    }
                }
            }
        }
    }
}
