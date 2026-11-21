import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window
import Farcaster.Client

// Compact navigation dock: five stable destinations in one
// quiet capsule, one independently configurable primary action outside it.
// Safe-area is still reserved by AppShell; never overlay the device gesture bar.
Item {
    id: root
    objectName: "mobileActionDock"

    property string activeView: "feed"
    property string activeFeed: "trending"
    property string shellRoute: ""
    property string primaryDestination: "home"
    property bool widgetsOpen: false
    property Item blurSourceItem: null
    // Reuse AppShell's shared (already blurred) color field, rather than
    // capturing the scrolling page a second time for chromatic NEON.
    property NeonReflectionSource reflectionSource: null
    property bool glassEnabled: false
    // GraphicsInfo.api can briefly be Unknown/Null while Qt recreates its
    // scenegraph on live theme changes. Treat that as a transient renderer
    // state, not an instruction to turn the glass into a solid surface.
    readonly property bool useBackdropBlur: glassEnabled && blurSourceItem !== null
        && root.visible && root.width > 0 && root.height > 0
    // Six destinations need compact glyph slots on phones. The selected
    // label is an enhancement reserved for wide handsets.
    readonly property bool smallHandset: width < 390
    // Use Qt's logical viewport and actual gesture inset, not physical
    // framebuffer pixels (720px on the emulator is usually 360dp).
    readonly property real gestureInset: Math.max(0,
        root.SafeArea.margins.bottom,
        root.Window.window ? root.Window.window.SafeArea.margins.bottom : 0)
    readonly property bool showSelectedLabel: width >= 450
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
    signal widgetsRequested()
    signal profileRequested()
    signal routeRequested(string route)

    implicitHeight: (Qt.platform.os === "android"
                     ? (root.smallHandset ? 70 : 74)
                     : (root.smallHandset ? 80 : 88)) + root.gestureInset
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
            && navigationCapsule.width >= expandedWidth + 5 * 40 + 12
        implicitHeight: root.smallHandset ? 44 : 48
        implicitWidth: labelVisible ? expandedWidth : (root.smallHandset ? 38 : 44)
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
        Layout.minimumWidth: labelVisible ? expandedWidth : (root.smallHandset ? 32 : 39)
        Layout.preferredWidth: implicitWidth
        Layout.preferredHeight: root.smallHandset ? 44 : 48
        flat: true
        hoverEnabled: true
        text: caption
        Accessible.name: caption

        background: Item {
            NeonSurface {
                // The Layout may stretch each navigation hit target, but the
                // selected highlight must remain an actual circle. Only the
                // optional wide-screen text mode uses a capsule.
                width: destination.labelVisible ? parent.width
                    : Math.min(parent.width, parent.height)
                height: parent.height
                anchors.centerIn: parent
                ambientNeon: destination.active
                interactive: false
                neonStrength: Theme.neonControlStrength
                neonColor: Qt.rgba(Theme.navigationGlassInk.r,
                                   Theme.navigationGlassInk.g,
                                   Theme.navigationGlassInk.b, 0.25)
                curveDepth: height / 2
                exponent: 2.0
                highQualityEdges: true
                fillColor: destination.active
                    ? Theme.dockSelectedWash : "transparent"
                borderWidth: 0
                scale: destination.down ? 0.96 : destination.active ? 1 : 0.93

                Behavior on scale {
                    enabled: root.animateDock
                    NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                }
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
                    color: destination.active ? Theme.navigationGlassInk : Theme.navigationGlassInkMuted
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
                    color: Theme.navigationGlassInk
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
        readonly property real extraInset: root.smallHandset ? 0 : Theme.scaleUi(10)
        anchors.leftMargin: (root.smallHandset ? Theme.space2 : Theme.mobileGutter) + extraInset
        anchors.rightMargin: (root.smallHandset ? Theme.space2 : Theme.mobileGutter) + extraInset
        anchors.topMargin: Qt.platform.os === "android" ? 4 : 8
        spacing: root.smallHandset ? Theme.space1 : Theme.space2
        layoutDirection: Theme.uiDirection

        Superellipse {
            id: navigationCapsule
            objectName: "mobileNavigationCapsule"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: root.smallHandset ? 50 : 56
            curveDepth: height / 2
            exponent: 2.0
            fillColor: "transparent"
            borderWidth: 0

            // Blur needs pixels OUTSIDE the visible pill. Cropping the
            // offscreen texture to exactly 56px forces the convolution to
            // repeat its edge pixels, producing the streaks visible over text
            // and high-contrast images in the browser renderer.
            //
            // Source coordinates refer to the active page in its own local
            // space. Keep overscan inside that page's bounds so a
            // capsule near the viewport edge never samples transparent black.
            readonly property real blurOverscan: 48
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
                    // Render the shape through an explicit texture source below.
                    // Relying on an invisible Item's layer as maskSource can
                    // produce an empty mask on some mobile scenegraph backends.
                    // The same analytic pill geometry used by the visible dock
                    // also drives the offscreen alpha-mask source.
                    Superellipse {
                        x: navigationCapsule.sampleLeft
                        y: navigationCapsule.sampleTop
                        width: navigationCapsule.width
                        height: navigationCapsule.height
                        curveDepth: height / 2
                        exponent: 2
                        fillColor: Theme.dockMaskWhite
                    }
                }

                ShaderEffectSource {
                    id: capsuleMaskTexture
                    anchors.fill: parent
                    visible: false
                    sourceItem: capsuleMask
                    sourceRect: Qt.rect(0, 0, capsuleMask.width, capsuleMask.height)
                    live: root.useBackdropBlur
                    recursive: false
                    hideSource: false
                }

                MultiEffect {
                    objectName: "mobileCapsuleGlassBlur"
                    anchors.fill: parent
                    visible: root.useBackdropBlur
                    source: capsuleBackdrop
                    blurEnabled: true
                    blur: 0.87
                    blurMax: 32
                    // Extend the radius beyond blurMax without increasing
                    // texture lookups; the Qt 6 default (0) simply leaves
                    // the radius unextended.
                    blurMultiplier: 1.3
                    saturation: -0.22
                    maskEnabled: true
                    maskSource: capsuleMaskTexture
                    maskThresholdMin: 0.5
                    // Preserve fractional edge coverage instead of converting
                    // the antialiased pill mask into a hard binary cutout.
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
                highQualityEdges: true
                fillColor: root.useBackdropBlur
                    ? Theme.navigationGlassTint : Theme.surfaceElevated
                borderWidth: 0
            }

            // The same content-derived chromatic material as the header.
            // The shader masks to the capsule's superellipse geometry;
            // no rectangular clipping or hard reflected divider is used.
            NeonReflection {
                objectName: "mobileNavigationChromaticReflection"
                anchors.fill: parent
                z: 1.5
                reflectionSource: root.reflectionSource
                strength: Theme.reflectionStrength * 1.45
                cornerRadius: navigationCapsule.height / 2
                cornerPower: 2.0
                borderInset: 0
                edgeStrength: 0
                bottomEdgeOnly: false
            }

            // Restrained depth, without a painted footer.
            Superellipse {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: -3
                z: -1
                fillColor: Theme.dockShadowTint
                curveDepth: height / 2
                exponent: 2.0
            }

            RowLayout {
                z: 2
                anchors.fill: parent
                anchors.margins: root.smallHandset ? 3 : 4
                spacing: root.smallHandset ? 0 : 1
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
                    objectName: "mobileDockWidgets"
                    active: root.widgetsOpen
                    displayLabel: root.showSelectedLabel
                    caption: qsTrId("genycaster.context.widgets")
                    iconGlyph: Theme.iconChart
                    onClicked: root.widgetsRequested()
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
            Layout.preferredHeight: root.smallHandset ? 48 : 52
            Layout.preferredWidth: root.smallHandset ? 48 : 52
            Layout.minimumWidth: root.smallHandset ? 48 : 52
            Layout.alignment: Qt.AlignVCenter
            flat: true
            hoverEnabled: true
            text: Theme.mobileAction === "photo" ? qsTrId("genycaster.home.post_photo")
                : Theme.mobileAction === "search" ? qsTrId("genycaster.settings.mobile_action_search")
                : Theme.mobileAction === "settings" ? qsTrId("genycaster.settings.appearance")
                : qsTrId("genycaster.bottom_nav.create")
            Accessible.name: text
            // Use the same themed fill, superellipse edges and restrained
            // elevation as the desktop FloatingComposeButton. The optional
            // photo/search/settings actions retain their chosen accent.
            background: Superellipse {
                curveDepth: height / 2
                exponent: Theme.superellipseExponent
                fillColor: mobilePrimaryAction.down && Theme.mobileActionColorPreset === "theme"
                    ? Theme.primaryPressed : Theme.mobileActionColor
                borderWidth: Theme.keyboardFocusVisible(mobilePrimaryAction)
                    ? 2 : Theme.strokeHairline
                borderColor: Theme.keyboardFocusVisible(mobilePrimaryAction)
                    ? Theme.focusOnAccent : Qt.rgba(Theme.accentInk.r,
                        Theme.accentInk.g, Theme.accentInk.b, 0.16)
                highQualityEdges: true
                elevation: Theme.elevationPopover
                shadowColor: Theme.shadow
                shadowOpacityScale: mobilePrimaryAction.down ? 0.4 : 0.94
                scale: mobilePrimaryAction.down ? 0.975 : 1
                Behavior on scale {
                    enabled: root.animateDock
                    NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                }
                Behavior on fillColor {
                    enabled: root.animateDock
                    ColorAnimation { duration: 190; easing.type: Easing.OutCubic }
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
