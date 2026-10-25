import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    signal trendingRequested()
    signal followingRequested()
    signal profileRequested()
    signal identityRequested()
    signal accountRequested()
    signal settingsRequested()
    signal routeRequested(string route)
    signal retryRequested()

    implicitWidth: Theme.navigationWidth
    color: Theme.backgroundPrimary

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

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space1
            Layout.rightMargin: Theme.space1
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Image {
                Layout.preferredWidth: 35
                Layout.preferredHeight: 35
                source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                sourceSize.width: 70
                sourceSize.height: 70
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Image {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 19
                    horizontalAlignment: Image.AlignLeft
                    fillMode: Image.PreserveAspectFit
                    source: Theme.dark
                            ? "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark-dark.svg"
                            : "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-wordmark.svg"
                    sourceSize.width: 340
                    sourceSize.height: 44
                    Accessible.name: "GenyCaster"
                }
                Label {
                    text: qsTrId("genycaster.release.beta_1_0")
                    color: Theme.inkMuted
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
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
            following: root.profileFollowing
            followers: root.profileFollowers
            trustScore: root.profileTrustScore
            trustScoreSource: root.profileTrustScoreSource
            loading: root.profileLoading || (root.isAuthenticated && !root.ownsProfileDetail)
            compact: true
            onProfileRequested: root.profileRequested()
            onAccountRequested: root.accountRequested()
        }
    }
}
