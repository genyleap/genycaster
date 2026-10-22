import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool compact: false
    property string section: "farcaster"
    property string farcasterSection: "my"
    property string explorerMode: "all"
    property bool requestedInitialDiscovery: false
    function ensureDiscovery() {
        if (!root.compact || requestedInitialDiscovery || miniAppCatalog.busy
                || miniAppCatalog.explorerApps.length > 0) return
        requestedInitialDiscovery = true
        Qt.callLater(function() {
            if (root.visible) miniAppCatalog.refresh()
            else root.requestedInitialDiscovery = false
        })
    }
    Component.onCompleted: root.ensureDiscovery()
    onVisibleChanged: if (visible) root.ensureDiscovery()
    readonly property var visibleApps: root.farcasterSection === "my"
                                       ? miniAppCatalog.myApps
                                       : miniAppCatalog.explorerApps
    // Display apps with genuine cover artwork first, without claiming that
    // catalogue order or this shelf constitutes an external popularity rank.
    readonly property var discoveryShelfApps: {
        const catalog = miniAppCatalog.explorerApps || []
        const illustrated = []
        for (let index = 0; index < catalog.length; ++index) {
            const app = catalog[index]
            if (String(app.coverSource || "").length > 0)
                illustrated.push(app)
        }
        // Unillustrated apps belong to the icon grid. A square icon in a
        // widescreen hero card is neither a real cover nor a useful fallback.
        return illustrated.slice(0, 8)
    }

    signal authenticationRequested()
    property var selectedMiniApp: ({})
    function showAppDetails(app) {
        if (!app || !String(app.homeUrl || "").length) return
        root.selectedMiniApp = app
        miniAppDetailDrawer.open()
    }

    function scrollToBeginning() {
        appsScroller.cancelFlick()
        appsScroller.contentY = 0
        appGrid.cancelFlick()
        appGrid.positionViewAtBeginning()
    }

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
        miniAppCatalog.recordRecentUse(app)
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
        onContentYChanged: {
            if (root.compact && root.section === "farcaster"
                    && root.farcasterSection === "explorer" && root.explorerMode === "all"
                    && !miniAppCatalog.busy && miniAppCatalog.hasMore
                    && contentY > 0 && contentY + height >= contentHeight - 420)
                miniAppCatalog.loadMore()
        }

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
                    visible: !root.compact
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

        // Discover sits above the library tabs, as in the Farcaster-style
        // mobile app gallery. All artwork comes from provider metadata.
        MiniAppFeaturedCarousel {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            compact: true
            heading: qsTr("Discover")
            apps: root.discoveryShelfApps
            visible: root.compact && root.section === "farcaster"
                     && miniAppCatalog.query.length === 0 && root.discoveryShelfApps.length > 0
            onOpenRequested: function(app) { root.openApp(app) }
            onSaveRequested: function(app) { miniAppCatalog.addToMyMiniApps(app) }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 52 : 64
            columns: 3
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
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: root.compact ? 48 : 64
                    fillColor: root.compact ? Qt.rgba(0, 0, 0, 0)
                                            : selected ? Theme.primarySurface : Theme.surfaceMuted
                    borderWidth: root.compact ? 0
                                              : selected ? Theme.strokeEmphasis : Theme.strokeHairline
                    borderColor: selected ? Theme.accent : Theme.borderSubtle
                    curveDepth: Theme.curveContainer

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: root.compact ? Theme.space1 : Theme.space3
                        spacing: root.compact ? Theme.space1 : Theme.space2
                        layoutDirection: Theme.uiDirection

                        Superellipse {
                            visible: !root.compact
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
                            Layout.minimumWidth: 0
                            horizontalAlignment: root.compact ? Text.AlignHCenter : Theme.textStartAlignment
                            text: String(destinationCard.modelData.title || "")
                            color: destinationCard.selected ? Theme.accent : Theme.ink
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }

                        Icon {
                            visible: destinationCard.selected && !root.compact
                            text: Theme.iconCheck
                            color: Theme.accent
                            preferredSize: Theme.iconSizeSmall
                        }
                    }

                    Rectangle {
                        visible: root.compact && destinationCard.selected
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 3
                        radius: 1.5
                        color: Theme.accent
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
                                       ? Math.max(60, farcasterColumn.implicitHeight)
                                       : 240)
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

                // Swipeable discovery cards reuse the same normalized app rows
                // and host-mediated launch/save actions as the catalogue below.
                MiniAppFeaturedCarousel {
                    id: featuredCarousel
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    compact: root.compact
                    apps: root.discoveryShelfApps
                    heading: root.explorerMode === "forYou"
                             ? qsTrId("genycaster.apps.for_you")
                             : qsTrId("genycaster.side_nav.explorer")
                    visible: !root.compact && root.farcasterSection === "explorer"
                             && miniAppCatalog.explorerApps.length > 0
                             && miniAppCatalog.query.length === 0
                    onOpenRequested: function(app) { root.openApp(app) }
                    onSaveRequested: function(app) { miniAppCatalog.addToMyMiniApps(app) }
                }

                MiniAppLibraryShelves {
                    objectName: "miniAppLibraryShelves"
                    visible: root.compact && root.farcasterSection === "my"
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    recentApps: miniAppCatalog.recentApps
                    savedApps: miniAppCatalog.myApps
                    onOpenRequested: function(app) { root.showAppDetails(app) }
                    onExploreRequested: root.farcasterSection = "explorer"
                }

                ColumnLayout {
                    visible: root.farcasterSection === "explorer" && root.compact
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    // Compact, keyboard-accessible text tabs rather than two
                    // full-width filled buttons. The remaining width is left
                    // clear so search and actual Store results stay visible.
                    RowLayout {
                        id: miniAppModeTabs
                        objectName: "miniAppModeTabs"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        spacing: Theme.space4
                        layoutDirection: Theme.uiDirection

                        Repeater {
                            model: [
                                { id: "all", title: qsTrId("genycaster.apps.all_apps") },
                                { id: "forYou", title: qsTrId("genycaster.apps.for_you") }
                            ]
                            delegate: Button {
                                id: modeTab
                                required property var modelData
                                readonly property bool selected: root.explorerMode === String(modelData.id)
                                text: String(modelData.title)
                                Layout.preferredWidth: Math.min(145, Math.max(78, tabText.implicitWidth + Theme.space4))
                                Layout.preferredHeight: 34
                                padding: 0
                                background: Item {
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: parent.width
                                        height: 2
                                        radius: 1
                                        color: Theme.accent
                                        visible: modeTab.selected
                                    }
                                }
                                contentItem: DirectionAwareLabel {
                                    id: tabText
                                    text: modeTab.text
                                    color: modeTab.selected ? Theme.ink : Theme.inkMuted
                                    font.pixelSize: Theme.typeMetaSize
                                    font.weight: modeTab.selected ? Theme.weightDemiBold : Theme.weightRegular
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                                onClicked: root.explorerMode = String(modelData.id)
                                Accessible.role: Accessible.PageTab
                                Accessible.name: modeTab.text
                            }
                        }
                        Item { Layout.fillWidth: true }
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
                    visible: !root.compact && root.farcasterSection === "explorer"
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
                    visible: !root.compact || root.farcasterSection !== "my"
                    Layout.fillWidth: true
                    Layout.fillHeight: !root.compact
                    // A zero-row GridView has no contentHeight, but its
                    // empty-state card still needs its own measured viewport.
                    // Without this reservation the centered illustration
                    // escapes upward into the navigation tabs on mobile.
                    Layout.preferredHeight: !visible ? 0 : root.compact
                                            ? (root.visibleApps.length === 0
                                               ? Math.max(280, Math.min(400, appsScroller.height * 0.43))
                                               : Math.max(1, appGrid.contentHeight))
                                            : 0

                    GridLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        visible: miniAppCatalog.busy && root.visibleApps.length === 0
                        layoutDirection: Theme.uiDirection
                        columns: width < 360 ? 1 : (width < 1040 ? 2 : 3)
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
                            width: Math.max(1, Math.min(parent.width - Theme.space6, 420))
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

                                readonly property int targetColumns: root.compact
                                                                    ? (width < 325 ? 2 : 3)
                                                                    : width < 660 ? 1 : (width < 1040 ? 2 : 3)
                                cellWidth: Math.max(1, width / Math.max(1, targetColumns))
                                // Store-like icon grid on phones; desktop still
                                // uses descriptive cards with launch controls.
                                cellHeight: root.compact ? 126 : 236 + Theme.space3

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

                                    Item {
                                        id: miniAppStoreTile
                                        visible: root.compact
                                        anchors.fill: parent
                                        anchors.margins: Theme.space1

                                        SuperellipseImage {
                                            id: storeIcon
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.top: parent.top
                                            width: Math.min(78, parent.width - 14)
                                            height: width
                                            source: String(cardCell.modelData.iconSource || "")
                                            mediaCategory: "miniapps"
                                            forceAutomaticDownload: true
                                            // Store icons should finish loading while the
                                            // parent page moves, unlike full feed media.
                                            deferNewLoadsWhileScrolling: false
                                            framePacedPresentation: false
                                            showLoadingIndicator: false
                                            showFailureIndicator: false
                                            showManualLoadIndicator: false
                                            fillMode: Image.PreserveAspectFit
                                            requestedSourceWidth: 120
                                            requestedSourceHeight: 120
                                            curveDepth: Theme.curveControl
                                            fillColor: Theme.surfaceRaised

                                            Icon {
                                                anchors.centerIn: parent
                                                visible: !storeIcon.ready
                                                text: Theme.iconApps
                                                color: Theme.accent
                                                preferredSize: 28
                                            }
                                        }
                                        DirectionAwareLabel {
                                            anchors.top: storeIcon.bottom
                                            anchors.topMargin: Theme.space2
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            text: String(cardCell.modelData.name || cardCell.modelData.domain || "")
                                            color: Theme.ink
                                            font.pixelSize: Theme.typeCaptionSize
                                            horizontalAlignment: Text.AlignHCenter
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }
                                        TapHandler { onTapped: root.showAppDetails(cardCell.modelData) }
                                        Accessible.role: Accessible.Button
                                        Accessible.name: String(cardCell.modelData.name || "")
                                    }

                                    Superellipse {
                                        id: card
                                        visible: !root.compact
                                        anchors.fill: parent
                                        anchors.leftMargin: root.compact ? Theme.space1 : Theme.space2
                                        anchors.rightMargin: root.compact ? Theme.space1 : Theme.space2
                                        anchors.bottomMargin: root.compact ? Theme.space1 : Theme.space3
                                        fillColor: Theme.surfaceRaised
                                        borderWidth: cardCell.running ? Theme.strokeEmphasis : 1
                                        borderColor: cardCell.running ? Theme.accent : Theme.borderMuted
                                        curveDepth: Theme.curveContainer
                                        elevation: root.compact ? Theme.elevationNone : Theme.elevationControl
                                        shadowOpacityScale: root.compact ? 0 : 0.34

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: root.compact ? Theme.space2 : Theme.space3
                                            spacing: root.compact ? Theme.space1 : Theme.space2

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.space2
                                                layoutDirection: Theme.uiDirection

                                                Item {
                                                    Layout.preferredWidth: root.compact ? 36 : 44
                                                    Layout.preferredHeight: Layout.preferredWidth

                                                    SuperellipseImage {
                                                        id: miniAppArtwork
                                                        anchors.fill: parent
                                                        source: String(cardCell.modelData.iconSource || "")
                                                        fillMode: Image.PreserveAspectFit
                                                        mediaCategory: "miniapps"
                                                        forceAutomaticDownload: true
                                                        showManualLoadIndicator: false
                                                        showFailureIndicator: false
                                                        showLoadingIndicator: false
                                                        requestedSourceWidth: root.compact ? 36 : 44
                                                        requestedSourceHeight: root.compact ? 36 : 44
                                                        curveDepth: Theme.curveControl
                                                        fillColor: Theme.primarySurface
                                                        borderWidth: 0
                                                    }

                                                    // The catalogue can omit artwork or return an
                                                    // unavailable URL. Always leave a recognizable
                                                    // app symbol rather than an empty pastel tile.
                                                    Icon {
                                                        anchors.centerIn: parent
                                                        visible: !miniAppArtwork.ready
                                                        text: Theme.iconApps
                                                        color: Theme.accent
                                                        preferredSize: 22
                                                    }
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
                                                        wrapMode: root.compact ? Text.WrapAtWordBoundaryOrAnywhere : Text.NoWrap
                                                        maximumLineCount: root.compact ? 2 : 1
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

                                                // A saved Mini App uses a persistent filled
                                                // success mark, not the same outline as Save.
                                                Superellipse {
                                                    visible: Boolean(cardCell.modelData.saved)
                                                    Layout.preferredWidth: 26
                                                    Layout.preferredHeight: 26
                                                    fillColor: Theme.positive
                                                    curveDepth: 13
                                                    Icon {
                                                        anchors.centerIn: parent
                                                        text: Theme.iconCheck
                                                        color: Theme.inkInverse
                                                        preferredSize: 16
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
                                                Layout.minimumHeight: root.compact ? 28 : 40
                                                text: String(cardCell.modelData.description || "")
                                                color: Theme.inkMuted
                                                font.pixelSize: Theme.typeMetaSize
                                                wrapMode: Text.Wrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }

                                            DirectionAwareLabel {
                                                visible: !root.compact && cardCell.modelData.requiredChains
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
                                                    iconOnly: false
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
                                                    visible: !root.compact
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
                                                    iconOnly: root.compact
                                                    readonly property bool saved: Boolean(cardCell.modelData.saved)
                                                    readonly property bool inLibrary: root.farcasterSection === "my"
                                                    iconGlyph: saved ? (inLibrary ? Theme.iconTrash : Theme.iconCheck) : Theme.iconPlus
                                                    positive: saved && !inLibrary
                                                    enabled: !saved || inLibrary
                                                    text: saved ? (inLibrary ? qsTrId("genycaster.apps.remove")
                                                                               : qsTrId("genycaster.apps.saved"))
                                                                : qsTrId("genycaster.apps.save")
                                                    onClicked: {
                                                        if (saved && inLibrary)
                                                            miniAppCatalog.removeFromMyMiniApps(String(cardCell.modelData.homeUrl || ""))
                                                        else if (!saved)
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

        ActionButton {
            objectName: "miniAppLoadMore"
            visible: root.section === "farcaster" && root.farcasterSection === "explorer"
                     && root.explorerMode === "all" && miniAppCatalog.hasMore
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: visible ? 40 : 0
            compact: true
            quiet: true
            busy: miniAppCatalog.busy
            text: qsTr("Load more apps")
            onClicked: miniAppCatalog.loadMore()
        }
    }
    }
    // App Store-style drill-in. Only a user-selected app allocates a large
    // information card. Tapping an icon never starts an embedded Mini App.
    GenyDrawer {
        id: miniAppDetailDrawer
        objectName: "miniAppDetailDrawer"
        bottomSheet: true
        fullScreen: false
        dimBackground: true
        interactive: false
        showHandle: true
        width: parent ? parent.width : 420
        height: parent ? Math.min(parent.height * 0.88, 740) : 640

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space4
            anchors.rightMargin: Theme.space4
            anchors.bottomMargin: Theme.space3
            spacing: Theme.space3

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTr("App details")
                    color: Theme.ink
                    font.weight: Theme.weightBold
                    font.pixelSize: Theme.typeTitle3Size
                }
                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.action.close")
                    onClicked: miniAppDetailDrawer.close()
                }
            }

            Flickable {
                id: detailScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: detailsColumn.implicitHeight + Theme.space3
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: detailsColumn
                    x: 0
                    y: 0
                    width: detailScroll.width
                    spacing: Theme.space3

                    SuperellipseImage {
                        id: detailCover
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? Math.min(180, detailScroll.width * 0.52) : 0
                        visible: String(root.selectedMiniApp.coverSource || "").length > 0
                        source: String(root.selectedMiniApp.coverSource || "")
                        mediaCategory: "miniapps"
                        forceAutomaticDownload: true
                        deferNewLoadsWhileScrolling: false
                        framePacedPresentation: false
                        showLoadingIndicator: true
                        showManualLoadIndicator: false
                        showFailureIndicator: false
                        fillMode: Image.PreserveAspectCrop
                        requestedSourceWidth: 560
                        requestedSourceHeight: 320
                        clipToSuperellipse: false
                        curveDepth: Theme.curveContainer
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3
                        layoutDirection: Theme.uiDirection

                        SuperellipseImage {
                            Layout.preferredWidth: 70
                            Layout.preferredHeight: 70
                            source: String(root.selectedMiniApp.iconSource || "")
                            mediaCategory: "miniapps"
                            forceAutomaticDownload: true
                            deferNewLoadsWhileScrolling: false
                            framePacedPresentation: false
                            showLoadingIndicator: false
                            showFailureIndicator: false
                            showManualLoadIndicator: false
                            fillMode: Image.PreserveAspectFit
                            requestedSourceWidth: 96
                            requestedSourceHeight: 96
                            curveDepth: Theme.curveControl
                            fillColor: Theme.surfaceRaised
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(root.selectedMiniApp.name || "")
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightBold
                                color: Theme.ink
                                wrapMode: Text.Wrap
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(root.selectedMiniApp.domain || "")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                forceLeftToRight: true
                                elide: Text.ElideMiddle
                            }
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(root.selectedMiniApp.description || "")
                        color: Theme.ink
                        font.pixelSize: Theme.typeMetaSize
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: String(root.selectedMiniApp.authorName || "").length > 0
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        SuperellipseImage {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            source: String(root.selectedMiniApp.authorAvatar || "")
                            mediaCategory: "avatars"
                            forceAutomaticDownload: true
                            deferNewLoadsWhileScrolling: false
                            framePacedPresentation: false
                            showLoadingIndicator: false
                            showFailureIndicator: false
                            showManualLoadIndicator: false
                            fillMode: Image.PreserveAspectCrop
                            requestedSourceWidth: 64
                            requestedSourceHeight: 64
                            curveDepth: Theme.curveControl
                            fillColor: Theme.surfaceMuted
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTr("Developer") + " · " + String(root.selectedMiniApp.authorName || "")
                                color: Theme.ink
                                font.pixelSize: Theme.typeMetaSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(root.selectedMiniApp.authorUsername || "")
                                      + (Number(root.selectedMiniApp.authorFid || 0) > 0
                                         ? " · FID " + String(root.selectedMiniApp.authorFid) : "")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: String(root.selectedMiniApp.category || "").length > 0
                        text: qsTr("Category") + ": " + String(root.selectedMiniApp.category || "")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeMetaSize
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: Boolean(root.selectedMiniApp.tags && root.selectedMiniApp.tags.length > 0)
                        text: (root.selectedMiniApp.tags || []).join(" · ")
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: Boolean(root.selectedMiniApp.requiredChains
                                         && root.selectedMiniApp.requiredChains.length > 0)
                        text: qsTr("Networks") + ": " + (root.selectedMiniApp.requiredChains || []).join(" · ")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: Boolean(root.selectedMiniApp.screenshots
                                         && root.selectedMiniApp.screenshots.length > 0)
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTr("Screenshots")
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            color: Theme.ink
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 154
                            orientation: ListView.Horizontal
                            layoutDirection: Qt.LeftToRight
                            spacing: Theme.space2
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: root.selectedMiniApp.screenshots || []
                            delegate: SuperellipseImage {
                                required property string modelData
                                width: 224
                                height: 148
                                source: modelData
                                mediaCategory: "miniapps"
                                forceAutomaticDownload: true
                                deferNewLoadsWhileScrolling: false
                                showLoadingIndicator: false
                                showFailureIndicator: false
                                showManualLoadIndicator: false
                                fillMode: Image.PreserveAspectCrop
                                requestedSourceWidth: 340
                                requestedSourceHeight: 224
                                clipToSuperellipse: false
                                curveDepth: Theme.curveControl
                            }
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: String(root.selectedMiniApp.source || "").length > 0
                        text: qsTr("Discovery source") + ": " + String(root.selectedMiniApp.source || "")
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.inkSubtle
                        wrapMode: Text.Wrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                ActionButton {
                    Layout.fillWidth: true
                    prominent: true
                    iconGlyph: Theme.iconPlay
                    text: qsTrId("genycaster.miniapp.open_internal")
                    onClicked: {
                        const app = root.selectedMiniApp
                        miniAppDetailDrawer.close()
                        root.openApp(app)
                    }
                }
                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: root.selectedMiniApp.saved ? Theme.iconCheck : Theme.iconPlus
                    text: root.selectedMiniApp.saved
                          ? qsTrId("genycaster.apps.saved") : qsTrId("genycaster.apps.save")
                    enabled: !Boolean(root.selectedMiniApp.saved)
                    onClicked: {
                        miniAppCatalog.addToMyMiniApps(root.selectedMiniApp)
                        root.selectedMiniApp = Object.assign({}, root.selectedMiniApp, { saved: true })
                    }
                }
            }
        }
    }
}
