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
    property bool browserEditing: false
    property bool applyingBrowserText: false
    property bool openingBrowserEditor: false
    property color browserInk: Theme.ink
    property color browserPlaceholderInk: Theme.inkSubtle

    function editorOptions() {
        return {
            geometry: presentationBridge.browserEditorGeometry(editor),
            placeholder: editor.placeholderText || "",
            sensitive: sensitive,
            masked: editor.echoMode !== undefined && editor.echoMode !== TextInput.Normal,
            multiline: editor.textFormat !== undefined,
            start: editor.selectionStart,
            end: editor.selectionEnd,
            maxLength: editor.maximumLength !== undefined ? editor.maximumLength : 262144,
            rtl: editor.effectiveTextDirection === Qt.RightToLeft,
            fontFamily: editor.font.family,
            fontSize: editor.font.pixelSize > 0 ? editor.font.pixelSize : Theme.typeBodySize,
            fontWeight: editor.font.weight,
            leftPadding: editor.leftPadding,
            rightPadding: editor.rightPadding,
            topPadding: editor.topPadding,
            bottomPadding: editor.bottomPadding,
            ink: browserInk.toString(),
            placeholderInk: browserPlaceholderInk.toString(),
            accent: Theme.accent.toString()
        }
    }
    function requestSystemEditor() {
        if (!browser || !usable() || browserEditing || openingBrowserEditor) return
        beginRequest("browser-edit")
        browserInk = editor.color.a > 0 ? editor.color : Theme.ink
        browserPlaceholderInk = editor.placeholderTextColor.a > 0
            ? editor.placeholderTextColor : Theme.inkSubtle
        // Keep the Qt platform window active. Hiding the platform input
        // context here can itself move focus during a browser gesture.
        openingBrowserEditor = true
        const token = pendingPaste
        const opened = presentationBridge.openBrowserTextEditor(token, plainText(), editorOptions())
        openingBrowserEditor = false
        // The adapter returns true only for an actual focused, hit-testable
        // DOM input inside the active Qt window.
        browserEditing = opened && pendingPaste === token
        if (!opened) {
            cancelPaste()
            pasteFailed = true
        }
    }
    function syncBrowserEditor(geometryOnly) {
        if (!browserEditing || !usable() || applyingBrowserText) return
        const options = editorOptions()
        options.geometryOnly = geometryOnly === true
        presentationBridge.syncBrowserTextEditor(pendingPaste,
            options.geometryOnly ? "" : plainText(), options)
    }
    Timer {
        // Scene coordinates can change while an ancestor scrolls or the
        // visual viewport shrinks for the keyboard.
        interval: 120
        repeat: true
        running: root.browserEditing
        onTriggered: root.syncBrowserEditor(true)
    }
    function requestPaste() {
        if (!usable()) return
        if (browser) {
            if (!editor.activeFocus) editor.forceActiveFocus()
            requestSystemEditor()
        } else if (typeof editor.clipboardPasteHandler !== "function") {
            editor.paste()
            return
        }
        // The explicit clipboard button still works when access is granted.
        // Keep a separate request token so the inline editor stays attached.
        const token = Date.now().toString(36) + ":" + Math.random().toString(36)
        directPaste = token
        pasteStart = editor.selectionStart
        pasteEnd = editor.selectionEnd
        presentationBridge.requestClipboardText(token)
    }
    property string directPaste: ""
    function cancelPaste() {
        const token = pendingPaste
        pendingPaste = ""
        directPaste = ""
        operation = ""
        browserEditing = false
        if (token.length && browser)
            presentationBridge.cancelBrowserTextEditor(token)
    }
    function replaceRange(start, end, value) {
        editor.deselect()
        editor.cursorPosition = start
        presentationBridge.commitBrowserEditorText(editor, value, end - start)
    }
    function applyText(value, kind) {
        if (!browserEditing && !editor.activeFocus) editor.forceActiveFocus()
        if (typeof editor.clipboardPasteHandler === "function" &&
            (kind !== "browser-edit" || /\s/.test(value))) {
            editor.clipboardPasteHandler(value)
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
        function onTextChanged() {
            if (!root.applyingBrowserText) root.directPaste = ""
            root.syncBrowserEditor()
        }
        function onActiveFocusChanged() {
            if (root.editor.activeFocus && root.browser && root.usable()) {
                // Let Qt finish its pointer/focus transaction first. A
                // focused field should already be a real browser target
                // before the next long press, not create it during that press.
                Qt.callLater(function() {
                    if (root.editor && root.editor.activeFocus && root.usable())
                        root.requestSystemEditor()
                })
            } else if (!root.editor.activeFocus && !root.applyingBrowserText
                       && !root.openingBrowserEditor) {
                root.cancelPaste()
            }
        }
        function onVisibleChanged() {
            if (!root.editor.visible) { root.cancelPaste(); root.pasteFailed = false }
        }
        function onEnabledChanged() { if (!root.editor.enabled) root.cancelPaste() }
        function onReadOnlyChanged() { if (root.editor.readOnly) root.cancelPaste() }
    }
    Connections {
        target: presentationBridge
        function onBrowserEditorSelectionChanged(requestId, start, end) {
            if (!root.browserEditing || requestId !== root.pendingPaste || !root.usable()
                || end > root.editor.length) return
            root.editor.select(start, end)
        }
        function onClipboardTextReceived(requestId, value, ok) {
            if (requestId === root.directPaste && root.directPaste.length) {
                root.directPaste = ""
                if (!root.usable()) return
                if (!ok) { root.pasteFailed = true; return }
                root.applyingBrowserText = true
                try { root.applyText(value, "native-paste") }
                finally { root.applyingBrowserText = false }
                root.syncBrowserEditor()
                return
            }
            if (requestId !== root.pendingPaste || !root.pendingPaste.length) return
            if (!ok) { root.cancelPaste(); return }
            if (!root.usable()) { root.cancelPaste(); return }
            root.applyingBrowserText = true
            try { root.applyText(value, "browser-edit") }
            finally { root.applyingBrowserText = false }
            if (root.usable()) root.syncBrowserEditor()
            else root.cancelPaste()
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
