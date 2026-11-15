import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: root

    property string iconGlyph: ""
    property bool showArrow: true
    property bool busy: false
    property bool neutralPrimary: false

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(58, primaryLabel.implicitHeight + Theme.space3 * 2)
    implicitWidth: Math.max(168, primaryLabel.implicitWidth + Theme.space8)
    leftPadding: Theme.space4
    rightPadding: Theme.space4

    Accessible.name: text

    readonly property color foreground: enabled
                                        ? neutralPrimary ? Theme.surface : Theme.accentInk
                                        : Theme.inkDisabled

    HoverHandler {
        id: primaryPointer
        enabled: Theme.neonEnabled && root.enabled
        blocking: false
    }

    background: NeonSurface {
        interactive: root.enabled
        ambientNeon: root.enabled
        neonBusy: root.busy
        inverted: root.neutralPrimary
        focused: Theme.keyboardFocusVisible(root)
        externalHovered: root.hovered
        externalPointerX: primaryPointer.hovered ? primaryPointer.point.position.x : width / 2
        externalPointerY: primaryPointer.hovered ? primaryPointer.point.position.y : height / 2
        neonStrength: Theme.neonCardStrength
        // White-on-dark primary buttons keep an accent reflection; the rim
        // follows the button outline rather than painting a second rectangle.
        neonColor: root.neutralPrimary
                   ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)
                   : "transparent"
        fillColor: !root.enabled
                   ? Theme.surfaceMuted
                   : root.neutralPrimary ? Theme.ink
                   : root.down ? Theme.primaryPressed
                               : root.hovered ? Theme.primaryFillHover
                                              : Theme.primaryFill
        borderWidth: Theme.keyboardFocusVisible(root) ? 2 : 0
        borderColor: Theme.focusOnAccent
        curveDepth: Theme.curveContainer
        elevation: root.enabled ? Theme.elevationControl : Theme.elevationNone

    }

    contentItem: RowLayout {
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        GenySpinner {
            visible: root.busy
            running: visible
            color: root.foreground
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }

        Icon {
            visible: !root.busy && root.iconGlyph.length > 0
            text: root.iconGlyph
            color: root.foreground
            preferredSize: Theme.iconSizeSmall
        }

        DirectionAwareLabel {
            id: primaryLabel
            Layout.fillWidth: true
            text: root.text
            color: root.foreground
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
        }

        Icon {
            visible: root.showArrow && !root.busy
            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
            color: root.foreground
            preferredSize: Theme.iconSizeSmall
        }
    }
}
