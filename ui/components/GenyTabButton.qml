import QtQuick
import QtQuick.Controls
import Farcaster.Client

Button {
    id: control

    property bool selected: false
    property real minimumWidth: 72
    property bool dense: false
    property bool segmented: true
    property bool showSelectionIndicator: true

    implicitHeight: 40
    implicitWidth: Math.max(control.minimumWidth, contentItem.implicitWidth + Theme.space4)
    hoverEnabled: true
    containmentMask: background
    flat: true
    scale: control.down ? 0.988 : 1.0

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: Label {
        text: control.text
        color: control.selected ? (control.segmented ? Theme.ink : Theme.accent)
                                : (control.hovered ? Theme.ink : Theme.inkMuted)
        font.family: Theme.fontFamily
        font.pixelSize: control.dense ? Theme.typeCaptionSize : Theme.typeLabelSize
        font.weight: control.selected ? Theme.weightDemiBold : Theme.weightMedium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Superellipse {
        fillColor: control.segmented
                   ? (control.selected
                      ? (control.hovered ? Theme.surfaceElevated : Theme.surfaceRaised)
                      : (control.hovered || control.down ? Theme.castHover : "transparent"))
                   : "transparent"
        borderWidth: control.activeFocus ? 2 : 0
        borderColor: Theme.focus
        curveDepth: Theme.curveControl
        elevation: control.segmented && control.selected ? Theme.elevationControl : Theme.elevationNone
        shadowColor: control.activeFocus ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: 0.62
        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }

        Superellipse {
            visible: control.selected && !control.segmented && control.showSelectionIndicator
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            width: Math.min(parent.width - Theme.space5, 38)
            height: 3
            fillColor: Theme.accent
            curveDepth: 1.5
        }
    }
}
