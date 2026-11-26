import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

TextArea {
    id: root

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false
    // Keep Qt's default ContextMenu and IME on native platforms.
    // DOM text ownership is isolated to the WASM adapter.
    EditorContextMenu { id: editorMenu; editor: root }
    function resetSelection() { deselect() }
    function dismissSelectionActions() { if (editorMenu) { editorMenu.browserSuspended = true; editorMenu.cancelPaste() } }
    function requestClipboardPaste() { editorMenu.requestPaste() }
    function requestSystemEditor() { editorMenu.requestSystemEditor() }
    onVisibleChanged: if (!visible) dismissSelectionActions()
    Component.onDestruction: dismissSelectionActions()
    // A persistent DOM target receives the first touch. QML paints idle
    // content (including rich spans); the browser paints only during focus.
    Binding {
        target: root
        property: "color"
        value: "transparent"
        when: editorMenu.browserFocused
    }
    Binding {
        target: root
        property: "placeholderTextColor"
        value: "transparent"
        when: editorMenu.browserFocused
    }
    Binding {
        target: root
        property: "selectionColor"
        value: "transparent"
        when: editorMenu.browserFocused
    }
    Binding {
        target: root
        property: "selectedTextColor"
        value: "transparent"
        when: editorMenu.browserFocused
    }
    Binding {
        target: root
        property: "cursorVisible"
        value: false
        when: editorMenu.browserEditing
    }
    function focusEditing() {
        if (Qt.platform.os === "wasm") editorMenu.requestSystemEditor()
        else forceActiveFocus()
    }
    readonly property bool editingFocus: editorMenu.browserEditing ? editorMenu.browserFocused : activeFocus

    readonly property string analyzedText: text.length > 0 ? text : placeholderText
    readonly property int effectiveTextDirection: forceLeftToRight ? Qt.LeftToRight
                                                  : forceRightToLeft ? Qt.RightToLeft
                                                  : (analyzedText.length === 0
                                                     ? Theme.uiDirection
                                                     : (localizationController.isRightToLeftText(analyzedText)
                                                        ? Qt.RightToLeft : Qt.LeftToRight))

    // Preserve a selected range when the floating Copy toolbar receives
    // a tap, including in secondary cast/thread editors.
    activeFocusOnPress: !editorMenu.browserEditing
    selectByMouse: true
    persistentSelection: true

    leftPadding: Theme.space4
    rightPadding: Theme.space4
    topPadding: Theme.space3
    bottomPadding: Theme.space3
    color: Theme.ink
    placeholderTextColor: Theme.inkSubtle
    selectionColor: Theme.primarySurfaceHover
    selectedTextColor: Theme.ink
    font.family: Theme.fontFamilyForText(analyzedText)
    font.pixelSize: Theme.typeBodySize
    hoverEnabled: true
    opacity: root.enabled ? 1.0 : 0.58
    LayoutMirroring.enabled: false
    horizontalAlignment: effectiveTextDirection === Qt.RightToLeft ? TextEdit.AlignRight : TextEdit.AlignLeft

    background: NeonSurface {
        interactive: root.enabled && !root.readOnly
        ambientNeon: root.editingFocus
        focused: root.editingFocus
        neonStrength: Theme.neonControlStrength
        neonColor: root.editingFocus
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
            : "transparent"
        fillColor: root.enabled
                   ? (root.editingFocus ? Theme.surfaceElevated : root.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                   : Theme.surfaceMuted
        borderWidth: root.editingFocus ? 2 : 1
        borderColor: root.editingFocus ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveContainer
        elevation: root.editingFocus ? Theme.elevationControl : Theme.elevationNone
        shadowColor: root.editingFocus ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: 0.82
    }
}
