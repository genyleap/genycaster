import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Shared text-only layout selector for Explorer and Profile. Selection and
// hover change ink, never paint an opaque segmented backing over the feed.
Item {
    id: root
    property string layoutMode: "magazine"
    property bool condensed: false
    property bool filterAvailable: false
    property string filterLabel: qsTrId("genycaster.filters.explorer")
    signal layoutRequested(string mode)
    signal filterRequested()

    implicitWidth: choices.implicitWidth
    implicitHeight: Math.max(choices.implicitHeight, Theme.compactControlHeight)

    RowLayout {
        id: choices
        anchors.fill: parent
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Item {
            id: segments
            objectName: "magazineViewSegments"
            // Both options use the same width so translated strings remain
            // centered; no background rectangle is part of the selector.
            readonly property real segmentWidth: Math.max(
                magazineChoice.implicitWidth, timelineChoice.implicitWidth)
            Layout.preferredWidth: segmentWidth * 2 + Theme.space2
            Layout.preferredHeight: Theme.compactControlHeight + 4
            implicitWidth: segmentWidth * 2 + Theme.space2
            implicitHeight: Theme.compactControlHeight + 4

            RowLayout {
                id: segmentChoices
                anchors.fill: parent
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                GenyTabButton {
                    id: magazineChoice
                    objectName: "explorerMagazineChoice"
                    Layout.preferredWidth: segments.segmentWidth
                    Layout.preferredHeight: Theme.compactControlHeight
                    Layout.alignment: Qt.AlignVCenter
                    minimumWidth: root.condensed ? 40 : 72
                    iconOnly: root.condensed
                    dense: root.condensed
                    segmented: false
                    showSelectionIndicator: false
                    selected: root.layoutMode !== "timeline"
                    iconGlyph: Theme.iconApps
                    text: qsTrId("genycaster.explorer.magazine")
                    onClicked: root.layoutRequested("magazine")
                }

                GenyTabButton {
                    id: timelineChoice
                    objectName: "explorerTimelineChoice"
                    Layout.preferredWidth: segments.segmentWidth
                    Layout.preferredHeight: Theme.compactControlHeight
                    Layout.alignment: Qt.AlignVCenter
                    minimumWidth: root.condensed ? 40 : 72
                    iconOnly: root.condensed
                    dense: root.condensed
                    segmented: false
                    showSelectionIndicator: false
                    selected: root.layoutMode === "timeline"
                    iconGlyph: Theme.iconList
                    text: qsTrId("genycaster.explorer.timeline")
                    onClicked: root.layoutRequested("timeline")
                }
            }
        }

        ActionButton {
            objectName: "magazineToolbarFilter"
            visible: root.filterAvailable
            Layout.preferredWidth: visible ? implicitWidth : 0
            Layout.preferredHeight: Theme.compactControlHeight
            compact: true
            quiet: false
            elevated: false
            pill: true
            iconOnly: root.condensed
            iconGlyph: "filter"
            text: root.filterLabel
            onClicked: root.filterRequested()
        }
    }
}
