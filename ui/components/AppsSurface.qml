import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool compact: false
    property string section: "farcaster"
    property string farcasterSection: "my"
    property string explorerMode: "all"
    readonly property var visibleApps: root.farcasterSection === "my"
                                       ? miniAppCatalog.myApps
                                       : miniAppCatalog.explorerApps

    signal authenticationRequested()

    function refreshExplorer() {
        if (root.explorerMode === "forYou") {
            const fid = Number(presentationBridge.accountFid || 0)
            miniAppCatalog.refreshRelevant(fid)
        } else {
            miniAppCatalog.refresh()
        }
    }

    onFarcasterSectionChanged: {
        if (root.farcasterSection === "explorer"
                && miniAppCatalog.explorerApps.length === 0
                && !miniAppCatalog.busy) {
            Qt.callLater(() => root.refreshExplorer())
        }
    }

    onExplorerModeChanged: {
        if (root.farcasterSection === "explorer" && !miniAppCatalog.busy)
            Qt.callLater(() => root.refreshExplorer())
    }

    function activateDestination(destination) {
        if (destination === "native") {
            root.section = "native"
            return
        }
        root.section = "farcaster"
        root.farcasterSection = destination === "explorer" ? "explorer" : "my"
    }

    function openApp(app) {
        const url = String(app && app.homeUrl ? app.homeUrl : "")
        const name = String(app && app.name ? app.name : "")
        if (url.length === 0)
            return
        if (Number(presentationBridge.accountFid || 0) <= 0) {
            root.authenticationRequested()
            return
        }
        miniAppHost.openWithMetadata(url, name, String(app && app.iconSource ? app.iconSource : ""))
    }

    Flickable {
        id: appsScroller
        anchors.fill: parent
        clip: true
        interactive: root.compact
        boundsBehavior: root.compact ? Flickable.DragOverBounds : Flickable.StopAtBounds
        boundsMovement: root.compact ? Flickable.FollowBoundsBehavior : Flickable.StopAtBounds
        contentWidth: width
        readonly property real pageInset: root.compact ? Theme.mobileGutter : Theme.space4
        contentHeight: root.compact ? pageColumn.implicitHeight + pageInset * 2 : height

        ScrollBar.vertical: GenyScrollBar {
            policy: ScrollBar.AlwaysOff
        }

        ColumnLayout {
            id: pageColumn
            x: appsScroller.pageInset
            y: appsScroller.pageInset
            width: Math.max(1, appsScroller.width - appsScroller.pageInset * 2)
            height: root.compact ? Math.max(1, implicitHeight)
                                 : Math.max(1, appsScroller.height - appsScroller.pageInset * 2)
            spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.apps.title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle1Size
                    font.weight: Theme.weightBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.apps.description")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    wrapMode: Text.Wrap
                }
            }

            ActionButton {
                visible: root.section === "farcaster" && root.farcasterSection === "explorer"
                compact: true
                quiet: true
                iconOnly: root.compact
                iconGlyph: Theme.iconRefresh
                text: miniAppCatalog.busy ? qsTrId("genycaster.feed_header.refreshing")
                                          : qsTrId("genycaster.feed_header.refresh")
                enabled: !miniAppCatalog.busy
                onClicked: root.refreshExplorer()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 178 : 64
            columns: root.compact ? 1 : 3
            rowSpacing: Theme.space2
            columnSpacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: [
                    { id: "my", icon: Theme.iconBookmark, title: qsTrId("genycaster.apps.my_miniapps") },
                    { id: "explorer", icon: Theme.iconCompass, title: qsTrId("genycaster.side_nav.explorer") },
                    { id: "native", icon: Theme.iconLeaf, title: qsTrId("genycaster.apps.native") }
                ]

                delegate: Superellipse {
                    id: destinationCard
                    required property var modelData
                    readonly property string destinationId: String(modelData.id)
                    readonly property bool selected: destinationId === "native"
                                                     ? root.section === "native"
                                                     : root.section === "farcaster"
                                                       && root.farcasterSection === destinationId
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compact ? 54 : 64
                    fillColor: selected ? Theme.primarySurface : Theme.surfaceMuted
                    borderWidth: selected ? Theme.strokeEmphasis : Theme.strokeHairline
                    borderColor: selected ? Theme.accent : Theme.borderSubtle
                    curveDepth: Theme.curveContainer

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection

                        Superellipse {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            fillColor: destinationCard.selected ? Theme.accent : Theme.surfaceRaised
                            curveDepth: Theme.curveControl
                            Icon {
                                anchors.centerIn: parent
                                text: String(destinationCard.modelData.icon || "")
                                color: destinationCard.selected ? Theme.accentInk : Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
                            }
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: String(destinationCard.modelData.title || "")
                            color: destinationCard.selected ? Theme.accent : Theme.ink
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }

                        Icon {
                            visible: destinationCard.selected
                            text: Theme.iconCheck
                            color: Theme.accent
                            preferredSize: Theme.iconSizeSmall
                        }
                    }

                    TapHandler { onTapped: root.activateDestination(destinationCard.destinationId) }
                    Accessible.role: Accessible.Button
                    Accessible.name: String(modelData.title || "")
                    Accessible.checked: selected
                }
            }
        }

        Item {
            id: sectionHost
            Layout.fillWidth: true
            Layout.fillHeight: !root.compact
            Layout.preferredHeight: root.compact
                                    ? (root.section === "farcaster"
                                       ? Math.max(320, farcasterColumn.implicitHeight)
                                       : 420)
                                    : 0

            ComingSoonSurface {
                anchors.fill: parent
                visible: root.section === "native"
                iconGlyph: Theme.iconLeaf
                title: qsTrId("genycaster.apps.native")
                description: qsTrId("genycaster.apps.native_description")
            }

            ColumnLayout {
                id: farcasterColumn
                anchors.fill: parent
                visible: root.section === "farcaster"
                spacing: Theme.space2

                ColumnLayout {
                    visible: root.farcasterSection === "explorer" && root.compact
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    GenyComboBox {
                        Layout.fillWidth: true
                        textRole: "title"
                        valueRole: "id"
                        model: [
                            { id: "all", title: qsTrId("genycaster.apps.all_apps") },
                            { id: "forYou", title: qsTrId("genycaster.apps.for_you") }
                        ]
                        currentIndex: root.explorerMode === "forYou" ? 1 : 0
                        onActivated: root.explorerMode = String(currentValue)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection

                        DirectionAwareTextField {
                            id: miniAppSearchMobile
                            Layout.fillWidth: true
                            placeholderText: qsTrId("genycaster.apps.search_placeholder")
                            text: miniAppCatalog.query
                            selectByMouse: true
                            onAccepted: miniAppCatalog.search(text)
                        }

                        ActionButton {
                            compact: true
                            iconOnly: true
                            iconGlyph: Theme.iconSearch
                            text: qsTrId("genycaster.feed_header.universal_search")
                            enabled: !miniAppCatalog.busy
                            onClicked: miniAppCatalog.search(miniAppSearchMobile.text)
                        }

                        ActionButton {
                            visible: root.explorerMode === "all" && Number(presentationBridge.accountFid || 0) > 0
                            compact: true
                            quiet: true
                            iconOnly: true
                            iconGlyph: Theme.iconProfile
                            text: qsTrId("genycaster.apps.my_fid")
                            enabled: !miniAppCatalog.busy
                            onClicked: miniAppCatalog.searchByFid(Number(presentationBridge.accountFid || 0))
                        }
                    }
                }

                GridLayout {
                    visible: root.farcasterSection === "explorer" && !root.compact
                    Layout.fillWidth: true
                    columns: 4
                    rowSpacing: Theme.space2
                    columnSpacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    GenyComboBox {
                        Layout.preferredWidth: 168
                        textRole: "title"
                        valueRole: "id"
                        model: [
                            { id: "all", title: qsTrId("genycaster.apps.all_apps") },
                            { id: "forYou", title: qsTrId("genycaster.apps.for_you") }
                        ]
                        currentIndex: root.explorerMode === "forYou" ? 1 : 0
                        onActivated: root.explorerMode = String(currentValue)
                    }

                    DirectionAwareTextField {
                        id: miniAppSearch
                        Layout.fillWidth: true
                        placeholderText: qsTrId("genycaster.apps.search_placeholder")
                        text: miniAppCatalog.query
                        selectByMouse: true
                        onAccepted: miniAppCatalog.search(text)
                    }

                    ActionButton {
                        compact: true
                        iconGlyph: Theme.iconSearch
                        text: qsTrId("genycaster.feed_header.universal_search")
                        enabled: !miniAppCatalog.busy
                        onClicked: miniAppCatalog.search(miniAppSearch.text)
                    }

                    ActionButton {
                        visible: root.explorerMode === "all" && Number(presentationBridge.accountFid || 0) > 0
                        compact: true
                        quiet: true
                        iconGlyph: Theme.iconProfile
                        text: qsTrId("genycaster.apps.my_fid")
                        enabled: !miniAppCatalog.busy
                        onClicked: miniAppCatalog.searchByFid(Number(presentationBridge.accountFid || 0))
                    }
                }

                DirectionAwareLabel {
                    visible: root.farcasterSection === "explorer"
                             && root.explorerMode === "forYou"
                             && !miniAppCatalog.personalizedDiscoveryAvailable
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.apps.personalization_requires_provider")
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    visible: root.farcasterSection === "explorer"
                             && (miniAppCatalog.providerLabel.length > 0 || miniAppCatalog.busy)
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 22 : 0
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    GenySpinner {
                        visible: miniAppCatalog.busy
                        running: visible
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                    Icon {
                        visible: !miniAppCatalog.busy
                        text: Theme.iconCloud
                        color: Theme.inkSubtle
                        preferredSize: Theme.iconSizeSmall
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: miniAppCatalog.busy ? qsTrId("genycaster.apps.discovering")
                                                  : miniAppCatalog.providerLabel
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }

                Superellipse {
                    visible: miniAppCatalog.errorText.length > 0 && root.farcasterSection === "explorer"
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? discoveryError.implicitHeight + Theme.space4 : 0
                    fillColor: Theme.warningSurface
                    borderWidth: 1
                    borderColor: Theme.warning
                    curveDepth: Theme.curveControl

                    DirectionAwareLabel {
                        id: discoveryError
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        text: miniAppCatalog.errorText
                        color: Theme.ink
                        font.pixelSize: Theme.typeMetaSize
                        wrapMode: Text.Wrap
                    }
                }

                Item {
                    id: contentViewport
                    Layout.fillWidth: true
                    Layout.fillHeight: !root.compact
                    Layout.preferredHeight: root.compact
                                            ? Math.max(280, appGrid.contentHeight)
                                            : 0

                    GridLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        visible: miniAppCatalog.busy && root.visibleApps.length === 0
                        layoutDirection: Theme.uiDirection
                        columns: root.compact || width < 660 ? 1 : (width < 1040 ? 2 : 3)
                        columnSpacing: Theme.space2
                        rowSpacing: Theme.space2

                        Repeater {
                            model: parent.columns
                            delegate: Superellipse {
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 190
                                fillColor: Theme.surfaceMuted
                                borderWidth: 1
                                borderColor: Theme.borderMuted
                                curveDepth: Theme.curveContainer
                                opacity: 0.72

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: Theme.space3
                                    spacing: Theme.space3
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2
                                        layoutDirection: Theme.uiDirection
                                        Superellipse { Layout.preferredWidth: 44; Layout.preferredHeight: 44; fillColor: Theme.primarySurface; curveDepth: Theme.curveControl }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.space1
                                            Superellipse { Layout.fillWidth: true; Layout.preferredHeight: 10; fillColor: Theme.borderMuted; curveDepth: 5 }
                                            Superellipse { Layout.preferredWidth: parent.width * 0.56; Layout.preferredHeight: 8; fillColor: Theme.borderSubtle; curveDepth: 4 }
                                        }
                                    }
                                    Superellipse { Layout.fillWidth: true; Layout.preferredHeight: 9; fillColor: Theme.borderSubtle; curveDepth: 4 }
                                    Superellipse { Layout.preferredWidth: parent.width * 0.72; Layout.preferredHeight: 9; fillColor: Theme.borderSubtle; curveDepth: 4 }
                                    Item { Layout.fillHeight: true }
                                    Superellipse { Layout.fillWidth: true; Layout.preferredHeight: Theme.compactControlHeight; fillColor: Theme.primarySurface; curveDepth: Theme.compactControlHeight / 2 }
                                }
                            }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: root.visibleApps.length === 0 && !miniAppCatalog.busy

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - Theme.space6, 420)
                            spacing: Theme.space2
                            Superellipse {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 52
                                Layout.preferredHeight: 52
                                fillColor: Theme.primarySurface
                                curveDepth: Theme.curveControl
                                Icon { anchors.centerIn: parent; text: Theme.iconApps; color: Theme.accent; preferredSize: Theme.typeTitle2Size }
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.farcasterSection === "my" ? qsTrId("genycaster.apps.my_empty") : qsTrId("genycaster.apps.explorer_empty")
                                color: Theme.ink
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightDemiBold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.farcasterSection === "my" ? qsTrId("genycaster.apps.my_empty_description") : qsTrId("genycaster.apps.explorer_empty_description")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeMetaSize
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }
                            ActionButton {
                                visible: root.farcasterSection === "my"
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTrId("genycaster.side_nav.explorer")
                                iconGlyph: Theme.iconExplore
                                prominent: true
                                onClicked: root.farcasterSection = "explorer"
                            }
                        }
                    }

                            GridView {
                                id: appGrid
                                anchors.fill: parent
                                visible: root.visibleApps.length > 0
                                clip: !root.compact
                                interactive: !root.compact
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.visibleApps
                                cacheBuffer: Math.min(height, 480)

                                readonly property int targetColumns: root.compact || width < 660 ? 1 : (width < 1040 ? 2 : 3)
                                cellWidth: Math.max(1, width / Math.max(1, targetColumns))
                                cellHeight: 236 + Theme.space3

                                ScrollBar.vertical: GenyScrollBar {
                                    policy: root.compact ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
                                }

                                delegate: Item {
                                    id: cardCell
                                    required property var modelData
                                    width: appGrid.cellWidth
                                    height: appGrid.cellHeight

                                    readonly property string appUrl: String(modelData.homeUrl || "")
                                    readonly property bool launchBusy: {
                                        miniAppTaskManager.revision
                                        return miniAppTaskManager.isLaunchingUrl(cardCell.appUrl)
                                    }
                                    readonly property string runtimeState: {
                                        miniAppTaskManager.revision
                                        return miniAppTaskManager.stateForUrl(cardCell.appUrl)
                                    }
                                    readonly property bool running: runtimeState === "foreground"
                                                                     || runtimeState === "background"
                                                                     || runtimeState === "minimized"

                                    Superellipse {
                                        id: card
                                        anchors.fill: parent
                                        anchors.leftMargin: root.compact ? 0 : Theme.space2
                                        anchors.rightMargin: root.compact ? 0 : Theme.space2
                                        anchors.bottomMargin: Theme.space3
                                        fillColor: Theme.surfaceRaised
                                        borderWidth: cardCell.running ? Theme.strokeEmphasis : 1
                                        borderColor: cardCell.running ? Theme.accent : Theme.borderMuted
                                        curveDepth: Theme.curveContainer
                                        elevation: Theme.elevationControl
                                        shadowOpacityScale: 0.34

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.space3
                                            spacing: Theme.space2

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.space2
                                                layoutDirection: Theme.uiDirection

                                                SuperellipseImage {
                                                    Layout.preferredWidth: 44
                                                    Layout.preferredHeight: 44
                                                    source: String(cardCell.modelData.iconSource || "")
                                                    fillMode: Image.PreserveAspectFit
                                                    mediaCategory: "miniapps"
                                                    showManualLoadIndicator: false
                                                    showFailureIndicator: false
                                                    requestedSourceWidth: 44
                                                    requestedSourceHeight: 44
                                                    curveDepth: Theme.curveControl
                                                    fillColor: Theme.primarySurface
                                                    borderWidth: 1
                                                    borderColor: Theme.borderMuted
                                                }
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 1
                                                    DirectionAwareLabel {
                                                        Layout.fillWidth: true
                                                        text: String(cardCell.modelData.name || cardCell.modelData.domain || "")
                                                        color: Theme.ink
                                                        font.pixelSize: Theme.typeBodySize
                                                        font.weight: Theme.weightDemiBold
                                                        elide: Text.ElideRight
                                                    }
                                                    DirectionAwareLabel {
                                                        Layout.fillWidth: true
                                                        text: String(cardCell.modelData.domain || "")
                                                        color: Theme.inkMuted
                                                        font.pixelSize: Theme.typeCaptionSize
                                                        forceLeftToRight: true
                                                        elide: Text.ElideMiddle
                                                    }
                                                }

                                                Superellipse {
                                                    visible: cardCell.running
                                                    Layout.preferredWidth: stateLabel.implicitWidth + Theme.space3
                                                    Layout.preferredHeight: 24
                                                    fillColor: Theme.primarySurface
                                                    curveDepth: 12
                                                    DirectionAwareLabel {
                                                        id: stateLabel
                                                        anchors.centerIn: parent
                                                        text: cardCell.runtimeState === "foreground"
                                                              ? qsTrId("genycaster.task_manager.foreground")
                                                              : cardCell.runtimeState === "background"
                                                                ? qsTrId("genycaster.feed_header.open")
                                                                : qsTrId("genycaster.task_manager.minimized")
                                                        color: Theme.accent
                                                        font.pixelSize: Theme.typeCaptionSize
                                                        font.weight: Theme.weightDemiBold
                                                    }
                                                }
                                            }

                                            DirectionAwareLabel {
                                                Layout.fillWidth: true
                                                Layout.minimumHeight: 40
                                                text: String(cardCell.modelData.description || "")
                                                color: Theme.inkMuted
                                                font.pixelSize: Theme.typeMetaSize
                                                wrapMode: Text.Wrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }

                                            DirectionAwareLabel {
                                                visible: cardCell.modelData.requiredChains
                                                         && cardCell.modelData.requiredChains.length > 0
                                                Layout.fillWidth: true
                                                text: cardCell.modelData.requiredChains.join(" · ")
                                                color: Theme.inkSubtle
                                                font.pixelSize: Theme.typeCaptionSize
                                                forceLeftToRight: true
                                                elide: Text.ElideRight
                                            }

                                            Item { Layout.fillHeight: true }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.space2
                                                layoutDirection: Theme.uiDirection

                                                ActionButton {
                                                    Layout.fillWidth: true
                                                    compact: true
                                                    prominent: true
                                                    positive: cardCell.runtimeState === "foreground"
                                                    busy: cardCell.launchBusy
                                                    enabled: !cardCell.launchBusy
                                                    iconGlyph: cardCell.runtimeState === "foreground"
                                                               ? Theme.iconCheck
                                                               : cardCell.running ? Theme.iconApps : Theme.iconPlay
                                                    text: cardCell.launchBusy
                                                          ? qsTrId("genycaster.miniapp.loading")
                                                          : cardCell.running
                                                            ? qsTrId("genycaster.task_manager.focus")
                                                            : qsTrId("genycaster.miniapp.open_internal")
                                                    onClicked: root.openApp(cardCell.modelData)
                                                }
                                                ActionButton {
                                                    compact: true
                                                    quiet: true
                                                    iconOnly: true
                                                    iconGlyph: Theme.iconShare
                                                    text: qsTrId("genycaster.apps.share")
                                                    onClicked: miniAppHost.requestShare(
                                                        String(cardCell.modelData.homeUrl || ""),
                                                        String(cardCell.modelData.name || cardCell.modelData.domain || ""))
                                                }
                                                ActionButton {
                                                    compact: true
                                                    quiet: true
                                                    iconGlyph: Boolean(cardCell.modelData.saved) ? Theme.iconTrash : Theme.iconPlus
                                                    text: Boolean(cardCell.modelData.saved)
                                                          ? qsTrId("genycaster.apps.remove")
                                                          : qsTrId("genycaster.apps.save")
                                                    onClicked: {
                                                        if (Boolean(cardCell.modelData.saved))
                                                            miniAppCatalog.removeFromMyMiniApps(String(cardCell.modelData.homeUrl || ""))
                                                        else
                                                            miniAppCatalog.addToMyMiniApps(cardCell.modelData)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                }
            }
        }
    }
    }
}
