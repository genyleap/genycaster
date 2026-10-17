import QtQuick
import QtQuick.Controls

// Explicit menu items keep each label stable across highlight/focus changes.
// Avoid the Qt Basic Menu.Action proxy which could draw blank on hover.
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

    EditorMenuItem {
        text: qsTrId("genycaster.editor.undo")
        enabled: root.editor !== null && root.editor.canUndo
        onTriggered: root.editor.undo()
    }
    EditorMenuItem {
        text: qsTrId("genycaster.editor.redo")
        enabled: root.editor !== null && root.editor.canRedo
        onTriggered: root.editor.redo()
    }
    MenuSeparator {
        topPadding: Theme.space1
        bottomPadding: Theme.space1
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.divider
        }
    }
    EditorMenuItem {
        text: qsTrId("genycaster.editor.cut")
        enabled: root.editor !== null && root.editor.selectedText.length > 0
        onTriggered: root.editor.cut()
    }
    EditorMenuItem {
        text: qsTrId("genycaster.feedback.copy")
        enabled: root.editor !== null && root.editor.selectedText.length > 0
        onTriggered: root.editor.copy()
    }
    EditorMenuItem {
        text: qsTrId("genycaster.editor.paste")
        enabled: root.editor !== null && !root.editor.readOnly
        onTriggered: root.editor.paste()
    }
    EditorMenuItem {
        text: qsTrId("genycaster.editor.select_all")
        enabled: root.editor !== null && root.editor.text.length > 0
        onTriggered: root.editor.selectAll()
    }
}
