import QtQuick
import QtQuick.Controls
import Farcaster.Client

Item {
    id: root
    property string layoutMode: "magazine"
    property int cardWidth: 500
    signal layoutRequested(string mode)
    signal cardWidthRequested(int width)

    implicitHeight: choices.implicitHeight + Theme.space2 * 2

    Flow {
        id: choices
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space2
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        DirectionAwareLabel {
            height: Theme.compactControlHeight
            text: qsTrId("genycaster.explorer.layout")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
            verticalAlignment: Text.AlignVCenter
        }

        ActionButton {
            objectName: "explorerMagazineChoice"
            compact: true
            quiet: true
            selected: root.layoutMode !== "timeline"
            iconGlyph: Theme.iconApps
            text: qsTrId("genycaster.explorer.magazine")
            onClicked: root.layoutRequested("magazine")
        }

        ActionButton {
            objectName: "explorerTimelineChoice"
            compact: true
            quiet: true
            selected: root.layoutMode === "timeline"
            iconGlyph: Theme.iconList
            text: qsTrId("genycaster.explorer.timeline")
            onClicked: root.layoutRequested("timeline")
        }

        GenyComboBox {
            objectName: "explorerCardSizeChoice"
            width: Math.min(choices.width, Math.max(160, naturalWidth))
            implicitHeight: Theme.compactControlHeight
            enabled: root.layoutMode !== "timeline"
            model: [qsTrId("genycaster.explorer.small_cards"),
                    qsTrId("genycaster.explorer.medium_cards"),
                    qsTrId("genycaster.explorer.large_cards")]
            currentIndex: root.cardWidth === 420 ? 0 : root.cardWidth === 640 ? 2 : 1
            onActivated: index => root.cardWidthRequested(index === 0 ? 420 : index === 2 ? 640 : 500)
            Accessible.name: qsTrId("genycaster.explorer.layout") + " · " + currentText
        }
    }
}
