import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Immutable, capture-sized presentation of one Stats snapshot. Pure visuals:
// no network, file, signer or protocol operations happen inside this component.
// The PNG generated from this item remains legible in unrelated Farcaster clients.
Item {
    id: root
    objectName: "genycasterRichStatsCard"
    width: 960
    height: 600
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    property var snapshot: ({})
    property string displayName: ""
    property string username: ""
    property int periodDays: 30
    property double capturedAt: 0
    property string cardStyle: "neon"
    readonly property bool lightStyle: cardStyle === "minimal"
    readonly property bool engagementAvailable: snapshot.hasEngagement === true
    readonly property color mainInk: lightStyle ? "#151B2D" : "#F8FAFF"
    readonly property color secondaryInk: lightStyle ? "#536179" : "#AEB9DA"
    readonly property color highlightInk: lightStyle ? "#594CE8" : "#A88DFF"
    readonly property color tileFill: lightStyle ? "#F0F3FB" : "#182342"
    readonly property string periodLabel: periodDays === 7 ? "7 days"
        : periodDays === 30 ? "30 days"
        : periodDays === 90 ? "90 days" : "365 days"

    function fmt(value) {
        const num = Math.max(0, Math.floor(Number(value || 0)))
        return String(num).replace(/\B(?=(\d{3})+(?!\d))/g, ",")
    }
    function amount(value) { return engagementAvailable ? fmt(value) : "—" }

    Rectangle {
        anchors.fill: parent
        radius: 38
        gradient: Gradient {
            GradientStop { position: 0; color: root.lightStyle ? "#FFFFFF" : "#0C142D" }
            GradientStop { position: 1; color: root.lightStyle ? "#E9EEFF" : "#18102F" }
        }
        border.color: root.lightStyle ? "#CDD6F6" : "#5853AD"
        border.width: 2
    }
    Rectangle {
        x: 680; y: -140; width: 390; height: 390; radius: 195
        opacity: root.lightStyle ? 0.12 : 0.22
        gradient: Gradient {
            GradientStop { position: 0; color: "#AA63FF" }
            GradientStop { position: 1; color: "transparent" }
        }
    }
    Rectangle {
        x: -105; y: 370; width: 370; height: 370; radius: 185
        opacity: root.lightStyle ? 0.08 : 0.18
        gradient: Gradient {
            GradientStop { position: 0; color: "#29CFFF" }
            GradientStop { position: 1; color: "transparent" }
        }
    }
    Column {
        x: 46; y: 36; width: parent.width - 92
        spacing: 19

        Row {
            width: parent.width
            height: 64
            spacing: 18
            Image {
                width: 60; height: 60
                source: "qrc:/qt/qml/Farcaster/Client/wasm/genycaster-icon.svg"
                sourceSize: Qt.size(180, 180)
                fillMode: Image.PreserveAspectFit
            }
            Column {
                width: 440
                y: 4
                spacing: 2
                Text {
                    text: root.displayName.length > 0 ? root.displayName : "Genycaster member"
                    color: root.mainInk
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    width: parent.width
                }
                Text {
                    text: root.username.length > 0
                        ? (root.username.charAt(0) === "@" ? root.username : "@" + root.username)
                        : "Genycaster Stats"
                    color: root.secondaryInk
                    font.pixelSize: 18
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "GENYCASTER  /  STATS"
                font.pixelSize: 16
                font.weight: Font.Bold
                color: root.highlightInk
            }
        }

        Row {
            spacing: 18
            Text {
                text: root.periodLabel + " Recap"
                color: root.mainInk
                font.pixelSize: 38
                font.weight: Font.Bold
            }
            Rectangle {
                y: 8; width: 116; height: 32; radius: 16
                color: root.lightStyle ? "#E5E4FF" : "#302350"
                Text {
                    anchors.centerIn: parent
                    text: "ACTIVITY"
                    color: root.highlightInk
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }
            }
        }

        Rectangle {
            width: parent.width; height: 168; radius: 28
            color: root.lightStyle ? "#F4F4FF" : "#222246"
            border.color: root.lightStyle ? "#DCDCFB" : "#56518F"
            border.width: 1
            Row {
                anchors.fill: parent
                anchors.margins: 23
                spacing: 35
                Text {
                    y: (parent.height - height) / 2
                    text: root.amount(root.snapshot.engagement)
                    font.pixelSize: 98
                    font.weight: Font.Bold
                    color: root.mainInk
                }
                Rectangle {
                    y: 22; width: 2; height: 79
                    color: root.lightStyle ? "#D0D3EB" : "#585783"
                }
                Column {
                    y: (parent.height - height) / 2
                    spacing: 5
                    Text {
                        text: "Interactions"
                        font.pixelSize: 31
                        font.weight: Font.DemiBold
                        color: root.mainInk
                    }
                    Text {
                        text: "received"
                        font.pixelSize: 25
                        color: root.secondaryInk
                    }
                }
            }
        }

        Row {
            width: parent.width
            height: 115
            spacing: 13
            Repeater {
                model: [
                    {label:"Casts", value:root.fmt(root.snapshot.casts)},
                    {label:"Replies written", value:root.fmt(root.snapshot.replies)},
                    {label:"Likes", value:root.amount(root.snapshot.likes)},
                    {label:"Recasts", value:root.amount(root.snapshot.recasts)},
                    {label:"Replies received", value:root.amount(root.snapshot.receivedReplies)}
                ]
                delegate: Rectangle {
                    required property var modelData
                    width: (parent.width - 52) / 5
                    height: 115
                    radius: 21
                    color: root.tileFill
                    border.color: root.lightStyle ? "#DDE3F3" : "#344065"
                    border.width: 1
                    Column {
                        anchors.centerIn: parent
                        spacing: 7
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.value
                            color: root.mainInk
                            font.pixelSize: 38
                            font.weight: Font.Bold
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: root.secondaryInk
                            font.pixelSize: 17
                        }
                    }
                }
            }
        }
        Row {
            width: parent.width; height: 43
            Text {
                text: "Posts from the selected period · engagement counted to date"
                font.pixelSize: 16
                color: root.secondaryInk
            }
            Item { width: Math.max(1, parent.width - 770); height: 1 }
            Text {
                text: root.capturedAt > 0
                    ? Qt.formatDateTime(new Date(root.capturedAt), "MMM d, yyyy")
                    : ""
                font.pixelSize: 16
                color: root.secondaryInk
            }
        }
    }
}
