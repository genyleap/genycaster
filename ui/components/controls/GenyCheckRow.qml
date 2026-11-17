import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// One cross-platform checkbox for backup, registration and recovery consent.
// A consistently painted row prevents hovering one checkbox from creating a
// large gray slab while the next checkbox appears as a bare icon and label.
// Button.checkable owns the state; never add a second onClicked toggle.
Button {
    id: control
    property string label: ""
    checkable: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    topPadding: Theme.space2
    bottomPadding: Theme.space2
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    implicitHeight: Math.max(Theme.touchTarget,
                             choiceContent.implicitHeight + topPadding + bottomPadding)
    implicitWidth: choiceContent.implicitWidth + leftPadding + rightPadding
    Accessible.role: Accessible.CheckBox
    Accessible.name: label
    Accessible.checked: checked

    // The entire 44px+ row is the hit target; checked, unchecked, hover and
    // keyboard-focus states use the same card geometry and do not jump.
    background: NeonSurface {
        interactive: control.enabled
        deferredNeon: true
        ambientNeon: false
        focused: Theme.keyboardFocusVisible(control)
        externalHovered: control.hovered
        fillColor: control.down ? Theme.surfacePressed
                   : control.checked ? Theme.surfaceSelected
                   : control.hovered ? Theme.surfaceHover : Theme.surfaceMuted
        borderWidth: Theme.keyboardFocusVisible(control) ? 2 : Theme.strokeHairline
        borderColor: Theme.keyboardFocusVisible(control) ? Theme.focus
                   : control.checked ? Theme.accent : Theme.borderMuted
        curveDepth: Theme.curveControl
        neonStrength: Theme.neonControlStrength
    }

    contentItem: RowLayout {
        id: choiceContent
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        // A visible outline even in Light mode, and an accessible tick
        // whose contrast follows the currently selected accent palette.
        Superellipse {
            Layout.preferredWidth: Theme.scaleUi(20)
            Layout.preferredHeight: Theme.scaleUi(20)
            Layout.alignment: Qt.AlignVCenter
            fillColor: control.checked ? Theme.accent : Theme.surface
            borderColor: control.checked ? Theme.accent : Theme.borderStrong
            borderWidth: Theme.strokeStandard
            curveDepth: Theme.scaleUi(5)

            Icon {
                anchors.centerIn: parent
                visible: control.checked
                text: Theme.iconCheck
                color: Theme.contrastInk(Theme.accent)
                preferredSize: Theme.scaleUi(14)
                Accessible.ignored: true
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            text: control.label
            color: control.enabled ? Theme.ink : Theme.inkDisabled
            // A consent checkbox is a setting label, not a section heading.
            font.pixelSize: Theme.typeLabelSize
            font.weight: Font.Normal
            lineHeight: 1.25
            lineHeightMode: Text.ProportionalHeight
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }
}
