import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Unified accessible full-row check control for recovery and consent.
Button {
    id: control
    property string label: ""
    checkable: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: 0
    implicitHeight: Math.max(48, checkboxContent.implicitHeight + Theme.space2)
    implicitWidth: checkboxContent.implicitWidth
    Accessible.role: Accessible.CheckBox
    Accessible.name: label
    Accessible.checked: checked

    background: Rectangle {
        radius: Theme.space2
        color: control.down ? Theme.surfacePressed
              : control.hovered ? Theme.surfaceHover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
    contentItem: RowLayout {
        id: checkboxContent
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection
        Superellipse {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignVCenter
            fillColor: control.checked ? Theme.ink : Theme.surface
            borderColor: Theme.keyboardFocusVisible(control)
                         ? Theme.focus : Theme.borderMuted
            borderWidth: Theme.keyboardFocusVisible(control) ? 2 : 1
            curveDepth: 6
            Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
            Icon {
                anchors.centerIn: parent
                text: Theme.iconCheck
                color: Theme.surface
                preferredSize: 15
                opacity: control.checked ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: control.label
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.ink
            wrapMode: Text.WordWrap
        }
    }
}
