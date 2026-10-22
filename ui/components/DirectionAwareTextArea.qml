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
        enabled: root.selectAllOnDoubleTap
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
    ContextMenu.menu: EditorContextMenu { editor: root }

    // A compact selection menu must appear beside the selected word in both
    // the cast composer and its reply/thread editors (not at the screen edge).
    // Keep the existing full context menu for desktop right-click.
    function showSelectionActions() {
        if (!root.selectedText.length || !selectionBar.parent) return
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
        if (!root.text.length) return
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

    // Two draggable grips supplement QInputMethod's optional native handles
    // on Android/iOS/WASM. Only the tiny handles capture pointer events:
    // the rest of TextArea retains mouse drag, ordinary tap, typing & scroll.
    Item {
        id: selectionGrips
        objectName: "composerTextSelectionHandles"
        anchors.fill: parent
        z: 12
        visible: root.selectAllOnDoubleTap && root.activeFocus
                 && root.selectedText.length > 0
                 && !root.inputMethodComposing
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
                width: 44
                height: 44
                // Keep grab targets inside the editor while their colored
                // pins point to the exact UTF-16 selection boundaries.
                x: Math.max(0, Math.min(root.width - width,
                    caretRect.x - (leading ? width * 0.65 : width * 0.35)))
                y: Math.max(0, Math.min(root.height - height,
                    caretRect.y + (leading ? -height * 0.40
                                           : caretRect.height - height * 0.60)))
                z: 1
                Rectangle {
                    anchors.centerIn: parent
                    width: 13
                    height: 13
                    radius: 6.5
                    color: Theme.accent
                    border.width: 2
                    border.color: Theme.editorSurface
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    cursorShape: Qt.SizeHorCursor
                    property int oppositeEdge: 0
                    onPressed: mouse => {
                        oppositeEdge = grip.leading ? root.selectionEnd : root.selectionStart
                        root.selectionHandleDragging = true
                        selectionDelay.stop()
                        selectionBar.close()
                    }
                    onPositionChanged: mouse => {
                        if (!pressed) return
                        const point = mapToItem(root, mouse.x, mouse.y)
                        const position = root.positionAt(point.x, point.y)
                        // Never collapse the selection during a handle drag.
                        const next = grip.leading ? Math.min(oppositeEdge - 1, position)
                                                  : Math.max(oppositeEdge + 1, position)
                        root.select(Math.max(0, Math.min(root.length, next)), oppositeEdge)
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
