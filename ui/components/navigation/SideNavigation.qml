import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Rectangle {
    id: root

    property string activeView: "feed"
    property string activeFeed: "following"
    property string shellRoute: ""
    property string primaryDestination: "home"
    property bool identityAvailable: false
    property bool isAuthenticated: false
    property bool writeReady: false
    property bool writeBusy: false
    property double accountFid: 0
    property string accountName: ""
    property string accountUsername: ""
    property string accountAvatarUrl: ""
    property var profileDetail: ({})
    property bool profileLoading: false
    property bool hasUnreadNotifications: false
    property int notificationUnreadCount: 0

    property string statusText: ""
    property string errorText: ""
    property string diagnosticText: ""
    property bool diagnosticsEnabled: false
    property bool runtimeReady: false
    property bool offline: false
    property bool sourceUnavailable: false
    property bool networkKnown: false
    property bool stale: false
    property bool busy: false
    property bool readStarted: false
    property bool hasUsableContent: false

    readonly property bool ownsProfileDetail: Number(root.profileDetail.fid || 0) > 0
                                              && Number(root.profileDetail.fid || 0) === Number(root.accountFid)
    // A raw Snapchain profile lacks totals. Keep unknown distinct from zero.
    readonly property int profileFollowers: root.ownsProfileDetail
                                           && root.profileDetail.socialCountsKnown !== false
                                           ? Number(root.profileDetail.followerCount || 0) : -1
    readonly property int profileFollowing: root.ownsProfileDetail
                                           && root.profileDetail.socialCountsKnown !== false
                                           ? Number(root.profileDetail.followingCount || 0) : -1
    readonly property real profileTrustScore: root.ownsProfileDetail && root.profileDetail.trustScore !== undefined
                                              ? Number(root.profileDetail.trustScore) : -1
    readonly property string profileTrustScoreSource: root.ownsProfileDetail
                                                      ? String(root.profileDetail.trustScoreSource || "") : ""

    // The bottom account card is account-scoped, never another person's streak.
    // Fetch verified activity when a session is ready, even before opening Profile.
    property double streakRequestedFid: 0
    // A Farcaster session can exist before the app-owned membership proof is
    // restored. Make one account-scoped silent recovery attempt, never convert
    // missing authority into an invented Basic status.
    property double proRecoveryAttemptedFid: 0
    function requestSidebarPro() {
        if (!root.isAuthenticated || !openProofConnections
            || Number(root.accountFid) <= 0
            || Number(openProofConnections.activeAccountFid) !== Number(root.accountFid)
            || root.proRecoveryAttemptedFid === Number(root.accountFid))
            return
        if (openProofConnections.proEntitlements.status === "verified")
            return
        if (openProofConnections.cloudSessionBusy || openProofConnections.accountProofBusy)
            return
        root.proRecoveryAttemptedFid = Number(root.accountFid)
        openProofConnections.retryProEntitlements()
    }
    function requestSidebarStreak() {
        if(!root.isAuthenticated || !openProofConnections || !openProofConnections.cloudAuthorizationReady
            || Number(root.accountFid)<=0 || openProofConnections.streakBusy)
            return
        if(root.streakRequestedFid===Number(root.accountFid))
            return
        root.streakRequestedFid=Number(root.accountFid)
        openProofConnections.fetchStreak()
    }
    onAccountFidChanged: {
        root.streakRequestedFid=0
        root.proRecoveryAttemptedFid=0
        requestSidebarStreak()
        requestSidebarPro()
    }
    onIsAuthenticatedChanged: {
        if(!root.isAuthenticated) {
            root.streakRequestedFid=0
            root.proRecoveryAttemptedFid=0
        } else {
            requestSidebarStreak()
            requestSidebarPro()
        }
    }
    Component.onCompleted: {
        requestSidebarStreak()
        requestSidebarPro()
    }
    Connections {
        target: openProofConnections ? openProofConnections : null
        function onStateChanged() {
            root.requestSidebarStreak()
            root.requestSidebarPro()
        }
    }

    signal trendingRequested()
    signal followingRequested()
    signal profileRequested()
    signal identityRequested()
    signal accountRequested()
    signal settingsRequested()
    signal proRequested()
    signal routeRequested(string route)
    signal retryRequested()

    implicitWidth: Theme.navigationWidth
    color: "transparent"

    Superellipse {
        id: navigationSurface
        anchors.fill: parent
        anchors.margins: Theme.space2
        fillColor: Theme.surfacePrimary
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.shapeSurface
    }

    ColumnLayout {
        anchors.fill: navigationSurface
        anchors.margins: Theme.space3
        spacing: Theme.space2

        // Keep the brand assets in a fixed, non-mirrored visual group.
        // RTL may reposition the group, but may never separate/reverse the
        // symbol, Latin wordmark or localized version caption.
        Item {
            id: navigationBrand
            objectName: "sidebarBrandCluster"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.scaleUi(52)
            Layout.leftMargin: Theme.space1
            Layout.rightMargin: Theme.space1
            LayoutMirroring.enabled: false
            LayoutMirroring.childrenInherit: false
            readonly property real markSize: Theme.scaleUi(35)
            readonly property real brandGap: Theme.space2

            Image {
                id: navigationBrandSymbol
                width: navigationBrand.markSize
                height: navigationBrand.markSize
                x: Theme.rtl ? navigationBrand.width - width : 0
                y: (navigationBrand.height - height) / 2
                source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                sourceSize.width: 70
                sourceSize.height: 70
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }
            Item {
                id: navigationBrandText
                readonly property real maxWidth: Math.max(0,navigationBrand.width
                                            - navigationBrand.markSize-navigationBrand.brandGap)
                width: maxWidth
                height: navigationBrand.height
                x: Theme.rtl ? 0 : navigationBrandSymbol.width + navigationBrand.brandGap
                LayoutMirroring.enabled: false
                LayoutMirroring.childrenInherit: false
                Image {
                    id: navigationWordmark
                    width: parent.width
                    height: Theme.scaleUi(22)
                    y: Theme.scaleUi(3)
                    horizontalAlignment: Theme.rtl ? Image.AlignRight : Image.AlignLeft
                    fillMode: Image.PreserveAspectFit
                    source: Theme.dark
                            ? "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark-dark.svg"
                            : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark.svg"
                    sourceSize.width: 340
                    sourceSize.height: 44
                    Accessible.name: qsTrId("genycaster.app.genycaster")
                }
                Text {
                    width: parent.width
                    y: navigationWordmark.y + navigationWordmark.height + Theme.space1
                    text: Qt.application.version ? "v" + LocaleFormat.localizeDigits(Qt.application.version) : ""
                    color: Theme.inkMuted
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Theme.rtl ? Text.AlignRight : Text.AlignLeft
                    elide: Text.ElideRight
                    renderType: Text.NativeRendering
                    Accessible.ignored: true
                }
            }
        }

        SystemStatusCard {
            Layout.fillWidth: true
            statusText: root.statusText
            errorText: root.errorText
            diagnosticText: root.diagnosticText
            diagnosticsEnabled: root.diagnosticsEnabled
            runtimeReady: root.runtimeReady
            offline: root.offline
            sourceUnavailable: root.sourceUnavailable
            networkKnown: root.networkKnown
            stale: root.stale
            busy: root.busy
            readStarted: root.readStarted
            hasUsableContent: root.hasUsableContent
            onRetryRequested: root.retryRequested()
        }

        ScrollView {
            id: navigationScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                width: navigationScroll.availableWidth
                height: Math.max(implicitHeight, navigationScroll.availableHeight)
                spacing: Theme.space1

                NavItem {
                    objectName: "sidebarAction-following"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.side_nav.home")
                    iconGlyph: Theme.iconHome
                    selected: root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "home"
                    onClicked: root.followingRequested()
                }
                NavItem {
                    objectName: "sidebarAction-trending"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.side_nav.explorer")
                    iconGlyph: Theme.iconExplore
                    selected: root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "explorer"
                    onClicked: root.trendingRequested()
                }
                NavItem {
                    objectName: "sidebarAction-apps"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.apps.title")
                    iconGlyph: Theme.iconApps
                    selected: root.shellRoute === "apps"
                    onClicked: root.routeRequested("apps")
                }
                NavItem {
                    objectName: "sidebarAction-channels"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.side_nav.channels")
                    iconGlyph: Theme.iconGlobe
                    selected: root.shellRoute === "channels"
                    onClicked: root.routeRequested("channels")
                }
                NavItem {
                    objectName: "sidebarAction-notifications"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.side_nav.notifications")
                    iconGlyph: Theme.iconNotification
                    badgeText: root.notificationUnreadCount > 0 && root.shellRoute !== "notifications"
                               ? (root.notificationUnreadCount > 99 ? (LocaleFormat.formatInteger(99) + "+") : LocaleFormat.formatInteger(root.notificationUnreadCount))
                               : ""
                    selected: root.shellRoute === "notifications"
                    onClicked: root.routeRequested("notifications")
                }
                NavItem {
                    objectName: "sidebarAction-bookmarks"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.side_nav.bookmarks")
                    iconGlyph: Theme.iconBookmark
                    selected: root.shellRoute === "bookmarks"
                    onClicked: root.routeRequested("bookmarks")
                }
                NavItem {
                    objectName: "sidebarAction-profile"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.side_nav.profile")
                    iconGlyph: Theme.iconProfile
                    selected: root.shellRoute === "" && root.activeView === "profile"
                    onClicked: root.isAuthenticated ? root.profileRequested() : root.accountRequested()
                }
                NavItem {
                    objectName: "sidebarAction-settings"
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.settings.title")
                    iconGlyph: Theme.iconSettings
                    onClicked: root.settingsRequested()
                }

                Item {
                    Layout.fillHeight: true
                    Layout.minimumHeight: Theme.space3
                }
            }
        }

        ProfileTrustCard {
            id: fixedProfileCard
            visible: root.isAuthenticated
            Layout.fillWidth: true
            Layout.topMargin: Theme.space1
            Layout.maximumHeight: implicitHeight
            authenticated: root.isAuthenticated
            fid: root.accountFid
            displayName: root.accountName
            username: root.accountUsername
            avatarUrl: root.accountAvatarUrl
            bio: root.ownsProfileDetail ? String(root.profileDetail.bio || "") : ""
            location: root.ownsProfileDetail ? String(root.profileDetail.location || "") : ""
            verified: root.ownsProfileDetail && Boolean(root.profileDetail.verified || false)
            proEligible: openProofConnections
                && openProofConnections.proEntitlements.status === "verified"
                && openProofConnections.proEntitlements.eligible === true
            proStatus: openProofConnections
                ? String(openProofConnections.proEntitlements.status || "unavailable") : "unavailable"
            proChecking: openProofConnections
                && (openProofConnections.proEntitlementsBusy
                    || openProofConnections.accountProofBusy
                    || openProofConnections.cloudSessionBusy)
            following: root.profileFollowing
            followers: root.profileFollowers
            streakSnapshot: openProofConnections ? openProofConnections.streak : ({})
            loading: root.profileLoading || (root.isAuthenticated && !root.ownsProfileDetail)
            compact: true
            onProfileRequested: root.profileRequested()
            onAccountRequested: root.accountRequested()
            onProRequested: root.proRequested()
        }
    }
}
