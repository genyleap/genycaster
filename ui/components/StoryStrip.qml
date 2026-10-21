import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var profiles: []
    property bool compact: false
    // Explicit signed-in viewer photo; do not derive it from public feed rows.
    property string ownAvatarUrl: ""

    signal profileRequested(var profile)
    signal ownStoryRequested()

    visible: root.profiles && root.profiles.length > 0
    implicitHeight: visible ? (root.compact ? 94 : 98) : 0

    ListView {
        id: storyList
        objectName: "recentPosterList"
        anchors.fill: parent
        orientation: ListView.Horizontal
        layoutDirection: Theme.uiDirection
        spacing: Theme.space2
        clip: true
        boundsBehavior: Flickable.DragOverBounds
        interactive: true
        model: root.profiles
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }

        delegate: StoryItem {
            required property int index
            required property var modelData
            addStory: Boolean(modelData.isSelf)
            compact: root.compact
            displayName: String(modelData.displayName || modelData.username || "")
            username: String(modelData.username || "")
            avatarUrl: modelData.isSelf && root.ownAvatarUrl.length > 0
                       ? root.ownAvatarUrl : String(modelData.avatarUrl || "")
            // Solid accent rings indicate fresh activity within 24 hours.
            // Older verified activity remains tappable but visually recedes.
            // Verified casts within 24h receive a colored ring; older or
            // unknown activity remains visible but subdued.
            storyState: !modelData.isSelf
                        && Number(modelData.createdAtMs || 0) > 0
                        && Number(modelData.createdAtMs) >= Date.now() - 86400000
                        ? "active" : "viewed"
            activityKind: String(modelData.activityKind || "cast")
            onActivated: {
                if (modelData.isSelf) root.ownStoryRequested()
                else root.profileRequested(modelData)
            }
        }
    }
}
