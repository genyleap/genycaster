import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One bounded, source-specific read model per provider. Missing heatmap cells
// are unobserved, not proof of inactivity or a valid contribution streak.
Item {
    id: root
    objectName: "genyPulsePanel"
    property double profileFid: 0
    property bool compact: false
    property bool isOwnProfile: false
    property string verifiedGitHub: ""
    property bool githubLinked: false
    property string githubAccountId: ""
    property string verifiedWallet: ""
    property bool expanded: false
    property string selectedDate: ""
    signal connectionsRequested()

    readonly property var farcasterData: genyPulseActivity.farcaster
    readonly property var githubData: genyPulseActivity.github
    readonly property var walletData: genyPulseActivity.onchain
    readonly property string providerIconRoot: "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/providers/"
    readonly property bool desktopGrid: width >= 800 && expanded
    readonly property int weeks: 13
    readonly property int gapSize: 3
    // Fit seven rows by thirteen weeks within the actual card, not viewport.
    readonly property bool showMetrics: farcasterContent.width >= 390
    readonly property int cellSize: {
        const usable = Math.max(160, farcasterContent.width - (showMetrics ? 140 : 0))
        return Math.max(8, Math.min(desktopGrid ? 27 : 23,
                                  Math.floor((usable - (weeks-1)*gapSize)/weeks)))
    }
    readonly property int githubCellSize: Math.max(10,Math.min(23,
        Math.floor((githubContent.width - 175)/5)))
    readonly property int todayDay: Math.floor(Date.now()/86400000)
    readonly property int firstDay: todayDay - weeks*7 + 1
    readonly property bool farcasterReady: (farcasterData.status==="ok" || farcasterData.status==="partial")
        && String(farcasterData.fid || "")===String(Math.floor(profileFid))
    readonly property string effectiveGithubLogin: verifiedGitHub.length>0
        ? verifiedGitHub
        : githubLinked && githubAccountId.length>0
          && String(githubData.accountId || "")===githubAccountId
            ? String(githubData.username || "") : ""
    readonly property bool githubReady: (githubData.status==="ok" || githubData.status==="partial")
        && effectiveGithubLogin.length>0
        && String(githubData.username || "").toLowerCase()===effectiveGithubLogin.toLowerCase()
    readonly property var farcasterDays: dayMap(farcasterData.days)
    readonly property var githubDays: dayMap(githubData.days)

    function dayMap(items) {
        const map=({})
        if(!items || typeof items.length!=="number")return map
        for(let i=0;i<items.length && i<366;++i) {
            const item=items[i] || ({})
            const date=String(item.date || "")
            const count=Number(item.count || 0)
            if(/^\d{4}-\d{2}-\d{2}$/.test(date) && isFinite(count) && count>0)
                map[date]=count
        }
        return map
    }
    function dayString(day) { return new Date(day*86400000).toISOString().slice(0,10) }
    function countAt(items,day) { return Number(items[dayString(day)] || 0) }
    function tileColor(count,source) {
        if(count<1)return Theme.dark ? "#282A31" : "#E6E8EC"
        const palette=source==="github"
            ? ["#B5E8D0","#68CA9B","#32966C"]
            : ["#C8B8F8","#9D81EA","#6552BD"]
        return palette[count>=5?2:count>=3?1:0]
    }
    function sourceSubtitle(source) {
        const status=String(source.status || "")
        if(status==="loading")return qsTrId("genycaster.activity.loading")
        if(status==="error")return qsTrId("genycaster.activity.fetch_error")
        if(status==="rate-limited")return qsTrId("genycaster.activity.rate_limited")
        if(status==="not-found")return qsTrId("genycaster.activity.github_not_found")
        if(status==="partial")return qsTrId("genycaster.activity.partial")
        if(status==="ok" && source.source==="github-events" && Number(source.count || 0)===0)
            return qsTrId("genycaster.activity.no_recent_github")
        if(status==="ok")return qsTrId("genycaster.activity.recent")
        return qsTrId("genycaster.activity.unavailable")
    }
    function refreshSources() {
        genyPulseActivity.requestFarcaster(Math.max(0,Math.floor(profileFid)))
        if(isOwnProfile && verifiedGitHub.length>0)
            genyPulseActivity.requestGitHub(verifiedGitHub)
        else if(isOwnProfile && githubLinked && githubAccountId.length>0)
            genyPulseActivity.requestGitHubId(githubAccountId)
        else genyPulseActivity.requestGitHub("")
        genyPulseActivity.setVerifiedWallet(verifiedWallet)
        if(!farcasterReady)selectedDate=""
    }

    onProfileFidChanged: refreshTimer.restart()
    onVerifiedGitHubChanged: refreshTimer.restart()
    onGithubAccountIdChanged: refreshTimer.restart()
    onGithubLinkedChanged: refreshTimer.restart()
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
            spacing: Theme.space2
            Label {
                Layout.fillWidth: true
                text: qsTrId("genycaster.activity.title")
                font.pixelSize: Theme.typeBodySize
                font.weight: Theme.weightDemiBold
                color: Theme.ink
            }
            ActionButton {
                objectName: "genyPulseExpandButton"
                compact: true
                quiet: false
                elevated: false
                pill: true
                iconSize: 13
                iconGlyph: root.expanded ? Theme.iconChevronUp : Theme.iconChevronDown
                text: root.expanded ? qsTrId("genycaster.activity.less")
                                    : qsTrId("genycaster.activity.explore")
                onClicked: root.expanded = !root.expanded
            }
        }

        GridLayout {
            id: activityGrid
            Layout.fillWidth: true
            columns: root.desktopGrid ? 2 : 1
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: root.desktopGrid ? Math.floor((root.width - Theme.space2)/2) : root.width
            Layout.maximumWidth: !root.expanded && root.width>=800 ? 650 : 100000
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
            implicitHeight: farcasterContent.implicitHeight + Theme.space3*2
            radius: Theme.space3
            color: Theme.surfaceMuted
            border.width: 1
            border.color: Theme.borderMuted

            ColumnLayout {
                id: farcasterContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space3
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Rectangle {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        radius: 9
                        color: Theme.dark ? "#30283E" : "#EEE9FA"
                        Image {
                            anchors.centerIn: parent
                            width: 21
                            height: 21
                            source: root.providerIconRoot + "farcaster.svg"
                            sourceSize.width: 42
                            sourceSize.height: 42
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            Accessible.ignored: true
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
                        text: "91d"
                        color: Theme.inkSubtle
                        font.pixelSize: Theme.typeCaptionSize
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4

                    Item {
                        Layout.preferredWidth: root.weeks * root.cellSize + (root.weeks-1) * root.gapSize
                        Layout.preferredHeight: 7 * root.cellSize + 6 * root.gapSize
                        Layout.alignment: Qt.AlignHCenter
                        Row {
                            id: calendarRow
                            spacing: root.gapSize
                            Repeater {
                                model: root.weeks
                                delegate: Column {
                                    required property int index
                                    spacing: root.gapSize
                                    Repeater {
                                        model: 7
                                        delegate: Rectangle {
                                            required property int index
                                            readonly property int day: root.firstDay + parent.index*7 + index
                                            readonly property int value: root.countAt(root.farcasterDays, day)
                                            width: root.cellSize
                                            height: root.cellSize
                                            radius: 3
                                            color: root.tileColor(root.farcasterReady ? value : 0, "farcaster")
                                            MouseArea {
                                                anchors.fill: parent
                                                enabled: root.farcasterReady && parent.value>0
                                                onClicked: root.selectedDate = root.dayString(parent.day)
                                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: root.showMetrics
                        Layout.fillWidth: true
                        Layout.minimumWidth: 90
                        Layout.maximumWidth: 122
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Theme.space3
                        ColumnLayout {
                            spacing: 0
                            Label {
                                text: root.farcasterReady ? String(root.farcasterData.days.length) : "—"
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightDemiBold
                                color: Theme.ink
                            }
                            Label {
                                text: qsTrId("genycaster.activity.active_days")
                                font.pixelSize: Theme.typeCaptionSize
                                color: Theme.inkMuted
                            }
                        }
                        ColumnLayout {
                            spacing: 0
                            Label {
                                text: root.farcasterReady ? String(root.farcasterData.replies || 0) : "—"
                                font.pixelSize: Theme.typeTitle3Size
                                font.weight: Theme.weightDemiBold
                                color: Theme.ink
                            }
                            Label {
                                text: qsTrId("genycaster.activity.reply_count")
                                font.pixelSize: Theme.typeCaptionSize
                                color: Theme.inkMuted
                            }
                        }
                    }
                }

                Label {
                    visible: root.selectedDate.length>0 || !root.farcasterReady
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
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: root.desktopGrid ? Math.floor((root.width - Theme.space2)/2) : root.width
            Layout.alignment: Qt.AlignTop
            visible: root.expanded
            spacing: Theme.space2

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: githubContent.implicitHeight + Theme.space3*2
                radius: Theme.space3
                color: Theme.surfaceMuted
                border.width: 1
                border.color: Theme.borderMuted

                ColumnLayout {
                    id: githubContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space3
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        Rectangle {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                            radius: 9
                            color: "#FFFFFF"
                            Image {
                                anchors.centerIn: parent
                                width: 21
                                height: 21
                                source: root.providerIconRoot + "github.svg"
                                sourceSize.width: 42
                                sourceSize.height: 42
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                Accessible.ignored: true
                            }
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
                                text: root.effectiveGithubLogin.length>0
                                      ? "@" + root.effectiveGithubLogin + " · " + root.sourceSubtitle(root.githubData)
                                      : root.githubLinked
                                        ? root.githubAccountId.length>0
                                          ? root.sourceSubtitle(root.githubData)
                                          : qsTrId("genycaster.activity.github_identity_pending")
                                        : qsTrId("genycaster.activity.not_linked")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }
                        Label {
                            visible: root.githubReady
                            text: qsTrId("genycaster.activity.events").arg(root.githubData.count || 0)
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        ActionButton {
                            visible: root.isOwnProfile && (!root.githubLinked || (!root.effectiveGithubLogin.length && !root.githubAccountId.length))
                            compact: true
                            quiet: false
                            elevated: false
                            pill: true
                            text: qsTrId("genycaster.activity.manage")
                            onClicked: root.connectionsRequested()
                        }
                    }

                    RowLayout {
                        visible: root.githubReady
                        Layout.fillWidth: true
                        spacing: Theme.space3
                        Row {
                            spacing: root.gapSize
                            Repeater {
                                model: 5
                                delegate: Column {
                                    required property int index
                                    spacing: root.gapSize
                                    Repeater {
                                        model: 7
                                        delegate: Rectangle {
                                            required property int index
                                            readonly property int day: root.todayDay - 34 + parent.index*7 + index
                                            width: root.githubCellSize; height: root.githubCellSize; radius: 4
                                            color: root.tileColor(root.countAt(root.githubDays,day),"github")
                                        }
                                    }
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.activity.github_window")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: walletContent.implicitHeight + Theme.space3*2
                radius: Theme.space3
                color: Theme.surfaceMuted
                border.width: 1
                border.color: Theme.borderMuted

                RowLayout {
                    id: walletContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space3
                    spacing: Theme.space2
                    Rectangle {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        radius: 9
                        color: "#FFFFFF"
                        Image {
                            anchors.centerIn: parent
                            width: 16
                            height: 24
                            source: root.providerIconRoot + "ethereum-wallet.svg"
                            sourceSize.width: 32
                            sourceSize.height: 48
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            Accessible.ignored: true
                        }
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
        } // activityGrid
    }
}
