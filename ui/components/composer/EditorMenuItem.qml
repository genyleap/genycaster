import QtQuick
import QtQuick.Controls

// Use a plain Item, not Qt's MenuItem/Action proxy. In Qt WASM Basic,
// highlighted MenuItems could paint an empty glyph/text even with a custom
// contentItem. Own the label and hit target throughout the hover cycle.
Item {
    id: root
    property string label: ""
    property bool actionEnabled: true
    property var hostMenu: null
    signal triggered()

    implicitHeight: 42
    implicitWidth: 196
    width: 196
    Accessible.role: Accessible.MenuItem
    Accessible.name: label
    Accessible.ignored: !visible
    Accessible.description: actionEnabled ? "" : label
    activeFocusOnTab: true

    Superellipse {
        anchors.fill: parent
        fillColor: hit.hovered && root.actionEnabled ? Theme.surfaceHover : "transparent"
        curveDepth: Theme.curveControl
        borderWidth: 0
    }
    Text {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space3
        anchors.right: parent.right
        anchors.rightMargin: Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.actionEnabled ? Theme.ink : Theme.inkMuted
        opacity: 1
        visible: true
        font.family: Theme.fontFamilyForText(text)
        font.pixelSize: Theme.typeBodySize
        elide: Text.ElideRight
    }
    MouseArea {
        id: hit
        anchors.fill: parent
        enabled: root.actionEnabled
        hoverEnabled: true
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.activate()
    }
    function activate() {
        if (!actionEnabled) return
        triggered()
        if (hostMenu) hostMenu.close()
    }
    Keys.onReturnPressed: event => { root.activate(); event.accepted = true }
    Keys.onSpacePressed: event => { root.activate(); event.accepted = true }
}
