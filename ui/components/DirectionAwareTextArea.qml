import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    function resetSelection() {
        selectionDelay.stop()
        selectionBar.close()
        root.deselect() // TextEdit.deselect(): selection only, never erases draft.
    }


    TapHandler {
        enabled: root.selectAllOnDoubleTap && !textSelectionSettings.systemSelectionEnabled
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        onDoubleTapped: {
            if (root.text.length > 0) {
                root.selectAll()
                root.showSelectionActions()
            }
        }
    }

    // Override Qt 6.9+'s generic native-looking context menu in every
    // visual, Markdown, list and thread editor using the shared theme.
    ContextMenu.menu: EditorContextMenu {
        editor: root
        nativePresentation: textSelectionSettings.systemSelectionEnabled
    }

    // A compact selection menu must appear beside the selected word in both
    // the cast composer and its reply/thread editors (not at the screen edge).
    // Keep the existing full context menu for desktop right-click.
    function showSelectionActions() {
        if (textSelectionSettings.systemSelectionEnabled
                || !root.selectedText.length || !selectionBar.parent) return
        const caret = root.cursorRectangle
        const position = root.mapToItem(selectionBar.parent, caret.x, caret.y)
        selectionBar.x = Math.max(8, Math.min(selectionBar.parent.width
                                            - selectionBar.implicitWidth - 8,
                                            position.x - selectionBar.implicitWidth / 2))
        selectionBar.y = Math.max(8, position.y - selectionBar.implicitHeight - 12)
        selectionBar.open()
    }

    Timer {
        id: selectionDelay
        interval: 260
        repeat: false
        onTriggered: if (root.activeFocus && root.selectedText.length > 0)
                         root.showSelectionActions()
    }
    onSelectedTextChanged: {
        if (selectedText.length > 0 && activeFocus
                && !selectionPressActive && !selectionGestureActive
                && !selectionHandleDragging)
            selectionDelay.restart()
        else {
            selectionDelay.stop()
            selectionBar.close()
        }
    }
    onPressAndHold: mouse => {
        if (textSelectionSettings.systemSelectionEnabled || !root.text.length) return
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
        selectionDelay.stop()
        selectionBar.close()
    }
    onReleased: mouse => {
        root.selectionPressActive = false
        root.selectionGestureActive = false
        if (root.selectedText.length > 0 && !root.selectionHandleDragging)
            root.showSelectionActions()
    }
    onActiveFocusChanged: {
        if (!activeFocus) {
            selectionPressActive = false
            selectionGestureActive = false
            selectionDelay.stop()
        }
    }

    // Paint and hit-test the handles in the window overlay rather than inside
    // TextArea. Otherwise touches at either end are clamped to the text item's
    // geometry and can be stolen by its Flickable/input method on iOS/WASM.
    Item {
        id: selectionGrips
        objectName: "composerTextSelectionHandles"
        parent: Overlay.overlay
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        z: 201
        visible: root.selectAllOnDoubleTap && !textSelectionSettings.systemSelectionEnabled
                 && root.activeFocus && root.selectedText.length > 0
                 && !root.inputMethodComposing && root.visible
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
        visible: !textSelectionSettings.systemSelectionEnabled
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
            spacing: Theme.space1
            ActionButton {
                compact: true
                quiet: true
                focusPolicy: Qt.NoFocus
                text: qsTrId("genycaster.feedback.copy")
                onClicked: {
                    // Use the browser clipboard bridge on WASM/iOS Safari;
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

    background: Superellipse {
        fillColor: root.enabled
                   ? (root.activeFocus ? Theme.surfaceElevated : root.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                   : Theme.surfaceMuted
        borderWidth: root.activeFocus ? 2 : 1
        borderColor: root.activeFocus ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveContainer
        elevation: root.activeFocus ? Theme.elevationControl : Theme.elevationNone
        shadowColor: root.activeFocus ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: 0.82
        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
    }
}
