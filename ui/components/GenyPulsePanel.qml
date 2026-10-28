import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Compact, source-scoped activity. Requests and normalization live in the
// native GenyPulse adapter. Unknown days are never claimed inactive.
Item {
    id: root
    objectName: "genyPulsePanel"
    property double profileFid: 0
    property bool compact: false
    property bool isOwnProfile: false
    property string verifiedGitHub: ""
    property string verifiedWallet: ""
    property bool expanded: false
    property string selectedDate: ""
    signal connectionsRequested()

    readonly property var farcasterData: genyPulseActivity.farcaster
    readonly property var githubData: genyPulseActivity.github
    readonly property var walletData: genyPulseActivity.onchain
    readonly property int weeks: compact ? 13 : 26
    readonly property int cellSize: compact ? 11 : 10
    readonly property int cellGap: 3
    readonly property int todayDay: Math.floor(Date.now() / 86400000)
    readonly property int firstDay: todayDay - weeks * 7 + 1
    readonly property bool farcasterReady: (farcasterData.status === "ok" || farcasterData.status === "partial")
                                           && String(farcasterData.fid || "") === String(Math.floor(profileFid))
    readonly property bool githubReady: (githubData.status === "ok" || githubData.status === "partial")
                                        && verifiedGitHub.length>0
                                        && String(githubData.username || "").toLowerCase() === verifiedGitHub.toLowerCase()
    readonly property var farcasterDays: dayMap(farcasterData.days)
    readonly property var githubDays: dayMap(githubData.days)

    function dayMap(items) {
        const values = ({})
        if (!items || typeof items.length !== "number") return values
        for (let i=0; i<items.length && i<366; ++i) {
            const item=items[i] || ({})
            const date=String(item.date || "")
            const count=Number(item.count || 0)
            if (/^\d{4}-\d{2}-\d{2}$/.test(date) && Number.isFinite(count) && count>0)
                values[date]=count
        }
        return values
    }
    function dayString(day) {
        return new Date(day*86400000).toISOString().slice(0,10)
    }
    function countAt(days, day) {
        return Number(days[root.dayString(day)] || 0)
    }
    function tileColor(count, source) {
        if (count<1) return Theme.dark ? "#282A32" : "#E6E8ED"
        const palette=source==="github"
            ? ["#9CDEC1","#4CBF91","#21805D"]
            : ["#C5B5FA","#997CE5","#6650B4"]
        return palette[count>=5?2:count>=3?1:0]
    }
    function sourceSubtitle(source) {
        const state=String(source.status || "")
        if(state==="loading")return qsTrId("genycaster.activity.loading")
        if(state==="error")return qsTrId("genycaster.activity.fetch_error")
        if(state==="partial")return qsTrId("genycaster.activity.partial")
        if(state==="ok")return qsTrId("genycaster.activity.recent")
        return qsTrId("genycaster.activity.unavailable")
    }

    function refreshSources() {
        genyPulseActivity.requestFarcaster(Math.max(0,Math.floor(root.profileFid)))
        genyPulseActivity.requestGitHub(root.isOwnProfile ? root.verifiedGitHub : "")
        genyPulseActivity.setVerifiedWallet(root.verifiedWallet)
    }
    onProfileFidChanged: refreshTimer.restart()
    onVerifiedGitHubChanged: refreshTimer.restart()
    onVerifiedWalletChanged: refreshTimer.restart()
    onIsOwnProfileChanged: refreshTimer.restart()
    Component.onCompleted: refreshTimer.start()
    Timer {
        id: refreshTimer
        interval: 80
        repeat: false
        onTriggered: root.refreshSources()
    }

    implicitHeight: content.implicitHeight
    width: parent ? parent.width : 320

    ColumnLayout {
        id: content
        width: parent.width
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space1
            Label {
                Layout.fillWidth: true
                text: qsTrId("genycaster.activity.title")
                font.pixelSize: Theme.typeBodySize
                font.weight: Theme.weightDemiBold
                color: Theme.ink
            }
            Button {
                objectName: "genyPulseExpandButton"
                flat: true
                padding: Theme.space1
                text: root.expanded ? qsTrId("genycaster.activity.less") :
                                      qsTrId("genycaster.activity.explore")
                font.pixelSize: Theme.typeCaptionSize
                onClicked: root.expanded = !root.expanded
                Accessible.name: text
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: farcasterPanel.implicitHeight + Theme.space2*2
            radius: Theme.space3
            color: Theme.surfaceMuted
            border.width: 1
            border.color: Theme.borderMuted

            ColumnLayout {
                id: farcasterPanel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space2
                spacing: Theme.space1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Rectangle {
                        Layout.preferredWidth: 25
                        Layout.preferredHeight: 25
                        radius: 8
                        color: Theme.dark ? "#30283E" : "#EEE9FA"
                        Label {
                            anchors.centerIn: parent
                            text: "◫"
                            font.pixelSize: 17
                            color: Theme.dark ? "#B5A0F2" : "#7560BF"
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: "Farcaster"
                            color: Theme.ink
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.farcasterReady
                                ? qsTrId("genycaster.activity.indexed_count")
                                    .arg(root.farcasterData.count || 0)
                                    .arg(root.farcasterData.replies || 0)
                                : root.sourceSubtitle(root.farcasterData)
                            font.pixelSize: Theme.typeCaptionSize
                            color: Theme.inkMuted
                            elide: Text.ElideRight
                        }
                    }
                    Label {
                        text: root.weeks + "w"
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                Row {
                    Layout.fillWidth: true
                    spacing: root.cellGap
                    Layout.topMargin: Theme.space1
                    Repeater {
                        model: root.weeks
                        delegate: Column {
                            required property int index
                            spacing: root.cellGap
                            Repeater {
                                model: 7
                                delegate: Rectangle {
                                    required property int index
                                    readonly property int day: root.firstDay + parent.index*7 + index
                                    readonly property int value: root.countAt(root.farcasterDays,day)
                                    width: root.cellSize
                                    height: root.cellSize
                                    radius: 3
                                    color: root.tileColor(root.farcasterReady ? value : 0,"farcaster")
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: root.farcasterReady && parent.value>0
                                        onClicked: root.selectedDate=root.dayString(parent.day)
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    Label {
                        Layout.fillWidth: true
                        text: root.selectedDate.length>0 && root.farcasterReady
                              ? qsTrId("genycaster.activity.day_summary")
                                    .arg(root.selectedDate)
                                    .arg(root.farcasterDays[root.selectedDate] || 0)
                              : root.sourceSubtitle(root.farcasterData)
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                    Label {
                        text: root.farcasterData.status==="partial"
                              ? qsTrId("genycaster.activity.limited")
                              : "Hypersnap"
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.expanded
            spacing: Theme.space2

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: githubPanel.implicitHeight + Theme.space2*2
                radius: Theme.space3
                color: Theme.surfaceMuted
                border.width: 1
                border.color: Theme.borderMuted

                ColumnLayout {
                    id: githubPanel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space2
                    spacing: Theme.space1
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        Rectangle {
                            Layout.preferredWidth: 25; Layout.preferredHeight: 25
                            radius: 8; color: Theme.surface
                            Label {anchors.centerIn: parent; text: "⌘"; color: "#46B884"; font.pixelSize: 17}
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 0
                            Label {
                                text: "GitHub"
                                color: Theme.ink
                                font.pixelSize: Theme.typeBodySize
                                font.weight: Theme.weightDemiBold
                            }
                            Label {
                                Layout.fillWidth: true
                                text: root.verifiedGitHub.length>0
                                      ? "@" + root.verifiedGitHub + " · " +
                                        root.sourceSubtitle(root.githubData)
                                      : qsTrId("genycaster.activity.not_linked")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }
                        Label {
                            text: root.githubReady
                                  ? qsTrId("genycaster.activity.events")
                                        .arg(root.githubData.count || 0)
                                  : ""
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                    Row {
                        visible: root.githubReady
                        Layout.fillWidth: true
                        spacing: root.cellGap
                        Repeater {
                            model: root.compact ? 13 : 26
                            delegate: Column {
                                required property int index
                                spacing: root.cellGap
                                Repeater {
                                    model: 7
                                    delegate: Rectangle {
                                        required property int index
                                        readonly property int day: root.firstDay + parent.index*7 + index
                                        width: root.cellSize; height: root.cellSize; radius: 3
                                        color: root.tileColor(root.countAt(root.githubDays,day),"github")
                                    }
                                }
                            }
                        }
                    }
                    Label {
                        visible: root.githubReady
                        text: qsTrId("genycaster.activity.github_window")
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.inkMuted
                    }
                    Button {
                        visible: root.isOwnProfile && root.verifiedGitHub.length===0
                        flat: true; padding: 0
                        text: qsTrId("genycaster.activity.manage")
                        font.pixelSize: Theme.typeCaptionSize
                        onClicked: root.connectionsRequested()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: walletPanel.implicitHeight + Theme.space2*2
                radius: Theme.space3
                color: Theme.surfaceMuted
                border.width: 1
                border.color: Theme.borderMuted
                RowLayout {
                    id: walletPanel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space2
                    spacing: Theme.space2
                    Rectangle {
                        Layout.preferredWidth: 25; Layout.preferredHeight: 25
                        radius: 8; color: Theme.surface
                        Label {anchors.centerIn: parent; text: "⬡"; font.pixelSize: 18; color: "#49A7D7"}
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0
                        Label {
                            text: "Onchain"
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            color: Theme.ink
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.walletData.status==="not-indexed"
                                  ? qsTrId("genycaster.activity.wallet_pending")
                                  : qsTrId("genycaster.activity.wallet_missing")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
