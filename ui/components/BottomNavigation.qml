import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// iOS Action Button-inspired mobile dock: five stable destinations in one
// quiet capsule, one independently configurable primary action outside it.
// Safe-area is still reserved by AppShell; never overlay the device gesture bar.
Item {
    id: root
    objectName: "mobileActionDock"

    property string activeView: "feed"
    property string activeFeed: "trending"
    property string shellRoute: ""
    property string primaryDestination: "home"
    property Item blurSourceItem: null
    property bool glassEnabled: false
    readonly property bool showSelectedLabel: width >= 375
    readonly property color footerColor: Theme.dark ? "#202128" : Theme.background

    signal trendingRequested()
    signal followingRequested()
    signal createRequested()
    signal photoRequested()
    signal searchRequested()
    signal settingsRequested()
    signal profileRequested()
    signal routeRequested(string route)

    implicitHeight: 88
    clip: false

    Rectangle {
        id: footerBackground
        objectName: "mobileFooterBackground"
        anchors.fill: parent
        color: root.footerColor
    }

    // The reading surface has bottom-rounded corners over the dock; no
    // performance-heavy scene capture or full-screen blur on WASM.
    Superellipse {
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

    component DockDestination: Button {
        id: destination
        property bool active: false
        property string caption: ""
        property string iconGlyph: ""
        property bool displayLabel: false
        implicitHeight: 49
        implicitWidth: active && displayLabel ? 87 : 43
        Layout.fillWidth: true
        Layout.minimumWidth: 39
        Layout.preferredWidth: implicitWidth
        Layout.preferredHeight: 49
        flat: true
        hoverEnabled: true
        text: caption
        Accessible.name: caption

        background: Superellipse {
            curveDepth: height / 2
            fillColor: destination.active
                ? (Theme.dark ? "#454952" : "#E4E6EB") : "transparent"
            borderWidth: destination.active ? Theme.strokeHairline : 0
            borderColor: Theme.dark ? "#646873" : "#D5D9E1"
        }
        contentItem: RowLayout {
            spacing: Theme.space1
            layoutDirection: Theme.uiDirection
            Item { Layout.fillWidth: true }
            Icon {
                text: destination.iconGlyph
                filled: destination.active
                preferredSize: Theme.iconSizeNavigation
                color: destination.active ? Theme.ink
                    : Theme.inkMuted
            }
            DirectionAwareLabel {
                visible: destination.active && destination.displayLabel
                text: destination.caption
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
                color: Theme.ink
                Layout.maximumWidth: 67
                elide: Text.ElideRight
            }
            Item { Layout.fillWidth: true }
        }
    }

    RowLayout {
        id: dockRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: Theme.mobileGutter
        anchors.rightMargin: Theme.mobileGutter
        anchors.topMargin: 10
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Superellipse {
            objectName: "mobileNavigationCapsule"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: 56
            curveDepth: height / 2
            fillColor: Theme.dark ? "#31343C" : "#F8F9FC"
            borderWidth: 1
            borderColor: Theme.dark ? "#60646F" : "#D8DBE2"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 1
                layoutDirection: Theme.uiDirection

                DockDestination {
                    objectName: "mobileDockHome"
                    active: root.shellRoute === "" && root.activeView === "feed"
                        && root.primaryDestination === "home"
                    displayLabel: root.showSelectedLabel
                    caption: qsTrId("genycaster.bottom_nav.home")
                    iconGlyph: Theme.iconHome
                    onClicked: root.followingRequested()
                }
                DockDestination {
                    objectName: "mobileDockExplore"
                    active: root.shellRoute === "" && root.activeView === "feed"
                        && root.primaryDestination === "explorer"
                    displayLabel: root.showSelectedLabel
                    caption: qsTrId("genycaster.side_nav.explorer")
                    iconGlyph: Theme.iconCompass
                    onClicked: root.trendingRequested()
                }
                DockDestination {
                    objectName: "mobileDockApps"
                    active: root.shellRoute === "apps"
                    displayLabel: root.showSelectedLabel
                    caption: qsTrId("genycaster.apps.title")
                    iconGlyph: Theme.iconApps
                    onClicked: root.routeRequested("apps")
                }
                DockDestination {
                    objectName: "mobileBottomBookmarksButton"
                    active: root.shellRoute === "bookmarks"
                    displayLabel: root.showSelectedLabel
                    caption: qsTrId("genycaster.side_nav.bookmarks")
                    iconGlyph: Theme.iconBookmark
                    onClicked: root.routeRequested("bookmarks")
                }
                DockDestination {
                    objectName: "mobileDockProfile"
                    active: root.shellRoute === "" && root.activeView === "profile"
                    displayLabel: root.showSelectedLabel
                    caption: qsTrId("genycaster.side_nav.profile")
                    iconGlyph: Theme.iconProfile
                    onClicked: root.profileRequested()
                }
            }
        }

        Button {
            id: mobilePrimaryAction
            objectName: "mobilePrimaryActionButton"
            Layout.preferredHeight: 56
            Layout.preferredWidth: 56
            Layout.minimumWidth: 56
            Layout.alignment: Qt.AlignVCenter
            flat: true
            hoverEnabled: true
            text: Theme.mobileAction === "photo" ? qsTrId("genycaster.home.post_photo")
                : Theme.mobileAction === "search" ? qsTrId("genycaster.settings.mobile_action_search")
                : Theme.mobileAction === "settings" ? qsTrId("genycaster.settings.appearance")
                : qsTrId("genycaster.bottom_nav.create")
            Accessible.name: text
            background: Superellipse {
                curveDepth: height / 2
                exponent: 2.01
                fillColor: Theme.mobileActionColor
                borderWidth: 1
                borderColor: Qt.rgba(1, 1, 1, 0.32)
                scale: mobilePrimaryAction.down ? 0.94 : 1
                Behavior on scale {
                    enabled: !Theme.reduceMotion
                    NumberAnimation { duration: Theme.motionFast }
                }
            }
            contentItem: Icon {
                text: Theme.mobileActionIcon
                color: Theme.contrastInk(Theme.mobileActionColor)
                preferredSize: Theme.iconSizeLarge
            }
            onClicked: {
                switch (Theme.mobileAction) {
                case "photo": root.photoRequested(); break
                case "search": root.searchRequested(); break
                case "settings": root.settingsRequested(); break
                default: root.createRequested(); break
                }
            }
        }
    }
}
