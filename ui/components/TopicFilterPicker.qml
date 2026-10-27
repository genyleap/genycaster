import QtQuick
import QtQuick.Layouts

// GenyCaster Topics are independent of Farcaster channels and personal bookmark collections.
Item {
    id: root
    property var catalog: []
    property bool compact: false
    property string selectedTopic: ""
    signal topicRequested(string slug)
    implicitWidth: root.compact ? 172 : 208
    implicitHeight: Theme.controlHeight

    function titleFor(slug) {
        switch (slug) {
        case "art": return qsTrId("genycaster.topic.art")
        case "development": return qsTrId("genycaster.topic.development")
        case "photography": return qsTrId("genycaster.topic.photography")
        case "design": return qsTrId("genycaster.topic.design")
        case "technology": return qsTrId("genycaster.topic.technology")
        case "music": return qsTrId("genycaster.topic.music")
        case "writing": return qsTrId("genycaster.topic.writing")
        case "science": return qsTrId("genycaster.topic.science")
        case "business": return qsTrId("genycaster.topic.business")
        case "gaming": return qsTrId("genycaster.topic.gaming")
        default: return slug
        }
    }

    readonly property var options: {
        const out = [{slug: "", label: "All topics"}]
        for (const value of root.catalog || []) {
            const slug = String(value.slug || "")
            if (/^[a-z][a-z0-9-]{1,63}$/.test(slug))
                out.push({slug: slug, label: root.titleFor(slug)})
        }
        return out
    }

    GenyComboBox {
        objectName: "topicFilterPicker"
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height
        model: root.options.map(entry => entry.label)
        currentIndex: Math.max(0, root.options.findIndex(entry=>entry.slug===root.selectedTopic))
        onActivated: index => {
            const entry = root.options[index]
            if (entry) root.topicRequested(entry.slug)
        }
        Accessible.name: "Filter casts by topic"
    }
}
