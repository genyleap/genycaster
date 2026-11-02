import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Owner-scoped verified streaks. The parent Drawer owns the only screen title;
// this component owns just the summary, calendar and milestone progression.
Item {
    id: root
    objectName: "profileStreakCard"

    property var snapshot: ({})
    property bool loading: false
    property string errorText: ""
    property bool compact: false
    property string activeTrack: "activity"
    property bool showAllBadges: false

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
            onActivated: id => root.activeTrack = id
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

            // The number is the single hero. Text and milestone align with
            // the reading direction, not with the physical left screen edge.
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Label {
                    objectName: "streakCurrentCount"
                    text: LocaleFormat.formatInteger(root.current)
                    color: Theme.warning
                    font.pixelSize: Theme.scaleUi(root.compact ? 40 : 38)
                    font.weight: Theme.weightBold
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: root.current === 1
                            ? qsTrId("genycaster.streak.day_in_row")
                            : qsTrId("genycaster.streak.days_in_row")
                        color: Theme.ink
                        font.pixelSize: Theme.typeBodySize
                        font.weight: Theme.weightDemiBold
                        horizontalAlignment: Theme.textStartAlignment
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.streak.best")
                            .arg(LocaleFormat.formatInteger(root.record))
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        horizontalAlignment: Theme.textStartAlignment
                    }
                }
                Label {
                    Layout.maximumWidth: root.width * (root.compact ? 0.34 : 0.30)
                    text: root.nextMilestone
                        ? qsTrId("genycaster.streak.next")
                            .arg(LocaleFormat.formatInteger(root.nextMilestone))
                        : qsTrId("genycaster.streak.legendary")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Theme.rtl ? Text.AlignLeft : Text.AlignRight
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.nextMilestone > 0
                height: Theme.scaleUi(4)
                radius: height / 2
                color: Theme.borderMuted
                Rectangle {
                    anchors.left: parent.left
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
        }
    }
}
