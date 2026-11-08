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
            Layout.preferredWidth: segmentChoices.implicitWidth + 8
            Layout.preferredHeight: Theme.compactControlHeight + 4
            implicitWidth: segmentChoices.implicitWidth + 8
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
                    objectName: "explorerMagazineChoice"
                    compact: true
                    quiet: true
                    elevated: false
                    iconOnly: root.condensed
                    selected: root.layoutMode !== "timeline"
                    customSelectedSurface: Theme.surfaceRaised
                    customSelectedColor: Theme.ink
                    iconGlyph: Theme.iconApps
                    text: qsTrId("genycaster.explorer.magazine")
                    onClicked: root.layoutRequested("magazine")
                }

                ActionButton {
                    objectName: "explorerTimelineChoice"
                    compact: true
                    quiet: true
                    elevated: false
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
