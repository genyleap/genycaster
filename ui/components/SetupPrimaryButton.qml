import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: root

    property string iconGlyph: ""
    property bool showArrow: true
    property bool busy: false

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(54, primaryLabel.implicitHeight + Theme.space3 * 2)
    implicitWidth: Math.max(168, primaryLabel.implicitWidth + Theme.space8)
    leftPadding: Theme.space4
    rightPadding: Theme.space4

    Accessible.name: text

    readonly property color foreground: enabled
                                        ? Theme.accentInk
                                        : Theme.inkDisabled

    background: Superellipse {
        fillColor: !root.enabled
                   ? Theme.surfaceMuted
                   : root.down ? Theme.primaryPressed
                               : root.hovered ? Theme.primaryFillHover
                                              : Theme.primaryFill
        borderWidth: Theme.keyboardFocusVisible(root) ? 2 : 0
        borderColor: Theme.focusOnAccent
        curveDepth: Theme.curveContainer
        elevation: root.enabled ? Theme.elevationControl : Theme.elevationNone

        Behavior on fillColor {
            ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }
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
            horizontalAlignment: Theme.textStartAlignment
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
