import QtQuick
import QtQuick.Controls
import Farcaster.Client

// All actions are rendered by our own Items, never Qt's styled MenuItem
// delegate. This keeps text legible on hover in WASM and Desktop.
Menu {
    id: root
    required property var editor
    property bool nativePresentation: false
    property string pendingPaste: ""
    property bool pasteFailed: false
    property int pasteStart: 0
    property int pasteEnd: 0
    readonly property bool sensitive: editor !== null
        && ((editor.inputMethodHints & Qt.ImhSensitiveData) !== 0
            || (editor.echoMode !== undefined && editor.echoMode !== TextInput.Normal))

    function requestPaste() {
        if (!editor || !editor.enabled || !editor.visible || editor.readOnly) return
        pasteFailed = false
        if (Qt.platform.os !== "wasm" && typeof editor.clipboardPasteHandler !== "function") {
            editor.paste()
            return
        }
        pasteStart = editor.selectionStart
        pasteEnd = editor.selectionEnd
        pendingPaste = Date.now().toString(36) + ":" + Math.random().toString(36)
        pasteTimeout.restart()
        presentationBridge.requestClipboardText(pendingPaste)
    }
    function cancelPaste() { pendingPaste = ""; pasteTimeout.stop() }
    Timer {
        id: pasteTimeout
        interval: 30000
        repeat: false
        onTriggered: {
            root.cancelPaste()
            root.pasteFailed = true
        }
    }
    Connections {
        target: root.editor
        function onTextChanged() { root.cancelPaste() }
        function onVisibleChanged() {
            if (!root.editor.visible) { root.cancelPaste(); root.pasteFailed = false }
        }
        function onEnabledChanged() { if (!root.editor.enabled) root.cancelPaste() }
    }
    Connections {
        target: presentationBridge
        function onClipboardTextReceived(requestId, value, ok) {
            if (requestId !== root.pendingPaste || !root.pendingPaste.length) return
            root.cancelPaste()
            if (!root.editor || !root.editor.visible || !root.editor.enabled || root.editor.readOnly) return
            if (!ok) { root.pasteFailed = true; return }
            if (!value.length) return
            if (typeof root.editor.clipboardPasteHandler === "function") {
                root.editor.clipboardPasteHandler(value)
            } else {
                root.editor.remove(root.pasteStart, root.pasteEnd)
                root.editor.insert(root.pasteStart, value)
            }
        }
    }
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
        actionEnabled: root.editor !== null && !root.sensitive && !root.editor.readOnly && root.editor.selectedText.length > 0
        hostMenu: root
        onTriggered: {
            presentationBridge.copyText(root.editor.selectedText)
            root.editor.remove(root.editor.selectionStart, root.editor.selectionEnd)
        }
    }
    EditorMenuItem {
        label: qsTrId("genycaster.feedback.copy")
        actionEnabled: root.editor !== null && !root.sensitive && root.editor.selectedText.length > 0
        hostMenu: root
        onTriggered: presentationBridge.copyText(root.editor.selectedText)
    }
    EditorMenuItem {
        label: qsTrId("genycaster.editor.paste")
        actionEnabled: root.editor !== null && !root.editor.readOnly
        hostMenu: root
        onTriggered: root.requestPaste()
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
