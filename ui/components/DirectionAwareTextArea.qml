import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TextArea {
    id: root

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false

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
        if (selectedText.length > 0 && activeFocus)
            selectionDelay.restart()
        else {
            selectionDelay.stop()
            selectionBar.close()
        }
    }
    onPressAndHold: mouse => {
        if (!root.text.length) return
        if (!root.selectedText.length) {
            root.cursorPosition = root.positionAt(mouse.x, mouse.y)
            root.selectWord()
        }
        selectionDelay.stop()
        root.showSelectionActions()
    }
    onActiveFocusChanged: if (!activeFocus) selectionBar.close()

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
                text: qsTrId("genycaster.editor.select_all")
                onClicked: {
                    root.selectAll()
                    selectionBar.close()
                }
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
