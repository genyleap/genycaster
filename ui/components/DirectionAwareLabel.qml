import QtQuick
import QtQuick.Controls

Label {
    id: control

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false
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
    horizontalAlignment: effectiveTextDirection === Qt.RightToLeft ? Text.AlignRight : Text.AlignLeft
}
