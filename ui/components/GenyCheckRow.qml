import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A single, accessible recovery choice. The hit target includes its entire
// label, while the indicator remains optically aligned on every platform.
Button {
    id: control
    property string label: ""
    checkable: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    topPadding: 7
    bottomPadding: 7
    leftPadding: 0
    rightPadding: 0
    implicitHeight: Math.max(44, choiceContent.implicitHeight + topPadding + bottomPadding)
    implicitWidth: choiceContent.implicitWidth
    Accessible.role: Accessible.CheckBox
    Accessible.name: label
    Accessible.checked: checked

    // No selected-row card: two adjacent choices must remain visually equal.
    background: Rectangle {
        radius: Theme.space2
        color: control.down || (control.hovered && control.enabled)
               ? Theme.surfaceHover : "transparent"
        opacity: control.down ? 0.72 : control.hovered ? 0.45 : 0
        Behavior on opacity {
            NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }
    }

    contentItem: RowLayout {
        id: choiceContent
        spacing: 10
        layoutDirection: Theme.uiDirection

        Rectangle {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignVCenter
            radius: 5
            color: control.checked ? Theme.accent : Theme.surface
            border.width: Theme.keyboardFocusVisible(control) ? 2 : 1.5
            border.color: Theme.keyboardFocusVisible(control) ? Theme.focus
                         : control.checked ? Theme.accent : Theme.borderStrong
            Behavior on color {
                ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
            }
            Icon {
                anchors.centerIn: parent
                text: Theme.iconCheck
                color: Theme.surface
                preferredSize: 13
                opacity: control.checked ? 1 : 0
                Behavior on opacity {
                    NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            text: control.label
            color: control.enabled ? Theme.ink : Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }
}
