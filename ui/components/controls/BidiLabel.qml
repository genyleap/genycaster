import QtQuick
import QtQuick.Controls
import Farcaster.Client

Label {
    id: control

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false
    // Chrome labels should align to the interface's reading edge even when
    // their content starts with a Latin brand, handle, or product name.
    // Keep the Unicode bidi algorithm in charge of the glyph order.
    property bool alignToUi: false
    // QQC2 Label does not support textInteractionFlags. Applying that
    // property prevents the whole QML root from loading in WebAssembly.
    // Keep this compatibility hint; selectable body text is implemented by
    // RichInlineTextItem, whose QTextDocument owns a real selection cursor.
    property bool selectableText: true

    readonly property string analyzedText: text.length > 0 ? text : (typeof placeholderText !== "undefined" ? placeholderText : "")
    readonly property int effectiveTextDirection: forceLeftToRight ? Qt.LeftToRight
                                                  : forceRightToLeft ? Qt.RightToLeft
                                                  : (analyzedText.length === 0
                                                     ? Theme.uiDirection
                                                     : (localizationController.isRightToLeftText(analyzedText)
                                                        ? Qt.RightToLeft : Qt.LeftToRight))

    LayoutMirroring.enabled: false
    font.family: Theme.fontFamilyForText(analyzedText)
    horizontalAlignment: alignToUi ? Theme.textStartAlignment
                       : effectiveTextDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft

    // QQC2 Label has no textInteractionFlags. Keep Control's sizing/layout
    // contract and expose read-only selection only after an explicit hold.
    // This also covers profile metadata, timestamps and other native labels.
    property bool selectionActive: false
    onTextChanged: selectionActive = false
    onVisibleChanged: if (!visible) selectionActive = false

    TapHandler {
        enabled: control.selectableText && !control.selectionActive
                 && control.text.length > 0
        gesturePolicy: TapHandler.DragThreshold
        longPressThreshold: 0.50
        onLongPressed: control.selectionActive = true
    }

    // Loading selection controls for *every* label would increase delegate
    // creation cost in the cast feed. Instantiate them only on long press.
    Loader {
        id: selectionOverlay
        active: control.selectionActive
        anchors.fill: parent
        z: 5
        sourceComponent: Component {
            Item {
                TextEdit {
                    id: selectionMirror
                    anchors.fill: parent
                    anchors.leftMargin: control.leftPadding
                    anchors.rightMargin: control.rightPadding
                    anchors.topMargin: control.topPadding
                    anchors.bottomMargin: control.bottomPadding
                    text: control.text
                    textFormat: TextEdit.PlainText
                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                    wrapMode: control.wrapMode === Text.Wrap
                              ? TextEdit.Wrap : TextEdit.NoWrap
                    horizontalAlignment: control.horizontalAlignment
                    font: control.font
                    color: "transparent"
                    selectedTextColor: Theme.textPrimary
                    selectionColor: Theme.accent
                    Component.onCompleted: selectAll()
                }

                Superellipse {
                    objectName: "labelCopySelection"
                    z: 2
                    anchors.top: parent.top
                    anchors.right: parent.right
                    width: Math.max(70, copyText.implicitWidth + Theme.space4 * 2)
                    height: 34
                    curveDepth: Theme.curveControl
                    fillColor: Theme.surfaceElevated
                    borderColor: Theme.borderStrong
                    borderWidth: Theme.strokeHairline
                    Text {
                        id: copyText
                        anchors.centerIn: parent
                        text: qsTrId("genycaster.feedback.copy")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            presentationBridge.copyText(
                                selectionMirror.selectedText.length > 0
                                  ? selectionMirror.selectedText : control.text)
                            control.selectionActive = false
                        }
                    }
                }
            }
        }
    }
}
