import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property bool prominent: false
    property bool quiet: false
    property bool destructive: false
    property bool positive: false
    property bool selected: false
    property string iconGlyph: ""
    property bool iconOnly: false
    property bool compact: false
    property bool alignStart: false
    property bool pill: false
    property bool elevated: true
    property real iconSize: Theme.iconSize
    property bool badgeDot: false
    property string badgeText: ""
    property bool busy: false

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    implicitWidth: iconOnly ? implicitHeight : Math.max(68, contentRow.implicitWidth + leftPadding + rightPadding)
    implicitHeight: compact ? Theme.compactControlHeight : Theme.controlHeight
    leftPadding: iconOnly ? 0 : (compact ? Theme.space3 : Theme.space4)
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    containmentMask: background
    Accessible.name: text

    onHoveredChanged: {
        if (!control.iconOnly || control.text.length === 0 || !control.hovered) {
            hintTimer.stop()
            if (hintLoader.item)
                hintLoader.item.close()
            return
        }
        hintTimer.restart()
    }

    Timer {
        id: hintTimer
        interval: 420
        repeat: false
        onTriggered: {
            if (!control.hovered || !control.iconOnly || control.text.length === 0 || !control.Window.window)
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

    readonly property color foregroundColor: !enabled ? Theme.inkDisabled
                                               : destructive ? Theme.danger
                                               : positive ? Theme.contrastInk(Theme.success)
                                               : prominent ? Theme.accentInk
                                               : selected ? Theme.accent
                                               : Theme.ink

    contentItem: RowLayout {
        id: contentRow
        spacing: control.iconOnly ? 0 : Theme.space2
        layoutDirection: Theme.uiDirection

        GenySpinner {
            visible: control.busy
            running: visible
            color: control.foregroundColor
            Layout.preferredWidth: Math.min(18, control.iconSize)
            Layout.preferredHeight: Layout.preferredWidth
            Layout.alignment: Qt.AlignCenter
        }

        Icon {
            visible: !control.busy && control.iconGlyph.length > 0
            text: control.iconGlyph
            filled: control.selected
            color: control.foregroundColor
            Behavior on color { ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
            preferredSize: control.iconSize
            Layout.alignment: Qt.AlignCenter
        }

        Label {
            visible: !control.iconOnly
            Layout.fillWidth: true
            text: control.text
            color: control.foregroundColor
            Behavior on color { ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeLabelSize
            font.weight: control.prominent || control.positive || control.selected ? Theme.weightDemiBold : Theme.weightMedium
            horizontalAlignment: control.alignStart ? Theme.textStartAlignment : Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
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

    background: Superellipse {
        fillColor: {
            if (!control.enabled)
                return control.quiet ? "transparent" : Theme.surfaceMuted
            if (control.destructive)
                return control.hovered || control.down ? Theme.dangerSurface : "transparent"
            if (control.positive)
                return control.down ? Qt.darker(Theme.success, 1.10)
                                    : control.hovered ? Qt.lighter(Theme.success, 1.06)
                                                      : Theme.success
            if (control.prominent)
                return control.down ? Theme.primaryPressed : control.hovered ? Theme.primaryFillHover : Theme.primaryFill
            if (control.selected)
                return control.down || control.hovered ? Theme.primarySurfaceHover : Theme.primarySurface
            if (control.quiet)
                return control.hovered || control.down ? Theme.castHover : "transparent"
            return control.down ? Theme.surfacePressed : control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
        }
        borderWidth: Theme.keyboardFocusVisible(control) ? 2
                     : (!control.prominent && !control.quiet && !control.destructive && !control.positive && !control.selected ? Theme.strokeStandard : 0)
        borderColor: Theme.keyboardFocusVisible(control)
                     ? (control.prominent || control.positive ? Theme.focusOnAccent : Theme.focus)
                     : Theme.borderMuted
        curveDepth: control.pill ? control.height / 2 : Theme.curveControl
        elevation: control.enabled && control.elevated
                   && (control.prominent || control.positive || (!control.quiet && !control.destructive && !control.selected))
                   ? Theme.elevationControl : Theme.elevationNone
        shadowColor: Theme.keyboardFocusVisible(control) ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: control.down ? 0.42 : (control.hovered ? 1.08 : 1.0)

        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
    }
}
