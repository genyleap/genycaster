import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root
    property string title: ""
    property string description: ""
    property string iconGlyph: Theme.iconLeaf

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.mobileGutter * 2, 620)
        spacing: Theme.space3

        Superellipse {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 58
            Layout.preferredHeight: 58
            fillColor: Theme.primarySurface
            curveDepth: Theme.curveLarge
            Icon { anchors.centerIn: parent; text: root.iconGlyph; color: Theme.accent; preferredSize: Theme.typeTitle1Size }
        }
        Label {
            Layout.fillWidth: true
            text: root.title
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeTitle1Size
            font.weight: Theme.weightBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Label {
            Layout.fillWidth: true
            text: root.description
            color: Theme.inkMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeBodySize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Superellipse {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space3
            implicitHeight: 84
            fillColor: Theme.surfaceMuted
            borderWidth: 1
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveLarge
            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space4
                layoutDirection: Theme.uiDirection
                spacing: Theme.space3
                Repeater {
                    model: 3
                    Superellipse {
                        required property int index
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        fillColor: Theme.surfaceRaised
                        curveDepth: Theme.curveContainer
                        opacity: 0.72 - index * 0.10
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTrId("genycaster.shell.coming_soon")
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.typeMetaSize
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
