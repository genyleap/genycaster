import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root

    property string iconGlyph: ""
    property bool showArrow: true
    property bool busy: false

    hoverEnabled: true
    implicitHeight: 54
    leftPadding: Theme.space4
    rightPadding: Theme.space4

    readonly property color foreground: enabled
                                        ? Theme.contrastInk(Theme.ink)
                                        : Theme.inkDisabled

    background: Superellipse {
        fillColor: !root.enabled
                   ? Theme.surfaceMuted
                   : root.down ? Qt.darker(Theme.ink, 1.06)
                               : root.hovered ? Qt.lighter(Theme.ink, 1.08)
                                              : Theme.ink
        borderWidth: root.activeFocus ? 2 : 0
        borderColor: Theme.focus
        curveDepth: Theme.curveContainer
        elevation: root.enabled ? Theme.elevationControl : Theme.elevationNone

        Behavior on fillColor {
            ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }
    }

    contentItem: RowLayout {
        spacing: Theme.space2

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

        Label {
            Layout.fillWidth: true
            text: root.text
            color: root.foreground
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Icon {
            visible: root.showArrow && !root.busy
            text: Theme.iconArrowRight
            color: root.foreground
            preferredSize: Theme.iconSizeSmall
        }
    }
}
