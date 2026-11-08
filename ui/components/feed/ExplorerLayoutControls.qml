import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// One compact display-and-filter toolbar for Explorer and Profile. The
// segmented surface keeps the selection stable instead of painting a row of
// unrelated buttons. All labels remain translated and support RTL.
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

        Superellipse {
            id: segments
            objectName: "magazineViewSegments"
            // Equal segment widths prevent the wider translated option
            // from shifting the icon/text center of its neighbor.
            readonly property real segmentWidth: Math.max(
                magazineChoice.implicitWidth, timelineChoice.implicitWidth)
            Layout.preferredWidth: segmentWidth * 2 + 12
            Layout.preferredHeight: Theme.compactControlHeight + 4
            implicitWidth: segmentWidth * 2 + 12
            implicitHeight: Theme.compactControlHeight + 4
            curveDepth: Theme.curveControl
            fillColor: Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderSubtle

            RowLayout {
                id: segmentChoices
                anchors.fill: parent
                anchors.margins: 4
                spacing: 2
                layoutDirection: Theme.uiDirection

                ActionButton {
                    id: magazineChoice
                    objectName: "explorerMagazineChoice"
                    Layout.preferredWidth: segments.segmentWidth
                    Layout.preferredHeight: Theme.compactControlHeight - 4
                    Layout.alignment: Qt.AlignVCenter
                    compact: true
                    quiet: true
                    elevated: false
                    iconSize: Theme.iconSizeSmall
                    balancedIcon: !root.condensed
                    iconOnly: root.condensed
                    selected: root.layoutMode !== "timeline"
                    customSelectedSurface: Theme.surfaceRaised
                    customSelectedColor: Theme.ink
                    iconGlyph: Theme.iconApps
                    text: qsTrId("genycaster.explorer.magazine")
                    onClicked: root.layoutRequested("magazine")
                }

                ActionButton {
                    id: timelineChoice
                    objectName: "explorerTimelineChoice"
                    Layout.preferredWidth: segments.segmentWidth
                    Layout.preferredHeight: Theme.compactControlHeight - 4
                    Layout.alignment: Qt.AlignVCenter
                    compact: true
                    quiet: true
                    elevated: false
                    iconSize: Theme.iconSizeSmall
                    balancedIcon: !root.condensed
                    iconOnly: root.condensed
                    selected: root.layoutMode === "timeline"
                    customSelectedSurface: Theme.surfaceRaised
                    customSelectedColor: Theme.ink
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
