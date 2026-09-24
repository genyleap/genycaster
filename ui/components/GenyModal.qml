import QtQuick
import QtQuick.Controls
import Farcaster.Client

Popup {
    id: control

    modal: true
    focus: true
    padding: Theme.space4
    x: parent ? Math.round(Math.max(0, (parent.width - width) / 2)) : 0
    y: parent ? Math.round(Math.max(0, (parent.height - height) / 2)) : 0
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle {
        color: Theme.overlay

        // A modal must be an input boundary, not only a visual dimmer. Without
        // this shield, unhandled pointer events can fall through the overlay and
        // trigger controls or external links in the page behind the popup.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            preventStealing: true
            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onClicked: function(mouse) { mouse.accepted = true }
            onDoubleClicked: function(mouse) { mouse.accepted = true }
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    background: PopupSurface {}

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: Theme.motionStandard
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: Theme.motionFast
            easing.type: Easing.InCubic
        }
    }
}
