import QtQuick
import QtQuick.Controls
import Farcaster.Client

// All actions are rendered by our own Items, never Qt's styled MenuItem
// delegate. This keeps text legible on hover in WASM and Desktop.
Menu {
    id: root
    required property var editor
    property bool nativePresentation: false
    popupType: nativePresentation ? Popup.Native : Popup.Item
    width: 220
    padding: Theme.space2
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Superellipse {
        fillColor: Theme.surfaceElevated
        borderColor: Theme.borderMuted
        borderWidth: Theme.strokeHairline
        curveDepth: Theme.curveContainer
    }
    EditorMenuItem {
        label: qsTrId("genycaster.editor.undo")
        actionEnabled: root.editor !== null && root.editor.canUndo
        hostMenu: root
        onTriggered: root.editor.undo()
    }
    EditorMenuItem {
        label: qsTrId("genycaster.editor.redo")
        actionEnabled: root.editor !== null && root.editor.canRedo
        hostMenu: root
        onTriggered: root.editor.redo()
    }
    Rectangle {
        implicitWidth: 196
        implicitHeight: 1
        color: Theme.borderMuted
    }
    EditorMenuItem {
        label: qsTrId("genycaster.editor.cut")
        actionEnabled: root.editor !== null && root.editor.selectedText.length > 0
        hostMenu: root
        onTriggered: {
            presentationBridge.copyText(root.editor.selectedText)
            root.editor.cut()
        }
    }
    EditorMenuItem {
        label: qsTrId("genycaster.feedback.copy")
        actionEnabled: root.editor !== null && root.editor.selectedText.length > 0
        hostMenu: root
        onTriggered: presentationBridge.copyText(root.editor.selectedText)
    }
    EditorMenuItem {
        label: qsTrId("genycaster.editor.paste")
        actionEnabled: root.editor !== null && !root.editor.readOnly
        hostMenu: root
        onTriggered: root.editor.paste()
    }
    EditorMenuItem {
        label: qsTrId("genycaster.editor.select_all")
        actionEnabled: root.editor !== null && root.editor.text.length > 0
        hostMenu: root
        onTriggered: root.editor.selectAll()
    }
    EditorMenuItem {
        label: qsTrId("genycaster.editor.clear_selection")
        actionEnabled: root.editor !== null && root.editor.selectedText.length > 0
        hostMenu: root
        onTriggered: root.editor.deselect()
    }
}
