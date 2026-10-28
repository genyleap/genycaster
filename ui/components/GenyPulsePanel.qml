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
    property string selectedGithubDate: ""
    property int githubYear: new Date().getUTCFullYear()
    property string githubRange: compact ? "3m" : "1y"
    readonly property var githubContributionData: genyPulseActivity.githubContributions
    readonly property bool githubContributionsReady:
        githubContributionData.status==="ok"
        && String(githubContributionData.username || "").toLowerCase()===
           effectiveGithubLogin.toLowerCase()
        && Number(githubContributionData.year || 0)===githubYear
    readonly property var githubContributionDays: githubContributionMap(githubContributionData.days)
    readonly property int githubYearMax: new Date().getUTCFullYear()
    readonly property int githubEndMonth: githubYear===githubYearMax ? new Date().getUTCMonth() : 11
    readonly property int githubFirstMonth: githubRange==="1m" ? githubEndMonth
        : githubRange==="3m" ? Math.max(0,githubEndMonth-2) : 0
    readonly property int githubStartRangeDay:
        Math.floor(Date.UTC(githubYear,githubFirstMonth,1)/86400000)
    readonly property int githubLastRangeDay:
        Math.min(Math.floor(Date.UTC(githubYear,githubEndMonth+1,0)/86400000),
                 githubYear===githubYearMax ? todayDay : 999999)
    // A stable year-to-date calendar for ALL range filters. The filtered
    // months stay at the end; older dates remain neutral rather than leaving
    // an empty half-card when the user chooses 1M or 3M.
    readonly property int githubCalendarFirstDay:
        Math.floor(Date.UTC(githubYear,0,1)/86400000)
    readonly property int githubCalendarLastDay:
        Math.min(Math.floor(Date.UTC(githubYear+1,0,0)/86400000),todayDay)
    readonly property int githubGridStart: githubCalendarFirstDay-
        new Date(githubCalendarFirstDay*86400000).getUTCDay()
    readonly property int githubWeeks:
        Math.ceil((githubCalendarLastDay-githubGridStart+1)/7)
    // Keep a fixed calendar scale across 1M / 3M / 1Y. Shorter
    // periods occupy the RIGHT edge of the same viewport, not the left.
    readonly property int githubTileWidth: root.width>=744 ? 12 : 10
    readonly property int githubTileHeight: githubTileWidth
    readonly property int githubGridWidth: githubWeeks*githubTileWidth+(githubWeeks-1)*gapSize
    readonly property int githubVisibleTotal: {
        if(!githubContributionsReady)return 0
        let total=0
        for(let day=githubStartRangeDay;day<=githubLastRangeDay;++day)
            total+=Number((githubContributionDays[dayString(day)]||{}).count || 0)
        return total
    }
    function githubContributionMap(days) {
        const map=({})
        if(!days || typeof days.length!=="number")return map
        for(let i=0;i<days.length && i<366;++i) {
            const row=days[i]||({})
            const date=String(row.date||"")
            if(/^\d{4}-\d{2}-\d{2}$/.test(date))
                map[date]={count:Number(row.count||0),level:Number(row.level||0)}
        }
        return map
    }
    function githubTileColor(level) {
        const colors=Theme.dark
          ? ["#232A27","#0E4429","#006D32","#26A641","#39D353"]
          : ["#E8EBE9","#9BE9A8","#40C463","#30A14E","#216E39"]
        return colors[Math.max(0,Math.min(4,level))] || colors[0]
    }

    signal connectionsRequested()

    readonly property var farcasterData: genyPulseActivity.farcaster
    readonly property var githubData: genyPulseActivity.github
    readonly property var walletData: genyPulseActivity.onchain
    readonly property string providerIconRoot: "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/providers/"
    readonly property int weeks: 13
    readonly property int gapSize: 3
    // Casts should use the same small square cadence as GitHub.
    // Keep the 91-day coverage explicit rather than inventing older days.
    readonly property int cellSize: githubTileWidth
    readonly property int cellHeight: githubTileHeight
    readonly property int castsGridWidth: weeks*cellSize + (weeks-1)*gapSize
    readonly property bool hasOnchainActivity: verifiedWallet.length>0
        && (walletData.status==="ok" || walletData.status==="partial")
    readonly property bool hasOptionalSources: githubLinked || hasOnchainActivity
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
        if(status==="authorization-required")return qsTrId("genycaster.activity.github_session_required")
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
        refreshGithubCalendar()
        if(!farcasterReady)selectedDate=""
    }

    function refreshGithubCalendar() {
        genyPulseActivity.requestGitHubContributions(isOwnProfile ? effectiveGithubLogin : "",githubYear)
    }
    onEffectiveGithubLoginChanged: refreshGithubCalendar()
    onGithubYearChanged: refreshGithubCalendar()
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
                visible: root.hasOptionalSources
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

        ColumnLayout {
            id: activityGrid
            Layout.fillWidth: true
            spacing: Theme.space2

        Rectangle {
            Layout.fillWidth: true
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
                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconCompose
                            preferredSize: 18
                            color: Theme.dark ? "#C8B8F8" : "#745BC8"
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: "Casts"
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

                // A centered, self-contained calendar: never split the day
                // count and the grid across opposite ends of a large card.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 7 * root.cellHeight + 6 * root.gapSize
                    Row {
                        id: calendarRow
                        anchors.horizontalCenter: parent.horizontalCenter
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
                                        height: root.cellHeight
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

                Label {
                    Layout.fillWidth: true
                    text: root.selectedDate.length>0 && root.farcasterReady
                          ? qsTrId("genycaster.activity.day_summary")
                                .arg(root.selectedDate)
                                .arg(root.farcasterDays[root.selectedDate] || 0)
                          : root.farcasterReady
                            ? qsTrId("genycaster.activity.active_days")
                                + ": " + Number(root.farcasterData.days.length)
                            : root.sourceSubtitle(root.farcasterData)
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideRight
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.expanded && root.hasOptionalSources
            spacing: Theme.space2

            Rectangle {
                visible: root.githubLinked
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
                                      ? "@" + root.effectiveGithubLogin
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
                            visible: root.githubContributionsReady
                            text: qsTrId("genycaster.activity.contributions_count").arg(root.githubVisibleTotal)
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

                    GridLayout {
                        Layout.fillWidth: true
                        columns: githubContent.width < 410 ? 1 : 2
                        rowSpacing: Theme.space1
                        columnSpacing: Theme.space2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            ActionButton {
                                compact: true; quiet: true; elevated: false; pill: true
                                iconOnly: true
                                iconGlyph: Theme.iconArrowLeft
                                text: qsTrId("genycaster.activity.previous_year")
                                enabled: root.githubYear>2014
                                onClicked: { root.githubYear--; root.selectedGithubDate="" }
                            }
                            Label {
                                text: String(root.githubYear)
                                color: Theme.ink
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                                Layout.minimumWidth: 40
                                horizontalAlignment: Text.AlignHCenter
                            }
                            ActionButton {
                                compact: true; quiet: true; elevated: false; pill: true
                                iconOnly: true
                                iconGlyph: Theme.iconArrowRight
                                text: qsTrId("genycaster.activity.next_year")
                                enabled: root.githubYear<root.githubYearMax
                                onClicked: { root.githubYear++; root.selectedGithubDate="" }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            Repeater {
                                model: ["1m","3m","1y"]
                                delegate: ActionButton {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    compact: true
                                    quiet: root.githubRange!==modelData
                                    selected: root.githubRange===modelData
                                    elevated: false
                                    pill: true
                                    text: modelData.toUpperCase()
                                    onClicked: {
                                        root.githubRange=modelData
                                        root.selectedGithubDate=""
                                    }
                                }
                            }
                        }
                    }

                    Flickable {
                        id: githubCalendarViewport
                        visible: root.githubContributionsReady
                        Layout.fillWidth: true
                        Layout.preferredHeight: 7*root.githubTileHeight+6*root.gapSize
                        clip: true
                        contentWidth: Math.max(width, root.githubGridWidth)
                        contentHeight: height
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        function showLatest() {
                            contentX=Math.max(0,contentWidth-width)
                        }
                        // Keep short ranges at the trailing/right edge, and
                        // show the most recent dates when 1Y overflows.
                        onContentWidthChanged: Qt.callLater(showLatest)
                        onWidthChanged: Qt.callLater(showLatest)
                        onVisibleChanged: if (visible) Qt.callLater(showLatest)
                        Connections {
                            target: root
                            function onGithubRangeChanged() {
                                Qt.callLater(githubCalendarViewport.showLatest)
                            }
                            function onGithubYearChanged() {
                                Qt.callLater(githubCalendarViewport.showLatest)
                            }
                        }
                        Row {
                            x: Math.max(0, (githubCalendarViewport.width-root.githubGridWidth)/2)
                            spacing: root.gapSize
                            Repeater {
                                model: root.githubWeeks
                                delegate: Column {
                                    required property int index
                                    spacing: root.gapSize
                                    Repeater {
                                        model: 7
                                        delegate: Rectangle {
                                            required property int index
                                            readonly property int day:
                                                root.githubGridStart+parent.index*7+index
                                            readonly property bool inCalendar:
                                                day>=root.githubCalendarFirstDay &&
                                                day<=root.githubCalendarLastDay
                                            readonly property bool inRange:
                                                inCalendar && day>=root.githubStartRangeDay &&
                                                day<=root.githubLastRangeDay
                                            readonly property string date: root.dayString(day)
                                            readonly property var datum:
                                                root.githubContributionDays[date] || ({})
                                            width: root.githubTileWidth
                                            height: root.githubTileHeight
                                            radius: 3
                                            color: !inCalendar ? "transparent"
                                                : inRange ? root.githubTileColor(Number(datum.level || 0))
                                                : (Theme.dark ? "#282B33" : "#E9EBEF")
                                            MouseArea {
                                                anchors.fill: parent
                                                enabled: parent.inRange
                                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                onClicked: root.selectedGithubDate=parent.date
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.githubContributionsReady && root.selectedGithubDate.length>0
                        text: qsTrId("genycaster.activity.day_contributions")
                            .arg(root.selectedGithubDate)
                            .arg(Number((root.githubContributionDays[root.selectedGithubDate]||{}).count || 0))
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.inkMuted
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.githubContributionsReady
                              ? qsTrId("genycaster.activity.github_public_calendar")
                              : root.effectiveGithubLogin.length>0
                                ? root.sourceSubtitle(root.githubContributionData)
                                : root.githubLinked ? qsTrId("genycaster.activity.github_identity_pending")
                                                    : qsTrId("genycaster.activity.not_linked")
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.inkMuted
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                visible: root.hasOnchainActivity
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
