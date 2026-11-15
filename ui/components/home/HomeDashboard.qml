import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root
    objectName: "homeDashboard"

    property string displayName: ""
    property string username: ""
    property string avatarUrl: ""
    property bool authenticated: false
    property string accountFid: ""
    property string readContract: ""
    property string readEndpoint: ""
    property string activeTab: "overview"
    property int statsDays: 30
    readonly property var proMembership: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property bool verifiedGenyPro: authenticated
        && Number(accountFid) > 0 && openProofConnections
        && Number(openProofConnections.activeAccountFid) === Number(accountFid)
        && proMembership.status === "verified"
        && proMembership.tier === "pro" && proMembership.eligible === true
    onVerifiedGenyProChanged: root.requestStats(false)
    function requestStats(force) {
        if (!root.authenticated || !root.verifiedGenyPro
                || root.accountFid === "" || root.accountFid === "0") {
            statsController.clear()
            return
        }
        if (root.visible && root.activeTab === "stats")
            statsController.load(root.accountFid, root.readContract, root.readEndpoint,
                                 root.statsDays, Boolean(force))
    }
    onActiveTabChanged: {
        root.finishComposerEditing()
        root.scrollToBeginning()
        root.requestStats(false)
    }
    onStatsDaysChanged: root.requestStats(false)
    onAccountFidChanged: root.requestStats(false)
    onAuthenticatedChanged: root.requestStats(false)
    onReadContractChanged: root.requestStats(false)
    onReadEndpointChanged: root.requestStats(false)
    onVisibleChanged: if (visible) root.requestStats(false)
    property bool compact: false
    property bool busy: false
    property int notificationUnreadCount: 0
    property var rows: []
    property int localHour: new Date().getHours()
    property string draftText: ""
    // A browser's on-screen keyboard can shrink the visual viewport. Keep the
    // draft and its actions visible rather than squeezing the full dashboard
    // and floating dock into the remaining height.
    readonly property bool composerFocused: mobileDraftEditor.activeFocus
    // A phone's usable height excludes the shell header and floating dock.
    // Keep navigation shortcuts above future capabilities on short screens.
    function scrollToBeginning() {
        if (root.compact) {
            mobileFlick.cancelFlick()
            mobileFlick.contentY = 0
        }
    }
    readonly property bool densePhone: root.compact && root.height < 840
    readonly property bool androidCompact: root.compact && Qt.platform.os === "android"
    // Dismiss the draft editor without discarding its contents.
    TapHandler {
        enabled: root.compact && root.composerFocused
        onTapped: point => {
            const local = mobileComposerSurface.mapFromItem(
                root, point.position.x, point.position.y)
            if (local.x < 0 || local.y < 0
                    || local.x >= mobileComposerSurface.width
                    || local.y >= mobileComposerSurface.height)
                root.finishComposerEditing()
        }
    }
    // The platform can dismiss its software keyboard while Qt's editor keeps
    // activeFocus. The shell explicitly ends editing on viewport restoration.
    function finishComposerEditing() {
        if (!root.composerFocused)
            return
        mobileDraftEditor.focus = false
        root.forceActiveFocus()
    }

    signal exploreRequested()
    signal composeRequested(string text)
    signal statsReportRequested(string text)
    signal proRequested()
    signal photoRequested()
    signal searchRequested()
    signal notificationsRequested()
    signal channelsRequested()
    signal appsRequested()
    signal settingsRequested()
    signal profileRequested(double fid)
    signal castRequested(string castId)

    readonly property string welcomeName: {
        const source = String(root.displayName || root.username || qsTrId("genycaster.side_nav.genycaster"))
        const trimmed = source.trim()
        if (trimmed.length === 0)
            return qsTrId("genycaster.side_nav.genycaster")
        const parts = trimmed.split(/\s+/)
        return parts.length > 0 ? parts[0] : trimmed
    }
    readonly property string greetingText: (root.localHour < 5
                                             ? qsTrId("genycaster.home.greeting_night")
                                             : root.localHour < 12
                                               ? qsTrId("genycaster.home.greeting_morning")
                                               : root.localHour < 17
                                                 ? qsTrId("genycaster.home.greeting_afternoon")
                                                 : root.localHour < 21
                                                   ? qsTrId("genycaster.home.greeting_evening")
                                                   : qsTrId("genycaster.home.greeting_night")).arg(
                                                       "\u2066" + root.welcomeName + "\u2069")
    implicitWidth: 980
    // One measured content rail for the greeting, composer, GENY banner and
    // workspace cards. No separate per-card desktop indentation.
    readonly property real homeContentMaxWidth: Theme.scaleUi(760)
    // The desktop hub has a generous intrinsic height; on mobile the
    // containing viewport must own the height so its Flickable can scroll.
    implicitHeight: root.compact ? 0 : 790
    property real bottomOverlayInset: 0

    Timer {
        interval: 60000
        repeat: true
        running: true
        onTriggered: root.localHour = new Date().getHours()
    }

    // This hub offers direct shortcuts, not a duplicate feed. Locked future
    // capabilities stay informational and never dispatch navigation or writes.
    function activateHubShortcut(destination) {
        if (destination === "search") root.searchRequested()
        else if (destination === "apps") root.appsRequested()
        else if (destination === "explorer") root.exploreRequested()
        else if (destination === "channels") root.channelsRequested()
        else if (destination === "pro") root.proRequested()
    }

    component HomeTabs: Item {
        implicitHeight: 43
        RowLayout {
            anchors.fill: parent
            spacing: Theme.space1
            layoutDirection: Theme.uiDirection
            GenyTabButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                minimumWidth: 0
                segmented: false
                selected: root.activeTab === "overview"
                text: qsTrId("genycaster.stats.overview")
                onClicked: root.activeTab = "overview"
            }
            GenyTabButton {
                objectName: "homeStatsTab"
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                minimumWidth: 0
                segmented: false
                selected: root.activeTab === "stats"
                text: qsTrId("genycaster.stats.stats")
                onClicked: root.activeTab = "stats"
            }
        }
    }

    component HubWorkspace: ColumnLayout {
        spacing: Theme.space3

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.home.hub_quick_access")
            color: Theme.textPrimary
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightDemiBold
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: [
                    { destination: "explorer", name: qsTrId("genycaster.side_nav.explorer"), description: qsTrId("genycaster.home.hub_discover_detail"), glyph: Theme.iconCompass },
                    { destination: "channels", name: qsTrId("genycaster.side_nav.channels"), description: qsTrId("genycaster.channels.intro"), glyph: Theme.iconGlobe },
                    { destination: "apps", name: qsTrId("genycaster.apps.title"), description: qsTrId("genycaster.home.apps_description"), glyph: Theme.iconApps },
                    { destination: "pro", name: qsTrId("genycaster.pro.title"), description: root.verifiedGenyPro
                          ? qsTrId("genycaster.pro.short")
                          : root.proMembership.status === "verified"
                            ? qsTrId("genycaster.pro.get_pro")
                            : qsTrId("genycaster.pro.check_status"), glyph: Theme.iconSpark }
                ]
                delegate: NeonCard {
                    id: shortcut
                    interactive: !locked && !root.compact
                    ambientNeon: true
                    neonStrength: Theme.neonCardStrength
                    externalHovered: actionMouse.containsMouse && !root.compact
                    externalPointerX: actionMouse.mouseX
                    externalPointerY: actionMouse.mouseY
                    required property var modelData
                    objectName: "homeShortcut_" + modelData.destination
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: root.compact ? (root.androidCompact ? 104 : 116) : 120
                    readonly property bool locked: false
                    fillColor: locked ? Theme.surfaceMuted
                               : actionMouse.pressed ? Theme.surfacePressed
                               : actionMouse.containsMouse ? Theme.surfaceHover : Theme.surface
                    borderWidth: 1
                    borderColor: !locked && actionMouse.containsMouse ? Theme.borderStrong : Theme.borderMuted
                    curveDepth: Theme.curveContainer
                    activeFocusOnTab: !locked
                    Accessible.role: locked ? Accessible.StaticText : Accessible.Button
                    Accessible.name: modelData.name
                    Accessible.description: modelData.description
                    Accessible.onPressAction: if (!locked) root.activateHubShortcut(modelData.destination)
                    Keys.onReturnPressed: if (!locked) root.activateHubShortcut(modelData.destination)
                    Keys.onSpacePressed: if (!locked) root.activateHubShortcut(modelData.destination)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space1

                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            Icon {
                                text: modelData.glyph
                                preferredSize: Theme.iconSizeLarge
                                color: shortcut.locked ? Theme.textTertiary : Theme.accent
                            }
                            Item { Layout.fillWidth: true }
                            Icon {
                                text: shortcut.locked ? "system/lock"
                                      : Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                preferredSize: Theme.iconSizeSmall
                                color: Theme.textTertiary
                            }
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: modelData.name
                            alignToUi: true
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: modelData.description
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: actionMouse
                        anchors.fill: parent
                        enabled: !shortcut.locked
                        hoverEnabled: !root.compact && !shortcut.locked
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activateHubShortcut(modelData.destination)
                    }
                }
            }
        }


    }

    // Replaces the promotional GENY carousel with an actionable, live inbox
    // entry. No fabricated prices, counters, or automatic animation loops.
    component HomeActivityCard: NeonCard {
        id: activityCard
        interactive: true
        ambientNeon: true
        neonStrength: Theme.neonCardStrength
        objectName: "homeActivityCard"
        Layout.fillWidth: true
        Layout.leftMargin: root.compact ? Theme.space3 : 0
        Layout.rightMargin: root.compact ? Theme.space3 : 0
        Layout.preferredHeight: root.compact ? 142 : 124
        fillColor: Theme.surfaceRaised
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderSubtle
        curveDepth: Theme.curveContainer

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection

                Superellipse {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    fillColor: Theme.primarySurface
                    borderWidth: 0
                    curveDepth: Theme.curveControl
                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconNotification
                        preferredSize: Theme.iconSize
                        color: Theme.accent
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: Theme.space1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.home.catch_up")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: !root.authenticated
                              ? qsTrId("genycaster.notifications.sign_in_description")
                              : root.notificationUnreadCount > 0
                                ? LocaleFormat.formatInteger(root.notificationUnreadCount)
                                  + " · " + qsTrId("genycaster.notifications.title")
                                : qsTrId("genycaster.notifications.empty_title")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }
                Superellipse {
                    visible: root.authenticated && root.notificationUnreadCount > 0
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: Math.max(32, activityCount.implicitWidth + Theme.space3)
                    fillColor: Theme.surfaceSelected
                    curveDepth: 16
                    DirectionAwareLabel {
                        id: activityCount
                        anchors.centerIn: parent
                        text: LocaleFormat.formatInteger(root.notificationUnreadCount)
                        color: Theme.accent
                        font.weight: Theme.weightBold
                        font.pixelSize: Theme.typeMetaSize
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                ActionButton {
                    compact: true
                    prominent: true
                    Layout.fillWidth: true
                    iconGlyph: Theme.iconNotification
                    text: qsTrId("genycaster.side_nav.notifications")
                    onClicked: root.notificationsRequested()
                }
                ActionButton {
                    compact: true
                    quiet: true
                    Layout.fillWidth: true
                    iconGlyph: Theme.iconGlobe
                    text: qsTrId("genycaster.side_nav.channels")
                    onClicked: root.channelsRequested()
                }
                ActionButton {
                    compact: true
                    quiet: true
                    Layout.fillWidth: true
                    iconGlyph: Theme.iconCompose
                    text: qsTrId("genycaster.header.new_cast")
                    iconOnly: root.compact
                    onClicked: root.composeRequested(root.draftText)
                }
            }
        }
    }

    RowLayout {
        id: desktopLayout
        visible: !root.compact
        anchors.fill: parent
        // Match the ContextRail widget gutters (Theme.space3) on both sides.
        // The full-width Stats workspace keeps its chart area while using
        // the same outer spacing as the other cards in the shell.
        anchors.margins: Theme.space3
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 560

            ColumnLayout {
                id: desktopHomeContent
                objectName: "homeAlignedContentRail"
                // Overview is a comfortable reading/composing rail, but
                // analytics is a workspace: do not cap it at 760 logical px.
                // Reflow charts against the real available width, including
                // resized windows and large desktop monitors.
                width: root.activeTab === "stats"
                    ? Math.max(1, parent.width)
                    : Math.max(1, Math.min(parent.width - Theme.space4 * 2,
                                           root.homeContentMaxWidth))
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: Theme.space3
                anchors.bottomMargin: Theme.space3
                spacing: Theme.space3

                HomeTabs {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: Math.min(root.homeContentMaxWidth, 420)
                }

                ColumnLayout {
                    visible: root.activeTab === "overview"
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Superellipse {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 66
                        Layout.preferredHeight: 66
                        fillColor: "transparent"
                        borderWidth: 0
                        curveDepth: 33
                        Avatar {
                            anchors.centerIn: parent
                            avatarSize: 52
                            sourceUrl: root.avatarUrl
                            displayName: root.displayName || root.username
                            visible: root.authenticated && root.avatarUrl.length > 0
                        }
                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconLeaf
                            color: Theme.accent
                            preferredSize: 42
                            visible: !root.authenticated || root.avatarUrl.length === 0
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: root.greetingText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeTitle1Size
                        font.weight: Theme.weightBold
                        wrapMode: Text.Wrap
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTrId("genycaster.home.hub_tagline")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
                    }
                }

                NeonSurface {
                    visible: root.activeTab === "overview"
                    id: desktopComposerSurface
                    interactive: true
                    ambientNeon: true
                    neonStrength: Theme.neonCardStrength
                    focused: desktopDraftEditor.activeFocus
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: root.homeContentMaxWidth
                    fillColor: Theme.surface
                    borderWidth: desktopDraftEditor.activeFocus ? Theme.strokeEmphasis : 1
                    borderColor: desktopDraftEditor.activeFocus ? Theme.accent : Theme.borderMuted
                    curveDepth: Theme.curveModal
                    implicitHeight: composerColumn.implicitHeight + Theme.space3 * 2

                    ColumnLayout {
                        id: composerColumn
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2

                        TextArea {
                            id: desktopDraftEditor
                            ContextMenu.menu: EditorContextMenu { editor: desktopDraftEditor }
                            Layout.fillWidth: true
                            implicitHeight: activeFocus || text.length > 0 ? 80 : 56
                            text: root.draftText
                            placeholderText: qsTrId("genycaster.home.composer_placeholder")
                            wrapMode: TextEdit.Wrap
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                            selectByMouse: true
                            background: Item {}
                            onTextChanged: root.draftText = text
                        }

                        RowLayout {
                            id: composerActionRow
                            readonly property bool compactActions: width < 620
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            ActionButton { compact: true; quiet: true; iconOnly: composerActionRow.compactActions; iconGlyph: Theme.iconImage; text: qsTrId("genycaster.home.post_photo"); onClicked: root.photoRequested() }
                            ActionButton { compact: true; quiet: true; iconOnly: composerActionRow.compactActions; iconGlyph: Theme.iconGlobe; text: qsTrId("genycaster.side_nav.channels"); onClicked: root.channelsRequested() }
                            ActionButton { compact: true; quiet: true; iconOnly: composerActionRow.compactActions; iconGlyph: Theme.iconSearch; text: qsTrId("genycaster.feed_header.universal_search"); onClicked: root.searchRequested() }
                            Item { Layout.fillWidth: true }
                            ActionButton {
                                compact: true
                                prominent: true
                                iconGlyph: Theme.iconCompose
                                text: qsTrId("genycaster.header.new_cast")
                                onClicked: root.composeRequested(root.draftText)
                            }
                        }
                    }
                }
HomeActivityCard {
                    visible: root.activeTab === "overview"
                }

                HubWorkspace {
                    visible: root.activeTab === "overview"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: root.homeContentMaxWidth
                }
                Flickable {
                    objectName: "homeDesktopStatsScroll"
                    visible: root.activeTab === "stats"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    contentWidth: width
                    contentHeight: desktopStats.implicitHeight + Theme.space4
                    clip: true
                    flickableDirection: Flickable.VerticalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AsNeeded }
                    StatsPanel {
                        id: desktopStats
                        width: parent.width
                        compact: false
                        tabActive: root.activeTab === "stats"
                        proUnlocked: root.verifiedGenyPro
                        periodDays: root.statsDays
                        onProRequested: root.proRequested()
                        onPeriodRequested: days => root.statsDays = days
                        onRefreshRequested: root.requestStats(true)
                        onShareRequested: text => root.statsReportRequested(text)
                        onCastRequested: hash => root.castRequested(hash)
                    }
                }
                Item {
                    visible: root.activeTab === "overview"
                    Layout.fillHeight: true
                    Layout.minimumHeight: Theme.space2
                }
            }
        }

    }

    Flickable {
        id: mobileFlick
        visible: root.compact
        anchors.fill: parent
        contentWidth: width
        // The former forced 108px tail made Home scroll into an empty area.
        // Real continuation rows now determine the content height instead.
        // Use the measured layout height, with room to scroll the final tiles
        // above the floating action dock. The dock itself stays an overlay.
        contentHeight: mobileColumn.implicitHeight + root.bottomOverlayInset + Theme.space4
        flickableDirection: Flickable.VerticalFlick
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AlwaysOff }

        // When a textarea takes focus, the composer becomes the first
        // meaningful content and the top of the flickable stays reachable.
        Connections {
            target: root
            function onComposerFocusedChanged() {
                if (root.composerFocused)
                    mobileFlick.contentY = 0
            }
        }

        ColumnLayout {
            id: mobileColumn
            width: parent.width
            spacing: root.densePhone ? Theme.space2 : Theme.space3
            // Keep the focused composer clear of the fixed application header
            // even after the navigation tiles are hidden by the keyboard.
            Item { Layout.preferredHeight: root.composerFocused
                                                  ? (root.androidCompact ? Theme.space3 : Theme.space4)
                                                  : root.densePhone ? Theme.space1
                                                  : root.androidCompact ? Theme.space2 : Theme.space3 }
            Superellipse {
                visible: !root.composerFocused && root.activeTab === "overview"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: root.androidCompact
                                       ? (root.densePhone ? 52 : 68)
                                       : (root.densePhone ? 60 : 80)
                Layout.preferredHeight: Layout.preferredWidth
                fillColor: "transparent"
                borderWidth: 0
                curveDepth: width / 2
                Avatar {
                    anchors.centerIn: parent
                    avatarSize: root.androidCompact
                                ? (root.densePhone ? 50 : 64)
                                : (root.densePhone ? 56 : 76)
                    sourceUrl: root.avatarUrl
                    displayName: root.displayName || root.username
                    visible: root.authenticated && root.avatarUrl.length > 0
                }
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconLeaf
                    color: Theme.accent
                    preferredSize: root.androidCompact
                                   ? (root.densePhone ? 34 : 44)
                                   : (root.densePhone ? 38 : 50)
                    visible: !root.authenticated || root.avatarUrl.length === 0
                }
            }
            DirectionAwareLabel { visible: !root.composerFocused && root.activeTab === "overview"; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: root.greetingText; color: Theme.textPrimary; font.pixelSize: root.androidCompact
                ? (root.densePhone ? Theme.typeTitle2Size : Theme.typeTitle1Size)
                : (root.densePhone ? Theme.typeTitle1Size : Theme.typeDisplaySize); font.weight: Theme.weightBold; wrapMode: Text.Wrap }
            DirectionAwareLabel { visible: !root.composerFocused && root.activeTab === "overview"; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTrId("genycaster.home.hub_tagline"); color: Theme.textSecondary; font.pixelSize: Theme.typeMetaSize }

            HomeTabs {
                visible: !root.composerFocused
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: 420
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
            }

            // A single aligned launcher: composing starts in the dedicated
            // full-screen editor, never in a cramped inline keyboard state.
            NeonSurface {
                id: mobileComposerSurface
                ambientNeon: true
                interactive: true
                neonStrength: Theme.neonControlStrength
                objectName: "homeComposerLauncher"
                visible: root.activeTab === "overview"
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                Layout.preferredHeight: root.androidCompact ? 52 : 60
                fillColor: Theme.surface
                borderWidth: 1
                borderColor: Theme.borderMuted
                curveDepth: Theme.curveModal
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTrId("genycaster.home.composer_placeholder")
                Accessible.onPressAction: root.composeRequested(root.draftText)
                Keys.onReturnPressed: root.composeRequested(root.draftText)
                Keys.onSpacePressed: root.composeRequested(root.draftText)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space4
                    anchors.rightMargin: Theme.space4
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.draftText.length > 0 ? root.draftText
                              : qsTrId("genycaster.home.composer_placeholder")
                        font.pixelSize: Theme.typeBodySize
                        color: root.draftText.length > 0 ? Theme.textPrimary : Theme.textSecondary
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    Icon {
                        text: Theme.iconCompose
                        preferredSize: Theme.iconSizeLarge
                        color: Theme.accent
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.composeRequested(root.draftText)
                }
            }
            // Retain this invisible TextArea so legacy focus cleanup and draft
            // bindings stay harmless; there is no inline keyboard entry.
            TextArea {
                id: mobileDraftEditor
                visible: false
                readOnly: true
                text: root.draftText
                onTextChanged: root.draftText = text
            }
HomeActivityCard {
                visible: root.activeTab === "overview"
            }

            StatsPanel {
                objectName: "homeMobileStats"
                visible: root.activeTab === "stats" && !root.composerFocused
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                compact: true
                tabActive: root.activeTab === "stats"
                proUnlocked: root.verifiedGenyPro
                periodDays: root.statsDays
                onProRequested: root.proRequested()
                onPeriodRequested: days => root.statsDays = days
                onRefreshRequested: root.requestStats(true)
                onShareRequested: text => root.statsReportRequested(text)
                onCastRequested: hash => root.castRequested(hash)
            }

            HubWorkspace {
                visible: !root.composerFocused && root.activeTab === "overview"
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                Layout.topMargin: Theme.space2
            }
        }
    }
}