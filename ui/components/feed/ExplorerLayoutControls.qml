import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root
    property string layoutMode: "magazine"
    property bool condensed: false
    signal layoutRequested(string mode)

    // Inline next to Everyone / Following, not a separate filter toolbar.
    implicitWidth: choices.implicitWidth
    implicitHeight: choices.implicitHeight

    RowLayout {
        id: choices
        anchors.fill: parent
        spacing: Theme.space1
        layoutDirection: Theme.uiDirection

        ActionButton {
            objectName: "explorerMagazineChoice"
            compact: true
            quiet: true
            iconOnly: root.condensed
            selected: root.layoutMode !== "timeline"
            iconGlyph: Theme.iconApps
            text: qsTrId("genycaster.explorer.magazine")
            onClicked: root.layoutRequested("magazine")
        }

        ActionButton {
            objectName: "explorerTimelineChoice"
            compact: true
            quiet: true
            iconOnly: root.condensed
            selected: root.layoutMode === "timeline"
            iconGlyph: Theme.iconList
            text: qsTrId("genycaster.explorer.timeline")
            onClicked: root.layoutRequested("timeline")
        }

    }
}
