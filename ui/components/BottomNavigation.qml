import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string activeView: "feed"
    property string activeFeed: "trending"
    property string shellRoute: ""
    property string primaryDestination: "home"
    // Retained for existing shell bindings. The mobile footer no longer
    // captures or blurs the live scene (expensive on WebAssembly).
    property Item blurSourceItem: null
    property bool glassEnabled: false
    readonly property color footerColor: "#202128"

    signal trendingRequested()
    signal followingRequested()
    signal createRequested()
    signal settingsRequested()
    signal routeRequested(string route)

    implicitHeight: 88
    clip: false

    // Unlike a floating dock, the dark footer reaches the left, right and
    // bottom viewport edges, including the device's rounded lower corners.
    // There is deliberately no capsule border or outer shadow.
    Rectangle {
        id: footerBackground
        objectName: "mobileFooterBackground"
        anchors.fill: parent
        color: root.footerColor
    }

    // Safari/Chrome-style sheet edge: the light content ends in gently curved
    // *bottom* corners above the uninterrupted dark lower region. Its top
    // corners stay square and the shell reserves room for this overlap.
    Superellipse {
        id: contentLowerEdge
        objectName: "mobileContentLowerEdge"
        anchors.left: parent.left
        anchors.right: parent.right
        y: -7
        height: 14
        fillColor: Theme.background
        curveDepth: 22
        curveTopLeft: false
        curveTopRight: false
        curveBottomLeft: true
        curveBottomRight: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.mobileGutter
        anchors.rightMargin: Theme.mobileGutter
        // Keep the hit targets comfortably above the iPhone's lower edge
        // while leaving the full-width dark safe-area backdrop in place.
        anchors.topMargin: 17
        anchors.bottomMargin: 23
        spacing: 0
        layoutDirection: Theme.uiDirection

        BottomNavButton {
            darkDock: true
            Layout.fillWidth: true
            text: qsTrId("genycaster.bottom_nav.home")
            iconGlyph: Theme.iconHome
            selected: root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "home"
            showLabel: false
            onClicked: root.followingRequested()
        }
        BottomNavButton {
            darkDock: true
            Layout.fillWidth: true
            text: qsTrId("genycaster.side_nav.explorer")
            iconGlyph: Theme.iconCompass
            selected: root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "explorer"
            showLabel: false
            onClicked: root.trendingRequested()
        }
        BottomNavButton {
            darkDock: true
            Layout.fillWidth: true
            text: qsTrId("genycaster.bottom_nav.create")
            iconGlyph: Theme.iconCompose
            showLabel: false
            onClicked: root.createRequested()
        }
        BottomNavButton {
            darkDock: true
            Layout.fillWidth: true
            text: qsTrId("genycaster.apps.title")
            iconGlyph: Theme.iconApps
            selected: root.shellRoute === "apps"
            showLabel: false
            onClicked: root.routeRequested("apps")
        }
        BottomNavButton {
            darkDock: true
            Layout.fillWidth: true
            text: qsTrId("genycaster.settings.title")
            iconGlyph: Theme.iconSettings
            showLabel: false
            onClicked: root.settingsRequested()
        }
    }
}
