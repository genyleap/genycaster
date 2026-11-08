import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root

    property string presentationMode: "for-you"
    property string visualFilter: "all"
    property bool compact: false

    signal modeRequested(string mode)
    signal visualFilterRequested(string filter)
    signal searchRequested()
    signal channelsRequested()

    implicitHeight: column.implicitHeight + Theme.space4

    // Keep the Explorer header aligned with the Stats/Profile card edges
    // on desktop. Preserve the established touch-safe phone gutter.
    readonly property int gutter: root.compact ? Theme.gutter(root.width) : Theme.space3

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.gutter
        anchors.rightMargin: root.gutter
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feed_header.explore")
                    color: Theme.textPrimary
                    font.pixelSize: root.compact ? Theme.typeTitle3Size : Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    elide: Text.ElideRight
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feed_header.discover_people_channels_and_casts")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeMetaSize
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                }
            }

            ActionButton {
                compact: true
                quiet: true
                iconOnly: root.compact
                iconGlyph: Theme.iconSearch
                text: qsTrId("genycaster.feed_header.universal_search")
                onClicked: root.searchRequested()
            }
        }

        GenyTabStrip {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight
            currentId: root.presentationMode
            segmented: false
            distribute: true
            showSelectionIndicator: false
            minimumTabWidth: 64
            spacing: 0
            tabs: root.compact ? [
                { id: "for-you", label: qsTrId("genycaster.feed_header.explore") },
                { id: "visuals", label: qsTrId("genycaster.home.visual_filter_nfts") },
                { id: "conversations", label: qsTrId("genycaster.home.conversations") },
                { id: "devs", label: qsTrId("genycaster.home.devs") }
            ] : [
                { id: "for-you", label: qsTrId("genycaster.feed_header.explore") },
                { id: "visuals", label: qsTrId("genycaster.home.visual_filter_nfts") },
                { id: "conversations", label: qsTrId("genycaster.home.conversations") },
                { id: "devs", label: qsTrId("genycaster.home.devs") },
                { id: "channels", label: qsTrId("genycaster.side_nav.channels") }
            ]
            onActivated: tabId => {
                if (tabId === "channels") root.channelsRequested()
                else root.modeRequested(tabId)
            }
        }

    }
}
