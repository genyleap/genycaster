import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

TextArea {
    id: root

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false
    // Editing gestures belong to the platform, not the feed-selection preference.
    ContextMenu.menu: editorMenu.platformMobile || editorMenu.browser ? null : editorMenu
    EditorContextMenu { id: editorMenu; editor: root }
    function resetSelection() { deselect() }
    function dismissSelectionActions() { if (editorMenu) editorMenu.cancelPaste() }
    function requestClipboardPaste() { editorMenu.requestPaste() }
    function requestSystemEditor() { editorMenu.requestSystemEditor() }
    onVisibleChanged: if (!visible) dismissSelectionActions()
    Component.onDestruction: dismissSelectionActions()
    // Only the live DOM input paints glyphs while it owns editing. The
    // original QML control still owns its geometry, border and data.
    Binding {
        target: root
        property: "color"
        value: "transparent"
        when: editorMenu.browserEditing
    }
    Binding {
        target: root
        property: "placeholderTextColor"
        value: "transparent"
        when: editorMenu.browserEditing
    }
    Binding {
        target: root
        property: "selectionColor"
        value: "transparent"
        when: editorMenu.browserEditing
    }
    Binding {
        target: root
        property: "selectedTextColor"
        value: "transparent"
        when: editorMenu.browserEditing
    }
    Binding {
        target: root
        property: "cursorVisible"
        value: false
        when: editorMenu.browserEditing
    }
    TapHandler {
        enabled: editorMenu.browser && root.enabled && !root.readOnly
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        gesturePolicy: TapHandler.DragThreshold
        onTapped: {
            root.forceActiveFocus()
            editorMenu.requestSystemEditor()
        }
        onLongPressed: {
            root.forceActiveFocus()
            editorMenu.requestSystemEditor()
        }
    }

    readonly property string analyzedText: text.length > 0 ? text : placeholderText
    readonly property int effectiveTextDirection: forceLeftToRight ? Qt.LeftToRight
                                                  : forceRightToLeft ? Qt.RightToLeft
                                                  : (analyzedText.length === 0
                                                     ? Theme.uiDirection
                                                     : (localizationController.isRightToLeftText(analyzedText)
                                                        ? Qt.RightToLeft : Qt.LeftToRight))

    // Preserve a selected range when the floating Copy toolbar receives
    // a tap, including in secondary cast/thread editors.
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
        ambientNeon: root.activeFocus
        focused: root.activeFocus
        neonStrength: Theme.neonControlStrength
        neonColor: root.activeFocus
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
            : "transparent"
        fillColor: root.enabled
                   ? (root.activeFocus ? Theme.surfaceElevated : root.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                   : Theme.surfaceMuted
        borderWidth: root.activeFocus ? 2 : 1
        borderColor: root.activeFocus ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveContainer
        elevation: root.activeFocus ? Theme.elevationControl : Theme.elevationNone
        shadowColor: root.activeFocus ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: 0.82
    }
}
