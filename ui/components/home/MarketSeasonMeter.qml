import QtQuick
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root
    property real index: -1
    readonly property bool available: Number.isFinite(index) && index >= 0 && index <= 100
    readonly property real progress: available ? Math.min(1, Math.max(0, index / 100)) : 0
    implicitHeight: 54
    Layout.preferredHeight: implicitHeight
    Layout.fillWidth: true

    Accessible.role: Accessible.Indicator
    Accessible.name: qsTrId("genycaster.market.season") + ": "
                    + (available ? LocaleFormat.formatInteger(Math.round(index)) + "/" + LocaleFormat.formatInteger(100) : "—")

    RowLayout {
        layoutDirection: Theme.uiDirection
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.market.bitcoin_season")
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
        }
        DirectionAwareLabel {
            text: qsTrId("genycaster.market.altcoin_season")
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
        }
    }

    Item {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 29
        height: 9

        Row {
            anchors.fill: parent
            spacing: 3
            Repeater {
                model: ["#EF9022", "#F8C58E", "#CCD3E2", "#91A5F4", Theme.accent]
                delegate: Rectangle {
                    required property var modelData
                    width: Math.max(0, (track.width - 12) / 5)
                    height: track.height
                    radius: 4
                    color: modelData
                }
            }
        }

        Rectangle {
            visible: root.available
            width: 17
            height: 17
            radius: 8.5
            y: (track.height - height) / 2
            x: Math.max(0, Math.min(track.width - width, track.width * root.progress - width / 2))
            color: Theme.textPrimary
            border.color: Theme.surface
            border.width: 4
            Behavior on x {
                enabled: !Theme.reduceMotion
                NumberAnimation { duration: 390; easing.type: Easing.OutCubic }
            }
        }
    }
}
