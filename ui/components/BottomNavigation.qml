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
    // The footer owns touch targets only: no full-width painted backdrop.
    // Page content keeps its bottom reservation so lists never end beneath
    // these floating controls.
    readonly property bool animateDock: !Theme.reduceMotion

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

    // A genuinely floating dock: the root is fully transparent. No footer
    // rectangle or curved page separator can cover the scrollable surface.
    // Reserve bottom space in AppShell instead of painting a dark footer.
    component DockDestination: Button {
        id: destination
        property bool active: false
        property string caption: ""
        property string iconGlyph: ""
        property bool displayLabel: false
        implicitHeight: 49
        implicitWidth: active && displayLabel ? 87 : 43
        Behavior on implicitWidth {
            enabled: root.animateDock
            NumberAnimation { duration: 210; easing.type: Easing.OutCubic }
        }
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
                ? (Theme.dark ? "#505460" : "#E7EAF0") : "transparent"
            borderWidth: 0
            scale: destination.down ? 0.96 : destination.active ? 1 : 0.93
            Behavior on fillColor {
                enabled: root.animateDock
                ColorAnimation { duration: 185; easing.type: Easing.OutCubic }
            }
            Behavior on scale {
                enabled: root.animateDock
                NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
            }
        }
        contentItem: RowLayout {
            spacing: Theme.space1
            layoutDirection: Theme.uiDirection
            Item { Layout.fillWidth: true }
            Icon {
                text: destination.iconGlyph
                filled: destination.active
                preferredSize: Theme.iconSizeNavigation
                color: destination.active ? Theme.ink : Theme.inkMuted
                scale: destination.active ? 1.07 : 1.0
                Behavior on scale {
                    enabled: root.animateDock
                    NumberAnimation { duration: 210; easing.type: Easing.OutCubic }
                }
                Behavior on color {
                    enabled: root.animateDock
                    ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
                }
            }
            DirectionAwareLabel {
                visible: destination.active && destination.displayLabel
                opacity: visible ? 1 : 0
                Behavior on opacity {
                    enabled: root.animateDock
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }
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
        anchors.topMargin: 8
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Superellipse {
            objectName: "mobileNavigationCapsule"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: 56
            curveDepth: height / 2
            fillColor: Theme.dark ? "#31343D" : "#F9FAFC"
            borderWidth: 0
            // Deliberately restrained material separation. No blur capture or
            // full-width glass layer: WebAssembly remains inexpensive.
            Superellipse {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: -3
                z: -1
                fillColor: Theme.dark ? "#40000000" : "#1B192236"
                curveDepth: height / 2
            }

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
                borderWidth: 0
                // Shadow is a single offset vector shape, not a blur pass.
                Superellipse {
                    anchors.fill: parent
                    anchors.topMargin: 3
                    anchors.bottomMargin: -3
                    z: -1
                    fillColor: Theme.dark ? "#48000000" : "#28000000"
                    curveDepth: height / 2
                }
                scale: mobilePrimaryAction.down ? 0.91 : 1
                Behavior on scale {
                    enabled: root.animateDock
                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
                Behavior on fillColor {
                    enabled: root.animateDock
                    ColorAnimation { duration: 230; easing.type: Easing.OutCubic }
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
