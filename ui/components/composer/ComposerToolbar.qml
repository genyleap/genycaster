import QtQuick
import Farcaster.Client

GenyTabStrip {
    id: root

    property string mode: "visual"
    signal modeRequested(string mode)

    tabs: [
        {"id": "visual", "label": qsTrId("genycaster.composer.visual")},
        {"id": "markdown", "label": qsTrId("genycaster.composer.markdown")},
        {"id": "preview", "label": qsTrId("genycaster.composer.farcaster_preview")}
    ]
    currentId: root.mode
    distribute: true
    segmented: true
    filledTabs: true
    minimumTabWidth: 84
    onActivated: tabId => root.modeRequested(tabId)
}
