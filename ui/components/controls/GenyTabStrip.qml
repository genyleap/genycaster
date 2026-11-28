import QtQuick
import QtQuick.Controls
import Farcaster.Client

Item {
    id: root

    property var tabs: []
    property string currentId: ""
    property bool distribute: false
    property bool segmented: true
    property bool filledTabs: false
    property real minimumTabWidth: 72
    property real spacing: Theme.space1
    property bool showSelectionIndicator: true

    signal activated(string tabId)

    implicitHeight: Theme.controlHeight

    function indexForId(tabId) {
        for (let index = 0; index < tabs.length; ++index) {
            if (String(tabs[index].id) === String(tabId))
                return index
        }
        return -1
    }

    Superellipse {
        visible: root.filledTabs
        anchors.fill: parent
        fillColor: Theme.surfaceMuted
        borderWidth: 1
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveControl
    }

    ListView {
        id: tabList
        anchors.fill: parent
        anchors.margins: root.segmented ? 3 : 0
        orientation: ListView.Horizontal
        layoutDirection: Theme.uiDirection
        spacing: root.segmented ? 2 : root.spacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        model: root.tabs
        currentIndex: root.indexForId(root.currentId)

        function revealCurrentTab() {
            if (tabList.visible && !(typeof renderingController !== "undefined"
                    && renderingController && renderingController.shuttingDown === true))
                revealTimer.restart()
        }
        Timer {
            id: revealTimer
            interval: 0
            repeat: false
            onTriggered: {
                if (typeof renderingController !== "undefined" && renderingController
                        && renderingController.shuttingDown === true)
                    return
                if (tabList.visible && tabList.currentIndex >= 0
                        && tabList.currentIndex < tabList.count)
                    tabList.positionViewAtIndex(tabList.currentIndex, ListView.Contain)
            }
        }
        onVisibleChanged: {
            if (visible) revealCurrentTab()
            else revealTimer.stop()
        }

        onCurrentIndexChanged: revealCurrentTab()
        onCountChanged: revealCurrentTab()
        onWidthChanged: revealCurrentTab()
        onLayoutDirectionChanged: revealCurrentTab()
        Component.onCompleted: revealCurrentTab()

        delegate: GenyTabButton {
            required property int index
            required property var modelData

            width: root.distribute
                   ? Math.max(root.minimumTabWidth,
                              (tabList.width - Math.max(0, tabList.count - 1) * tabList.spacing)
                              / Math.max(1, tabList.count))
                   : implicitWidth
            height: tabList.height
            minimumWidth: root.minimumTabWidth
            segmented: root.segmented
            filledTabs: root.filledTabs
            showSelectionIndicator: root.showSelectionIndicator
            dense: root.distribute
                   && tabList.width / Math.max(1, tabList.count) < 84
            text: String(modelData.label || "")
            iconGlyph: String(modelData.icon || "")
            selected: String(modelData.id || "") === root.currentId
            onClicked: root.activated(String(modelData.id || ""))
        }

        ScrollBar.horizontal: GenyScrollBar {
            policy: tabList.contentWidth > tabList.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }
    }
}
