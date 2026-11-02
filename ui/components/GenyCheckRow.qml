import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Recovery uses the same dark-ink checkbox as the legal-consent screen,
// not the separately configurable accent color. Cross-platform styling is
// owned by GenyCaster rather than the native CheckBox indicator.
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

        // Match the legal-consent checkbox in InitialSetup exactly. Its ink
        // fill is deliberately independent of the user-selected accent theme.
        Superellipse {
            Layout.preferredWidth: 23
            Layout.preferredHeight: 23
            Layout.alignment: Qt.AlignVCenter
            fillColor: control.checked ? Theme.ink : "transparent"
            borderColor: Theme.inkMuted
            borderWidth: 1
            curveDepth: 5

            Icon {
                anchors.centerIn: parent
                visible: control.checked
                text: Theme.iconCheck
                color: Theme.surface
                preferredSize: 15
                Accessible.ignored: true
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            text: control.label
            color: control.enabled ? Theme.ink : Theme.inkDisabled
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightMedium
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }
}
