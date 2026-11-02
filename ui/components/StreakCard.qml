import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Owner-scoped verified streaks. The parent Drawer owns the only screen title;
// this component owns just the summary, calendar and milestone progression.
Item {
    id: root
    objectName: "profileStreakCard"

    signal profileRequested(double fid)

    property var snapshot: ({})
    property bool loading: false
    property string errorText: ""
    property bool compact: false
    property string activeTrack: "activity"
    property bool showAllBadges: false
    property bool showLeaderboard: false
    readonly property bool leaderboardVisible:
        openProofConnections.streakPreferences.leaderboard_visible === true
    readonly property var leaderboardEntries: openProofConnections.streakLeaderboard || []

    readonly property var track: activeTrack === "creator"
        ? (snapshot.creator || ({})) : (snapshot.activity || ({}))
    readonly property bool available: snapshot.time_zone === "UTC"
    readonly property var badges: track.badges || []
    readonly property var milestones: [1, 3, 7, 14, 30, 100, 365]
    readonly property int current: Number(track.current || 0)
    readonly property int record: Number(track.longest || 0)
    readonly property int nextMilestone: Number(track.next_milestone || 0)
    readonly property bool saturdayWeekAvailable: Theme.rtl
        && Array.isArray(track.week_saturday) && track.week_saturday.length === 7
    readonly property var displayWeek: saturdayWeekAvailable
        ? track.week_saturday : (track.week || [])
    readonly property var weekdayLabels: saturdayWeekAvailable
        ? [qsTrId("genycaster.streak.sat"), qsTrId("genycaster.streak.sun"),
           qsTrId("genycaster.streak.mon"), qsTrId("genycaster.streak.tue"),
           qsTrId("genycaster.streak.wed"), qsTrId("genycaster.streak.thu"),
           qsTrId("genycaster.streak.fri")]
        : [qsTrId("genycaster.streak.mon"), qsTrId("genycaster.streak.tue"),
           qsTrId("genycaster.streak.wed"), qsTrId("genycaster.streak.thu"),
           qsTrId("genycaster.streak.fri"), qsTrId("genycaster.streak.sat"),
           qsTrId("genycaster.streak.sun")]

    implicitHeight: body.implicitHeight

    ColumnLayout {
        id: body
        width: parent.width
        spacing: Theme.space3
        LayoutMirroring.enabled: false

        GenyTabStrip {
            Layout.fillWidth: true
            currentId: root.activeTrack
            distribute: true
            segmented: true
            minimumTabWidth: 76
            tabs: [
                {id: "activity", label: qsTrId("genycaster.streak.activity")},
                {id: "creator", label: qsTrId("genycaster.streak.creator")}
            ]
            onActivated: id => {
                root.activeTrack = id
                if(root.showLeaderboard)
                    openProofConnections.fetchStreakLeaderboard(id)
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !root.available
            text: root.loading ? qsTrId("genycaster.streak.loading")
                : qsTrId("genycaster.streak.unavailable")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
            horizontalAlignment: Theme.textStartAlignment
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.available
            spacing: Theme.space3

            // Reference-inspired centered achievement hero; one focal point
            // rather than competing statistics across the sheet.
            ColumnLayout {
                objectName: "streakCenteredHero"
                Layout.fillWidth: true
                spacing: Theme.space1

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Theme.scaleUi(root.compact ? 80 : 92)
                    Layout.preferredHeight: Layout.preferredWidth
                    radius: width / 2
                    color: Theme.warningSurface
                    border.color: Theme.borderMuted
                    border.width: 1
                    Icon {
                        anchors.centerIn: parent
                        preferredSize: Theme.scaleUi(root.compact ? 42 : 48)
                        text: Theme.iconEnergy
                        color: Theme.warning
                    }
                }
                Label {
                    objectName: "streakCurrentCount"
                    Layout.fillWidth: true
                    text: LocaleFormat.formatInteger(root.current)
                    color: Theme.warning
                    font.pixelSize: Theme.scaleUi(root.compact ? 72 : 78)
                    font.weight: Theme.weightBold
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: root.current === 1
                        ? qsTrId("genycaster.streak.day_in_row")
                        : qsTrId("genycaster.streak.days_in_row")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.streak.best")
                        .arg(LocaleFormat.formatInteger(root.record))
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Text.AlignHCenter
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.nextMilestone
                    ? qsTrId("genycaster.streak.next")
                        .arg(LocaleFormat.formatInteger(root.nextMilestone))
                    : qsTrId("genycaster.streak.legendary")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                horizontalAlignment: Theme.textStartAlignment
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.nextMilestone > 0
                height: Theme.scaleUi(4)
                radius: height / 2
                color: Theme.borderMuted
                Rectangle {
                    // Advance the milestone bar from the reading start.
                    x: Theme.rtl ? parent.width - width : 0
                    height: parent.height
                    width: parent.width * Math.min(1,
                        Math.max(0, root.current / Math.max(1, root.nextMilestone)))
                    radius: parent.radius
                    color: Theme.warning
                }
            }

            RowLayout {
                objectName: "streakWeeklyCalendar"
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space1

                Repeater {
                    model: root.displayWeek
                    delegate: ColumnLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: root.weekdayLabels[index]
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: Theme.scaleUi(root.compact ? 32 : 35)
                            height: width
                            radius: width / 2
                            color: modelData.completed
                                ? Theme.warningSurface : Theme.surfaceMuted
                            border.color: modelData.completed
                                ? Theme.warning : Theme.borderMuted
                            border.width: modelData.completed ? 2 : 1
                            Icon {
                                anchors.centerIn: parent
                                text: modelData.completed ? Theme.iconCheck : ""
                                preferredSize: 15
                                color: Theme.warning
                            }
                            Rectangle {
                                visible: !modelData.completed
                                anchors.centerIn: parent
                                width: Theme.scaleUi(8)
                                height: width
                                radius: width / 2
                                color: modelData.future ? Theme.inkSubtle
                                    : Theme.inkMuted
                                opacity: modelData.future ? 0.24 : 0.4
                            }
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.activeTrack === "creator"
                    ? qsTrId("genycaster.streak.creator_rule")
                    : qsTrId("genycaster.streak.activity_rule")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.WordWrap
                horizontalAlignment: Theme.textStartAlignment
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.divider
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.streak.badges")
                        .arg(LocaleFormat.formatInteger(root.badges.length))
                        .arg(LocaleFormat.formatInteger(root.milestones.length))
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                    horizontalAlignment: Theme.textStartAlignment
                }
                ActionButton {
                    text: root.showAllBadges ? qsTrId("genycaster.streak.show_less")
                        : qsTrId("genycaster.streak.see_all")
                    quiet: true
                    compact: true
                    onClicked: root.showAllBadges = !root.showAllBadges
                }
            }

            // A single horizontal strip, not a big grid. More badges scroll,
            // preserving the compact height of the mobile sheet.
            ListView {
                objectName: "streakBadgeStrip"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.scaleUi(83)
                orientation: ListView.Horizontal
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.showAllBadges
                    ? root.milestones : root.milestones.slice(0, 4)

                delegate: Item {
                    required property var modelData
                    readonly property bool earned: root.badges.indexOf(modelData) >= 0
                    width: Theme.scaleUi(58)
                    height: ListView.view.height
                    Rectangle {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Theme.scaleUi(44)
                        height: width
                        radius: width / 2
                        color: earned ? Theme.warningSurface : Theme.surfaceMuted
                        border.color: earned ? Theme.warning : Theme.borderMuted
                        border.width: earned ? 2 : 1
                        Icon {
                            anchors.centerIn: parent
                            text: earned ? Theme.iconFidelity : "system/lock"
                            preferredSize: 18
                            color: earned ? Theme.warning : Theme.inkMuted
                        }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        text: qsTrId("genycaster.streak.badge_days")
                            .arg(LocaleFormat.formatInteger(modelData))
                        font.pixelSize: Theme.typeCaptionSize
                        color: earned ? Theme.ink : Theme.inkMuted
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.divider
            }
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                Label {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.streak.leaderboard")
                    color: Theme.ink
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                }
                ActionButton {
                    compact: true
                    quiet: true
                    text: root.showLeaderboard
                        ? qsTrId("genycaster.streak.show_less")
                        : qsTrId("genycaster.streak.top_100")
                    onClicked: {
                        root.showLeaderboard = !root.showLeaderboard
                        if(root.showLeaderboard)
                            openProofConnections.fetchStreakLeaderboard(root.activeTrack)
                    }
                }
            }
            // A three-person preview is visible even without joining.
            // Expanding reveals all 100 in ONE parent Flickable; a nested
            // ListView used to swallow wheel/touch scrolling at the bottom.
            ColumnLayout {
                objectName: "streakLeaderboardContent"
                Layout.fillWidth: true
                spacing: Theme.space2

                Label {
                    Layout.fillWidth: true
                    visible: root.showLeaderboard
                    text: qsTrId("genycaster.streak.leaderboard_description")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Theme.textStartAlignment
                }
                ActionButton {
                    Layout.fillWidth: true
                    visible: root.showLeaderboard && openProofConnections.cloudAuthorizationReady
                    enabled: !openProofConnections.streakLeaderboardBusy
                    compact: true
                    quiet: !root.leaderboardVisible
                    selected: root.leaderboardVisible
                    text: root.leaderboardVisible
                        ? qsTrId("genycaster.streak.leave_leaderboard")
                        : qsTrId("genycaster.streak.join_leaderboard")
                    onClicked: {
                        // Membership affects the user's OWN rank, never the
                        // ability to read everyone else's public ranking.
                        openProofConnections.setStreakPreferences(
                            String(openProofConnections.streakPreferences.activity_source || "casts"),
                            !root.leaderboardVisible)
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.showLeaderboard
                        && !openProofConnections.streakLeaderboardBusy
                        && root.leaderboardEntries.length === 0
                    text: qsTrId("genycaster.streak.leaderboard_empty")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }
                GenySpinner {
                    Layout.alignment: Qt.AlignHCenter
                    running: visible
                    visible: openProofConnections.streakLeaderboardBusy
                }
                ColumnLayout {
                    objectName: "streakTop100"
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Repeater {
                        model: root.showLeaderboard
                            ? root.leaderboardEntries : root.leaderboardEntries.slice(0, 3)
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.scaleUi(root.compact ? 62 : 68)
                            radius: Theme.curveCompact
                            color: Theme.surfaceMuted
                            border.color: Number(modelData.rank) === 1
                                ? Theme.warning : Theme.borderMuted
                            border.width: Number(modelData.rank) === 1 ? 1 : 0

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.space3
                                anchors.rightMargin: Theme.space3
                                spacing: Theme.space2
                                layoutDirection: Theme.uiDirection
                                Label {
                                    text: LocaleFormat.formatInteger(Number(modelData.rank))
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightBold
                                    color: Number(modelData.rank) <= 3
                                        ? Theme.warning : Theme.inkMuted
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.preferredWidth: Theme.scaleUi(22)
                                }
                                Avatar {
                                    avatarSize: Theme.scaleUi(root.compact ? 34 : 38)
                                    Layout.preferredWidth: avatarSize
                                    Layout.preferredHeight: avatarSize
                                    sourceUrl: String(modelData.avatar_url || "")
                                    displayName: String(modelData.display_name || modelData.username || "")
                                    forceAutomaticDownload: true
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        Layout.fillWidth: true
                                        text: String(modelData.display_name || modelData.username || "")
                                            || qsTrId("genycaster.streak.fid")
                                                .arg(LocaleFormat.formatInteger(Number(modelData.fid)))
                                        font.pixelSize: Theme.typeBodySize
                                        font.weight: Theme.weightDemiBold
                                        color: Theme.ink
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: String(modelData.username || "")
                                            ? "@" + String(modelData.username) + " · " +
                                                qsTrId("genycaster.streak.fid")
                                                .arg(LocaleFormat.formatInteger(Number(modelData.fid)))
                                            : qsTrId("genycaster.streak.fid")
                                                .arg(LocaleFormat.formatInteger(Number(modelData.fid)))
                                        font.pixelSize: Theme.typeCaptionSize
                                        color: Theme.inkMuted
                                        elide: Text.ElideRight
                                    }
                                }
                                ColumnLayout {
                                    spacing: 1
                                    Label {
                                        Layout.alignment: Qt.AlignRight
                                        text: qsTrId("genycaster.streak.leader_days")
                                            .arg(LocaleFormat.formatInteger(Number(modelData.current)))
                                        color: Theme.warning
                                        font.weight: Theme.weightDemiBold
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                    Label {
                                        Layout.alignment: Qt.AlignRight
                                        text: qsTrId("genycaster.streak.best")
                                            .arg(LocaleFormat.formatInteger(Number(modelData.longest)))
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                            }
                            Accessible.role: Accessible.Button
                            Accessible.name: String(modelData.display_name || modelData.username || modelData.fid)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.profileRequested(Number(modelData.fid))
                            }
                        }
                    }
                }
            }
        }
    }
}
