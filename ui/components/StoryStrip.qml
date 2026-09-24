import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var profiles: []
    property bool compact: false

    signal profileRequested(var profile)

    implicitHeight: root.compact ? 80 : 86
    visible: root.profiles && root.profiles.length > 0

    ListView {
        id: storyList
        objectName: "recentPosterList"
        anchors.fill: parent
        orientation: ListView.Horizontal
        layoutDirection: Theme.uiDirection
        spacing: Theme.space1
        clip: true
        boundsBehavior: Flickable.DragOverBounds
        interactive: true
        model: root.profiles
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }

        delegate: StoryItem {
            required property int index
            required property var modelData
            addStory: false
            compact: root.compact
            displayName: String(modelData.displayName || modelData.username || "")
            username: String(modelData.username || "")
            avatarUrl: String(modelData.avatarUrl || "")
            storyState: "active"
            activityKind: String(modelData.activityKind || "cast")
            onActivated: root.profileRequested(modelData)
        }
    }
}
