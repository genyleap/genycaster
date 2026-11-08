import QtQuick
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root
    property string caption: ""
    property string value: "—"
    property string description: ""
    property string iconName: ""
    Layout.fillWidth: true
    Layout.preferredWidth: 0
    Layout.preferredHeight: 114
    fillColor: Theme.surface
    borderColor: Theme.borderMuted
    borderWidth: 1
    curveDepth: Theme.curveContainer

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.caption
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }
            Icon {
                text: root.iconName
                color: Theme.textSecondary
                preferredSize: Theme.iconSizeLarge
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.value
            color: Theme.textPrimary
            font.pixelSize: Theme.typeTitle1Size
            font.weight: Theme.weightBold
            elide: Text.ElideRight
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.description
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            elide: Text.ElideRight
        }
    }
}
