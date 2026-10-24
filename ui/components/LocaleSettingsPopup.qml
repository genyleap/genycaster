import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GenyDrawer {
    id: root
    objectName: "settingsPopup"

    property int currentSection: 0
    property bool mobileIndexOpen: true
    property bool showRecoveryExplanation: false
    readonly property bool webAssembly: Qt.platform.os === "wasm"
    // Overlay.overlay measures the actual Qt window on both native desktop
    // and desktop WebAssembly. Popup parent widths may be transient during
    // incubation, incorrectly selecting the phone settings hierarchy.
    readonly property real viewportWidth: Overlay.overlay && Overlay.overlay.width > 0
                                          ? Overlay.overlay.width
                                          : (root.parent ? root.parent.width : 0)
    readonly property bool mobilePresentation: viewportWidth > 0
                                              && viewportWidth < Theme.breakpointMobile
    function scrollToBeginning() {
        // Only explicit Settings entry resets state. Switching controls or
        // returning from a nested detail does not discard unsaved values.
        const pages = [mobileSettingsIndex, appearanceScroll, localeScroll,
                       marketScroll, readSettingsScroll, updatesScroll,
                       recoverySettingsScroll, nodeSettingsScroll]
        for (let i = 0; i < pages.length; ++i) {
            if (pages[i].contentItem)
                pages[i].contentItem.contentY = 0
        }
    }

    function openMobileSection(section) {
        if (section === -1) {
            root.close()
            root.dataStorageRequested()
            return
        }
        if (section === -2) {
            root.close()
            root.signOutConfirmationRequested()
            return
        }
        if (section < 0 || section > 6
                || (section === 5 && !presentationBridge.isAuthenticated)) return
        root.currentSection = section
        root.mobileIndexOpen = false
    }
    function sectionTitle(section) {
        const titles = [
            qsTrId("genycaster.settings.appearance"),
            qsTrId("genycaster.settings.language_region"),
            qsTrId("genycaster.settings.market_pulse"),
            qsTrId("genycaster.settings.content_loading"),
            qsTrId("genycaster.settings.updates"),
            qsTrId("genycaster.setup.recovery_title"),
            qsTrId("genycaster.nodes.title")
        ]
        return titles[Math.max(0, Math.min(titles.length - 1, section))]
    }
    readonly property bool compactLayout: width < 640
    readonly property int contentColumns: width >= 700 ? 2 : 1
    property bool marketInputError: false
    property bool validatorUrlError: false
    property bool customColorError: false

    signal customAccentStudioRequested(string color)
    signal dataStorageRequested()
    signal signOutConfirmationRequested()

    function applyCustomAccentColor(color) {
        const applied = appearanceController.setCustomAccentColor(String(color || ""))
        root.customColorError = !applied
        if (applied)
            customAccentField.text = appearanceController.customAccentColor
        return applied
    }

    bottomSheet: mobilePresentation
    // Scrolling settings must not drag the sheet. Dismiss only by Back/Close,
    // Escape, or a deliberate press in the uncovered backdrop above it.
    interactive: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    showHandle: false
    dimBackground: true
    contentPadding: mobilePresentation ? Theme.mobileGutter : Theme.space4
    width: parent ? (mobilePresentation ? parent.width : Math.min(760, parent.width * 0.92)) : 760
    // Settings is a full-screen navigable destination on phones: never leave
    // a distracting exposed strip above it. Outside-click dismissal still
    // applies whenever the surface has an uncovered backdrop (e.g. desktop).
    height: parent ? parent.height : 820

    onOpened: {
        if (!presentationBridge.isAuthenticated && root.currentSection === 5) {
            root.currentSection = 0
            root.mobileIndexOpen = true
        }
        customAccentField.text = appearanceController.customAccentColor
        root.customColorError = false
        root.showRecoveryExplanation = false
    }
    onClosed: root.mobileIndexOpen = true
    Connections {
        target: presentationBridge
        function onAuthenticationStateChanged() {
            if (!presentationBridge.isAuthenticated && root.currentSection === 5) {
                root.currentSection = 0
                root.mobileIndexOpen = true
            }
        }
    }


    function formatStorageUsage(value) {
        const bytes = Math.max(0, Number(value || 0))
        if (bytes < 1024 * 1024)
            return LocaleFormat.formatNumber(bytes / 1024, 1) + " KiB"
        if (bytes < 1024 * 1024 * 1024)
            return LocaleFormat.formatNumber(bytes / (1024 * 1024), 1) + " MiB"
        return LocaleFormat.formatNumber(bytes / (1024 * 1024 * 1024), 2) + " GiB"
    }

    function indexFor(model, id) {
        for (let index = 0; index < model.length; ++index) {
            if (String(model[index].id) === String(id))
                return index
        }
        return 0
    }

    function marketSnapshotForKey(key) {
        const rows = liveInsights.marketAssets || []
        for (let index = 0; index < rows.length; ++index) {
            if (String(rows[index].key || "") === String(key || ""))
                return rows[index]
        }
        return ({})
    }

    function marketPrice(value) {
        return liveInsights.formatMarketPrice(Number(value || 0))
    }

    function marketChange(value) {
        const number = Number(value || 0)
        if (!Number.isFinite(number)) return "—"
        return (number >= 0 ? "+" : "") + LocaleFormat.formatNumber(number, 2) + "%"
    }

    function fallbackAssetIcon(asset) {
        const symbol = String(asset && asset.symbol ? asset.symbol : "").toLowerCase()
        const network = String(asset && asset.network ? asset.network : "").toLowerCase()
        if (symbol === "btc") return "crypto/bitcoin"
        if (symbol === "eth") return "crypto/ethereum"
        if (network === "base") return "crypto/base"
        return "crypto/ethereum"
    }

    component ReadRangeCard: Superellipse {
        id: rangeCard

        property string titleText: ""
        property string descriptionText: ""
        property string iconGlyph: Theme.iconRefresh
        property int value: 20
        property int fromValue: 1
        property int toValue: 100
        signal valueRequested(int value)

        Layout.fillWidth: true
        implicitHeight: rangeContent.implicitHeight + Theme.space2 * 2
        fillColor: Theme.surfaceMuted
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderSubtle
        curveDepth: Theme.curveContainer

        ColumnLayout {
            id: rangeContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space2
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Superellipse {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    fillColor: Theme.primarySurface
                    curveDepth: Theme.curveCompact
                    Icon { anchors.centerIn: parent; text: rangeCard.iconGlyph; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: rangeCard.titleText
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: rangeCard.descriptionText
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                }

                Superellipse {
                    Layout.preferredWidth: Math.max(48, rangeValueLabel.implicitWidth + Theme.space3)
                    Layout.preferredHeight: 28
                    fillColor: Theme.primarySurface
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.accent
                    curveDepth: 14
                    DirectionAwareLabel {
                        id: rangeValueLabel
                        anchors.centerIn: parent
                        text: LocaleFormat.formatInteger(rangeCard.value)
                        color: Theme.accent
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightBold
                        forceLeftToRight: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Qt.LeftToRight
                spacing: Theme.space2

                Label {
                    text: LocaleFormat.formatInteger(rangeCard.fromValue)
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                }

                GenySlider {
                    id: rangeSlider
                    Layout.fillWidth: true
                    compact: true
                    from: rangeCard.fromValue
                    to: rangeCard.toValue
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    value: rangeCard.value
                    activeColor: Theme.accent
                    onMoved: rangeCard.valueRequested(Math.round(value))
                }

                Label {
                    text: LocaleFormat.formatInteger(rangeCard.toValue)
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
        }
    }

    component ReadToggleCard: Superellipse {
        id: toggleCard

        property string titleText: ""
        property string descriptionText: ""
        property string iconGlyph: Theme.iconSettings
        property bool checked: false
        signal checkedRequested(bool checked)

        Layout.fillWidth: true
        implicitHeight: toggleContent.implicitHeight + Theme.space2 * 2
        fillColor: Theme.surfaceMuted
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderSubtle
        curveDepth: Theme.curveContainer

        RowLayout {
            id: toggleContent
            anchors.fill: parent
            anchors.margins: Theme.space2
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveCompact
                Icon { anchors.centerIn: parent; text: toggleCard.iconGlyph; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: toggleCard.titleText
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: toggleCard.descriptionText
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }

            GenySwitch {
                checked: toggleCard.checked
                onToggled: toggleCard.checkedRequested(checked)
            }
        }
    }

    // Phone navigation mirrors the grouped Settings hierarchy used by
    // mobile clients. Detail screens reuse the actual desktop controls, so
    // there is one preferences model rather than decorative duplicate pages.
    component MobileSettingsGroup: ColumnLayout {
        id: group
        property string heading: ""
        property var entries: []
        spacing: Theme.space2
        Layout.fillWidth: true

        DirectionAwareLabel {
            visible: group.heading.length > 0
            Layout.fillWidth: true
            Layout.topMargin: Theme.space1
            Layout.leftMargin: Theme.space1
            Layout.rightMargin: Theme.space1
            text: group.heading
            color: Theme.inkSubtle
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: groupRows.implicitHeight
            radius: Theme.space4
            color: Theme.surfaceMuted
            border.width: Theme.strokeHairline
            border.color: Theme.borderSubtle
            clip: true

            ColumnLayout {
                id: groupRows
                width: parent.width
                spacing: 0

                Repeater {
                    model: group.entries
                    delegate: Item {
                        id: mobileSettingRow
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(56, Theme.controlHeight + Theme.space2)
                        Accessible.role: Accessible.Button
                        Accessible.name: String(modelData.label || "")

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space3
                            anchors.rightMargin: Theme.space3
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection

                            Icon {
                                text: String(mobileSettingRow.modelData.icon || Theme.iconSettings)
                                preferredSize: Theme.iconSize
                                color: Theme.ink
                                Layout.preferredWidth: 26
                                Layout.alignment: Qt.AlignVCenter
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: String(mobileSettingRow.modelData.label || "")
                                color: Theme.ink
                                font.pixelSize: Theme.typeBodySize
                                font.weight: Theme.weightMedium
                                elide: Text.ElideRight
                            }
                            Icon {
                                text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                color: Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
                                Layout.alignment: Qt.AlignVCenter
                            }
                        }
                        Rectangle {
                            visible: mobileSettingRow.index + 1 < group.entries.length
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: Theme.space3
                            anchors.rightMargin: Theme.space3
                            height: Math.max(1, Theme.strokeHairline)
                            color: Theme.divider
                        }
                        TapHandler {
                            onTapped: root.openMobileSection(Number(mobileSettingRow.modelData.section))
                        }
                    }
                }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: root.mobilePresentation ? Theme.space2 : Theme.space3

        ModalHeader {
            visible: !root.mobilePresentation
            Layout.fillWidth: true
            title: qsTrId("genycaster.settings.title")
            subtitle: root.compactLayout ? "" : qsTrId("genycaster.settings.description")
            onCloseRequested: root.close()
        }

        // Full-height phone hierarchy: a persistent Back control and a
        // single vertically scrolling list. No compressed tabs or scroll trap.
        RowLayout {
            id: mobileSettingsHeader
            objectName: "mobileSettingsHeader"
            visible: root.mobilePresentation
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 58 : 0
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            ActionButton {
                objectName: "mobileSettingsBack"
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.rtl ? Theme.iconArrowRight : Theme.iconArrowLeft
                text: root.mobileIndexOpen ? qsTrId("genycaster.shell.close")
                                           : qsTrId("genycaster.storage.back_to_settings")
                onClicked: {
                    if (root.mobileIndexOpen) root.close()
                    else root.mobileIndexOpen = true
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.mobileIndexOpen ? qsTrId("genycaster.settings.title")
                                           : root.sectionTitle(root.currentSection)
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                color: Theme.ink
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightBold
            }
            Item { Layout.preferredWidth: 48; Layout.preferredHeight: 48 }
        }

        ScrollView {
            id: mobileSettingsIndex
            objectName: "mobileSettingsIndex"
            visible: root.mobilePresentation && root.mobileIndexOpen
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: GenyScrollBar {}

            ColumnLayout {
                width: mobileSettingsIndex.availableWidth
                spacing: Theme.space4

                MobileSettingsGroup {
                    visible: presentationBridge.isAuthenticated
                    heading: ""
                    entries: [
                        { label: qsTrId("genycaster.settings.mobile_recovery_options"), icon: "system/lock", section: 5 }
                    ]
                }
                MobileSettingsGroup {
                    visible: presentationBridge.isAuthenticated
                    heading: qsTrId("genycaster.settings.mobile_account_security")
                    entries: [
                        { label: qsTrId("genycaster.shell.sign_out"), icon: Theme.iconBack, section: -2 }
                    ]
                }
                MobileSettingsGroup {
                    heading: qsTrId("genycaster.settings.mobile_personalization")
                    entries: [
                        { label: qsTrId("genycaster.settings.appearance"), icon: Theme.iconPalette, section: 0 },
                        { label: qsTrId("genycaster.settings.language_region"), icon: Theme.iconLanguage, section: 1 }
                    ]
                }
                MobileSettingsGroup {
                    heading: qsTrId("genycaster.settings.mobile_feeds_content")
                    entries: [
                        { label: qsTrId("genycaster.settings.market_pulse"), icon: Theme.iconChart, section: 2 },
                        { label: qsTrId("genycaster.settings.content_loading"), icon: Theme.iconDownload, section: 3 },
                        { label: qsTrId("genycaster.nodes.title"), icon: Theme.iconCloud, section: 6 }
                    ]
                }
                MobileSettingsGroup {
                    heading: qsTrId("genycaster.settings.mobile_app_storage")
                    entries: [
                        { label: qsTrId("genycaster.storage.title"), icon: Theme.iconCloud, section: -1 },
                        { label: qsTrId("genycaster.settings.updates"), icon: Theme.iconRefresh, section: 4 }
                    ]
                }
                Item { Layout.preferredHeight: Theme.space5 }
            }
        }

        Superellipse {
            visible: !root.mobilePresentation
            Layout.fillWidth: true
            implicitHeight: settingsTabs.implicitHeight + Theme.space1 * 2
            fillColor: Theme.surfaceMuted
            curveDepth: Theme.curveControl

            GridLayout {
                id: settingsTabs
                // Seven destinations: four columns on wide desktop keeps
                // two readable rows; mobile navigates through the index.
                columns: root.compactLayout ? 3 : root.width >= 720 ? 4 : 2
                rowSpacing: Theme.space1
                columnSpacing: Theme.space1
                // Desktop labels get their natural line width across two rows;
                // only truly compact mobile layouts use icon-only navigation.
                uniformCellWidths: !root.compactLayout
                anchors.fill: parent
                anchors.margins: Theme.space1
                layoutDirection: Theme.uiDirection

                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    selected: root.currentSection === 0
                    iconOnly: root.compactLayout
                    Layout.minimumWidth: 0
                    iconGlyph: Theme.iconPalette
                    text: qsTrId("genycaster.settings.appearance")
                    onClicked: root.currentSection = 0
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    selected: root.currentSection === 1
                    iconOnly: root.compactLayout
                    Layout.minimumWidth: 0
                    iconGlyph: Theme.iconLanguage
                    text: qsTrId("genycaster.settings.language_region")
                    onClicked: root.currentSection = 1
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    selected: root.currentSection === 2
                    iconOnly: root.compactLayout
                    Layout.minimumWidth: 0
                    iconGlyph: Theme.iconChart
                    text: qsTrId("genycaster.settings.market_pulse")
                    onClicked: root.currentSection = 2
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    selected: root.currentSection === 3
                    iconOnly: root.compactLayout
                    Layout.minimumWidth: 0
                    iconGlyph: Theme.iconDownload
                    text: qsTrId("genycaster.settings.content_loading")
                    onClicked: root.currentSection = 3
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    selected: root.currentSection === 4
                    iconOnly: root.compactLayout
                    Layout.minimumWidth: 0
                    iconGlyph: Theme.iconRefresh
                    text: qsTrId("genycaster.settings.updates")
                    onClicked: root.currentSection = 4
                }
                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    selected: root.currentSection === 6
                    iconOnly: root.compactLayout
                    Layout.minimumWidth: 0
                    iconGlyph: Theme.iconCloud
                    text: qsTrId("genycaster.nodes.title")
                    onClicked: root.currentSection = 6
                }
                ActionButton {
                    objectName: "settingsRecoveryTab"
                    visible: presentationBridge.isAuthenticated
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    selected: root.currentSection === 5
                    iconOnly: root.compactLayout
                    Layout.minimumWidth: 0
                    iconGlyph: "system/lock"
                    text: qsTrId("genycaster.setup.recovery_title")
                    onClicked: root.currentSection = 5
                }
            }
        }

        StackLayout {
            visible: !root.mobilePresentation || !root.mobileIndexOpen
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentSection

            ScrollView {
                id: appearanceScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: GenyScrollBar {}

                ColumnLayout {
                    width: appearanceScroll.availableWidth
                    spacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.appearance")
                            color: Theme.ink
                            font.pixelSize: Theme.typeTitle2Size
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.appearance_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            wrapMode: Text.Wrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.mode")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        Superellipse {
                            Layout.fillWidth: true
                            implicitHeight: Theme.controlHeight + Theme.space1 * 2
                            fillColor: Theme.surfaceMuted
                            curveDepth: Theme.curveControl
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.space1
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space1
                                ActionButton {
                                    Layout.fillWidth: true
                                    quiet: true
                                    selected: appearanceController.themeMode === "system"
                                    iconGlyph: Theme.iconSystem
                                    text: qsTrId("genycaster.settings.mode_system")
                                    onClicked: appearanceController.setThemeMode("system")
                                }
                                ActionButton {
                                    Layout.fillWidth: true
                                    quiet: true
                                    selected: appearanceController.themeMode === "dark"
                                    iconGlyph: Theme.iconMoon
                                    text: qsTrId("genycaster.settings.mode_dark")
                                    onClicked: appearanceController.setThemeMode("dark")
                                }
                                ActionButton {
                                    Layout.fillWidth: true
                                    quiet: true
                                    selected: appearanceController.themeMode === "light"
                                    iconGlyph: Theme.iconSun
                                    text: qsTrId("genycaster.settings.mode_light")
                                    onClicked: appearanceController.setThemeMode("light")
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                    ColumnLayout {
                        objectName: "textSelectionPreference"
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.text_selection")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.text_selection_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            ActionButton {
                                Layout.fillWidth: true
                                compact: true
                                selected: textSelectionSettings.mode === "genycaster"
                                text: qsTrId("genycaster.settings.selection_genycaster")
                                onClicked: textSelectionSettings.setMode("genycaster")
                            }
                            ActionButton {
                                Layout.fillWidth: true
                                compact: true
                                selected: textSelectionSettings.systemSelectionEnabled
                                enabled: textSelectionSettings.nativeAvailable
                                text: qsTrId("genycaster.settings.selection_system")
                                onClicked: textSelectionSettings.setMode("system")
                            }
                        }
                        DirectionAwareLabel {
                            visible: !textSelectionSettings.nativeAvailable
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.inkMuted
                            text: qsTrId("genycaster.settings.selection_browser_limit")
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.color_theme")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            columns: width >= 560 ? 4 : 2
                            rowSpacing: Theme.space2
                            columnSpacing: Theme.space2

                            Repeater {
                                model: [
                                    { id: "geny", label: qsTrId("genycaster.settings.theme_geny"), color: Theme.swatchGeny },
                                    { id: "ocean", label: qsTrId("genycaster.settings.theme_ocean"), color: Theme.swatchOcean },
                                    { id: "violet", label: qsTrId("genycaster.settings.theme_violet"), color: Theme.swatchViolet },
                                    { id: "ember", label: qsTrId("genycaster.settings.theme_ember"), color: Theme.swatchEmber },
                                    { id: "rose", label: qsTrId("genycaster.settings.theme_rose"), color: Theme.swatchRose },
                                    { id: "mint", label: qsTrId("genycaster.settings.theme_mint"), color: Theme.swatchMint },
                                    { id: "mono", label: qsTrId("genycaster.settings.theme_mono"), color: Theme.swatchMono }
                                ]
                                delegate: Superellipse {
                                    id: themePreset
                                    required property var modelData
                                    readonly property bool selected: appearanceController.colorThemeId === String(modelData.id)
                                    Layout.fillWidth: true
                                    implicitHeight: 50
                                    fillColor: selected ? Theme.surfaceSelected : presetHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                                    borderWidth: selected ? 2 : Theme.strokeHairline
                                    borderColor: selected ? Theme.accent : Theme.borderSubtle
                                    curveDepth: Theme.curveControl

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: Theme.space2
                                        layoutDirection: Theme.uiDirection
                                        spacing: Theme.space2

                                        Superellipse {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            fillColor: themePreset.modelData.color
                                            borderWidth: Theme.strokeHairline
                                            borderColor: Theme.borderStrong
                                            curveDepth: 8
                                            exponent: 3.8
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: themePreset.modelData.label
                                            color: Theme.ink
                                            font.pixelSize: Theme.typeMetaSize
                                            font.weight: themePreset.selected ? Theme.weightDemiBold : Theme.weightMedium
                                            elide: Text.ElideRight
                                        }
                                        Icon {
                                            visible: themePreset.selected
                                            text: Theme.iconCheck
                                            color: Theme.accent
                                            preferredSize: Theme.iconSizeSmall
                                        }
                                    }

                                    HoverHandler { id: presetHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler {
                                        gesturePolicy: TapHandler.ReleaseWithinBounds
                                        onTapped: appearanceController.setColorThemeId(String(themePreset.modelData.id))
                                    }
                                }
                            }
                        }

                        Superellipse {
                            Layout.fillWidth: true
                            implicitHeight: customColorContent.implicitHeight + Theme.space3 * 2
                            fillColor: appearanceController.colorThemeId === "custom" ? Theme.surfaceSelected : Theme.surfaceMuted
                            borderWidth: appearanceController.colorThemeId === "custom" ? 2 : Theme.strokeHairline
                            borderColor: appearanceController.colorThemeId === "custom" ? Theme.accent : Theme.borderSubtle
                            curveDepth: Theme.curveContainer

                            ColumnLayout {
                                id: customColorContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: Theme.space3
                                anchors.rightMargin: Theme.space3
                                spacing: Theme.space2

                                RowLayout {
                                    Layout.fillWidth: true
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space2

                                    Superellipse {
                                        Layout.preferredWidth: 34
                                        Layout.preferredHeight: 34
                                        fillColor: Theme.swatchCustom
                                        borderWidth: Theme.strokeHairline
                                        borderColor: Theme.borderStrong
                                        curveDepth: 10
                                        exponent: 3.8
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.custom_color")
                                            color: Theme.ink
                                            font.pixelSize: Theme.typeMetaSize
                                            font.weight: Theme.weightDemiBold
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: qsTrId("genycaster.settings.custom_color_description")
                                            color: Theme.inkMuted
                                            font.pixelSize: Theme.typeCaptionSize
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }

                                Superellipse {
                                    Layout.fillWidth: true
                                    implicitHeight: 54
                                    fillColor: Theme.primarySurface
                                    borderWidth: Theme.strokeHairline
                                    borderColor: Theme.borderSubtle
                                    curveDepth: Theme.curveControl

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: Theme.space2
                                        spacing: Theme.space2
                                        layoutDirection: Theme.uiDirection

                                        Superellipse {
                                            Layout.preferredWidth: 36
                                            Layout.preferredHeight: 36
                                            fillColor: appearanceController.customAccentColor
                                            borderWidth: Theme.strokeHairline
                                            borderColor: Theme.borderStrong
                                            curveDepth: 11
                                            exponent: 3.8
                                        }
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: String(appearanceController.customAccentColor).toUpperCase()
                                            color: Theme.ink
                                            font.family: Theme.latinFontFamily
                                            font.pixelSize: Theme.typeMetaSize
                                            font.weight: Theme.weightDemiBold
                                        }
                                        ActionButton {
                                            compact: true
                                            quiet: true
                                            iconGlyph: Theme.iconPalette
                                            text: qsTrId("genycaster.settings.custom_color")
                                            onClicked: root.customAccentStudioRequested(appearanceController.customAccentColor)
                                        }
                                    }

                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space2
                                    DirectionAwareTextField {
                                        id: customAccentField
                                        Layout.fillWidth: true
                                        placeholderText: qsTrId("genycaster.settings.custom_color_placeholder")
                                        onTextChanged: root.customColorError = false
                                        onAccepted: {
                                            const applied = appearanceController.setCustomAccentColor(text)
                                            root.customColorError = !applied
                                            if (applied) text = appearanceController.customAccentColor
                                        }
                                    }
                                    ActionButton {
                                        compact: true
                                        prominent: true
                                        text: qsTrId("genycaster.composer.apply")
                                        onClicked: {
                                            const applied = appearanceController.setCustomAccentColor(customAccentField.text)
                                            root.customColorError = !applied
                                            if (applied) customAccentField.text = appearanceController.customAccentColor
                                        }
                                    }
                                }

                                DirectionAwareLabel {
                                    visible: root.customColorError
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.custom_color_invalid")
                                    color: Theme.danger
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.rendering.display")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.scale_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            columns: root.contentColumns
                            rowSpacing: Theme.space2
                            columnSpacing: Theme.space2

                            DisplayScaleControl {
                                title: qsTrId("genycaster.settings.text_size")
                                previewText: "Aa"
                                value: appearanceController.fontScale
                                from: 0.85
                                to: 1.30
                                stepSize: 0.05
                                onValueRequested: value => appearanceController.setFontScale(value)
                            }

                            DisplayScaleControl {
                                title: qsTrId("genycaster.settings.interface_size")
                                iconGlyph: Theme.iconSystem
                                value: appearanceController.interfaceScale
                                from: 0.90
                                to: 1.20
                                stepSize: 0.05
                                onValueRequested: value => appearanceController.setInterfaceScale(value)
                            }
                        }


                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.social_workspace")
                            color: Theme.ink
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.social_workspace_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            columns: root.contentColumns
                            rowSpacing: Theme.space2
                            columnSpacing: Theme.space2

                            PanelWidthControl {
                                title: qsTrId("genycaster.settings.navigation_width")
                                description: qsTrId("genycaster.settings.navigation_width_description")
                                iconGlyph: Theme.iconSystem
                                value: appearanceController.socialNavigationWidth
                                from: 264
                                to: 460
                                stepSize: 8
                                onValueRequested: value => appearanceController.setSocialNavigationWidth(value)
                            }

                            PanelWidthControl {
                                title: qsTrId("genycaster.settings.context_sidebar_width")
                                description: qsTrId("genycaster.settings.context_sidebar_width_description")
                                iconGlyph: Theme.iconProfile
                                value: appearanceController.socialContextWidth
                                from: 304
                                to: 620
                                stepSize: 8
                                onValueRequested: value => appearanceController.setSocialContextWidth(value)
                            }
                        }

                        ActionButton {
                            Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                            compact: true
                            quiet: true
                            iconGlyph: Theme.iconRefresh
                            text: qsTrId("genycaster.settings.reset_social_layout")
                            onClicked: appearanceController.resetSocialLayout()
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.settings.power_save")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightDemiBold
                    }
                    // Power Save is colocated with Reduce Motion so mobile and
                    // desktop share one discoverable animation preference group.
                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: energyRow.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        curveDepth: Theme.curveContainer
                        RowLayout {
                            id: energyRow
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.power_save")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.power_save_description")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                            GenySwitch {
                                checked: Theme.powerSave
                                onToggled: Theme.setPowerSave(checked)
                                Accessible.name: qsTrId("genycaster.settings.power_save")
                            }
                        }
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: avatarMotionRow.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        curveDepth: Theme.curveContainer
                        RowLayout {
                            id: avatarMotionRow
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.animate_avatars")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.animate_avatars_description")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                            GenySwitch {
                                checked: Theme.animateAvatars
                                onToggled: Theme.setAnimateAvatars(checked)
                                Accessible.name: qsTrId("genycaster.settings.animate_avatars")
                            }
                        }
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: reduceMotionRow.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        curveDepth: Theme.curveContainer
                        RowLayout {
                            id: reduceMotionRow
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space3
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.reduce_motion")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.reduce_motion_description")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }
                            GenySwitch {
                                checked: appearanceController.reduceMotion
                                Accessible.name: qsTrId("genycaster.settings.reduce_motion")
                                Accessible.description: qsTrId("genycaster.settings.reduce_motion_description")
                                onToggled: appearanceController.setReduceMotion(checked)
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.miniapp.presentation_title")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.miniapp.presentation_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            columns: width >= 620 ? 4 : 2
                            rowSpacing: Theme.space2
                            columnSpacing: Theme.space2

                            Repeater {
                                model: [
                                    { id: "window", title: qsTrId("genycaster.miniapp.mode_window") },
                                    { id: "bottom", title: qsTrId("genycaster.miniapp.mode_bottom") },
                                    { id: "left", title: qsTrId("genycaster.miniapp.mode_left") },
                                    { id: "right", title: qsTrId("genycaster.miniapp.mode_right") }
                                ]

                                delegate: Superellipse {
                                    id: presentationCard
                                    required property var modelData
                                    readonly property bool selected: miniAppHost.presentationMode === String(modelData.id)
                                    readonly property string modeId: String(modelData.id)
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 146
                                    fillColor: selected ? Theme.primarySurface : Theme.surfaceMuted
                                    borderWidth: selected ? Theme.strokeEmphasis : Theme.strokeHairline
                                    borderColor: selected ? Theme.accent : Theme.borderSubtle
                                    curveDepth: Theme.curveContainer
                                    elevation: selected ? Theme.elevationControl : Theme.elevationNone

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: Theme.space2
                                        spacing: Theme.space2

                                        MiniAppPresentationPreview {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumHeight: 96
                                            mode: presentationCard.modeId
                                            selected: presentationCard.selected
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.space1
                                            layoutDirection: Theme.uiDirection

                                            Superellipse {
                                                Layout.preferredWidth: 8
                                                Layout.preferredHeight: 8
                                                fillColor: presentationCard.selected ? Theme.accent : Theme.borderStrong
                                                borderWidth: 0
                                                curveDepth: 4
                                            }

                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                text: String(presentationCard.modelData.title || "")
                                                color: presentationCard.selected ? Theme.accent : Theme.ink
                                                font.pixelSize: Theme.typeCaptionSize
                                                font.weight: Theme.weightDemiBold
                                                horizontalAlignment: Text.AlignHCenter
                                                elide: Text.ElideRight
                                            }

                                            Item { Layout.preferredWidth: 8 }
                                        }
                                    }

                                    TapHandler {
                                        onTapped: miniAppHost.setPresentationMode(presentationCard.modeId)
                                    }
                                    Accessible.role: Accessible.Button
                                    Accessible.name: String(modelData.title || "")
                                    Accessible.checked: selected
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: Theme.space4 }
                }
            }

            ScrollView {
                id: localeScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: GenyScrollBar {}

                ColumnLayout {
                    width: localeScroll.availableWidth
                    spacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.language_region")
                            color: Theme.ink
                            font.pixelSize: Theme.typeTitle2Size
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.locale.regional_format_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            wrapMode: Text.Wrap
                        }
                    }


                    GridLayout {
                        id: localeGrid
                        Layout.fillWidth: true
                        columns: root.contentColumns
                        layoutDirection: Theme.uiDirection
                        columnSpacing: Theme.space3
                        rowSpacing: Theme.space3

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.language"); color: Theme.ink; font.pixelSize: Theme.typeLabelSize; font.weight: Theme.weightDemiBold }
                            GenyComboBox {
                                Layout.fillWidth: true
                                model: localizationController.availableLanguages
                                textRole: "label"
                                currentIndex: root.indexFor(model, localizationController.languageTag)
                                onActivated: index => localizationController.setLanguageTag(String(model[index].id))
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.regional_format"); color: Theme.ink; font.pixelSize: Theme.typeLabelSize; font.weight: Theme.weightDemiBold }
                            GenyComboBox {
                                Layout.fillWidth: true
                                model: localizationController.availableRegionalLocales
                                textRole: "label"
                                currentIndex: root.indexFor(model, localizationController.regionalLocaleTag)
                                onActivated: index => localizationController.setRegionalLocaleTag(String(model[index].id))
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.calendar"); color: Theme.ink; font.pixelSize: Theme.typeLabelSize; font.weight: Theme.weightDemiBold }
                            GenyComboBox {
                                Layout.fillWidth: true
                                model: localizationController.availableCalendars
                                textRole: "label"
                                currentIndex: root.indexFor(model, localizationController.calendarId)
                                onActivated: index => localizationController.setCalendarId(String(model[index].id))
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.timestamp_display"); color: Theme.ink; font.pixelSize: Theme.typeLabelSize; font.weight: Theme.weightDemiBold }
                            GenyComboBox {
                                Layout.fillWidth: true
                                model: localizationController.availableTimestampDisplayModes
                                textRole: "label"
                                currentIndex: root.indexFor(model, localizationController.timestampDisplayMode)
                                onActivated: index => localizationController.setTimestampDisplayMode(String(model[index].id))
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.locale.timestamp_display_description")
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.Wrap
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.time_zone"); color: Theme.ink; font.pixelSize: Theme.typeLabelSize; font.weight: Theme.weightDemiBold }
                            GenyComboBox {
                                Layout.fillWidth: true
                                popupMaximumWidth: Math.max(680, width * 1.5)
                                model: localizationController.availableTimeZones
                                textRole: "label"
                                currentIndex: root.indexFor(model, localizationController.timeZoneId)
                                onActivated: index => localizationController.setTimeZoneId(String(model[index].id))
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.currency"); color: Theme.ink; font.pixelSize: Theme.typeLabelSize; font.weight: Theme.weightDemiBold }
                            GenyComboBox {
                                Layout.fillWidth: true
                                model: localizationController.availableCurrencies
                                textRole: "label"
                                currentIndex: root.indexFor(model, localizationController.currencyCode)
                                onActivated: index => localizationController.setCurrencyCode(String(model[index].id))
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.additional_currencies"); color: Theme.ink; font.pixelSize: Theme.typeLabelSize; font.weight: Theme.weightDemiBold }
                        DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.additional_currencies_description"); color: Theme.inkMuted; font.pixelSize: Theme.typeCaptionSize; wrapMode: Text.Wrap }
                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            GenyComboBox {
                                id: additionalCurrencyBox
                                Layout.fillWidth: true
                                model: localizationController.availableCurrencies
                                textRole: "label"
                                currentIndex: 0
                            }
                            ActionButton {
                                compact: true
                                text: qsTrId("genycaster.locale.add_currency")
                                enabled: additionalCurrencyBox.currentIndex > 0
                                         && Boolean(additionalCurrencyBox.model[additionalCurrencyBox.currentIndex].additional)
                                         && !localizationController.additionalCurrencyEnabled(String(additionalCurrencyBox.model[additionalCurrencyBox.currentIndex].id))
                                onClicked: {
                                    localizationController.toggleAdditionalCurrencyCode(String(additionalCurrencyBox.model[additionalCurrencyBox.currentIndex].id))
                                    additionalCurrencyBox.currentIndex = 0
                                }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space1
                            Repeater {
                                model: localizationController.additionalCurrencyCodes
                                delegate: ActionButton {
                                    required property string modelData
                                    compact: true
                                    quiet: true
                                    text: modelData
                                    iconGlyph: Theme.iconClose
                                    onClicked: localizationController.toggleAdditionalCurrencyCode(modelData)
                                }
                            }
                        }
                        DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.currency_formatting_only"); color: Theme.inkSubtle; font.pixelSize: Theme.typeCaptionSize; wrapMode: Text.Wrap }
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: previewColumn.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        curveDepth: Theme.curveContainer
                        ColumnLayout {
                            id: previewColumn
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space1
                            DirectionAwareLabel { Layout.fillWidth: true; text: qsTrId("genycaster.locale.preview"); color: Theme.ink; font.pixelSize: Theme.typeMetaSize; font.weight: Theme.weightDemiBold }
                            DirectionAwareLabel { Layout.fillWidth: true; text: LocaleFormat.formatTimestamp(Date.now(), "long"); color: Theme.inkMuted; font.pixelSize: Theme.typeMetaSize; wrapMode: Text.Wrap }
                            DirectionAwareLabel { Layout.fillWidth: true; text: LocaleFormat.formatNumber(1234567.89, 2); color: Theme.inkMuted; font.pixelSize: Theme.typeMetaSize }
                            DirectionAwareLabel { Layout.fillWidth: true; text: LocaleFormat.formatMonetaryUnit(1234.56, localizationController.effectiveCurrencyCode, 2); color: Theme.inkMuted; font.pixelSize: Theme.typeMetaSize }
                        }
                    }

                    Item { Layout.preferredHeight: Theme.space4 }
                }
            }

            ScrollView {
                id: marketScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: GenyScrollBar {}

                ColumnLayout {
                    width: marketScroll.availableWidth
                    spacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.market_pulse")
                            color: Theme.ink
                            font.pixelSize: Theme.typeTitle2Size
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.market_pulse_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            wrapMode: Text.Wrap
                        }
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: sourceRow.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        curveDepth: Theme.curveContainer
                        RowLayout {
                            id: sourceRow
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            Superellipse {
                                Layout.preferredWidth: 9
                                Layout.preferredHeight: 9
                                fillColor: liveInsights.marketRefreshing ? Theme.warning
                                           : liveInsights.providerAvailable ? Theme.positive : Theme.inkDisabled
                                curveDepth: 4.5
                                exponent: 2.01
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: liveInsights.marketStatus
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.Wrap
                            }
                            ActionButton {
                                compact: true
                                quiet: true
                                iconOnly: true
                                iconGlyph: Theme.iconRefresh
                                text: qsTrId("genycaster.profile.refresh")
                                enabled: liveInsights.providerAvailable && !liveInsights.marketRefreshing
                                onClicked: liveInsights.refreshMarket()
                            }
                        }
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: precisionContent.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderSubtle
                        curveDepth: Theme.curveContainer

                        ColumnLayout {
                            id: precisionContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.space3
                            anchors.rightMargin: Theme.space3
                            spacing: Theme.space2

                            RowLayout {
                                Layout.fillWidth: true
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space2
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: qsTrId("genycaster.settings.market_price_precision")
                                        color: Theme.ink
                                        font.pixelSize: Theme.typeLabelSize
                                        font.weight: Theme.weightDemiBold
                                    }
                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: qsTrId("genycaster.settings.market_price_precision_description")
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                        wrapMode: Text.Wrap
                                    }
                                }
                                Label {
                                    LayoutMirroring.enabled: false
                                    text: LocaleFormat.formatInteger(liveInsights.marketPriceDecimals)
                                    color: Theme.accent
                                    font.family: Theme.fontFamilyForText(text)
                                    font.pixelSize: Theme.typeTitle3Size
                                    font.weight: Theme.weightBold
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                layoutDirection: Qt.LeftToRight
                                spacing: Theme.space2

                                Label {
                                    text: "2"
                                    color: Theme.inkSubtle
                                    font.family: Theme.fontFamilyForText(text)
                                    font.pixelSize: Theme.typeCaptionSize
                                }

                                Slider {
                                    id: marketPrecisionSlider
                                    Layout.fillWidth: true
                                    from: 2
                                    to: 12
                                    stepSize: 1
                                    snapMode: Slider.SnapAlways
                                    value: 8
                                    Accessible.name: qsTrId("genycaster.settings.market_price_precision")
                                    Component.onCompleted: value = liveInsights.marketPriceDecimals
                                    onMoved: liveInsights.setMarketPriceDecimals(Math.round(value))

                                    Connections {
                                        target: liveInsights
                                        function onMarketPriceDecimalsChanged() {
                                            if (!marketPrecisionSlider.pressed)
                                                marketPrecisionSlider.value = liveInsights.marketPriceDecimals
                                        }
                                    }

                                    background: Item {
                                        x: marketPrecisionSlider.leftPadding
                                        y: Math.round((marketPrecisionSlider.height - height) / 2)
                                        width: marketPrecisionSlider.availableWidth
                                        height: 12

                                        Superellipse {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width
                                            height: 5
                                            fillColor: Theme.surfaceRaised
                                            borderWidth: Theme.strokeHairline
                                            borderColor: Theme.borderSubtle
                                            curveDepth: 2.5
                                        }

                                        Superellipse {
                                            anchors.left: parent.left
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: Math.max(5, parent.width * marketPrecisionSlider.visualPosition)
                                            height: 5
                                            fillColor: Theme.accent
                                            curveDepth: 2.5
                                        }
                                    }

                                    handle: Superellipse {
                                        x: marketPrecisionSlider.leftPadding
                                           + marketPrecisionSlider.visualPosition
                                             * (marketPrecisionSlider.availableWidth - width)
                                        y: Math.round((marketPrecisionSlider.height - height) / 2)
                                        width: 22
                                        height: 22
                                        fillColor: marketPrecisionSlider.pressed
                                                   ? Theme.primaryFillHover : Theme.primaryFill
                                        borderWidth: 2
                                        borderColor: Theme.surfaceElevated
                                        curveDepth: 11
                                        elevation: Theme.elevationControl
                                        scale: marketPrecisionSlider.pressed ? 1.08 : 1.0
                                        Behavior on scale {
                                            NumberAnimation {
                                                duration: Theme.reduceMotion ? 0 : Theme.motionFast
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                }

                                Label {
                                    text: "12"
                                    color: Theme.inkSubtle
                                    font.family: Theme.fontFamilyForText(text)
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }

                            Superellipse {
                                Layout.fillWidth: true
                                implicitHeight: 42
                                fillColor: Theme.surfaceRaised
                                borderWidth: Theme.strokeHairline
                                borderColor: Theme.borderSubtle
                                curveDepth: Theme.curveCompact

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.space2
                                    anchors.rightMargin: Theme.space2
                                    spacing: Theme.space2
                                    layoutDirection: Theme.uiDirection

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: root.marketPrice(0.000000123456)
                                        color: Theme.ink
                                        font.pixelSize: Theme.typeLabelSize
                                        font.weight: Theme.weightDemiBold
                                    }

                                    DirectionAwareLabel {
                                        text: LocaleFormat.formatInteger(
                                                  liveInsights.marketPriceDecimals)
                                        color: Theme.accent
                                        font.pixelSize: Theme.typeCaptionSize
                                        font.weight: Theme.weightBold
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.market_assets")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.market_assets_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }

                        Repeater {
                            model: liveInsights.marketCatalog
                            delegate: Superellipse {
                                id: marketAssetDelegate
                                required property var modelData
                                required property int index
                                readonly property var quote: root.marketSnapshotForKey(modelData.key)
                                readonly property string logoSource: String(quote.imageSource || modelData.imageSource || "")
                                readonly property bool quoted: Boolean(quote.available)
                                Layout.fillWidth: true
                                implicitHeight: assetRow.implicitHeight + Theme.space2 * 2
                                fillColor: Theme.surfaceRaised
                                borderWidth: Theme.strokeHairline
                                borderColor: Theme.borderSubtle
                                curveDepth: Theme.curveControl

                                RowLayout {
                                    id: assetRow
                                    anchors.fill: parent
                                    anchors.margins: Theme.space2
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space2

                                    MarketAssetIcon {
                                        Layout.preferredWidth: 46
                                        Layout.preferredHeight: 46
                                        asset: ({
                                            symbol: modelData.symbol,
                                            name: modelData.name,
                                            imageSource: marketAssetDelegate.logoSource
                                        })
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 1
                                        DirectionAwareLabel {
                                            Layout.fillWidth: true
                                            text: String(modelData.name || modelData.symbol || "Token")
                                            color: Theme.ink
                                            font.pixelSize: Theme.typeMetaSize
                                            font.weight: Theme.weightDemiBold
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            LayoutMirroring.enabled: false
                                            text: String(modelData.symbol || "TOKEN")
                                                  + (String(modelData.network || "").length > 0 ? " · " + String(modelData.network).toUpperCase() : "")
                                                  + (String(modelData.address || "").length > 0 ? " · " + String(modelData.address) : "")
                                            color: Theme.inkSubtle
                                            font.family: Theme.latinFontFamily
                                            font.pixelSize: Theme.typeCaptionSize
                                            horizontalAlignment: Text.AlignLeft
                                            elide: Text.ElideMiddle
                                        }
                                    }

                                    ColumnLayout {
                                        visible: marketAssetDelegate.quoted
                                        Layout.preferredWidth: 104
                                        spacing: 0
                                        Label {
                                            Layout.fillWidth: true
                                            LayoutMirroring.enabled: false
                                            text: root.marketPrice(marketAssetDelegate.quote.priceUsd)
                                            color: Theme.ink
                                            font.family: Theme.fontFamilyForText(text)
                                            font.pixelSize: Theme.typeCaptionSize
                                            font.weight: Theme.weightDemiBold
                                            horizontalAlignment: Text.AlignRight
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            LayoutMirroring.enabled: false
                                            text: root.marketChange(marketAssetDelegate.quote.change24h)
                                            color: Number(marketAssetDelegate.quote.change24h || 0) >= 0 ? Theme.positive : Theme.negative
                                            font.family: Theme.fontFamilyForText(text)
                                            font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }

                                    ColumnLayout {
                                        spacing: 0
                                        ActionButton {
                                            compact: true
                                            quiet: true
                                            iconOnly: true
                                            iconGlyph: Theme.iconChevronUp
                                            enabled: index > 0
                                            text: qsTrId("genycaster.settings.move_asset_up")
                                            onClicked: liveInsights.moveAsset(String(modelData.key), -1)
                                        }
                                        ActionButton {
                                            compact: true
                                            quiet: true
                                            iconOnly: true
                                            iconGlyph: Theme.iconChevronDown
                                            enabled: index + 1 < liveInsights.marketCatalog.length
                                            text: qsTrId("genycaster.settings.move_asset_down")
                                            onClicked: liveInsights.moveAsset(String(modelData.key), 1)
                                        }
                                    }

                                    ActionButton {
                                        visible: Boolean(modelData.custom)
                                        compact: true
                                        quiet: true
                                        iconOnly: true
                                        iconGlyph: Theme.iconTrash
                                        text: qsTrId("genycaster.settings.remove_market_asset")
                                        onClicked: liveInsights.removeCustomToken(String(modelData.key))
                                    }

                                    GenySwitch {
                                        checked: Boolean(modelData.enabled)
                                        Accessible.name: String(modelData.name || modelData.symbol || "Token")
                                        onToggled: liveInsights.setAssetEnabled(String(modelData.key), checked)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.add_market_token")
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.add_market_token_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            GenyComboBox {
                                id: tokenNetworkBox
                                Layout.preferredWidth: Math.min(180, marketScroll.availableWidth * 0.30)
                                model: [{ id: "base", label: "Base" }, { id: "eth", label: "Ethereum" }]
                                textRole: "label"
                            }
                            DirectionAwareTextField {
                                id: tokenAddressField
                                Layout.fillWidth: true
                                placeholderText: qsTrId("genycaster.settings.contract_address_placeholder")
                                onTextChanged: root.marketInputError = false
                            }
                            ActionButton {
                                compact: true
                                iconGlyph: Theme.iconPlus
                                text: qsTrId("genycaster.settings.add_market_token")
                                onClicked: {
                                    const network = String(tokenNetworkBox.model[tokenNetworkBox.currentIndex].id)
                                    const added = liveInsights.addCustomToken(network, tokenAddressField.text)
                                    root.marketInputError = !added
                                    if (added)
                                        tokenAddressField.clear()
                                }
                            }
                        }
                        DirectionAwareLabel {
                            visible: root.marketInputError
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.invalid_market_token")
                            color: Theme.danger
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                    }

                    Item { Layout.preferredHeight: Theme.space4 }
                }
            }

            ScrollView {
                id: readSettingsScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: GenyScrollBar {}

                ColumnLayout {
                    width: readSettingsScroll.availableWidth
                    spacing: Theme.space3

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.content_loading")
                            color: Theme.ink
                            font.pixelSize: Theme.typeTitle3Size
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.content_loading_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            lineHeight: Theme.typeMetaLineHeight
                            wrapMode: Text.Wrap
                        }
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        iconGlyph: Theme.iconCloud
                        alignStart: true
                        text: qsTrId("genycaster.nodes.title")
                        onClicked: root.currentSection = 6
                    }

                    Superellipse {
                        Layout.fillWidth: true
                        implicitHeight: dataStorageEntryContent.implicitHeight + Theme.space2 * 2
                        fillColor: dataStorageEntryHover.hovered ? Theme.primarySurface : Theme.surfaceMuted
                        borderWidth: Theme.strokeHairline
                        borderColor: dataStorageEntryHover.hovered ? Theme.borderStrong : Theme.borderMuted
                        curveDepth: Theme.curveContainer

                        RowLayout {
                            id: dataStorageEntryContent
                            anchors.fill: parent
                            anchors.margins: Theme.space2
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2

                            Superellipse {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                fillColor: Theme.surfaceElevated
                                curveDepth: Theme.curveCompact
                                Icon { anchors.centerIn: parent; text: Theme.iconDownload; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.storage.title")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.storage.settings_entry_description")
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                }
                            }

                            ColumnLayout {
                                spacing: 0
                                Layout.alignment: Qt.AlignVCenter
                                DirectionAwareLabel {
                                    text: root.formatStorageUsage(dataStorage.cacheUsageBytes)
                                    color: Theme.accent
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightBold
                                    forceLeftToRight: true
                                }
                                Icon {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                    color: Theme.inkSubtle
                                    preferredSize: Theme.iconSizeSmall
                                }
                            }
                        }

                        HoverHandler { id: dataStorageEntryHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: root.dataStorageRequested()
                        }
                    }

                    ReadRangeCard {
                        titleText: qsTrId("genycaster.settings.posts_per_refresh")
                        descriptionText: qsTrId("genycaster.settings.posts_per_refresh_description")
                        iconGlyph: Theme.iconRefresh
                        value: presentationBridge.feedPageSize
                        fromValue: 1
                        toValue: 100
                        onValueRequested: value => presentationBridge.setFeedPageSize(value)
                    }

                    ReadRangeCard {
                        titleText: qsTrId("genycaster.settings.context_hydration_limit")
                        descriptionText: qsTrId("genycaster.settings.context_hydration_limit_description")
                        iconGlyph: Theme.iconComment
                        value: presentationBridge.contextHydrationLimit
                        fromValue: 1
                        toValue: 100
                        onValueRequested: value => presentationBridge.setContextHydrationLimit(value)
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

                    ReadToggleCard {
                        titleText: qsTrId("genycaster.settings.buffer_new_posts")
                        descriptionText: qsTrId("genycaster.settings.buffer_new_posts_description")
                        iconGlyph: Theme.iconRefresh
                        checked: presentationBridge.bufferNewPosts
                        onCheckedRequested: checked => presentationBridge.setBufferNewPosts(checked)
                    }

                    ReadToggleCard {
                        titleText: qsTrId("genycaster.settings.link_preview_prefetch")
                        descriptionText: qsTrId("genycaster.settings.link_preview_prefetch_description")
                        iconGlyph: Theme.iconLink
                        checked: presentationBridge.linkPreviewPrefetchEnabled
                        onCheckedRequested: checked => presentationBridge.setLinkPreviewPrefetchEnabled(checked)
                    }

                }
            }

            ScrollView {
                id: updatesScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: GenyScrollBar {}

                ColumnLayout {
                    width: updatesScroll.availableWidth
                    spacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.app_updates")
                            color: Theme.ink
                            font.pixelSize: Theme.typeTitle2Size
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            objectName: "settingsBetaReleaseLabel"
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.beta_release_short")
                            color: Theme.accent
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.settings.beta_program_info")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.WordWrap
                        }
                    }

                    DirectionAwareLabel {
                        objectName: "wasmUpdateDescription"
                        visible: root.webAssembly
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.settings.wasm_update_description")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                    }

                    Superellipse {
                        objectName: "wasmUpdatePanel"
                        visible: root.webAssembly
                        Layout.fillWidth: true
                        implicitHeight: webUpdaterColumn.implicitHeight + Theme.space4 * 2
                        fillColor: Theme.surfaceMuted
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderSubtle
                        curveDepth: Theme.curveContainer

                        ColumnLayout {
                            id: webUpdaterColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Theme.space4
                            spacing: Theme.space3

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: updateController.webRefreshing
                                      ? qsTrId("genycaster.settings.wasm_updating")
                                      : qsTrId("genycaster.settings.wasm_refresh_hint")
                                font.pixelSize: Theme.typeBodySize
                                color: Theme.ink
                                wrapMode: Text.WordWrap
                            }
                            DirectionAwareLabel {
                                visible: updateController.error.length > 0
                                Layout.fillWidth: true
                                text: updateController.error
                                color: Theme.warning
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.WordWrap
                            }
                            ActionButton {
                                objectName: "wasmRefreshUpdateButton"
                                Layout.fillWidth: true
                                prominent: true
                                iconGlyph: Theme.iconRefresh
                                enabled: !updateController.webRefreshing
                                text: updateController.webRefreshing
                                      ? qsTrId("genycaster.settings.wasm_updating")
                                      : qsTrId("genycaster.settings.wasm_refresh_now")
                                onClicked: updateController.refreshWebApp()
                            }
                            GenySpinner {
                                visible: updateController.webRefreshing
                                running: visible
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    ReadToggleCard {
                        visible: !root.webAssembly
                        titleText: qsTrId("genycaster.settings.check_for_updates_automatically")
                        descriptionText: qsTrId("genycaster.settings.look_for_published_genycaster_releases_once_per_day_no")
                        iconGlyph: Theme.iconRefresh
                        checked: updateController.autoCheckEnabled
                        onCheckedRequested: checked => updateController.setAutoCheckEnabled(checked)
                    }

                    Superellipse {
                        visible: !root.webAssembly
                        Layout.fillWidth: true
                        implicitHeight: updaterColumn.implicitHeight + Theme.space3 * 2
                        fillColor: Theme.surfaceMuted
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderSubtle
                        curveDepth: Theme.curveContainer

                        ColumnLayout {
                            id: updaterColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Theme.space3
                            spacing: Theme.space2

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: updateController.status
                                color: updateController.error.length > 0 ? Theme.warning : Theme.ink
                                font.pixelSize: Theme.typeMetaSize
                                wrapMode: Text.WordWrap
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                visible: updateController.latestVersion.length > 0
                                text: qsTrId("genycaster.settings.latest_release_1").arg(updateController.latestVersion)
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                visible: updateController.error.length > 0
                                text: updateController.error
                                color: Theme.warning
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space2
                                GenySpinner {
                                    visible: updateController.checking || updateController.downloading
                                    running: visible
                                }
                                DirectionAwareLabel {
                                    visible: updateController.downloading
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.settings.download_1").arg(Math.round(updateController.downloadProgress * 100))
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                                Item { Layout.fillWidth: true }
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: updateController.checking ? qsTrId("genycaster.settings.checking") : qsTrId("genycaster.settings.check_now")
                                enabled: !updateController.checking && !updateController.downloading
                                onClicked: updateController.checkForUpdates()
                            }
                            ActionButton {
                                Layout.fillWidth: true
                                visible: updateController.downloadSupported && updateController.canDownload
                                enabled: !updateController.checking && !updateController.downloading
                                prominent: true
                                text: updateController.downloadedFilePath.length > 0
                                      ? qsTrId("genycaster.settings.open_verified_installer")
                                      : qsTrId("genycaster.settings.download_verified_update")
                                onClicked: {
                                    if (updateController.downloadedFilePath.length > 0)
                                        updateController.openDownloadedUpdate()
                                    else
                                        updateController.downloadUpdate()
                                }
                            }
                            ActionButton {
                                Layout.fillWidth: true
                                quiet: true
                                text: qsTrId("genycaster.settings.release_notes_and_downloads")
                                onClicked: updateController.openReleasePage()
                            }
                        }
                    }
                }
            }

            ScrollView {
                id: recoverySettingsScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: GenyScrollBar {}

                ColumnLayout {
                    width: recoverySettingsScroll.availableWidth
                    spacing: Theme.space4

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.recovery_title")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle2Size
                        font.weight: Theme.weightBold
                    }
                    DirectionAwareLabel {
                        visible: !presentationBridge.writeReady
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.recovery_privacy")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                    }
                    DirectionAwareLabel {
                        visible: presentationBridge.signerErrorText.length > 0
                        Layout.fillWidth: true
                        text: presentationBridge.signerErrorText
                        color: Theme.warning
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "settingsRecoveryImport"
                        Layout.fillWidth: true
                        prominent: true
                        enabled: presentationBridge.isAuthenticated
                                 && !presentationBridge.signerBusy
                        visible: presentationBridge.isAuthenticated && !presentationBridge.writeReady
                        text: qsTrId("genycaster.shell.enable_publishing_with_recovery_phrase")
                        onClicked: {
                            root.close()
                            presentationBridge.provisionSigner()
                        }
                    }
                    DirectionAwareLabel {
                        visible: presentationBridge.isAuthenticated && presentationBridge.writeReady
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.recovery_signer_ready")
                        color: Theme.positive
                        font.pixelSize: Theme.typeLabelSize
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "settingsRecoveryExplanation"
                        visible: presentationBridge.isAuthenticated
                        Layout.fillWidth: true
                        quiet: true
                        elevated: false
                        iconGlyph: Theme.iconInfo
                        text: qsTrId("genycaster.setup.recovery_why_no_words")
                        onClicked: root.showRecoveryExplanation = !root.showRecoveryExplanation
                    }
                    DirectionAwareLabel {
                        visible: root.showRecoveryExplanation
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.setup.recovery_phrase_unavailable")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.WordWrap
                    }
                    ActionButton {
                        objectName: "settingsRecoveryRevoke"
                        visible: presentationBridge.isAuthenticated
                        Layout.fillWidth: true
                        destructive: true
                        text: qsTrId("genycaster.shell.revoke_signer")
                        enabled: presentationBridge.writeReady && !presentationBridge.signerBusy
                        onClicked: presentationBridge.revokeSigner()
                    }
                    ActionButton {
                        objectName: "settingsRecoverySignOut"
                        visible: presentationBridge.isAuthenticated
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.shell.sign_out")
                        onClicked: {
                            root.close()
                            root.signOutConfirmationRequested()
                        }
                    }
                }
            }
            ScrollView {
                id: nodeSettingsScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: GenyScrollBar {}

                NodeManagementPanel {
                    width: nodeSettingsScroll.availableWidth
                }
            }
        }

        Superellipse {
            visible: root.currentSection === 3
            Layout.fillWidth: true
            implicitHeight: readApiNoteRow.implicitHeight + Theme.space2 * 2
            fillColor: Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveCompact

            RowLayout {
                id: readApiNoteRow
                anchors.fill: parent
                anchors.margins: Theme.space2
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                Superellipse {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    fillColor: Theme.primarySurface
                    curveDepth: Theme.curveTiny
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconInfo
                        color: Theme.accent
                        preferredSize: Theme.typeCaptionSize
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.settings.read_api_note")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    lineHeight: Theme.typeCaptionLineHeight
                    wrapMode: Text.Wrap
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            ActionButton {
                compact: true
                quiet: true
                text: qsTrId("genycaster.settings.reset_all")
                onClicked: {
                    appearanceController.resetAppearance()
                    localizationController.resetToSystemDefaults()
                    liveInsights.setMarketPriceDecimals(8)
                    presentationBridge.setFeedPageSize(20)
                    presentationBridge.setContextHydrationLimit(40)
                    presentationBridge.setLinkPreviewPrefetchEnabled(true)
                    presentationBridge.setInlineYouTubeEnabled(true)
                    presentationBridge.setBufferNewPosts(true)
                }
            }
            Item { Layout.fillWidth: true }
            ActionButton {
                prominent: true
                text: qsTrId("genycaster.locale.done")
                onClicked: root.close()
            }
        }
    }
}
