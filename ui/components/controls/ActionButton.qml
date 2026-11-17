import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property bool prominent: false
    // Neutral (ink-filled) CTA uses the same material, geometry and input path.
    property bool neutralPrimary: false
    // Opt-in to a shared adjacent capture. Ordinary feed actions never
    // allocate their own texture or reflect ancestor content.
    property NeonReflectionSource reflectionSource: null
    property bool quiet: false
    property bool destructive: false
    property bool destructiveFilled: false
    property bool positive: false
    property bool selected: false
    property color customIconColor: "transparent"
    property color customForegroundColor: "transparent"
    // A copy action can acknowledge successful clipboard dispatch without
    // closing its menu before the user sees the result.
    property bool copyConfirmed: false
    property bool copyFailed: false
    function acknowledgeCopy(succeeded) {
        control.copyConfirmed = succeeded === true
        control.copyFailed = succeeded === false
    }
    function clearCopyFeedback() {
        control.copyConfirmed = false
        control.copyFailed = false
    }
    property color customSelectedColor: "transparent"
    property color customSelectedSurface: "transparent"
    property string iconGlyph: ""
    property bool iconOnly: false
    property bool compact: false
    property bool wrapText: false
    property bool alignStart: false
    // Keep the label optically centered when a leading icon is present.
    // Opt-in for symmetric compact navigation actions.
    property bool balancedIcon: false
    property bool pill: false
    property bool elevated: true
    property real iconSize: Theme.iconSize
    property bool badgeDot: false
    property string badgeText: ""
    property bool busy: false
    // Opt in for refresh actions that keep their recognizable glyph rotating
    // while the backing request is actually in progress.
    property bool spinIconWhileBusy: false

    // Match the reference NeonSurface: pointer tracking belongs to the
    // actual interactive Control, not a ShaderEffect hidden in background.
    HoverHandler {
        id: neonPointer
        enabled: Theme.neonEnabled && !control.mobileViewport
        blocking: false
    }

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    implicitWidth: iconOnly ? implicitHeight : Math.max(68, contentRow.implicitWidth + leftPadding + rightPadding)
    implicitHeight: wrapText
                    ? Math.max(compact ? Theme.compactControlHeight : Theme.controlHeight,
                               contentRow.implicitHeight + Theme.space2 * 2)
                    : compact ? Theme.compactControlHeight : Theme.controlHeight
    leftPadding: iconOnly ? 0 : (compact ? Theme.space3 : Theme.space4)
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0
    // Touch browsers synthesize a lingering hover after tap. Keep the hint
    // only for pointer-capable desktop layouts, never for mobile controls.
    readonly property bool mobileViewport: control.Window.window !== null
                                          && control.Window.window.width < Theme.breakpointMobile
    hoverEnabled: !mobileViewport
    containmentMask: background
    Accessible.name: text

    onHoveredChanged: {
        if (control.mobileViewport || !control.iconOnly
                || control.text.length === 0 || !control.hovered) {
            hintTimer.stop()
            if (hintLoader.item)
                hintLoader.item.close()
            return
        }
        hintTimer.restart()
    }
    onMobileViewportChanged: {
        if (control.mobileViewport) {
            hintTimer.stop()
            if (hintLoader.item)
                hintLoader.item.close()
        }
    }
    onPressed: {
        hintTimer.stop()
        if (hintLoader.item)
            hintLoader.item.close()
    }

    Timer {
        id: hintTimer
        interval: 420
        repeat: false
        onTriggered: {
            if (control.mobileViewport || !control.hovered || !control.iconOnly
                    || control.text.length === 0 || !control.Window.window)
                return
            hintLoader.active = true
            const hint = hintLoader.item
            if (!hint)
                return
            const overlay = control.Overlay.overlay
            if (!overlay)
                return
            const point = control.mapToItem(overlay, 0, 0)
            hint.x = Math.max(Theme.space2,
                             Math.min(overlay.width - hint.width - Theme.space2,
                                      point.x + (control.width - hint.width) / 2))
            hint.y = Math.max(Theme.space2, point.y - hint.height - Theme.space2)
            hint.open()
        }
    }

    Loader {
        id: hintLoader
        active: false
        sourceComponent: Component {
            Popup {
                objectName: "actionHint"
                parent: control.Overlay.overlay
                modal: false
                focus: false
                padding: Theme.space2
                closePolicy: Popup.NoAutoClose
                background: PopupSurface {
                    curveDepth: Theme.curveCompact
                    elevation: Theme.elevationPopover
                }
                contentItem: DirectionAwareLabel {
                    text: control.text
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeCaptionSize
                }
                enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionFast } }
                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.motionFast } }
            }
        }
    }

    scale: control.down ? 0.985 : 1.0
    opacity: control.enabled || control.busy ? 1.0 : 0.58

    Behavior on opacity {
        NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
    }

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    // Destructive semantics must retain their danger hue even when a write
    // operation temporarily disables the action (opacity indicates disabled).
    readonly property color foregroundColor: destructiveFilled ? Theme.dangerInk
                                               : destructive ? Theme.danger
                                               : copyFailed ? Theme.danger
                                               : copyConfirmed ? Theme.success
                                               : customForegroundColor.a > 0 ? customForegroundColor
                                               : !enabled && !busy ? Theme.inkDisabled
                                               : neutralPrimary ? Theme.surface
                                               : positive ? Theme.contrastInk(Theme.success)
                                               : prominent ? Theme.accentInk
                                               : selected ? (customSelectedColor.a > 0 ? customSelectedColor : Theme.accent)
                                               : Theme.ink

    contentItem: RowLayout {
        id: contentRow
        spacing: control.iconOnly ? 0 : Theme.space2
        layoutDirection: Theme.uiDirection

        GenySpinner {
            visible: control.busy && !control.spinIconWhileBusy
            running: visible
            color: control.foregroundColor
            Layout.preferredWidth: Math.min(18, control.iconSize)
            Layout.preferredHeight: Layout.preferredWidth
            Layout.alignment: Qt.AlignCenter
        }

        Icon {
            id: actionGlyph
            visible: (!control.busy || control.spinIconWhileBusy)
                     && (control.iconGlyph.length > 0 || control.copyConfirmed || control.copyFailed)
            text: control.copyFailed ? Theme.iconWarning
                  : control.copyConfirmed ? Theme.iconCheck : control.iconGlyph
            RotationAnimator on rotation {
                from: 0
                to: 360
                duration: Theme.reduceMotion ? 1400 : 840
                loops: Animation.Infinite
                running: control.busy && control.spinIconWhileBusy
                onStopped: actionGlyph.rotation = 0
            }
            onVisibleChanged: if (!visible) rotation = 0
            filled: control.selected
            color: control.copyFailed ? Theme.danger : control.copyConfirmed ? Theme.success
                   : control.customIconColor.a > 0 ? control.customIconColor : control.foregroundColor
            Behavior on color {
                ColorAnimation { duration: Theme.reduceMotion ? 0 : 110 }
            }
            preferredSize: control.iconSize
            Layout.alignment: Qt.AlignCenter
        }

        Label {
            visible: !control.iconOnly
            Layout.fillWidth: true
            text: control.text
            color: control.foregroundColor
            Behavior on color {
                ColorAnimation { duration: Theme.reduceMotion ? 0 : 110 }
            }
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeLabelSize
            font.weight: control.prominent || control.positive || control.selected ? Theme.weightDemiBold : Theme.weightMedium
            horizontalAlignment: control.alignStart ? Theme.textStartAlignment : Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: control.wrapText ? Text.Wrap : Text.NoWrap
            elide: control.wrapText ? Text.ElideNone : Text.ElideRight
        }

        Item {
            visible: control.balancedIcon && !control.iconOnly && control.iconGlyph.length > 0
            Layout.preferredWidth: control.iconSize
            Layout.preferredHeight: control.iconSize
        }
    }

    Superellipse {
        visible: control.badgeText.length > 0
        z: 21
        width: Math.max(18, badgeLabel.implicitWidth + 8)
        height: 18
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 0
        anchors.rightMargin: 0
        fillColor: Theme.danger
        curveDepth: 9

        Label {
            id: badgeLabel
            anchors.centerIn: parent
            text: control.badgeText
            color: Theme.dangerInk
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightBold
        }
    }

    Superellipse {
        visible: control.badgeDot && control.badgeText.length === 0
        z: 20
        width: 9
        height: 9
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 2
        anchors.rightMargin: 2
        fillColor: Theme.danger
        curveDepth: 4.5
        Accessible.ignored: true
    }

    background: NeonSurface {
        reflectionSource: control.reflectionSource
        reflectedBorder: true
        // Keep the Kit's hover-capable halo instantiated for the lifetime of
        // an enabled button. Binding interactivity to hovered unloaded the
        // ShaderEffect on mouse exit and snapped the fade away.
        interactive: control.enabled
        // Hundreds of quiet Cast-action buttons may coexist in a feed.
        // Preserve the Kit's hover reflection but allocate GPU material
        // only during entry/exit rather than one shader per idle button.
        deferredNeon: control.quiet && !control.selected && !control.prominent
                      && !control.positive && !control.destructiveFilled
                      && !control.neutralPrimary
        ambientNeon: control.prominent || control.selected || control.positive
                     || control.destructiveFilled || control.neutralPrimary
        neonBusy: control.busy
        // Bright fills need a light glint regardless of the page palette;
        // an ink-filled neutral CTA uses an accent-tinted dark-edge glint.
        inverted: control.neutralPrimary ? Theme.dark
                  : (control.prominent || control.positive || control.destructiveFilled)
                    ? !Theme.dark : false
        focused: Theme.keyboardFocusVisible(control)
        externalHovered: control.hovered && !control.mobileViewport
        externalPointerX: neonPointer.hovered ? neonPointer.point.position.x : width / 2
        externalPointerY: neonPointer.hovered ? neonPointer.point.position.y : height / 2
        neonStrength: control.prominent || control.destructiveFilled
                      || control.neutralPrimary || control.positive
                      ? Theme.neonCardStrength : Theme.neonControlStrength
        ambientStrength: control.prominent || control.destructiveFilled
                         || control.neutralPrimary || control.positive
                         ? Math.min(0.68, Theme.neonAmbientStrength * 1.55)
                         : Theme.neonAmbientStrength
        neonColor: control.neutralPrimary
                   ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.90)
                   : control.prominent || control.positive || control.destructiveFilled
                     ? Qt.rgba(1, 1, 1, 0.72)
                   : control.selected ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                                               Theme.accent.b, 0.60)
                   : "transparent"
        fillColor: {
            if (!control.enabled && !control.busy)
                return control.quiet ? "transparent" : Theme.surfaceMuted
            if (control.destructiveFilled)
                return control.down ? Qt.darker(Theme.danger, 1.12) : Theme.danger
            if (control.destructive)
                return control.hovered || control.down ? Theme.dangerSurface : "transparent"
            if (control.neutralPrimary)
                return control.down ? Qt.lighter(Theme.ink, 1.10) : Theme.ink
            if (control.positive)
                return control.down ? Qt.darker(Theme.success, 1.10)
                                    : control.hovered ? Qt.lighter(Theme.success, 1.06)
                                                      : Theme.success
            if (control.prominent)
                return control.down ? Theme.primaryPressed : control.hovered ? Theme.primaryFillHover : Theme.primaryFill
            if (control.selected)
                return control.customSelectedSurface.a > 0
                    ? control.customSelectedSurface
                    : control.down || control.hovered ? Theme.primarySurfaceHover : Theme.primarySurface
            if (control.quiet)
                return control.hovered || control.down ? Theme.castHover : "transparent"
            return control.down ? Theme.surfacePressed : control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
        }
        borderWidth: Theme.keyboardFocusVisible(control) ? 2
                     : control.prominent || control.destructiveFilled
                       || control.positive || control.neutralPrimary || control.selected
                       ? Theme.strokeHairline
                     : (!control.quiet && !control.destructive
                        ? Theme.strokeStandard : 0)
        borderColor: Theme.keyboardFocusVisible(control)
                     ? (control.prominent || control.positive || control.destructiveFilled
                        ? Theme.focusOnAccent : Theme.focus)
                     : control.neutralPrimary
                       ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                 control.hovered ? 0.70 : 0.28)
                     : control.prominent || control.positive || control.destructiveFilled
                       ? Qt.rgba(1, 1, 1, control.hovered ? 0.62 : 0.26)
                     : Theme.borderMuted
        curveDepth: control.pill ? control.height / 2
                    : control.prominent || control.destructiveFilled || control.neutralPrimary
                      ? Theme.curveContainer : Theme.curveControl
        elevation: control.enabled && control.elevated
                   && (control.prominent || control.positive || (!control.quiet && !control.destructive && !control.selected))
                   ? Theme.elevationControl : Theme.elevationNone
        shadowColor: Theme.keyboardFocusVisible(control) ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: control.down ? 0.42 : (control.hovered ? 1.08 : 1.0)

    }
}
