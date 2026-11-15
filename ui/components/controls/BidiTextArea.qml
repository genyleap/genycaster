import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

TextArea {
    id: root

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false
    // Opt-in for composer fields: mobile users double-tap to select the whole
    // draft rather than only the word under their finger.
    property bool selectAllOnDoubleTap: false
    // Touchscreen platforms may not provide QInputMethod selection handles
    // for Qt Quick TextArea, particularly in WASM. These Qt-native handles
    // operate on TextEdit character indices, preserving normal desktop
    // mouse dragging, keyboard shift-selection, IME and text editing.
    property bool selectionGestureActive: false
    property bool selectionPressActive: false
    property bool selectionHandleDragging: false
    property int selectionReleaseRetries: 0
    // Canvas-only fallback. Qt provides platform text-selection affordances on
    // native mobile and desktop; do not draw a second toolbar over them.
    readonly property bool customCanvasSelection: Qt.platform.os === "wasm"
        && !textSelectionSettings.systemSelectionEnabled
    readonly property bool platformNativeMenu: Qt.platform.os === "ios"
        || Qt.platform.os === "android"
        || textSelectionSettings.systemSelectionEnabled

    function resetSelection() {
        selectionDelay.stop()
        selectionBar.close()
        root.deselect() // TextEdit.deselect(): selection only, never erases draft.
    }
    function dismissSelectionActions() {
        selectionDelay.stop()
        selectionBar.close()
        selectionPressActive = false
        selectionGestureActive = false
        selectionHandleDragging = false
        selectionReleaseRetries = 0
    }
    onVisibleChanged: if (!visible) dismissSelectionActions()
    Component.onDestruction: dismissSelectionActions()


    TapHandler {
        enabled: root.selectAllOnDoubleTap && root.customCanvasSelection
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        onDoubleTapped: {
            if (root.text.length > 0) {
                root.selectAll()
                root.showSelectionActions()
            }
        }
    }

    // Qt native menus require real MenuItems: EditorContextMenu uses plain
    // Items to work around canvas painting bugs and must never be Popup.Native.
    // Prefer platform menu actions and Qt's own selection behavior everywhere
    // except the WebAssembly canvas / explicit Genycaster desktop mode.
    ContextMenu.menu: root.platformNativeMenu ? nativeContextMenu : canvasContextMenu
    EditorContextMenu {
        id: canvasContextMenu
        editor: root
        nativePresentation: false
    }
    Menu {
        id: nativeContextMenu
        popupType: Popup.Native
        MenuItem {
            text: qsTrId("genycaster.editor.undo")
            enabled: root.canUndo
            onTriggered: root.undo()
        }
        MenuItem {
            text: qsTrId("genycaster.editor.redo")
            enabled: root.canRedo
            onTriggered: root.redo()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTrId("genycaster.editor.cut")
            enabled: !root.readOnly && root.selectedText.length > 0
            onTriggered: root.cut()
        }
        MenuItem {
            text: qsTrId("genycaster.feedback.copy")
            enabled: root.selectedText.length > 0
            onTriggered: root.copy()
        }
        MenuItem {
            text: qsTrId("genycaster.editor.paste")
            enabled: !root.readOnly
            onTriggered: root.paste()
        }
        MenuItem {
            text: qsTrId("genycaster.editor.select_all")
            enabled: root.text.length > 0
            onTriggered: root.selectAll()
        }
    }

    // A compact selection menu must appear beside the selected word in both
    // the cast composer and its reply/thread editors (not at the screen edge).
    // Keep the existing full context menu for desktop right-click.
    function showSelectionActions() {
        // On touch and browser Qt runtimes, TextArea can lose activeFocus while the
        // selection/keyboard gesture is being delivered. Selection itself is
        // authoritative; the parent's visibility and composer close lifecycle
        // prevent this window-overlay popup from leaking onto Home.
        if (!root.customCanvasSelection || !root.visible
                || !root.selectedText.length || !selectionBar.parent) return
        const start = root.positionToRectangle(root.selectionStart)
        const end = root.positionToRectangle(root.selectionEnd)
        // Anchor to the selected range, not cursorRectangle (the latter can
        // point offscreen after Select All or an IME update).
        const first = root.mapToItem(selectionBar.parent, start.x, start.y)
        const last = root.mapToItem(selectionBar.parent, end.x, end.y)
        const selectionTop = Math.min(first.y, last.y)
        const selectionBottom = Math.max(first.y + start.height, last.y + end.height)
        const center = (first.x + last.x) / 2
        const menuWidth = selectionBar.implicitWidth
        const menuHeight = selectionBar.implicitHeight
        const edge = 8
        selectionBar.x = Math.max(edge, Math.min(
            Math.max(edge, selectionBar.parent.width - menuWidth - edge),
            center - menuWidth / 2))
        const above = selectionTop - menuHeight - 12
        const below = selectionBottom + 12
        selectionBar.y = Math.max(edge, Math.min(
            Math.max(edge, selectionBar.parent.height - menuHeight - edge),
            above >= edge ? above : below))
        selectionBar.open()
    }

    Timer {
        id: selectionDelay
        interval: 330
        repeat: false
        onTriggered: {
            if (!root.customCanvasSelection || !root.visible
                    || root.selectedText.length === 0 || root.selectionHandleDragging)
                return
            // Qt can deliver selectedTextChanged before onReleased, or drop
            // the TextArea release when the browser's touch grab changes.
            // Retry after the finger lifts without losing the selection.
            if (root.selectionPressActive || root.selectionGestureActive) {
                // A browser may not forward the final touch release to
                // TextArea after a selection gesture. Bound the retry loop so
                // the user still gets Copy and Select All.
                if (++root.selectionReleaseRetries < 4) {
                    selectionDelay.restart()
                    return
                }
                root.selectionPressActive = false
                root.selectionGestureActive = false
            }
            root.selectionReleaseRetries = 0
            root.showSelectionActions()
        }
    }
    onSelectedTextChanged: {
        if (!root.customCanvasSelection) return
        root.selectionReleaseRetries = 0
        if (selectedText.length === 0) {
            selectionDelay.stop()
            selectionBar.close()
        } else if (!selectionHandleDragging && root.visible) {
            // The selection signal, not onReleased, is the reliable completion
            // path for canvas-based touch selection in browser runtimes.
            selectionDelay.restart()
        }
    }
    onPressAndHold: mouse => {
        if (!root.customCanvasSelection || !root.text.length) return
        root.selectionGestureActive = true
        if (!root.selectedText.length) {
            root.cursorPosition = root.positionAt(mouse.x, mouse.y)
            root.selectWord()
        }
        // Let the finger lift before presenting the toolbar: otherwise the
        // popup steals the gesture that should extend the text selection.
        selectionDelay.stop()
        selectionBar.close()
    }
    onPressed: mouse => {
        root.selectionPressActive = true
        root.selectionReleaseRetries = 0
        selectionDelay.stop()
        selectionBar.close()
    }
    onReleased: mouse => {
        root.selectionPressActive = false
        root.selectionGestureActive = false
        if (root.customCanvasSelection && root.selectedText.length > 0
                && !root.selectionHandleDragging)
            selectionDelay.restart()
    }
    onActiveFocusChanged: {
        if (!activeFocus) {
            selectionPressActive = false
            selectionGestureActive = false
            // A focus change alone is not selection cancellation on Qt/WASM.
            // The selection can outlive the keyboard's temporary focus owner.
            if (root.selectedText.length === 0) {
                selectionDelay.stop()
                selectionBar.close()
            }
        }
    }

    // Paint and hit-test the handles in the window overlay rather than inside
    // TextArea. Otherwise touches at either end are clamped to the text item's
    // geometry and can be stolen by its Flickable/input method on touch runtimes.
    Item {
        id: selectionGrips
        objectName: "composerTextSelectionHandles"
        parent: Overlay.overlay
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        z: 201
        visible: root.selectAllOnDoubleTap && root.customCanvasSelection
                 && root.selectedText.length > 0 && root.visible
        readonly property rect startRect: root.positionToRectangle(root.selectionStart)
        readonly property rect endRect: root.positionToRectangle(root.selectionEnd)

        Repeater {
            model: 2
            delegate: Item {
                id: grip
                required property int index
                readonly property bool leading: index === 0
                readonly property rect caretRect: leading
                    ? selectionGrips.startRect : selectionGrips.endRect
                readonly property point onScreen: root.mapToItem(selectionGrips,
                    caretRect.x, caretRect.y)
                width: 44
                height: 44
                x: Math.max(0, Math.min(selectionGrips.width - width,
                    onScreen.x - width * 0.5))
                y: Math.max(0, Math.min(selectionGrips.height - height,
                    onScreen.y + (leading ? -height * 0.5
                                         : caretRect.height - height * 0.5)))
                z: 2
                Rectangle {
                    x: parent.width / 2 - 1
                    y: grip.leading ? parent.height * 0.48 : parent.height * 0.1
                    width: 2
                    height: Math.max(14, grip.caretRect.height * 0.7)
                    color: Theme.dark ? "#0A84FF" : "#007AFF"
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    radius: 7
                    color: Theme.dark ? "#0A84FF" : "#007AFF"
                    border.width: 2
                    border.color: Theme.editorSurface
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    cursorShape: Qt.SizeHorCursor
                    property int oppositeEdge: 0
                    property real pressDeltaX: 0
                    property real pressDeltaY: 0
                    onPressed: mouse => {
                        oppositeEdge = grip.leading ? root.selectionEnd : root.selectionStart
                        const point = mapToItem(root, mouse.x, mouse.y)
                        pressDeltaX = point.x - grip.caretRect.x
                        pressDeltaY = point.y - grip.caretRect.y
                        root.selectionHandleDragging = true
                        selectionDelay.stop()
                        selectionBar.close()
                    }
                    onPositionChanged: mouse => {
                        if (!pressed) return
                        const point = mapToItem(root, mouse.x, mouse.y)
                        const position = root.positionAt(
                            point.x - pressDeltaX, point.y - pressDeltaY)
                        // Keep at least one character selected; the opposite
                        // handle remains fixed throughout the gesture.
                        const next = grip.leading ? Math.min(oppositeEdge - 1, position)
                                                  : Math.max(oppositeEdge + 1, position)
                        const bounded = Math.max(0, Math.min(root.length, next))
                        root.select(bounded, oppositeEdge)
                    }
                    onReleased: {
                        root.selectionHandleDragging = false
                        root.showSelectionActions()
                    }
                    onCanceled: root.selectionHandleDragging = false
                }
            }
        }
    }

    // Keep the selection alive while the user taps Copy. Tapping a toolbar
    // button can change activeFocus before its onClicked runs in Qt/WASM.

    Popup {
        id: selectionBar
        objectName: "editorSelectionActions"
        parent: Overlay.overlay
        focus: false
        modal: false
        dim: false
        // Popup.visible is state controlled by open()/close(). Binding it to
        // !systemSelectionEnabled forced the popup open, even on Home.
        padding: Theme.space1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Superellipse {
            fillColor: Theme.surfaceElevated
            borderColor: Theme.borderMuted
            borderWidth: 1
            curveDepth: Theme.curveContainer
            elevation: Theme.elevationControl
        }
        contentItem: RowLayout {
            layoutDirection: Theme.uiDirection
            spacing: Theme.space1
            ActionButton {
                compact: true
                quiet: true
                focusPolicy: Qt.NoFocus
                text: qsTrId("genycaster.feedback.copy")
                onClicked: {
                    // Use the browser clipboard bridge on WebAssembly;
                    // TextArea.copy() alone can stop at Qt's virtual clipboard.
                    presentationBridge.copyText(root.selectedText)
                    selectionBar.close()
                }
            }
            ActionButton {
                compact: true
                quiet: true
                focusPolicy: Qt.NoFocus
                text: qsTrId("genycaster.editor.cut")
                onClicked: {
                    presentationBridge.copyText(root.selectedText)
                    root.cut()
                    selectionBar.close()
                }
            }
            ActionButton {
                compact: true
                quiet: true
                focusPolicy: Qt.NoFocus
                text: qsTrId("genycaster.editor.select_all")
                onClicked: {
                    root.selectAll()
                    root.showSelectionActions()
                }
            }
            ActionButton {
                compact: true
                quiet: true
                focusPolicy: Qt.NoFocus
                text: qsTrId("genycaster.editor.clear_selection")
                onClicked: root.resetSelection()
            }
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
