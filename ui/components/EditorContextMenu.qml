import QtQuick
import QtQuick.Controls

// One visual-system context menu for every editor mode. Qt 6.9+ otherwise
// installs its unstyled default TextArea context menu.
Menu {
    id: root
    required property var editor

    popupType: Popup.Item
    width: 210
    padding: Theme.space2
    overlap: 0
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Superellipse {
        fillColor: Theme.surfaceElevated
        borderColor: Theme.borderMuted
        borderWidth: Theme.strokeHairline
        curveDepth: Theme.curveContainer
    }

    delegate: MenuItem {
        id: actionItem
        implicitWidth: root.width - root.leftPadding - root.rightPadding
        implicitHeight: 36
        leftPadding: Theme.space3
        rightPadding: Theme.space3
        contentItem: Text {
            text: actionItem.text
            color: actionItem.enabled ? Theme.ink : Theme.inkSubtle
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeBodySize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Superellipse {
            curveDepth: Theme.curveControl
            fillColor: actionItem.highlighted ? Theme.surfaceHover : "transparent"
            borderWidth: 0
        }
    }

    Action {
        text: qsTrId("genycaster.editor.undo")
        enabled: root.editor !== null && root.editor.canUndo
        onTriggered: root.editor.undo()
    }
    Action {
        text: qsTrId("genycaster.editor.redo")
        enabled: root.editor !== null && root.editor.canRedo
        onTriggered: root.editor.redo()
    }
    MenuSeparator {
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.divider
        }
        topPadding: Theme.space1
        bottomPadding: Theme.space1
    }
    Action {
        text: qsTrId("genycaster.editor.cut")
        enabled: root.editor !== null && root.editor.selectedText.length > 0
        onTriggered: root.editor.cut()
    }
    Action {
        text: qsTrId("genycaster.feedback.copy")
        enabled: root.editor !== null && root.editor.selectedText.length > 0
        onTriggered: root.editor.copy()
    }
    Action {
        text: qsTrId("genycaster.editor.paste")
        enabled: root.editor !== null && !root.editor.readOnly
        onTriggered: root.editor.paste()
    }
    Action {
        text: qsTrId("genycaster.editor.select_all")
        enabled: root.editor !== null && root.editor.text.length > 0
        onTriggered: root.editor.selectAll()
    }
}
