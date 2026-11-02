import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Streak v1 uses owner-scoped, independently verified CastAdd events from
// Genycaster's database. No local fake data, login rewards or inferred days.
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
    readonly property var week: track.week || []
    readonly property var badges: track.badges || []
    readonly property var milestones: [1,3,7,14,30,100,365]
    readonly property int current: Number(track.current || 0)
    readonly property int record: Number(track.longest || 0)
    readonly property bool available: snapshot.time_zone === "UTC"

    implicitHeight: card.implicitHeight

    Rectangle {
        id: card
        width: parent.width
        implicitHeight: layout.implicitHeight + Theme.space4 * 2
        radius: Theme.curveContainer
        color: Theme.surface
        border.color: Theme.borderMuted
        border.width: 1

        ColumnLayout {
            id: layout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space4
            spacing: Theme.space3

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Icon {
                    text: Theme.iconEnergy
                    preferredSize: 21
                    color: Theme.accent
                }
                Label {
                    text: "Streak"
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    color: Theme.ink
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: "UTC"
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
            }

            GenyTabStrip {
                Layout.fillWidth: true
                currentId: root.activeTrack
                distribute: true
                segmented: true
                minimumTabWidth: 88
                tabs: [
                    {id:"activity",label:"Activity"},
                    {id:"creator",label:"Creator"}
                ]
                onActivated: id => root.activeTrack=id
            }

            Label {
                Layout.fillWidth: true
                visible: !root.available
                text: root.errorText.length ? root.errorText
                    : root.loading ? "Loading Streak…" : "Streak is not available yet."
                wrapMode: Text.WordWrap
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.available
                spacing: Theme.space3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Label {
                        text: String(root.current)
                        color: Theme.ink
                        font.pixelSize: Theme.scaleUi(33)
                        font.weight: Theme.weightBold
                    }
                    ColumnLayout {
                        spacing: 1
                        Label {
                            text: "day" + (root.current===1 ? "" : "s") + " in a row"
                            color: Theme.ink
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                        }
                        Label {
                            text: "Best: " + root.record + " days"
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                    Item {Layout.fillWidth: true}
                    Label {
                        text: root.track.next_milestone
                            ? "Next: " + root.track.next_milestone + " days" : "Legendary"
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    Repeater {
                        model: root.week
                        delegate: ColumnLayout {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: ["M","T","W","T","F","S","S"][index]
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: Theme.scaleUi(root.compact ? 29 : 34)
                                height: width
                                radius: width/2
                                color: modelData.completed ? Theme.accent : Theme.surfaceMuted
                                border.color: modelData.completed ? Theme.accent
                                    : modelData.future ? Theme.borderMuted : Theme.border
                                border.width: 1
                                Icon {
                                    anchors.centerIn: parent
                                    text: modelData.completed ? Theme.iconCheck : ""
                                    preferredSize: 15
                                    color: Theme.contrastInk(Theme.accent)
                                }
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: root.activeTrack==="creator"
                        ? "One verified original Cast per day counts."
                        : "A verified Cast or Reply counts once per day."
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.divider
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Badges  ·  " + root.badges.length + "/" + root.milestones.length
                        font.pixelSize: Theme.typeBodySize
                        font.weight: Theme.weightDemiBold
                        color: Theme.ink
                    }
                    Item {Layout.fillWidth: true}
                    ActionButton {
                        text: root.showAllBadges ? "Show less" : "See all"
                        quiet: true
                        compact: true
                        onClicked: root.showAllBadges=!root.showAllBadges
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 4 : 7
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space2
                    Repeater {
                        model: root.showAllBadges ? root.milestones
                            : root.milestones.slice(0,root.compact ? 3 : 4)
                        delegate: ColumnLayout {
                            required property var modelData
                            readonly property bool earned: root.badges.indexOf(modelData)>=0
                            Layout.fillWidth: true
                            spacing: Theme.space1
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: Theme.scaleUi(42)
                                height: width
                                radius: width/2
                                color: earned ? Theme.surfaceMuted : Theme.surface
                                border.color: earned ? Theme.accent : Theme.borderMuted
                                border.width: earned ? 2 : 1
                                Icon {
                                    anchors.centerIn: parent
                                    text: earned ? Theme.iconFidelity : "system/lock"
                                    preferredSize: 18
                                    color: earned ? Theme.accent : Theme.inkMuted
                                }
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: String(modelData) + "d"
                                font.pixelSize: Theme.typeCaptionSize
                                color: earned ? Theme.ink : Theme.inkMuted
                            }
                        }
                    }
                }
            }
        }
    }
}
