import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property string iconGlyph: ""
    property string metricText: ""
    property bool active: false
    property bool iconFilled: false
    property real iconSize: Theme.iconSizeSmall

    implicitWidth: Math.max(Theme.controlHeight, content.implicitWidth + Theme.space2 * 2)
    implicitHeight: Theme.controlHeight
    padding: 0
    // No engagement visual depends on hover. Tracking hover for three
    // controls in every recycled post only makes stationary desktop pointers
    // generate state churn while the timeline moves underneath them.
    hoverEnabled: false
    containmentMask: background
    scale: control.down ? 0.985 : 1.0

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: RowLayout {
        id: content
        layoutDirection: Theme.uiDirection
        spacing: control.metricText.length > 0 ? Theme.space1 : 0

        Icon {
            text: control.iconGlyph
            filled: control.iconFilled
            color: control.active ? Theme.accent : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
            preferredSize: control.iconSize
        }

        Label {
            visible: control.metricText.length > 0
            text: control.metricText
            color: control.active ? Theme.accent : (control.enabled ? Theme.inkMuted : Theme.inkDisabled)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeCaptionSize
            font.weight: control.active ? Theme.weightDemiBold : Theme.weightMedium
        }
    }

    background: Superellipse {
        anchors.fill: parent
        anchors.margins: 1
        fillColor: "transparent"
        borderWidth: control.activeFocus ? 2 : 0
        borderColor: Theme.focus
        curveDepth: height / 2
        shadowColor: control.activeFocus ? Theme.focusHalo : Theme.shadow
    }
}
