import QtQuick
import QtQuick.Controls
import Farcaster.Client

TextField {
    id: root

    // Keep Qt's default ContextMenu and IME on native platforms.
    // DOM text ownership is isolated to the WASM adapter.
    EditorContextMenu { id: editorMenu; editor: root }
    property var clipboardPasteHandler: null
    readonly property bool clipboardPasteFailed: editorMenu.pasteFailed
    function requestClipboardPaste() { editorMenu.requestPaste() }
    function cancelClipboardPaste() { editorMenu.cancelPaste() }
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

    activeFocusOnPress: !editorMenu.browserEditing
    selectByMouse: true

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false

    readonly property string analyzedText: text.length > 0 ? text : placeholderText
    readonly property int effectiveTextDirection: forceLeftToRight ? Qt.LeftToRight
                                                  : forceRightToLeft ? Qt.RightToLeft
                                                  : (analyzedText.length === 0
                                                     ? Theme.uiDirection
                                                     : (localizationController.isRightToLeftText(analyzedText)
                                                        ? Qt.RightToLeft : Qt.LeftToRight))

    implicitHeight: Theme.controlHeight
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    topPadding: 0
    bottomPadding: 0
    color: Theme.ink
    placeholderTextColor: Theme.inkSubtle
    selectionColor: Theme.primarySurfaceHover
    selectedTextColor: Theme.ink
    font.family: Theme.fontFamilyForText(analyzedText)
    font.pixelSize: Theme.typeLabelSize
    hoverEnabled: true
    opacity: root.enabled ? 1.0 : 0.58
    LayoutMirroring.enabled: false
    horizontalAlignment: effectiveTextDirection === Qt.RightToLeft ? TextInput.AlignRight : TextInput.AlignLeft

    background: NeonSurface {
        interactive: root.enabled && !root.readOnly
        ambientNeon: root.editingFocus
        focused: root.editingFocus
        neonStrength: Theme.neonControlStrength
        neonColor: root.editingFocus
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)
            : "transparent"
        fillColor: root.enabled
                   ? (root.editingFocus ? Theme.surfaceElevated : root.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                   : Theme.surfaceMuted
        borderWidth: root.editingFocus ? 2 : 1
        borderColor: root.editingFocus ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveControl
        elevation: root.editingFocus ? Theme.elevationControl : Theme.elevationNone
        shadowColor: root.editingFocus ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: 0.82
    }
}
