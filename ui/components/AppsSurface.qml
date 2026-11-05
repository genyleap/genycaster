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
    property double lastAutomaticPageMs: 0
    function ensureDiscovery() {
        if (!root.compact || requestedInitialDiscovery || miniAppCatalog.busy
                || miniAppCatalog.explorerApps.length > 0) return
        requestedInitialDiscovery = true
        Qt.callLater(function() {
            if (root.visible) miniAppCatalog.refresh()
            else root.requestedInitialDiscovery = false
        })
    }
    Component.onCompleted: {
        root.ensureDiscovery()
        if (openProofConnections && openProofConnections.cloudAuthorizationReady)
            openProofConnections.fetchProEntitlements()
    }
    onVisibleChanged: if (visible) {
        root.ensureDiscovery()
        if (openProofConnections && openProofConnections.cloudAuthorizationReady)
            openProofConnections.fetchProEntitlements()
    }
    property string topicFilter: "all"
    readonly property var availableCategories: {
        LocaleFormat.observeRevision()
        const source = root.farcasterSection === "my"
                       ? (miniAppCatalog.myApps || []) : (miniAppCatalog.explorerApps || [])
        const categories = new Set()
        for (const app of source) {
            const category = String(app.category || "").trim()
            if (category.length > 0)
                categories.add(category)
        }
        return [{id: "all", title: qsTrId("genycaster.apps.all_topics")}].concat(
            Array.from(categories).sort((a, b) => a.localeCompare(b))
                .map(category => ({id: category, title: LocaleFormat.formatMiniAppCategory(category)})))
    }
    readonly property var visibleApps: {
        const source = root.farcasterSection === "my"
                       ? (miniAppCatalog.myApps || []) : (miniAppCatalog.explorerApps || [])
        if (root.topicFilter === "all") return source
        return source.filter(app => String(app.category || "").trim() === root.topicFilter)
    }
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
    signal taskManagerRequested()
    signal proDetailsRequested()
    readonly property bool sessionQuotaReached:
        miniAppHost.sessionLimit > 0
        && miniAppTaskManager.taskCount >= miniAppHost.sessionLimit
    readonly property bool sessionLimitError: root.quotaErrorKind.length > 0
    readonly property var proEntitlements: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property bool proVerified: proEntitlements.status === "verified"
        && proEntitlements.eligible === true
    property string quotaErrorKind: ""
    property var selectedMiniApp: ({})
    property var pendingLaunchApp: null
    property var pendingEntitlementLaunch: null
    property string launchErrorText: ""

    function showLaunchError(message, kind) {
        root.launchErrorText = String(message || "")
        root.quotaErrorKind = String(kind || "")
        launchErrorTimeout.restart()
    }
    function clearPendingEntitlementLaunch() {
        proLaunchTimeout.stop()
        root.pendingEntitlementLaunch = null
    }
    function directLaunch(app) {
        const url = String(app && app.homeUrl || "")
        if (!url) return
        root.launchErrorText = ""
        root.quotaErrorKind = ""
        root.pendingLaunchApp = app
        miniAppHost.openWithMetadata(url, String(app.name || ""),
                                     String(app.iconSource || ""))
    }
    function resolvePendingEntitlementLaunch() {
        const pending = root.pendingEntitlementLaunch
        if (!pending) return
        if (!presentationBridge.isAuthenticated
                || Number(presentationBridge.accountFid || 0) !== pending.fid) {
            root.clearPendingEntitlementLaunch()
            return
        }
        if (!openProofConnections || !openProofConnections.cloudAuthorizationReady
                || openProofConnections.proEntitlementsBusy)
            return
        const status = root.proEntitlements
        if (status.status !== "verified") {
            root.clearPendingEntitlementLaunch()
            root.showLaunchError(qsTrId("genycaster.pro.verification_unavailable"), "verify")
            return
        }
        root.clearPendingEntitlementLaunch()
        if (status.eligible === true && miniAppHost.sessionLimit >= 2
                && miniAppTaskManager.taskCount < miniAppHost.sessionLimit) {
            // The C++ entitlement handler has already installed the limit.
            root.directLaunch(pending.app)
        } else if (status.eligible === true && miniAppHost.sessionLimit >= 4) {
            root.showLaunchError(qsTrId("genycaster.miniapp.session_limit"), "full")
        } else if (status.eligible === true) {
            // A verified Pro response must first propagate to BOTH the
            // native runtime and its public UI-facing host controller.
            root.showLaunchError(qsTrId("genycaster.pro.verification_unavailable"), "verify")
        } else {
            root.showLaunchError(qsTrId("genycaster.pro.requires_pro"), "basic")
        }
    }
    function requestEntitlementForLaunch(app) {
        // The account-controlled endpoint, not a profile badge or user-owned
        // property, decides whether a second native WebKit session is allowed.
        root.pendingEntitlementLaunch = {
            app: app, fid: Number(presentationBridge.accountFid || 0)
        }
        root.launchErrorText = ""
        root.quotaErrorKind = ""
        proLaunchTimeout.restart()
        if (openProofConnections)
            openProofConnections.retryProEntitlements()
    }
    Timer {
        id: proLaunchTimeout
        // Session restore (9s) plus entitlement verification (14s).
        interval: 28000
        repeat: false
        onTriggered: {
            if (!root.pendingEntitlementLaunch) return
            root.clearPendingEntitlementLaunch()
            root.showLaunchError(qsTrId("genycaster.pro.verification_unavailable"), "verify")
        }
    }
    Timer {
        id: launchErrorTimeout
        interval: 7000
        onTriggered: {
            root.launchErrorText = ""
            root.quotaErrorKind = ""
        }
    }
    Connections {
        target: openProofConnections ? openProofConnections : null
        function onStateChanged() {
            if (root.pendingEntitlementLaunch && openProofConnections.cloudAuthorizationReady
                    && !openProofConnections.proEntitlementsBusy)
                openProofConnections.fetchProEntitlements(true)
        }
        function onProEntitlementsChanged() {
            if (root.pendingEntitlementLaunch)
                Qt.callLater(root.resolvePendingEntitlementLaunch)
        }
    }
    Connections {
        target: miniAppTaskManager
        function onTasksChanged() {
            if (root.pendingEntitlementLaunch
                    && miniAppTaskManager.taskCount < miniAppHost.sessionLimit) {
                const pending = root.pendingEntitlementLaunch
                root.clearPendingEntitlementLaunch()
                if (presentationBridge.isAuthenticated
                        && Number(presentationBridge.accountFid || 0) === pending.fid)
                    root.directLaunch(pending.app)
            } else if (root.quotaErrorKind.length > 0
                    && miniAppTaskManager.taskCount < miniAppHost.sessionLimit) {
                root.launchErrorText = ""
                root.quotaErrorKind = ""
                launchErrorTimeout.stop()
            }
        }
    }
    Connections {
        target: miniAppHost
        function onSessionLimitChanged() {
            if (miniAppHost.sessionLimit > 1 && root.quotaErrorKind !== "full") {
                root.launchErrorText = ""
                root.quotaErrorKind = ""
                launchErrorTimeout.stop()
            }
            if (root.pendingEntitlementLaunch)
                Qt.callLater(root.resolvePendingEntitlementLaunch)
        }
    }
    Connections {
        target: presentationBridge
        function onAuthenticationStateChanged() {
            if (root.pendingEntitlementLaunch
                    && (!presentationBridge.isAuthenticated
                        || Number(presentationBridge.accountFid || 0)
                           !== root.pendingEntitlementLaunch.fid))
                root.clearPendingEntitlementLaunch()
        }
    }
    Connections {
        target: miniAppHost
        function onExternalLaunchSucceeded() {
            if (root.pendingLaunchApp !== null)
                miniAppCatalog.recordRecentUse(root.pendingLaunchApp)
            root.pendingLaunchApp = null
        }
        function onStateChanged() {
            if (miniAppHost.errorText.length > 0) {
                // Do not show the four-session message when the running host
                // is actually constrained to a single Basic session.
                const isQuota = miniAppHost.errorText === qsTrId("genycaster.miniapp.session_limit")
                if (isQuota && miniAppHost.sessionLimit < 4) {
                    root.showLaunchError(root.proVerified
                        ? qsTrId("genycaster.pro.verification_unavailable")
                        : root.proEntitlements.status === "verified"
                          ? qsTrId("genycaster.pro.requires_pro")
                          : qsTrId("genycaster.pro.verification_unavailable"),
                        root.proVerified ? "verify"
                        : root.proEntitlements.status === "verified" ? "basic" : "verify")
                } else {
                    root.showLaunchError(miniAppHost.errorText, isQuota ? "full" : "")
                }
                root.pendingLaunchApp = null
            } else if (miniAppHost.active && root.pendingLaunchApp !== null) {
                miniAppCatalog.recordRecentUse(root.pendingLaunchApp)
                root.pendingLaunchApp = null
            }
        }
    }
    function showAppDetails(app) {
        if (!app || !String(app.homeUrl || "").length) return
        screenshotSection.screenshotFailures = 0
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
        if (!url) return
        root.launchErrorText = ""
        root.quotaErrorKind = ""
        if (Number(presentationBridge.accountFid || 0) <= 0) {
            root.authenticationRequested()
            return
        }
        // Resuming an existing session does not consume an extra slot.
        const current = String(miniAppTaskManager.stateForUrl(url) || "")
        const alreadyOpen = ["foreground", "background", "minimized",
                             "resolving", "loading"].indexOf(current) >= 0
        if (!alreadyOpen && root.sessionQuotaReached
                && miniAppHost.sessionLimit < 4) {
            root.requestEntitlementForLaunch(app)
            return
        }
        root.directLaunch(app)
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
            // Only a user gesture may fetch another page. Relayout after an
            // append must not immediately cascade into more API requests.
            const now = Date.now()
            if (root.compact && (dragging || flicking)
                    && root.section === "farcaster"
                    && root.farcasterSection === "explorer" && root.explorerMode === "all"
                    && !miniAppCatalog.busy && miniAppCatalog.hasMore
                    && miniAppCatalog.errorText.length === 0
                    && now - root.lastAutomaticPageMs > 2500
                    && contentY > 0 && contentY + height >= contentHeight - 240) {
                root.lastAutomaticPageMs = now
                miniAppCatalog.loadMore()
            }
        }

        ScrollBar.vertical: GenyScrollBar {
            policy: ScrollBar.AlwaysOff
        }

        ContentRefreshIndicator {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            z: 5
            active: miniAppCatalog.busy && root.visibleApps.length > 0
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
                objectName: "miniAppActiveSessions"
                visible: root.section === "farcaster" && miniAppTaskManager.taskCount > 0
                compact: true
                quiet: true
                iconGlyph: Theme.iconCpu
                customForegroundColor: root.sessionQuotaReached ? Theme.warning : "transparent"
                text: LocaleFormat.formatInteger(miniAppTaskManager.taskCount)
                      + " / " + LocaleFormat.formatInteger(miniAppHost.sessionLimit)
                      + (root.compact ? "" : " · " + qsTrId("genycaster.task_manager.title"))
                onClicked: root.taskManagerRequested()
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
            heading: qsTrId("genycaster.apps.discover")
            apps: root.discoveryShelfApps
            visible: root.compact && root.section === "farcaster"
                     && miniAppCatalog.query.length === 0 && root.discoveryShelfApps.length > 0
            onOpenRequested: function(app) { root.openApp(app) }
            onSaveRequested: function(app) { miniAppCatalog.addToMyMiniApps(app) }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 52 : 48
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
                    Layout.preferredHeight: root.compact ? 48 : 48
                    // Desktop behaves like a Store category selector, not
                    // three oversized filled dashboard buttons.
                    fillColor: root.compact ? Qt.rgba(0, 0, 0, 0)
                                            : selected ? Theme.primarySurface : Theme.surface
                    borderWidth: root.compact ? 0 : Theme.strokeHairline
                    borderColor: selected ? Theme.accent : Theme.borderMuted
                    curveDepth: Theme.curveContainer

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: root.compact ? Theme.space1 : Theme.space3
                        spacing: root.compact ? Theme.space1 : Theme.space2
                        layoutDirection: Theme.uiDirection

                        Superellipse {
                            visible: !root.compact
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
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
                    onLaunchRequested: function(app) { root.openApp(app) }
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

                    RowLayout {
                        objectName: "miniAppMobileTopicFilters"
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        GenyComboBox {
                            objectName: "miniAppMobileTopicFilter"
                            // Size hints must not depend on the layout they
                            // invalidate. The outer viewport owns this budget.
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: Math.min(175, pageColumn.width * 0.56)
                            model: root.availableCategories
                            textRole: "title"
                            valueRole: "id"
                            currentIndex: {
                                const index = root.availableCategories.findIndex(
                                    item => item.id === root.topicFilter)
                                return Math.max(0, index)
                            }
                            onActivated: root.topicFilter = String(currentValue)
                        }
                        Item { Layout.fillWidth: true }
                    }
                }

                RowLayout {
                    visible: root.farcasterSection === "explorer" && !root.compact
                    Layout.fillWidth: true
                    spacing: Theme.space2
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

                    GenyComboBox {
                        objectName: "miniAppTopicFilter"
                        Layout.preferredWidth: 166
                        textRole: "title"
                        valueRole: "id"
                        model: root.availableCategories
                        currentIndex: {
                            const i = root.availableCategories.findIndex(
                                row => row.id === root.topicFilter)
                            return i >= 0 ? i : 0
                        }
                        onActivated: root.topicFilter = String(currentValue)
                    }

                    DirectionAwareTextField {
                        id: miniAppSearch
                        Layout.fillWidth: true
                        Layout.preferredWidth: 290
                        Layout.maximumWidth: 420
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

                    Item { Layout.fillWidth: true }
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
                              : miniAppCatalog.providerLabel === "Genycaster public Mini Apps catalogue"
                                ? qsTrId("genycaster.apps.catalogue_public")
                              : miniAppCatalog.providerLabel === "Farcaster public developer index"
                                ? qsTrId("genycaster.apps.catalogue_farcaster")
                              : miniAppCatalog.providerLabel === "Neynar Mini Apps · docs access"
                                ? qsTrId("genycaster.apps.catalogue_neynar_docs")
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
                        columns: root.compact ? 1
                                              : (width < 360 ? 1 : (width < 1040 ? 2 : 3))
                        columnSpacing: Theme.space2
                        rowSpacing: Theme.space2

                        Repeater {
                            model: root.compact ? 6 : parent.columns
                            delegate: Superellipse {
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.compact ? 116 : 190
                                fillColor: Theme.surfaceMuted
                                borderWidth: 1
                                borderColor: Theme.borderMuted
                                curveDepth: Theme.curveContainer
                                opacity: 0.72

                                // On mobile a loading shelf must share the exact
                                // store grid footprint, not jump from a tall card
                                // layout to 3 columns when the response arrives.
                                RowLayout {
                                    visible: root.compact
                                    anchors.fill: parent
                                    anchors.margins: Theme.space2
                                    spacing: Theme.space3
                                    Superellipse { Layout.preferredWidth: 56; Layout.preferredHeight: 56; fillColor: Theme.borderMuted; curveDepth: Theme.curveControl }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.space2
                                        Superellipse { Layout.fillWidth: true; Layout.preferredHeight: 10; fillColor: Theme.borderMuted; curveDepth: 5 }
                                        Superellipse { Layout.fillWidth: true; Layout.preferredHeight: 8; fillColor: Theme.borderSubtle; curveDepth: 4 }
                                    }
                                    Superellipse { Layout.preferredWidth: 68; Layout.preferredHeight: 32; fillColor: Theme.primarySurface; curveDepth: 16 }
                                }

                                ColumnLayout {
                                    visible: !root.compact
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

                                // Two scannable desktop columns with real
                                // row-level controls; the phone icon grid stays
                                // identical to its existing contract.
                                readonly property int targetColumns: root.compact
                                                                    ? 1
                                                                    : (width < Theme.scaleUi(1160) ? 1 : 2)
                                cellWidth: Math.max(1, width / Math.max(1, targetColumns))
                                // Desktop rows are dense and borderless;
                                // phone icon-grid geometry is unchanged.
                                cellHeight: root.compact
                                            ? (width < Theme.scaleUi(540)
                                               ? Theme.scaleUi(148) : Theme.scaleUi(118))
                                            : (width < Theme.scaleUi(660)
                                               ? Theme.scaleUi(152) : Theme.scaleUi(112))

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
                                    readonly property bool proCheckBusy: root.pendingEntitlementLaunch !== null
                                        && String(root.pendingEntitlementLaunch.app.homeUrl || "") === cardCell.appUrl
                                    readonly property string runtimeState: {
                                        miniAppTaskManager.revision
                                        return miniAppTaskManager.stateForUrl(cardCell.appUrl)
                                    }
                                    readonly property bool running: runtimeState === "foreground"
                                                                     || runtimeState === "background"
                                                                     || runtimeState === "minimized"
                                    // Phone-width rows give the app identity its own line;
                                    // actions remain visible rather than truncating Resume.
                                    readonly property bool mobileActionsStacked: root.compact
                                        && width < Theme.scaleUi(540)
                                    readonly property bool desktopActionsStacked: !root.compact
                                        && width < Theme.scaleUi(660)

                                    Item {
                                        id: miniAppStoreTile
                                        objectName: "miniAppMobileStoreRow"
                                        visible: root.compact
                                        anchors.fill: parent
                                        Rectangle {
                                            anchors.bottom: parent.bottom
                                            width: parent.width
                                            height: Theme.strokeHairline
                                            color: Theme.divider
                                        }
                                        GridLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.space1
                                            columns: cardCell.mobileActionsStacked ? 1 : 2
                                            rowSpacing: Theme.space1
                                            columnSpacing: Theme.space3
                                            layoutDirection: Theme.uiDirection
                                            RowLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: Theme.space2
                                                layoutDirection: Theme.uiDirection
                                            Item {
                                                Layout.preferredWidth: 56
                                                Layout.preferredHeight: 56
                                                SuperellipseImage {
                                                    id: storeIcon
                                                    anchors.fill: parent
                                                    source: String(cardCell.modelData.iconSource || "")
                                                    mediaCategory: "miniapps"
                                                    forceAutomaticDownload: true
                                                    deferNewLoadsWhileScrolling: false
                                                    framePacedPresentation: false
                                                    showLoadingIndicator: false
                                                    showFailureIndicator: false
                                                    showManualLoadIndicator: false
                                                    fillMode: Image.PreserveAspectFit
                                                    requestedSourceWidth: 112
                                                    requestedSourceHeight: 112
                                                    curveDepth: Theme.curveControl
                                                    fillColor: Theme.surfaceRaised
                                                }
                                                MiniAppArtPlaceholder {
                                                    anchors.fill: parent
                                                    imageReady: storeIcon.ready
                                                    loadFailed: storeIcon.loadFailed
                                                    hasSource: String(cardCell.modelData.iconSource || "").length > 0
                                                }
                                                TapHandler { onTapped: root.showAppDetails(cardCell.modelData) }
                                            }
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 2
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                                    horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                                    textFormat: Text.PlainText
                                                    text: String(cardCell.modelData.name || cardCell.modelData.domain || "")
                                                    color: Theme.ink
                                                    font.pixelSize: Theme.typeBodySize
                                                    font.weight: Theme.weightDemiBold
                                                    maximumLineCount: 1
                                                    elide: Text.ElideRight
                                                    TapHandler { onTapped: root.showAppDetails(cardCell.modelData) }
                                                }
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                                    horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                                    textFormat: Text.PlainText
                                                    text: LocaleFormat.formatMiniAppCategory(cardCell.modelData.category)
                                                         || String(cardCell.modelData.domain || "")
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeCaptionSize
                                                    elide: Text.ElideRight
                                                }
                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                                    horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                                    textFormat: Text.PlainText
                                                    text: String(cardCell.modelData.description || "")
                                                    color: Theme.inkSubtle
                                                    font.pixelSize: Theme.typeMetaSize
                                                    maximumLineCount: 2
                                                    wrapMode: Text.Wrap
                                                    elide: Text.ElideRight
                                                }
                                            }
                                            }
                                            RowLayout {
                                                Layout.fillWidth: cardCell.mobileActionsStacked
                                                Layout.alignment: Qt.AlignVCenter
                                                spacing: Theme.space1
                                                layoutDirection: Theme.uiDirection
                                                Item {
                                                    visible: cardCell.mobileActionsStacked
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 1
                                                }
                                                ActionButton {
                                                    objectName: "miniAppMobileOpenButton"
                                                    Layout.preferredWidth: Math.max(96, implicitWidth)
                                                    Layout.minimumWidth: Math.max(96, implicitWidth)
                                                    compact: true
                                                    pill: true
                                                    elevated: false
                                                    busy: cardCell.launchBusy || cardCell.proCheckBusy
                                                    enabled: !cardCell.launchBusy && !cardCell.proCheckBusy
                                                    text: cardCell.proCheckBusy ? qsTrId("genycaster.pro.check_status")
                                                        : cardCell.launchBusy ? qsTrId("genycaster.apps.opening")
                                                        : cardCell.running ? qsTrId("genycaster.apps.resume") : qsTrId("genycaster.apps.open")
                                                    onClicked: root.openApp(cardCell.modelData)
                                                }
                                                RowLayout {
                                                    Layout.alignment: Qt.AlignVCenter
                                                    spacing: Theme.space1
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
                                                        iconOnly: true
                                                        readonly property bool saved: Boolean(cardCell.modelData.saved)
                                                        iconGlyph: saved ? Theme.iconCheck : Theme.iconPlus
                                                        positive: saved
                                                        pill: saved
                                                        quiet: !saved
                                                        enabled: true
                                                        text: saved ? qsTrId("genycaster.apps.saved") : qsTrId("genycaster.apps.save")
                                                        onClicked: {
                                                            if (!saved) miniAppCatalog.addToMyMiniApps(cardCell.modelData)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Desktop Store shelf: flat app rows, not
                                    // oversized nested promotional cards. The
                                    // featured carousel above retains its covers.
                                    Item {
                                        id: storeRow
                                        objectName: "miniAppDesktopStoreRow"
                                        visible: !root.compact
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.space3
                                        anchors.rightMargin: Theme.space3

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: Theme.strokeHairline
                                            color: Theme.divider
                                        }

                                        GridLayout {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            columns: cardCell.desktopActionsStacked ? 1 : 2
                                            rowSpacing: Theme.space1
                                            columnSpacing: Theme.space3
                                            layoutDirection: Theme.uiDirection

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: Theme.space3
                                                layoutDirection: Theme.uiDirection
                                            Item {
                                                Layout.preferredWidth: 60
                                                Layout.preferredHeight: 60
                                                Layout.alignment: Qt.AlignVCenter

                                                SuperellipseImage {
                                                    id: desktopAppIcon
                                                    anchors.fill: parent
                                                    source: String(cardCell.modelData.iconSource || "")
                                                    mediaCategory: "miniapps"
                                                    fillMode: Image.PreserveAspectFit
                                                    forceAutomaticDownload: true
                                                    deferNewLoadsWhileScrolling: false
                                                    framePacedPresentation: false
                                                    showManualLoadIndicator: false
                                                    showFailureIndicator: false
                                                    showLoadingIndicator: false
                                                    requestedSourceWidth: 112
                                                    requestedSourceHeight: 112
                                                    curveDepth: Theme.curveControl
                                                    fillColor: Theme.surfaceRaised
                                                }

                                                MiniAppArtPlaceholder {
                                                    anchors.fill: parent
                                                    imageReady: desktopAppIcon.ready
                                                    loadFailed: desktopAppIcon.loadFailed
                                                    hasSource: String(cardCell.modelData.iconSource || "").length > 0
                                                }

                                                TapHandler {
                                                    onTapped: root.showAppDetails(cardCell.modelData)
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 2

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                                    horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                                    textFormat: Text.PlainText
                                                    text: String(cardCell.modelData.name || cardCell.modelData.domain || "")
                                                    color: Theme.ink
                                                    font.pixelSize: Theme.typeBodySize
                                                    font.weight: Theme.weightDemiBold
                                                    elide: Text.ElideRight
                                                    maximumLineCount: 1
                                                    TapHandler {
                                                        onTapped: root.showAppDetails(cardCell.modelData)
                                                    }
                                                }

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                                    horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                                    textFormat: Text.PlainText
                                                    text: LocaleFormat.formatMiniAppCategory(cardCell.modelData.category)
                                                                 || String(cardCell.modelData.domain || "")
                                                    color: Theme.inkMuted
                                                    font.pixelSize: Theme.typeCaptionSize
                                                    elide: Text.ElideRight
                                                    maximumLineCount: 1
                                                }

                                                DirectionAwareLabel {
                                                    Layout.fillWidth: true
                                                    // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                                    horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                                    textFormat: Text.PlainText
                                                    text: String(cardCell.modelData.description || "")
                                                    color: Theme.inkSubtle
                                                    font.pixelSize: Theme.typeMetaSize
                                                    wrapMode: Text.Wrap
                                                    maximumLineCount: 2
                                                    elide: Text.ElideRight
                                                }
                                            }
                                            }

                                            RowLayout {
                                                Layout.fillWidth: cardCell.desktopActionsStacked
                                                Layout.alignment: Qt.AlignVCenter
                                                spacing: Theme.space1
                                                layoutDirection: Theme.uiDirection
                                                Item {
                                                    visible: cardCell.desktopActionsStacked
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 1
                                                }

                                            ActionButton {
                                                objectName: "miniAppStoreOpenButton"
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.preferredWidth: Math.max(104, implicitWidth)
                                                Layout.minimumWidth: Math.max(104, implicitWidth)
                                                compact: true
                                                pill: true
                                                elevated: false
                                                busy: cardCell.launchBusy || cardCell.proCheckBusy
                                                enabled: !cardCell.launchBusy && !cardCell.proCheckBusy
                                                text: cardCell.proCheckBusy ? qsTrId("genycaster.pro.check_status")
                                                      : cardCell.launchBusy ? qsTrId("genycaster.apps.opening")
                                                      : cardCell.running ? qsTrId("genycaster.apps.resume") : qsTrId("genycaster.apps.open")
                                                onClicked: root.openApp(cardCell.modelData)
                                            }

                                            ActionButton {
                                                Layout.alignment: Qt.AlignVCenter
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
                                                Layout.alignment: Qt.AlignVCenter
                                                compact: true
                                                iconOnly: true
                                                readonly property bool saved: Boolean(cardCell.modelData.saved)
                                                readonly property bool inLibrary: root.farcasterSection === "my"
                                                iconGlyph: saved ? (inLibrary ? Theme.iconTrash : Theme.iconCheck)
                                                                 : Theme.iconPlus
                                                positive: saved && !inLibrary
                                                pill: saved && !inLibrary
                                                quiet: !saved || inLibrary
                                                enabled: true
                                                text: saved ? (inLibrary ? qsTrId("genycaster.apps.remove") : qsTrId("genycaster.apps.saved"))
                                                            : qsTrId("genycaster.apps.save")
                                                onClicked: {
                                                    if (saved && inLibrary)
                                                        miniAppCatalog.removeFromMyMiniApps(
                                                            String(cardCell.modelData.homeUrl || ""))
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
            text: qsTrId("genycaster.apps.load_more")
            onClicked: miniAppCatalog.loadMore()
        }
    }
    }
    // Make host startup failures visible; unsupported native platforms must
    // not look like a dead Open button. Recent-use history records success only.
    Superellipse {
        visible: root.launchErrorText.length > 0
        z: 150
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space3
        height: Math.max(52,
            Math.max(launchFailure.implicitHeight, limitManageButton.implicitHeight)
            + Theme.space3 * 2)
        fillColor: root.sessionLimitError ? Theme.surfaceRaised : Theme.dangerSurface
        borderWidth: Theme.strokeHairline
        borderColor: root.sessionLimitError ? Theme.warning : Theme.danger
        curveDepth: Theme.curveControl
        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            DirectionAwareLabel {
                id: launchFailure
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.launchErrorText
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                wrapMode: Text.WordWrap
            }
            ActionButton {
                id: limitManageButton
                objectName: "miniAppLimitManageSessions"
                visible: root.sessionLimitError
                compact: true
                quiet: true
                iconGlyph: Theme.iconCpu
                text: root.quotaErrorKind === "full"
                      ? qsTrId("genycaster.task_manager.title")
                      : qsTrId("genycaster.pro.title")
                onClicked: {
                    const kind = root.quotaErrorKind
                    root.launchErrorText = ""
                    root.quotaErrorKind = ""
                    launchErrorTimeout.stop()
                    if (kind === "full")
                        root.taskManagerRequested()
                    else
                        root.proDetailsRequested()
                }
            }
        }
    }

    // App Store-style drill-in. Only a user-selected app allocates a large
    // information card. Tapping an icon never starts an embedded Mini App.
    GenyDrawer {
        id: miniAppDetailDrawer
        objectName: "miniAppDetailDrawer"
        readonly property bool mobilePresentation: parent ? parent.width < Theme.breakpointMobile : true
        bottomSheet: mobilePresentation
        fullScreen: false
        dimBackground: true
        interactive: mobilePresentation
        showHandle: mobilePresentation
        width: parent ? (mobilePresentation ? parent.width : Math.min(480, parent.width * 0.9)) : 420
        // Fit short Mini App details to their content instead of leaving
        // a blank shelf above the fixed Open action. Long details scroll.
        height: parent ? (mobilePresentation
            ? Math.min(parent.height * 0.92, Math.max(360,
                detailsColumn.implicitHeight + Theme.scaleUi(150)))
            : parent.height) : 640

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space4
            anchors.rightMargin: Theme.space4
            anchors.bottomMargin: Theme.space3
            spacing: Theme.space3

            ModalHeader {
                objectName: "miniAppDetailModalHeader"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.scaleUi(48)
                title: qsTrId("genycaster.apps.details")
                sheetPresentation: miniAppDetailDrawer.mobilePresentation
                onCloseRequested: miniAppDetailDrawer.close()
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
                        // Match the actual banner ratio instead of cutting off
                        // its edges with a fixed-height, center-cropped box.
                        Layout.preferredHeight: visible ? Math.max(112, Math.min(280,
                            detailScroll.width * (detailCover.sourceImplicitWidth > 0
                                && detailCover.sourceImplicitHeight > 0
                                ? detailCover.sourceImplicitHeight / detailCover.sourceImplicitWidth
                                : 0.52))) : 0
                        visible: String(root.selectedMiniApp.coverSource || "").length > 0
                        source: String(root.selectedMiniApp.coverSource || "")
                        mediaCategory: "miniapps"
                        forceAutomaticDownload: true
                        deferNewLoadsWhileScrolling: false
                        framePacedPresentation: false
                        showLoadingIndicator: true
                        showManualLoadIndicator: false
                        showFailureIndicator: false
                        fillMode: Image.PreserveAspectFit
                        requestedSourceWidth: 720
                        requestedSourceHeight: 480
                        clipToSuperellipse: true
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
                                // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                textFormat: Text.PlainText
                                text: String(root.selectedMiniApp.name || "")
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightBold
                                color: Theme.ink
                                wrapMode: Text.Wrap
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                textFormat: Text.PlainText
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
                        // Foreign content keeps its own BiDi order while aligning to the UI edge.
                        horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                        textFormat: Text.PlainText
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
                                // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                textFormat: Text.PlainText
                                text: qsTrId("genycaster.apps.developer") + " · " + String(root.selectedMiniApp.authorName || "")
                                color: Theme.ink
                                font.pixelSize: Theme.typeMetaSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                // Foreign content keeps its own BiDi order while aligning to the UI edge.
                                horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                                textFormat: Text.PlainText
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
                        text: qsTrId("genycaster.apps.category_label") + ": "
                              + LocaleFormat.formatMiniAppCategory(root.selectedMiniApp.category)
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeMetaSize
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: Boolean(root.selectedMiniApp.tags && root.selectedMiniApp.tags.length > 0)
                        // Foreign content keeps its own BiDi order while aligning to the UI edge.
                        horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                        textFormat: Text.PlainText
                        text: (root.selectedMiniApp.tags || []).join(" · ")
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: Boolean(root.selectedMiniApp.requiredChains
                                         && root.selectedMiniApp.requiredChains.length > 0)
                        text: qsTrId("genycaster.apps.networks") + ": " + (root.selectedMiniApp.requiredChains || []).join(" · ")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                    ColumnLayout {
                        id: screenshotSection
                        Layout.fillWidth: true
                        // The publisher may remove screenshots while their URLs
                        // remain in the catalogue. Never show empty error tiles.
                        property int screenshotFailures: 0
                        readonly property int screenshotTotal: root.selectedMiniApp.screenshots
                            ? root.selectedMiniApp.screenshots.length : 0
                        visible: screenshotTotal > 0 && screenshotFailures < screenshotTotal
                        spacing: Theme.space2
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.apps.screenshots")
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            color: Theme.ink
                        }
                        Flickable {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 156
                            contentWidth: screenshotRow.implicitWidth
                            contentHeight: height
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.HorizontalFlick
                            Row {
                                id: screenshotRow
                                spacing: Theme.space2
                                Repeater {
                                    // At most five screenshots: load all five so
                                    // failures offscreen can collapse the section.
                                    model: root.selectedMiniApp.screenshots || []
                                    delegate: SuperellipseImage {
                                        id: screenshotPreview
                                        required property string modelData
                                        property bool failureCounted: false
                                        visible: !loadFailed
                                        width: sourceImplicitWidth > 0 && sourceImplicitHeight > 0
                                            ? Math.max(94, Math.min(240,
                                                height * sourceImplicitWidth / sourceImplicitHeight))
                                            : 220
                                        height: 150
                                        source: modelData
                                        mediaCategory: "miniapps"
                                        forceAutomaticDownload: true
                                        deferNewLoadsWhileScrolling: false
                                        framePacedPresentation: false
                                        showLoadingIndicator: false
                                        showFailureIndicator: false
                                        showManualLoadIndicator: false
                                        fillMode: Image.PreserveAspectFit
                                        requestedSourceWidth: 480
                                        requestedSourceHeight: 480
                                        clipToSuperellipse: true
                                        curveDepth: Theme.curveControl
                                        onLoadFailedChanged: {
                                            if (loadFailed && !failureCounted) {
                                                failureCounted = true
                                                screenshotSection.screenshotFailures++
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: String(root.selectedMiniApp.source || "").length > 0
                        horizontalAlignment: Theme.uiDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
                        textFormat: Text.PlainText
                        text: qsTrId("genycaster.apps.discovery_source") + ": " + String(root.selectedMiniApp.source || "")
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
                    text: qsTrId("genycaster.apps.open")
                    Accessible.description: qsTrId("genycaster.miniapp.open_internal")
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
                    positive: Boolean(root.selectedMiniApp.saved)
                    pill: Boolean(root.selectedMiniApp.saved)
                    iconGlyph: root.selectedMiniApp.saved ? Theme.iconCheck : Theme.iconPlus
                    text: root.selectedMiniApp.saved
                          ? qsTrId("genycaster.apps.saved") : qsTrId("genycaster.apps.save")
                    enabled: true
                    onClicked: {
                        if (Boolean(root.selectedMiniApp.saved)) return
                        miniAppCatalog.addToMyMiniApps(root.selectedMiniApp)
                        root.selectedMiniApp = Object.assign({}, root.selectedMiniApp, { saved: true })
                    }
                }
            }
        }
    }
}
