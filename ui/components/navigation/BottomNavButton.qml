import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: control

    property bool selected: false
    property bool prominent: false
    property string iconGlyph: ""
    property string badgeText: ""
    property bool showLabel: true
    property bool darkDock: false

    implicitHeight: showLabel ? 56 : 48
    flat: true
    hoverEnabled: true
    containmentMask: background
    Accessible.name: text
    scale: control.down ? 0.985 : 1.0

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
    }

    contentItem: ColumnLayout {
        anchors.centerIn: parent
        spacing: 2
        width: parent.width - Theme.space2 * 2

        Item {
            Layout.alignment: Qt.AlignHCenter
            width: 44
            height: 38

            Superellipse {
                anchors.fill: parent
                fillColor: control.selected
                           ? (control.darkDock ? Qt.rgba(1, 1, 1, 0.14) : Theme.surfaceSelected)
                           : "transparent"
                borderWidth: control.selected && control.darkDock ? 1 : 0
                borderColor: Qt.rgba(1, 1, 1, 0.18)
                curveDepth: height / 2

                Loader {
                    anchors.fill: parent
                    active: Theme.neonEnabled && control.selected
                    sourceComponent: NeonHalo {
                        ambient: true
                        cornerRadius: height / 2
                        tintColor: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                           Theme.accent.b, 0.46)
                    }
                }
            }

            Icon {
                anchors.centerIn: parent
                text: control.iconGlyph
                // Selected destinations share the same quiet accent capsule
                // used by Home actions and Explore filters.
                filled: control.selected
                color: !control.enabled ? (control.darkDock ? "#747781" : Theme.inkDisabled)
                       : control.selected ? (control.darkDock ? "#ffffff" : Theme.accent)
                       : (control.darkDock ? "#cbd0d9" : Theme.inkMuted)
                preferredSize: Theme.iconSizeNavigation
                opacity: control.selected ? 1.0 : (control.hovered ? 0.94 : (control.darkDock ? 0.86 : 0.72))
                scale: control.selected ? 1.04 : 1.0
                Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
                Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            }

            Superellipse {
                visible: control.badgeText.length > 0
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: -3
                anchors.rightMargin: -5
                width: Math.max(18, badgeLabel.implicitWidth + 8)
                height: 18
                fillColor: Theme.danger
                curveDepth: 9
                z: 10

                Label {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: control.badgeText
                    color: Theme.accentInk
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightBold
                }
            }
        }

        Label {
            visible: control.showLabel
            Layout.fillWidth: true
            text: control.text
            color: !control.enabled ? Theme.inkDisabled
                   : control.selected ? Theme.ink
                   : Theme.inkMuted
            font.family: Theme.fontFamily
            font.pixelSize: Math.max(11, Theme.typeCaptionSize - 1)
            font.weight: control.selected ? Theme.weightDemiBold : Theme.weightMedium
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

    }

    background: Item { }
}
