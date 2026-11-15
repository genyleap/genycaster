import QtQuick
import QtQuick.Controls
import Farcaster.Client

TextField {
    id: root

    // Use the same accessible, theme-aware context menu as the rich editor.
    ContextMenu.menu: EditorContextMenu { editor: root }

    property bool forceLeftToRight: false
    property bool forceRightToLeft: false

    readonly property string analyzedText: text.length > 0 ? text : placeholderText
    readonly property int effectiveTextDirection: forceLeftToRight ? Qt.LeftToRight
                                                  : forceRightToLeft ? Qt.RightToLeft
                                                  : (analyzedText.length === 0
                                                     ? Theme.uiDirection
                                                     : (localizationController.isRightToLeftText(analyzedText)
                                                        ? Qt.RightToLeft : Qt.LeftToRight))

    implicitHeight: Theme.controlHeight
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    topPadding: 0
    bottomPadding: 0
    color: Theme.ink
    placeholderTextColor: Theme.inkSubtle
    selectionColor: Theme.primarySurfaceHover
    selectedTextColor: Theme.ink
    font.family: Theme.fontFamilyForText(analyzedText)
    font.pixelSize: Theme.typeLabelSize
    hoverEnabled: true
    opacity: root.enabled ? 1.0 : 0.58
    LayoutMirroring.enabled: false
    horizontalAlignment: effectiveTextDirection === Qt.RightToLeft ? TextInput.AlignRight : TextInput.AlignLeft

    background: NeonSurface {
        interactive: root.enabled && !root.readOnly
        ambientNeon: root.activeFocus
        focused: root.activeFocus
        neonStrength: Theme.neonControlStrength
        neonColor: root.activeFocus
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)
            : "transparent"
        fillColor: root.enabled
                   ? (root.activeFocus ? Theme.surfaceElevated : root.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                   : Theme.surfaceMuted
        borderWidth: root.activeFocus ? 2 : 1
        borderColor: root.activeFocus ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveControl
        elevation: root.activeFocus ? Theme.elevationControl : Theme.elevationNone
        shadowColor: root.activeFocus ? Theme.focusHalo : Theme.shadow
        shadowOpacityScale: 0.82
    }
}
