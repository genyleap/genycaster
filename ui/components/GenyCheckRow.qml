import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Recovery choices use the same Superellipse, surface, accent and focus
// tokens as GenyCaster's standard controls. Never use the native CheckBox
// indicator (which differs between macOS, Windows, Android and WASM).
Button {
    id: control
    property string label: ""
    checkable: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    topPadding: Theme.space2
    bottomPadding: Theme.space2
    leftPadding: 0
    rightPadding: 0
    implicitHeight: Math.max(Theme.touchTarget,
                             choiceContent.implicitHeight + topPadding + bottomPadding)
    implicitWidth: choiceContent.implicitWidth
    Accessible.role: Accessible.CheckBox
    Accessible.name: label
    Accessible.checked: checked

    // A recovery phrase is sensitive: no full-row hover plate or animation.
    // The entire row remains a keyboard and pointer accessible hit target.
    background: Item {}

    contentItem: RowLayout {
        id: choiceContent
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection

        Superellipse {
            Layout.preferredWidth: Theme.iconSizeNavigation
            Layout.preferredHeight: Theme.iconSizeNavigation
            Layout.alignment: Qt.AlignVCenter
            curveDepth: Theme.curveTiny
            exponent: Theme.superellipseExponent
            fillColor: control.checked ? Theme.primarySurface
                                      : control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
            borderWidth: Theme.keyboardFocusVisible(control)
                         ? Theme.strokeEmphasis + Theme.strokeHairline
                         : control.checked ? Theme.strokeEmphasis : Theme.strokeStandard
            borderColor: Theme.keyboardFocusVisible(control) ? Theme.focus
                         : control.checked ? Theme.accent
                         : control.hovered ? Theme.borderStrong : Theme.borderMuted

            Icon {
                anchors.centerIn: parent
                text: Theme.iconCheck
                color: Theme.accent
                preferredSize: Theme.iconSizeSmall
                visible: control.checked
                Accessible.ignored: true
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            text: control.label
            color: control.enabled ? Theme.ink : Theme.inkDisabled
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightMedium
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }
}
