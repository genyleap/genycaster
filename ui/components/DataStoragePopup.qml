import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Adaptive Data & Storage surface shared by desktop and mobile clients.
 *
 * The QML layer only edits/query policies. Cache enumeration, eviction, network
 * accounting and persistence remain in DataStorageController off the render path.
 */
GenyDrawer {
    id: root
    objectName: "dataStoragePopup"

    readonly property bool mobilePresentation: parent ? parent.width < Theme.breakpointMobile : true
    readonly property bool compactLayout: width < 640
    readonly property bool twoColumnLayout: width >= 840
    readonly property real columnGap: Theme.space4
    property int policyRevision: 0
    property int storageRevision: 0
    property int usageRevision: 0
    property string usageScope: "all"

    signal backRequested()

    bottomSheet: root.mobilePresentation
    dimBackground: true
    contentPadding: root.compactLayout ? Theme.gutter(root.width) : Theme.space5
    width: parent ? (bottomSheet ? parent.width : Math.min(960, parent.width * 0.94)) : 960
    height: parent ? (bottomSheet ? Math.min(parent.height * 0.94, 900) : parent.height) : 900

    onOpened: dataStorage.refreshStorageUsage()

    Connections {
        target: dataStorage
        function onPolicyChanged() { ++root.policyRevision }
        function onStorageChanged() { ++root.storageRevision }
        function onUsageChanged() { ++root.usageRevision }
    }

    function formatBytes(value) {
        const bytes = Math.max(0, Number(value || 0))
        if (bytes < 1024) return LocaleFormat.formatInteger(Math.round(bytes)) + " B"
        const units = ["KiB", "MiB", "GiB", "TiB"]
        let scaled = bytes / 1024
        let unit = 0
        while (scaled >= 1024 && unit + 1 < units.length) {
            scaled /= 1024
            ++unit
        }
        const decimals = scaled >= 100 ? 0 : scaled >= 10 ? 1 : 2
        return LocaleFormat.formatNumber(scaled, decimals) + " " + units[unit]
    }

    function formatPercent(value, total) {
        const denominator = Math.max(0, Number(total || 0))
        if (!(denominator > 0))
            return LocaleFormat.formatNumber(0, 1) + "%"
        const percent = Math.max(0, Math.min(100, Number(value || 0) * 100 / denominator))
        return LocaleFormat.formatNumber(percent, percent >= 10 ? 1 : 2) + "%"
    }

    function categoryLabel(id) {
        switch (String(id || "")) {
        case "photos": return qsTrId("genycaster.storage.photos")
        case "video-previews": return qsTrId("genycaster.storage.video_previews")
        case "nfts": return qsTrId("genycaster.storage.nfts")
        case "miniapps": return qsTrId("genycaster.storage.mini_apps")
        case "link-previews": return qsTrId("genycaster.storage.link_previews")
        case "avatars": return qsTrId("genycaster.storage.avatars")
        case "animated-emoji": return qsTrId("genycaster.storage.animated_emoji")
        default: return qsTrId("genycaster.storage.other")
        }
    }

    function categoryIcon(id) {
        switch (String(id || "")) {
        case "photos": return Theme.iconImage
        case "video-previews": return Theme.iconPlay
        case "miniapps": return Theme.iconApps
        case "link-previews": return Theme.iconLink
        case "avatars": return Theme.iconUser
        case "animated-emoji": return Theme.iconEmoji
        case "nfts": return Theme.iconStore
        default: return Theme.iconFile
        }
    }

    function categoryColor(id) {
        switch (String(id || "")) {
        case "photos": return Theme.swatchGeny
        case "video-previews": return Theme.swatchOcean
        case "nfts": return Theme.swatchViolet
        case "miniapps": return Theme.swatchEmber
        case "link-previews": return Theme.swatchRose
        case "avatars": return Theme.swatchMint
        case "animated-emoji": return Theme.accent
        default: return Theme.inkSubtle
        }
    }

    function scopedUsageRows() {
        root.usageRevision
        return dataStorage.usageBreakdownForNetwork(root.usageScope)
    }

    function scopedUsageTotal() {
        root.usageRevision
        return dataStorage.usageBytesForNetwork(root.usageScope)
    }

    function networkLabel(id) {
        switch (String(id || "")) {
        case "wifi": return qsTrId("genycaster.storage.wifi")
        case "ethernet": return qsTrId("genycaster.storage.ethernet")
        case "cellular": return qsTrId("genycaster.storage.cellular")
        default: return qsTrId("genycaster.storage.unknown_network")
        }
    }

    function retentionOptions() {
        return [
            { days: 1, label: qsTrId("genycaster.storage.one_day") },
            { days: 3, label: qsTrId("genycaster.storage.three_days") },
            { days: 7, label: qsTrId("genycaster.storage.one_week") },
            { days: 30, label: qsTrId("genycaster.storage.one_month") },
            { days: 90, label: qsTrId("genycaster.storage.three_months") },
            { days: 365, label: qsTrId("genycaster.storage.one_year") },
            { days: 0, label: qsTrId("genycaster.storage.never") }
        ]
    }

    function retentionIndex(category) {
        root.policyRevision
        const value = dataStorage.retentionDays(category)
        const options = root.retentionOptions()
        for (let index = 0; index < options.length; ++index) {
            if (Number(options[index].days) === Number(value))
                return index
        }
        return options.length - 1
    }

    function sizeOptions() {
        return [1, 2, 4, 8, 16, 24, 32, 48, 64]
    }

    function sizeIndex(category, network) {
        root.policyRevision
        const value = dataStorage.autoDownloadLimitMiB(category, network)
        const options = root.sizeOptions()
        let best = 0
        let distance = Number.POSITIVE_INFINITY
        for (let index = 0; index < options.length; ++index) {
            const candidate = Math.abs(Number(options[index]) - Number(value))
            if (candidate < distance) {
                best = index
                distance = candidate
            }
        }
        return best
    }

    readonly property var downloadCategories: [
        { id: "photos", label: qsTrId("genycaster.storage.photos"), icon: Theme.iconImage },
        { id: "video-previews", label: qsTrId("genycaster.storage.video_previews"), icon: Theme.iconPlay },
        { id: "nfts", label: qsTrId("genycaster.storage.nfts"), icon: Theme.iconStore },
        { id: "miniapps", label: qsTrId("genycaster.storage.mini_apps"), icon: Theme.iconApps },
        { id: "link-previews", label: qsTrId("genycaster.storage.link_previews"), icon: Theme.iconLink }
    ]

    readonly property var retentionCategories: [
        { id: "photos", label: qsTrId("genycaster.storage.photos") },
        { id: "video-previews", label: qsTrId("genycaster.storage.video_previews") },
        { id: "nfts", label: qsTrId("genycaster.storage.nfts") },
        { id: "miniapps", label: qsTrId("genycaster.storage.mini_apps") },
        { id: "link-previews", label: qsTrId("genycaster.storage.link_previews") },
        { id: "avatars", label: qsTrId("genycaster.storage.avatars") },
        { id: "animated-emoji", label: qsTrId("genycaster.storage.animated_emoji") },
        { id: "other", label: qsTrId("genycaster.storage.other") }
    ]

    component SectionCard: Superellipse {
        id: card
        property string titleText: ""
        property string descriptionText: ""
        property string iconGlyph: Theme.iconSettings
        property color accentColor: Theme.accent
        default property alias bodyData: body.data

        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + Theme.space3 * 2
        fillColor: Theme.surfaceRaised
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space3

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space3

                Superellipse {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    fillColor: Qt.rgba(card.accentColor.r, card.accentColor.g, card.accentColor.b,
                                       Theme.dark ? 0.18 : 0.11)
                    borderWidth: Theme.strokeHairline
                    borderColor: Qt.rgba(card.accentColor.r, card.accentColor.g, card.accentColor.b,
                                         Theme.dark ? 0.34 : 0.22)
                    curveDepth: Theme.curveCompact
                    Icon {
                        anchors.centerIn: parent
                        text: card.iconGlyph
                        color: card.accentColor
                        preferredSize: Theme.iconSizeSmall
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: card.titleText
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightBold
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        visible: card.descriptionText.length > 0
                        Layout.fillWidth: true
                        text: card.descriptionText
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        lineHeight: Theme.typeCaptionLineHeight
                        wrapMode: Text.Wrap
                    }
                }
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.strokeHairline
                fillColor: Theme.divider
                curveDepth: Theme.strokeHairline / 2
            }

            ColumnLayout {
                id: body
                Layout.fillWidth: true
                spacing: Theme.space2
            }
        }
    }

    component SettingRow: Superellipse {
        id: settingRow
        default property alias rowData: row.data
        property int contentSpacing: Theme.space2
        property real horizontalPadding: Theme.space3
        property real verticalPadding: Theme.space2

        Layout.fillWidth: true
        implicitHeight: row.implicitHeight + settingRow.verticalPadding * 2
        fillColor: Theme.surfaceMuted
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveCompact

        RowLayout {
            id: row
            anchors.fill: parent
            anchors.leftMargin: settingRow.horizontalPadding
            anchors.rightMargin: settingRow.horizontalPadding
            anchors.topMargin: settingRow.verticalPadding
            anchors.bottomMargin: settingRow.verticalPadding
            layoutDirection: Theme.uiDirection
            spacing: settingRow.contentSpacing
        }
    }

    component MetricPill: Superellipse {
        id: metric
        property string labelText: ""
        property string valueText: ""
        Layout.fillWidth: true
        implicitHeight: 52
        fillColor: Theme.surfaceMuted
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderSubtle
        curveDepth: Theme.curveCompact
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space2
            spacing: 0
            DirectionAwareLabel { Layout.fillWidth: true; text: metric.valueText; color: Theme.ink; font.pixelSize: Theme.typeBodySize; font.weight: Theme.weightBold; forceLeftToRight: true }
            DirectionAwareLabel { Layout.fillWidth: true; text: metric.labelText; color: Theme.inkMuted; font.pixelSize: Theme.typeCaptionSize; elide: Text.ElideRight }
        }
    }

    component DonutChart: Item {
        id: chart
        property var rows: []
        property real totalBytes: 0
        property string centerText: root.formatBytes(chart.totalBytes)
        property string captionText: ""

        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: root.compactLayout ? 156 : 172
        Layout.preferredHeight: Layout.preferredWidth
        implicitWidth: Layout.preferredWidth
        implicitHeight: Layout.preferredHeight

        function startFraction(index) {
            if (!(chart.totalBytes > 0))
                return 0
            let preceding = 0
            for (let i = 0; i < index; ++i)
                preceding += Math.max(0, Number(chart.rows[i].bytes || 0))
            return Math.max(0, Math.min(1, preceding / chart.totalBytes))
        }

        VectorStroke {
            anchors.fill: parent
            pathKind: VectorStroke.Arc
            strokeColor: Theme.borderMuted
            strokeWidth: root.compactLayout ? 18 : 20
            inset: Theme.space1
            roundCaps: false
            startAngle: -90
            sweepAngle: 360
        }

        Repeater {
            model: chart.rows
            delegate: VectorStroke {
                required property int index
                required property var modelData
                anchors.fill: parent
                pathKind: VectorStroke.Arc
                strokeColor: root.categoryColor(modelData.id)
                strokeWidth: root.compactLayout ? 18 : 20
                inset: Theme.space1
                roundCaps: false
                startAngle: -90 + 360 * chart.startFraction(index)
                sweepAngle: chart.totalBytes > 0
                            ? 360 * Math.max(0, Number(modelData.bytes || 0)) / chart.totalBytes
                            : 0
            }
        }

        Column {
            anchors.centerIn: parent
            width: Math.max(80, parent.width * 0.56)
            spacing: Theme.space1

            DirectionAwareLabel {
                width: parent.width
                text: chart.centerText
                color: Theme.ink
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightBold
                horizontalAlignment: Text.AlignHCenter
                forceLeftToRight: true
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                visible: chart.captionText.length > 0
                width: parent.width
                text: chart.captionText
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }

    component AutoDownloadCard: SectionCard {
        id: autoCard
        required property string networkId
        titleText: autoCard.networkId === "wifi"
                   ? qsTrId("genycaster.storage.using_wifi")
                   : qsTrId("genycaster.storage.using_cellular")
        descriptionText: qsTrId("genycaster.storage.auto_download_network_description")
        iconGlyph: autoCard.networkId === "wifi" ? Theme.iconWifi : Theme.iconGlobe
        accentColor: autoCard.networkId === "wifi" ? Theme.swatchMint : Theme.swatchEmber

        Repeater {
            model: root.downloadCategories
            delegate: Superellipse {
                id: downloadRow
                required property var modelData
                readonly property bool ruleEnabled: {
                    root.policyRevision
                    return dataStorage.autoDownloadEnabled(String(modelData.id), autoCard.networkId)
                }

                Layout.fillWidth: true
                implicitHeight: downloadColumn.implicitHeight + Theme.space2 * 2
                fillColor: downloadRow.ruleEnabled ? Theme.surface : Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: downloadRow.ruleEnabled ? Theme.borderSubtle : Theme.borderMuted
                curveDepth: Theme.curveCompact

                ColumnLayout {
                    id: downloadColumn
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    spacing: Theme.space1

                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2

                        Superellipse {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            fillColor: downloadRow.ruleEnabled ? Theme.primarySurface : Theme.surfacePressed
                            curveDepth: Theme.curveCompact

                            Icon {
                                anchors.centerIn: parent
                                text: String(downloadRow.modelData.icon)
                                color: downloadRow.ruleEnabled ? autoCard.accentColor : Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
                            }
                        }

                        GenySwitch {
                            Layout.fillWidth: true
                            text: String(downloadRow.modelData.label)
                            checked: downloadRow.ruleEnabled
                            onToggled: dataStorage.setAutoDownloadEnabled(
                                           String(downloadRow.modelData.id), autoCard.networkId, checked)
                        }
                    }

                    RowLayout {
                        visible: downloadRow.ruleEnabled
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.space6
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.storage.maximum_file_size")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }

                        GenyComboBox {
                            Layout.preferredWidth: root.compactLayout ? 124 : 140
                            model: root.sizeOptions().map(value => LocaleFormat.formatInteger(value) + " MiB")
                            currentIndex: root.sizeIndex(String(downloadRow.modelData.id), autoCard.networkId)
                            onActivated: optionIndex => dataStorage.setAutoDownloadLimitMiB(
                                             String(downloadRow.modelData.id), autoCard.networkId,
                                             Number(root.sizeOptions()[optionIndex]))
                        }
                    }
                }
            }
        }
    }

    component StorageUsageCard: SectionCard {
        titleText: qsTrId("genycaster.storage.storage_usage")
        descriptionText: qsTrId("genycaster.storage.storage_usage_description")
        iconGlyph: Theme.iconChart
        accentColor: Theme.swatchGeny

        DonutChart {
            rows: dataStorage.cacheBreakdown
            totalBytes: Number(dataStorage.cacheUsageBytes)
            captionText: qsTrId("genycaster.storage.cached_media")
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            MetricPill { labelText: qsTrId("genycaster.storage.cached_media"); valueText: root.formatBytes(dataStorage.cacheUsageBytes) }
            MetricPill { labelText: qsTrId("genycaster.storage.free_disk"); valueText: root.formatBytes(dataStorage.freeDiskBytes) }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.storage.cache_limit") + " · "
                      + LocaleFormat.formatInteger(dataStorage.cacheLimitMiB) + " MiB"
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }
            GenyProgressBar {
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, Number(dataStorage.cacheLimitMiB) * 1024 * 1024)
                value: Math.min(to, Number(dataStorage.cacheUsageBytes))
                progressColor: Theme.swatchGeny
            }
        }

        Repeater {
            model: dataStorage.cacheBreakdown
            delegate: SettingRow {
                required property var modelData

                Superellipse {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    fillColor: Qt.rgba(root.categoryColor(modelData.id).r,
                                       root.categoryColor(modelData.id).g,
                                       root.categoryColor(modelData.id).b,
                                       Theme.dark ? 0.18 : 0.10)
                    curveDepth: Theme.curveTiny
                    Icon {
                        anchors.centerIn: parent
                        text: root.categoryIcon(modelData.id)
                        color: root.categoryColor(modelData.id)
                        preferredSize: Theme.iconSizeSmall
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    DirectionAwareLabel { Layout.fillWidth: true; text: root.categoryLabel(modelData.id); color: Theme.ink; font.pixelSize: Theme.typeMetaSize; font.weight: Theme.weightMedium }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.formatBytes(modelData.bytes) + " · "
                              + root.formatPercent(modelData.bytes, dataStorage.cacheUsageBytes) + " · "
                              + LocaleFormat.formatInteger(Number(modelData.files || 0)) + " "
                              + qsTrId("genycaster.storage.files")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }
                ActionButton {
                    visible: Number(modelData.bytes || 0) > 0
                    compact: true
                    quiet: true
                    iconOnly: true
                    destructive: true
                    iconGlyph: Theme.iconTrash
                    text: qsTrId("genycaster.storage.clear_category")
                    onClicked: dataStorage.clearCache(String(modelData.id))
                }
            }
        }

        ActionButton {
            Layout.fillWidth: true
            destructive: true
            iconGlyph: Theme.iconTrash
            busy: dataStorage.maintenanceBusy
            enabled: dataStorage.cacheUsageBytes > 0 && !dataStorage.maintenanceBusy
            text: qsTrId("genycaster.storage.clear_entire_cache") + " " + root.formatBytes(dataStorage.cacheUsageBytes)
            onClicked: dataStorage.clearEntireCache()
        }
    }

    component CachePolicyCard: SectionCard {
        titleText: qsTrId("genycaster.storage.cache_policy")
        descriptionText: qsTrId("genycaster.storage.cache_policy_description")
        iconGlyph: Theme.iconDownload
        accentColor: Theme.swatchOcean

        GenySwitch {
            Layout.fillWidth: true
            text: qsTrId("genycaster.storage.keep_media_on_disk")
            description: qsTrId("genycaster.storage.keep_media_on_disk_description")
            checked: dataStorage.diskCacheEnabled
            onToggled: dataStorage.setDiskCacheEnabled(checked)
        }

        ColumnLayout {
            Layout.fillWidth: true
            enabled: dataStorage.diskCacheEnabled
            opacity: enabled ? 1 : 0.55
            spacing: Theme.space1
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.storage.cache_limit"); color: Theme.ink; font.pixelSize: Theme.typeMetaSize; font.weight: Theme.weightMedium }
                DirectionAwareLabel { text: LocaleFormat.formatInteger(dataStorage.cacheLimitMiB) + " MiB"; color: Theme.accent; font.pixelSize: Theme.typeMetaSize; font.weight: Theme.weightBold; forceLeftToRight: true }
            }
            GenySlider {
                Layout.fillWidth: true
                from: 64
                to: 8192
                stepSize: 64
                snapMode: Slider.SnapAlways
                value: dataStorage.cacheLimitMiB
                activeColor: Theme.swatchOcean
                Accessible.name: qsTrId("genycaster.storage.cache_limit")
                onMoved: dataStorage.setCacheLimitMiB(Math.round(value / 64) * 64)
            }
        }

        Superellipse {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            fillColor: Theme.divider
            curveDepth: 1
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.storage.auto_remove_cached_media")
            color: Theme.ink
            font.pixelSize: Theme.typeMetaSize
            font.weight: Theme.weightDemiBold
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.storage.auto_remove_description")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        Repeater {
            model: root.retentionCategories
            delegate: SettingRow {
                required property var modelData
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: String(modelData.label)
                    color: Theme.ink
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightMedium
                }
                GenyComboBox {
                    Layout.preferredWidth: root.compactLayout ? 142 : 174
                    model: root.retentionOptions().map(option => option.label)
                    currentIndex: root.retentionIndex(String(modelData.id))
                    onActivated: optionIndex => dataStorage.setRetentionDays(
                                     String(modelData.id),
                                     Number(root.retentionOptions()[optionIndex].days))
                }
            }
        }
    }

    component MediaDataUsageCard: SectionCard {
        titleText: qsTrId("genycaster.storage.media_data_usage")
        descriptionText: qsTrId("genycaster.storage.media_data_usage_description")
        iconGlyph: Theme.iconChart
        accentColor: Theme.swatchViolet

        GenyTabStrip {
            Layout.fillWidth: true
            distribute: true
            segmented: true
            currentId: root.usageScope
            tabs: [
                { id: "all", label: qsTrId("genycaster.storage.all") },
                { id: "wifi", label: qsTrId("genycaster.storage.wifi") },
                { id: "cellular", label: qsTrId("genycaster.storage.cellular") }
            ]
            onActivated: tabId => root.usageScope = tabId
        }

        DonutChart {
            rows: root.scopedUsageRows()
            totalBytes: root.scopedUsageTotal()
            captionText: root.usageScope === "wifi"
                         ? qsTrId("genycaster.storage.wifi")
                         : root.usageScope === "cellular"
                           ? qsTrId("genycaster.storage.cellular")
                           : qsTrId("genycaster.storage.all")
        }

        GridLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            columns: root.compactLayout ? 1 : 3
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2
            MetricPill { labelText: qsTrId("genycaster.storage.total"); valueText: root.formatBytes(dataStorage.totalUsageBytes) }
            MetricPill { labelText: qsTrId("genycaster.storage.wifi"); valueText: root.formatBytes(dataStorage.wifiUsageBytes) }
            MetricPill { labelText: qsTrId("genycaster.storage.cellular"); valueText: root.formatBytes(dataStorage.cellularUsageBytes) }
        }

        SettingRow {
            Superellipse {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveTiny
                Icon { anchors.centerIn: parent; text: Theme.iconGlobe; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.storage.current_network") + ": " + root.networkLabel(dataStorage.networkType)
                color: Theme.ink
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightMedium
            }
            DirectionAwareLabel {
                text: dataStorage.networkMetered ? qsTrId("genycaster.storage.metered") : qsTrId("genycaster.storage.unmetered")
                color: dataStorage.networkMetered ? Theme.warning : Theme.success
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }
        }

        Repeater {
            model: root.scopedUsageRows()
            delegate: SettingRow {
                required property var modelData
                Superellipse {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    fillColor: Qt.rgba(root.categoryColor(modelData.id).r,
                                       root.categoryColor(modelData.id).g,
                                       root.categoryColor(modelData.id).b,
                                       Theme.dark ? 0.16 : 0.09)
                    curveDepth: Theme.curveTiny
                    Icon { anchors.centerIn: parent; text: root.categoryIcon(modelData.id); color: root.categoryColor(modelData.id); preferredSize: Theme.typeCaptionSize }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.categoryLabel(modelData.id) + " · "
                          + root.formatPercent(modelData.bytes, root.scopedUsageTotal())
                    color: Theme.ink
                    font.pixelSize: Theme.typeMetaSize
                }
                DirectionAwareLabel { text: root.formatBytes(modelData.bytes); color: Theme.inkMuted; font.pixelSize: Theme.typeCaptionSize; forceLeftToRight: true }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.storage.usage_since") + " " + LocaleFormat.formatDateTime(dataStorage.usageSinceMs)
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        ActionButton {
            Layout.fillWidth: true
            quiet: true
            iconGlyph: Theme.iconRefresh
            text: qsTrId("genycaster.storage.reset_statistics")
            onClicked: dataStorage.resetDataUsage()
        }
    }

    component DownloadBehaviorCard: SectionCard {
        titleText: qsTrId("genycaster.storage.download_behavior")
        descriptionText: qsTrId("genycaster.storage.download_behavior_description")
        iconGlyph: Theme.iconInfo
        accentColor: Theme.swatchRose

        SettingRow {
            Superellipse {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveTiny
                Icon { anchors.centerIn: parent; text: Theme.iconDownload; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.storage.tap_to_load_explanation")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }

        SettingRow {
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.storage.avatar_note")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }

        ActionButton {
            Layout.fillWidth: true
            quiet: true
            iconGlyph: Theme.iconRefresh
            text: qsTrId("genycaster.storage.reset_auto_download")
            onClicked: dataStorage.resetAutoDownloadSettings()
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space3

        Superellipse {
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight + Theme.space3 * 2
            fillColor: Theme.surfaceRaised
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveContainer

            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.margins: Theme.space3
                layoutDirection: Theme.uiDirection
                spacing: Theme.space3

                ActionButton {
                    compact: true
                    quiet: false
                    iconOnly: true
                    iconGlyph: Theme.iconBack
                    text: qsTrId("genycaster.storage.back_to_settings")
                    onClicked: root.backRequested()
                }

                Superellipse {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    fillColor: Theme.primarySurface
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: Theme.curveCompact
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconDownload
                        color: Theme.accent
                        preferredSize: Theme.iconSizeSmall
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.storage.title")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightBold
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.storage.description")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        lineHeight: Theme.typeCaptionLineHeight
                        wrapMode: Text.Wrap
                    }
                }

                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.locale.close")
                    onClicked: root.close()
                }
            }
        }

        ScrollView {
            id: storageScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: GenyScrollBar {}

            Item {
                width: storageScroll.availableWidth
                implicitHeight: root.twoColumnLayout ? wideColumns.implicitHeight : compactColumn.implicitHeight

                RowLayout {
                    id: wideColumns
                    visible: root.twoColumnLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    layoutDirection: Theme.uiDirection
                    spacing: root.columnGap

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.space3

                        StorageUsageCard {}
                        AutoDownloadCard { networkId: "wifi" }
                        MediaDataUsageCard {}
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.space3

                        CachePolicyCard {}
                        AutoDownloadCard { networkId: "cellular" }
                        DownloadBehaviorCard {}
                    }
                }

                ColumnLayout {
                    id: compactColumn
                    visible: !root.twoColumnLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: Theme.space3

                    StorageUsageCard {}
                    CachePolicyCard {}
                    AutoDownloadCard { networkId: "wifi" }
                    AutoDownloadCard { networkId: "cellular" }
                    MediaDataUsageCard {}
                    DownloadBehaviorCard {}
                }
            }
        }
    }
}
