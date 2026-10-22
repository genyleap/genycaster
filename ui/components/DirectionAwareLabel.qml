import QtQuick
import QtQuick.Controls

Label {
    id: control

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false
    // Most prose throughout Genycaster uses this label. Allow selecting and
    // copying it directly on desktop and touch/long-press where Qt supports it.
    // Interactive action labels should opt out via selectableText: false.
    property bool selectableText: true
    textInteractionFlags: selectableText
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
