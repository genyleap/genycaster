import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Height derives from actual localized text, rather than forcing three rows
// into a fixed 120px card at every font scale.
Superellipse {
    id: root
    property string caption: ""
    property string value: "—"
    property string description: ""
    property string iconName: ""

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.preferredWidth: 0
    Layout.preferredHeight: Math.max(146, cardContent.implicitHeight + Theme.space3 * 2)
    fillColor: Theme.surface
    borderColor: Theme.borderMuted
    borderWidth: 1
    curveDepth: Theme.curveContainer

    ColumnLayout {
        id: cardContent
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                fillColor: Theme.surfaceMuted
                curveDepth: 9
                Icon {
                    anchors.centerIn: parent
                    text: root.iconName
                    color: Theme.accent
                    preferredSize: Theme.iconSize
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.caption
                alignToUi: true
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.value
            alignToUi: true
            font.pixelSize: root.width < 240 ? Math.max(Theme.typeMetaSize + 5, Theme.typeTitle1Size - 5)
                                             : Theme.typeTitle1Size
            font.weight: Theme.weightBold
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            Accessible.name: root.caption + ": " + root.value
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.description
            alignToUi: true
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
    }
}
