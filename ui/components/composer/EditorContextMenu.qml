import QtQuick
import QtQuick.Controls
import Farcaster.Client

// Native editing is independent of the preference for selecting rendered casts.
// WASM cannot expose OS selection on a canvas; it uses a real browser input
// for the explicit text-edit transaction. No authentication logic lives here.
Menu {
    id: root
    required property var editor
    readonly property bool platformMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"
    readonly property bool browser: Qt.platform.os === "wasm"
    property string pendingPaste: ""
    property string operation: ""
    property bool pasteFailed: false
    property int pasteStart: 0
    property int pasteEnd: 0
    readonly property bool sensitive: !!editor
        && ((editor.inputMethodHints & Qt.ImhSensitiveData) !== 0
            || (editor.echoMode !== undefined && editor.echoMode !== TextInput.Normal))
    popupType: Popup.Native
    modal: true
    dim: false

    function usable() {
        return !!editor && editor.enabled && editor.visible && !editor.readOnly
    }
    function plainText() {
        return typeof editor.getText === "function" ? editor.getText(0, editor.length) : editor.text
    }
    function beginRequest(kind) {
        cancelPaste()
        pasteFailed = false
        operation = kind
        pasteStart = editor.selectionStart
        pasteEnd = editor.selectionEnd
        pendingPaste = Date.now().toString(36) + ":" + Math.random().toString(36)
    }
    function requestSystemEditor(pasteOnly) {
        if (!browser || !usable()) return
        beginRequest(pasteOnly ? "browser-paste" : "browser-edit")
        const value = pasteOnly ? "" : plainText()
        const opened = presentationBridge.openBrowserTextEditor(pendingPaste, value, {
            title: pasteOnly ? qsTrId("genycaster.editor.paste") : qsTrId("genycaster.status.edit"),
            apply: pasteOnly ? qsTrId("genycaster.editor.paste") : qsTrId("genycaster.composer.apply"),
            cancel: qsTrId("genycaster.shell.cancel"),
            placeholder: editor.placeholderText || "",
            hint: qsTrId("genycaster.editor.system_edit_hint"),
            sensitive: sensitive,
            multiline: pasteOnly || editor.textFormat !== undefined,
            start: pasteOnly ? 0 : editor.selectionStart,
            end: pasteOnly ? 0 : editor.selectionEnd,
            maxLength: editor.maximumLength !== undefined ? editor.maximumLength : 262144,
            rtl: !sensitive && editor.effectiveTextDirection === Qt.RightToLeft,
            surface: Theme.surfaceElevated.toString(),
            ink: Theme.ink.toString(),
            muted: Theme.inkMuted.toString(),
            border: Theme.borderMuted.toString(),
            accent: Theme.accent.toString()
        })
        if (!opened) {
            cancelPaste()
            pasteFailed = true
        }
    }
    function requestPaste() {
        if (!usable()) return
        if (browser) {
            // A browser may deny programmatic reads, especially in Safari.
            // A real input accepts the platform Paste command without this permission.
            requestSystemEditor(true)
        } else if (typeof editor.clipboardPasteHandler === "function") {
            beginRequest("native-paste")
            presentationBridge.requestClipboardText(pendingPaste)
        } else {
            editor.paste()
        }
    }
    function cancelPaste() {
        const token = pendingPaste
        pendingPaste = ""
        operation = ""
        if (token.length && browser)
            presentationBridge.cancelBrowserTextEditor(token)
    }
    function replaceRange(start, end, value) {
        editor.deselect()
        editor.cursorPosition = start
        presentationBridge.commitBrowserEditorText(editor, value, end - start)
    }
    function applyText(value, kind) {
        editor.forceActiveFocus()
        if (typeof editor.clipboardPasteHandler === "function") {
            if (kind === "browser-edit" && !value.length) editor.clear()
            else editor.clipboardPasteHandler(value)
            return
        }
        if (kind !== "browser-edit") {
            if (value.length) replaceRange(pasteStart, pasteEnd, value)
            return
        }
        // Preserve unchanged rich spans. Replace only the differing plain-text
        // range; browser markup is never interpreted as a rich-text command.
        const before = plainText()
        if (before === value) return
        let start = 0
        while (start < before.length && start < value.length && before[start] === value[start]) ++start
        let oldEnd = before.length
        let newEnd = value.length
        while (oldEnd > start && newEnd > start && before[oldEnd - 1] === value[newEnd - 1]) {
            --oldEnd
            --newEnd
        }
        // Keep both UTF-16 boundaries on code points, including emoji.
        if (start > 0 && (/[\uD800-\uDBFF]/.test(before[start-1])
                         || /[\uD800-\uDBFF]/.test(value[start-1]))) --start
        if (oldEnd < before.length && newEnd < value.length
            && (/[\uDC00-\uDFFF]/.test(before[oldEnd]) || /[\uDC00-\uDFFF]/.test(value[newEnd]))) {
            ++oldEnd
            ++newEnd
        }
        replaceRange(start, oldEnd, value.slice(start, newEnd))
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
            const kind = root.operation
            root.pendingPaste = ""
            root.operation = ""
            if (!root.usable()) return
            if (ok) root.applyText(value, kind)
            else if (kind === "native-paste") root.pasteFailed = true
        }
    }
    Component.onDestruction: cancelPaste()

    MenuItem {
        text: qsTrId("genycaster.editor.undo")
        enabled: !!root.editor && root.editor.canUndo && !root.editor.readOnly
        onTriggered: root.editor.undo()
    }
    MenuItem {
        text: qsTrId("genycaster.editor.redo")
        enabled: !!root.editor && root.editor.canRedo && !root.editor.readOnly
        onTriggered: root.editor.redo()
    }
    MenuSeparator {}
    MenuItem {
        text: qsTrId("genycaster.editor.cut")
        enabled: root.usable() && !root.sensitive && root.editor.selectedText.length > 0
        onTriggered: root.editor.cut()
    }
    MenuItem {
        text: qsTrId("genycaster.feedback.copy")
        enabled: !!root.editor && !root.sensitive && root.editor.selectedText.length > 0
        onTriggered: root.editor.copy()
    }
    MenuItem {
        text: qsTrId("genycaster.editor.paste")
        enabled: root.usable()
        onTriggered: root.requestPaste()
    }
    MenuItem {
        text: qsTrId("genycaster.editor.select_all")
        enabled: !!root.editor && root.editor.length > 0
        onTriggered: root.editor.selectAll()
    }
    MenuItem {
        text: qsTrId("genycaster.editor.clear_selection")
        enabled: !!root.editor && root.editor.selectedText.length > 0
        onTriggered: root.editor.deselect()
    }
}
