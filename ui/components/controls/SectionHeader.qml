import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property bool compact: false

    signal backRequested()

    implicitHeight: headerRow.implicitHeight

    RowLayout {
        id: headerRow
        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: true
        layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Theme.space2

        ActionButton {
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconBack
            text: qsTrId("genycaster.section.back_back_to_feed")
            onClicked: root.backRequested()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Label {
                Layout.fillWidth: true
                text: root.title
                color: Theme.ink
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Theme.textStartAlignment
                elide: Text.ElideRight
            }

            Label {
                visible: root.subtitle.length > 0 && !root.compact
                Layout.fillWidth: true
                text: root.subtitle
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Theme.textStartAlignment
                elide: Text.ElideRight
            }
        }
    }
}
