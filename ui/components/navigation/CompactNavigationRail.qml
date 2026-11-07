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
    property bool isAuthenticated: false
    property bool identityAvailable: false
    property bool hasUnreadNotifications: false
    property int notificationUnreadCount: 0

    signal trendingRequested()
    signal followingRequested()
    signal profileRequested()
    signal identityRequested()
    signal settingsRequested()
    signal routeRequested(string route)

    implicitWidth: 76
    color: Theme.backgroundPrimary

    Superellipse {
        id: railSurface
        anchors.fill: parent
        anchors.margins: Theme.space1
        fillColor: Theme.surfacePrimary
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.shapeSurface
        elevation: Theme.elevationControl
        shadowOpacityScale: 0.42
    }

    ColumnLayout {
        anchors.fill: railSurface
        anchors.topMargin: Theme.space3
        anchors.bottomMargin: Theme.space2
        spacing: Theme.space1

        // Use the exact GenyCaster G-arrow mark from the expanded sidebar,
        // not the Genyleap DNA/leaf icon. The SVG already owns its background.
        Item {
            objectName: "compactSidebarGenycasterBrand"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Theme.scaleUi(40)
            Layout.preferredHeight: Theme.scaleUi(40)
            Layout.bottomMargin: Theme.space3
            Image {
                objectName: "compactSidebarGenycasterLogo"
                anchors.fill: parent
                source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                sourceSize.width: 128
                sourceSize.height: 128
                fillMode: Image.PreserveAspectFit
                mipmap: true
                Accessible.name: qsTrId("genycaster.app.genycaster")
            }
        }

        Repeater {
            model: [
                { key: "home", icon: Theme.iconHome, label: qsTrId("genycaster.side_nav.home"), available: true },
                { key: "explorer", icon: Theme.iconExplore, label: qsTrId("genycaster.side_nav.explorer"), available: true },
                { key: "apps", icon: Theme.iconApps, label: qsTrId("genycaster.apps.title"), available: true },
                { key: "channels", icon: Theme.iconGlobe, label: qsTrId("genycaster.side_nav.channels"), available: true },
                { key: "notifications", icon: Theme.iconNotification, label: qsTrId("genycaster.side_nav.notifications"), available: true },
                { key: "bookmarks", icon: Theme.iconBookmark, label: qsTrId("genycaster.side_nav.bookmarks"), available: true }
            ]

            ActionButton {
                required property var modelData
                Layout.alignment: Qt.AlignHCenter
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: modelData.icon
                iconSize: Theme.iconSizeNavigation
                badgeText: modelData.key === "notifications" && root.notificationUnreadCount > 0 && root.shellRoute !== "notifications"
                           ? (root.notificationUnreadCount > 99 ? (LocaleFormat.formatInteger(99) + "+") : LocaleFormat.formatInteger(root.notificationUnreadCount))
                           : ""
                text: modelData.label
                enabled: Boolean(modelData.available)
                selected: modelData.key === "home" ? root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "home"
                          : modelData.key === "explorer" ? root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "explorer"
                          : root.shellRoute === modelData.key
                onClicked: {
                    if (modelData.key === "home") root.followingRequested()
                    else if (modelData.key === "explorer") root.trendingRequested()
                    else root.routeRequested(modelData.key)
                }
            }
        }

        Item { Layout.fillHeight: true }

        ActionButton {
            Layout.alignment: Qt.AlignHCenter
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconProfile
            iconSize: Theme.iconSizeNavigation
            text: qsTrId("genycaster.side_nav.profile")
            selected: root.shellRoute === "" && root.activeView === "profile"
            onClicked: root.isAuthenticated ? root.profileRequested() : root.identityRequested()
        }

        ActionButton {
            Layout.alignment: Qt.AlignHCenter
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconSettings
            iconSize: Theme.iconSizeNavigation
            text: qsTrId("genycaster.settings.title")
            onClicked: root.settingsRequested()
        }
    }
}
