import QtQuick
import QtQuick.Controls
import Farcaster.Client

// Nonvisual platform adapter. Native controls keep Qt's built-in menu.
// WASM mounts a real DOM target before the first pointer gesture.
Item {
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
    width: 0
    height: 0
    property bool browserFocused: false
    property bool browserSuspended: false

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
            geometry: presentationBridge.browserEditorGeometry(editor, root.Overlay.overlay),
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
            lineHeight: 1.25,
            leftPadding: editor.leftPadding,
            rightPadding: editor.rightPadding,
            topPadding: editor.topPadding,
            bottomPadding: editor.bottomPadding,
            ink: browserInk.toString(),
            placeholderInk: browserPlaceholderInk.toString(),
            accent: Theme.accent.toString()
        }
    }
    function mountBrowserEditor() {
        if (!browser || !usable() || browserEditing || openingBrowserEditor || browserSuspended) return
        const options = editorOptions()
        if (!(options.geometry.clipWidth > 0 && options.geometry.clipHeight > 0)) return
        beginRequest("browser-edit")
        browserInk = editor.color.a > 0 ? editor.color : Theme.ink
        browserPlaceholderInk = editor.placeholderTextColor.a > 0
            ? editor.placeholderTextColor : Theme.inkSubtle
        openingBrowserEditor = true
        const token = pendingPaste
        const opened = presentationBridge.openBrowserTextEditor(token, plainText(), editorOptions())
        openingBrowserEditor = false
        browserEditing = opened && pendingPaste === token
        if (!opened) cancelPaste()
    }
    function requestSystemEditor() {
        browserSuspended = false
        mountBrowserEditor()
        if (!browserEditing) return
        const options = editorOptions()
        options.focusRequested = true
        if (!presentationBridge.syncBrowserTextEditor(pendingPaste, plainText(), options))
            cancelPaste()
    }
    function syncBrowserEditor(geometryOnly) {
        if (!browser || applyingBrowserText || browserSuspended) return
        if (!usable()) { cancelPaste(); return }
        if (!browserEditing) { mountBrowserEditor(); return }
        const options = editorOptions()
        options.geometryOnly = geometryOnly === true
        if (!presentationBridge.syncBrowserTextEditor(pendingPaste,
            options.geometryOnly ? "" : plainText(), options))
            cancelPaste()
    }
    Timer {
        interval: 100
        repeat: true
        running: root.browser && root.usable() && !root.browserSuspended
        triggeredOnStart: true
        onTriggered: root.syncBrowserEditor(true)
    }
    function requestPaste() {
        if (!usable()) return
        if (browser) {
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
        browserFocused = false
        if (token.length && browser)
            presentationBridge.cancelBrowserTextEditor(token)
    }
    function replaceRange(start, end, value) {
        editor.deselect()
        editor.cursorPosition = start
        presentationBridge.commitBrowserEditorText(editor, value, end - start)
    }
    function applyText(value, kind) {
        if (!browser && !editor.activeFocus) editor.forceActiveFocus()
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
        function onVisibleChanged() {
            if (!root.editor.visible) { root.cancelPaste(); root.pasteFailed = false }
            else { root.browserSuspended = false; root.syncBrowserEditor(true) }
        }
        function onEnabledChanged() { if (!root.editor.enabled) root.cancelPaste() }
        function onReadOnlyChanged() { if (root.editor.readOnly) root.cancelPaste() }
    }
    Connections {
        target: presentationBridge
        function onBrowserEditorFocusChanged(requestId, focused) {
            if (requestId === root.pendingPaste && root.browserEditing)
                root.browserFocused = focused
        }
        function onBrowserEditorAccepted(requestId) {
            if (requestId === root.pendingPaste && root.usable()
                && typeof root.editor.accepted === "function" && root.editor.acceptableInput)
                root.editor.accepted()
        }
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

}
