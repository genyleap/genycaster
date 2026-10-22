import QtQuick
import QtQuick.Controls

Label {
    id: control

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false
    property bool selectable: false
    textInteractionFlags: selectable
                          ? (Qt.TextSelectableByMouse | Qt.TextSelectableByKeyboard)
                          : Qt.NoTextInteraction

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
