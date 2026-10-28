import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Derived, read-only activity preview. The profile feed is paginated:
// missing days are unknown, not certified inactive days or a real streak.
Item {
    id: root
    objectName: "genyPulsePanel"
    property var rows: []
    property double profileFid: 0
    property string profileFeed: "user-posts"
    property bool compact: false
    property bool isOwnProfile: false
    property bool connectionsVerified: false
    property var connections: []
    property bool offline: false
    property bool expanded: false
    property string selectedDate: ""
    signal connectionsRequested()

    readonly property int weekCount: compact ? 13 : 26
    readonly property int todayUtc: Math.floor(Date.now() / 86400000)
    readonly property int startUtc: todayUtc - weekCount * 7 + 1
    readonly property bool feedSupported: profileFeed === "user-posts"
                                          || profileFeed === "user-replies"
    readonly property var buckets: aggregateCasts()
    readonly property int sampleCount: Number(buckets.total || 0)
    readonly property int sampledDays: Number(buckets.days || 0)
    readonly property var sources: [
        {id:"farcaster", title:"Farcaster", detail:qsTrId("genycaster.activity.farcaster_detail"), accent:"#A28BFA"},
        {id:"github", title:"GitHub", detail:qsTrId("genycaster.activity.github_detail"), accent:"#55C783"},
        {id:"onchain", title:"Onchain", detail:qsTrId("genycaster.activity.onchain_detail"), accent:"#6ABFE2"}
    ]

    function aggregateCasts() {
        const counts = ({})
        const seen = ({})
        let total = 0
        const feed = root.profileFeed
        if (!root.feedSupported || !(root.profileFid > 0))
            return {counts: counts, total: 0, days: 0}
        const source = root.rows || []
        for (let i = 0; i < source.length && i < 10000; ++i) {
            const row = source[i] || ({})
            if (Number(row.authorFid || 0) !== root.profileFid) continue
            const time = Number(row.createdAtMs || 0)
            if (!isFinite(time) || time < 1000000000000) continue
            const day = Math.floor(time / 86400000)
            if (day < root.startUtc || day > root.todayUtc) continue
            const castId = String(row.castId || row.hash || row.id || "")
            if (!castId || seen[castId]) continue
            seen[castId] = true
            counts[day] = (counts[day] || 0) + 1
            ++total
        }
        return {counts: counts, total: total, days: Object.keys(counts).length}
    }

    function connectionState(provider) {
        if (!root.isOwnProfile || !root.connectionsVerified) return "unavailable"
        const source = root.connections || []
        for (let i = 0; i < source.length; ++i)
            if (String((source[i] || {}).provider || "") === provider)
                return "linked"
        return "not-linked"
    }

    function bucketCount(day) { return Number(root.buckets.counts[day] || 0) }

    function dayLabel(day) {
        return new Date(day * 86400000).toISOString().slice(0, 10)
    }

    function cellColor(source, count) {
        if (source === "farcaster" && count > 0)
            return count >= 5 ? "#A28BFA" : count >= 3 ? "#7961C5" : "#514478"
        return Theme.dark ? "#292B33" : "#E7E9ED"
    }

    implicitHeight: content.implicitHeight
    width: parent ? parent.width : 320

    ColumnLayout {
        id: content
        width: parent.width
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            Label {
                Layout.fillWidth: true
                text: qsTrId("genycaster.activity.title")
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightDemiBold
                color: Theme.ink
            }
            Button {
                objectName: "genyPulseExpandButton"
                flat: true
                padding: Theme.space1
                text: root.expanded ? qsTrId("genycaster.activity.less") : qsTrId("genycaster.activity.explore")
                onClicked: root.expanded = !root.expanded
                Accessible.name: text
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: summary.implicitHeight + Theme.space3 * 2
            radius: Theme.space3
            color: Theme.surfaceMuted
            border.width: 1
            border.color: Theme.borderMuted

            ColumnLayout {
                id: summary
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space3
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Rectangle {
                        width: 29; height: 29; radius: 9
                        color: Theme.dark ? "#312C43" : "#F0ECFF"
                        Label {
                            anchors.centerIn: parent
                            text: "◫"
                            color: Theme.dark ? "#A28BFA" : "#6651B5"
                            font.pixelSize: 19
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label {
                            text: qsTrId("genycaster.activity.farcaster_title")
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            color: Theme.ink
                        }
                        Label {
                            text: root.feedSupported
                                  ? qsTrId("genycaster.activity.sample_summary").arg(root.sampleCount).arg(root.sampledDays)
                                  : qsTrId("genycaster.activity.open_posts")
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.inkMuted
                        }
                    }
                    Label {
                        text: qsTrId("genycaster.activity.preview")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }

                Row {
                    Layout.fillWidth: true
                    spacing: root.compact ? 3 : 4
                    Repeater {
                        model: root.weekCount
                        delegate: Column {
                            required property int index
                            spacing: root.compact ? 3 : 4
                            Repeater {
                                model: 7
                                delegate: Rectangle {
                                    required property int index
                                    readonly property int day: root.startUtc + parent.index * 7 + index
                                    readonly property int count: root.bucketCount(day)
                                    width: root.compact ? Math.max(5, Math.floor((summary.width - (root.weekCount - 1) * 3) / root.weekCount))
                                                        : Math.max(7, Math.floor((summary.width - (root.weekCount - 1) * 4) / root.weekCount))
                                    height: width
                                    radius: Math.max(2, Math.floor(width / 4))
                                    color: root.cellColor("farcaster", count)
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: parent.count > 0
                                        onClicked: root.selectedDate = root.dayLabel(parent.day)
                                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    }
                                }
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.selectedDate.length > 0
                    text: qsTrId("genycaster.activity.day_summary").arg(root.selectedDate).arg(root.bucketCount(Math.floor(Date.parse(root.selectedDate + "T00:00:00Z") / 86400000)))
                    color: Theme.ink
                    font.pixelSize: Theme.typeCaptionSize
                }
                Label {
                    Layout.fillWidth: true
                    text: root.offline ? qsTrId("genycaster.activity.offline")
                                       : qsTrId("genycaster.activity.disclaimer")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            visible: root.expanded
            Layout.fillWidth: true
            spacing: Theme.space2

            Repeater {
                model: 2
                delegate: Rectangle {
                    required property int index
                    readonly property var info: root.sources[index + 1]
                    readonly property string provider: index === 0 ? "github" : "ethereum-wallet"
                    readonly property string state: root.connectionState(provider)
                    Layout.fillWidth: true
                    implicitHeight: statusColumn.implicitHeight + Theme.space3 * 2
                    radius: Theme.space3
                    color: Theme.surfaceMuted
                    border.width: 1
                    border.color: Theme.borderMuted

                    ColumnLayout {
                        id: statusColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.space3
                        spacing: Theme.space2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            Rectangle {
                                width: 29; height: 29; radius: 9
                                color: Theme.surface
                                Label {
                                    anchors.centerIn: parent
                                    text: index === 0 ? "⌘" : "⬡"
                                    color: info.accent
                                    font.pixelSize: 20
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Label {
                                    text: info.title
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightDemiBold
                                }
                                Label {
                                    text: info.detail
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }
                            Label {
                                text: state === "linked" ? qsTrId("genycaster.activity.linked") :
                                      state === "not-linked" ? qsTrId("genycaster.activity.not_linked") : qsTrId("genycaster.activity.unavailable")
                                color: state === "linked" ? info.accent : Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: state === "linked"
                                  ? qsTrId("genycaster.activity.index_pending")
                                  : state === "not-linked"
                                    ? qsTrId("genycaster.activity.connect_hint")
                                    : qsTrId("genycaster.activity.no_history")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                        Button {
                            visible: root.isOwnProfile && state === "not-linked"
                            flat: true
                            text: qsTrId("genycaster.activity.manage")
                            onClicked: root.connectionsRequested()
                        }
                    }
                }
            }
        }
    }
}
