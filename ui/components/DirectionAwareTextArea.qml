import QtQuick
import QtQuick.Controls

TextArea {
    id: root

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false

    // Override Qt 6.9+'s generic native-looking context menu in every
    // visual, Markdown, list and thread editor using the shared theme.
    ContextMenu.menu: EditorContextMenu { editor: root }

    readonly property string analyzedText: text.length > 0 ? text : placeholderText
    readonly property int effectiveTextDirection: forceLeftToRight ? Qt.LeftToRight
                                                  : forceRightToLeft ? Qt.RightToLeft
                                                  : (analyzedText.length === 0
                                                     ? Theme.uiDirection
                                                     : (localizationController.isRightToLeftText(analyzedText)
                                                        ? Qt.RightToLeft : Qt.LeftToRight))

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
