import QtQuick
import QtQuick.Controls

// Keep the label bound to the actual MenuItem rather than an Action delegate.
// Qt's generated Action/MenuItem proxy can repaint empty text when the
// pointer highlights an item in the WebAssembly Basic style.
MenuItem {
    id: root
    implicitWidth: 202
    implicitHeight: 38
    opacity: 1.0
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    hoverEnabled: true

    contentItem: Text {
        text: root.text
        visible: true
        opacity: 1.0
        color: root.highlighted ? Theme.ink
              : root.enabled ? Theme.ink : Theme.inkMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.typeBodySize
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Superellipse {
        curveDepth: Theme.curveControl
        fillColor: root.highlighted ? Theme.surfaceHover : "transparent"
        borderWidth: 0
    }
}
