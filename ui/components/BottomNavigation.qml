import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

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
    // GraphicsInfo.api can briefly be Unknown/Null while Qt recreates its
    // scenegraph on live theme changes. Treat that as a transient renderer
    // state, not an instruction to turn the glass into a solid surface.
    readonly property bool useBackdropBlur: glassEnabled && blurSourceItem !== null
        && root.visible && root.width > 0 && root.height > 0
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
        readonly property real iconSlot: Math.max(24, Theme.iconSizeNavigation + 2)
        readonly property real expandedWidth: iconSlot + Theme.space1
            + Math.ceil(captionMetrics.advanceWidth) + 16
        readonly property bool labelVisible: active && displayLabel
            && navigationCapsule.width >= expandedWidth + 4 * 44 + 12
        implicitHeight: 48
        implicitWidth: labelVisible ? expandedWidth : 44
        leftPadding: 8
        rightPadding: 8
        topPadding: 0
        bottomPadding: 0
        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: false

        TextMetrics {
            id: captionMetrics
            font: destinationLabel.font
            text: destination.caption
        }
        Layout.fillWidth: true
        Layout.minimumWidth: labelVisible ? expandedWidth : 39
        Layout.preferredWidth: implicitWidth
        Layout.preferredHeight: 48
        flat: true
        hoverEnabled: true
        text: caption
        Accessible.name: caption

        background: Superellipse {
            curveDepth: height / 2
            exponent: 2.0
            // The selected destination is a subtle wash, not an opaque button
            // floating inside the glass navigation capsule.
            fillColor: destination.active
                ? (Theme.dark ? "#24FFFFFF" : "#1E252C3C") : "transparent"
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
        contentItem: Item {
            clip: true
            Row {
                anchors.centerIn: parent
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection
                Icon {
                    width: destination.iconSlot
                    height: destination.iconSlot
                    text: destination.iconGlyph
                    filled: destination.active
                    preferredSize: Theme.iconSizeNavigation
                    color: destination.active ? Theme.ink : Theme.inkMuted
                    Behavior on color {
                        enabled: root.animateDock
                        ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }
                }
                DirectionAwareLabel {
                    id: destinationLabel
                    visible: destination.labelVisible
                    width: Math.ceil(captionMetrics.advanceWidth)
                    height: destination.iconSlot
                    text: destination.caption
                    textFormat: Text.PlainText
                    selectableText: false
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.NoWrap
                }
            }
        }
    }

    RowLayout {
        id: dockRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        // Slightly shorter floating dock, with generous exterior breathing
        // room. Use smaller extra margins on narrow phones so the five
        // destinations retain usable touch widths.
        readonly property real extraInset: root.width >= 375
                                           ? Theme.scaleUi(10) : Theme.scaleUi(4)
        anchors.leftMargin: Theme.mobileGutter + extraInset
        anchors.rightMargin: Theme.mobileGutter + extraInset
        anchors.topMargin: 8
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Superellipse {
            id: navigationCapsule
            objectName: "mobileNavigationCapsule"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: 56
            curveDepth: height / 2
            exponent: 2.0
            fillColor: "transparent"
            borderWidth: 0

            // Blur needs pixels OUTSIDE the visible pill. Cropping the
            // offscreen texture to exactly 56px forces the convolution to
            // repeat its edge pixels, producing the streaks visible over text
            // and high-contrast images on Safari/WebGL.
            //
            // Source coordinates refer to the active page in its own local
            // space. Keep overscan inside that page's bounds so a
            // capsule near the viewport edge never samples transparent black.
            readonly property real blurOverscan: 36
            readonly property point backdropOrigin: {
                // mapFromItem() does not itself subscribe to geometry changes.
                // Observe the dock/viewport geometry so resize, RTL and the
                // selected label cannot leave the capture at its old position.
                root.x; root.y; root.width; root.height
                dockRow.x; dockRow.y
                navigationCapsule.x; navigationCapsule.y
                navigationCapsule.width; navigationCapsule.height
                if (!root.blurSourceItem)
                    return Qt.point(0, 0)
                root.blurSourceItem.x; root.blurSourceItem.y
                root.blurSourceItem.width; root.blurSourceItem.height
                return root.blurSourceItem.mapFromItem(navigationCapsule, 0, 0)
            }
            readonly property real sampleLeft: root.blurSourceItem
                ? Math.min(blurOverscan, Math.max(0, backdropOrigin.x)) : 0
            readonly property real sampleTop: root.blurSourceItem
                ? Math.min(blurOverscan, Math.max(0, backdropOrigin.y)) : 0
            readonly property real sampleRight: root.blurSourceItem
                ? Math.min(blurOverscan, Math.max(0,
                    root.blurSourceItem.width - backdropOrigin.x - width)) : 0
            readonly property real sampleBottom: root.blurSourceItem
                ? Math.min(blurOverscan, Math.max(0,
                    root.blurSourceItem.height - backdropOrigin.y - height)) : 0

            Item {
                id: expandedBackdrop
                x: -navigationCapsule.sampleLeft
                y: -navigationCapsule.sampleTop
                width: navigationCapsule.width + navigationCapsule.sampleLeft
                       + navigationCapsule.sampleRight
                height: navigationCapsule.height + navigationCapsule.sampleTop
                        + navigationCapsule.sampleBottom
                z: 0

                // Capture the final, opaque page in one pass. A transparent
                // text-only source lets the original sharp text show through
                // its blurred alpha, especially after switching appearance.
                ShaderEffectSource {
                    id: capsuleBackdrop
                    objectName: "mobileCapsuleBackdropCapture"
                    anchors.fill: parent
                    visible: false
                    sourceItem: root.useBackdropBlur ? root.blurSourceItem : null
                    sourceRect: Qt.rect(
                        navigationCapsule.backdropOrigin.x - navigationCapsule.sampleLeft,
                        navigationCapsule.backdropOrigin.y - navigationCapsule.sampleTop,
                        expandedBackdrop.width, expandedBackdrop.height)
                    live: root.useBackdropBlur
                    recursive: false
                    hideSource: false
                    smooth: true
                }

                // Repaint after the theme's palette bindings settle. This is
                // owned by the dock, so there is no delayed callback after unload.
                Timer {
                    id: backdropRefresh
                    interval: 0
                    repeat: false
                    onTriggered: {
                        if (root.useBackdropBlur)
                            capsuleBackdrop.scheduleUpdate()
                    }
                }
                Connections {
                    target: Theme
                    function onDarkChanged() { backdropRefresh.restart() }
                }

                Item {
                    id: capsuleMask
                    anchors.fill: parent
                    visible: false
                    layer.enabled: root.useBackdropBlur

                    Rectangle {
                        x: navigationCapsule.sampleLeft
                        y: navigationCapsule.sampleTop
                        width: navigationCapsule.width
                        height: navigationCapsule.height
                        radius: height / 2
                        color: "#FFFFFF"
                        antialiasing: true
                    }
                }

                MultiEffect {
                    objectName: "mobileCapsuleGlassBlur"
                    anchors.fill: parent
                    visible: root.useBackdropBlur
                    source: capsuleBackdrop
                    blurEnabled: true
                    blur: 0.72
                    blurMax: 32
                    blurMultiplier: 0.0
                    saturation: -0.22
                    maskEnabled: true
                    maskSource: capsuleMask
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1.0
                    autoPaddingEnabled: false
                    antialiasing: true
                }
            }
            // Translucent milk-glass tint for contrast; not an opaque pill.
            Superellipse {
                objectName: "mobileCapsuleGlassTint"
                anchors.fill: parent
                z: 1
                curveDepth: height / 2
                exponent: 2.0
                fillColor: root.useBackdropBlur
                    ? Theme.navigationGlassTint : Theme.surfaceElevated
                borderWidth: 0
            }

            // Restrained depth, without a painted footer.
            Superellipse {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: -3
                z: -1
                fillColor: Theme.dark ? "#40000000" : "#1B192236"
                curveDepth: height / 2
                exponent: 2.0
            }

            RowLayout {
                z: 2
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
                    exponent: 2.0
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
