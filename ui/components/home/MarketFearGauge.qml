import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Gauge uses actual Fear & Greed index (0..100), never synthesized samples.
Item {
    id: root
    property real index: -1
    property string classification: ""
    readonly property bool available: Number.isFinite(index) && index >= 0 && index <= 100
    readonly property real position: available ? Math.max(0, Math.min(1, index / 100)) : 0

    Layout.fillWidth: true
    Layout.preferredHeight: 132
    implicitHeight: 132
    Accessible.role: Accessible.Indicator
    Accessible.name: qsTrId("genycaster.market.fear") + ": "
                    + (available ? LocaleFormat.formatInteger(Math.round(index)) + "/" + LocaleFormat.formatInteger(100)
                                 + " " + classification : "—")

    Item {
        id: dial
        width: Math.min(196, Math.max(80, root.width - 20))
        height: width
        anchors.horizontalCenter: parent.horizontalCenter
        y: 2

        // Five uniform segments, from extreme fear (red) to greed (green).
        // Native Qt vector paths retain crisp edges under device scaling.
        Repeater {
            model: [
                "#E64F53", "#F58A2D", "#F9CA3A", "#8BCD34", "#26B98B"
            ]
            delegate: VectorStroke {
                required property int index
                required property string modelData
                anchors.fill: parent
                pathKind: VectorStroke.Arc
                strokeWidth: 9
                inset: 8
                roundCaps: true
                startAngle: 180 + index * 36 + 4
                sweepAngle: 28
                strokeColor: modelData
            }
        }

        Rectangle {
            visible: root.available
            readonly property real radiusValue: dial.width / 2 - 12.5
            readonly property real degrees: (180 + root.position * 180) * Math.PI / 180
            width: 13
            height: 13
            radius: width / 2
            color: Theme.textPrimary
            border.color: Theme.surface
            border.width: 3
            x: dial.width / 2 + Math.cos(degrees) * radiusValue - width / 2
            // Qt PathAngleArc traverses 180→360 above the center in screen coordinates.
            // Match that path, rather than mirroring the indicator below the gauge.
            y: dial.height / 2 + Math.sin(degrees) * radiusValue - height / 2
            Behavior on x { enabled: !Theme.reduceMotion; NumberAnimation { duration: 360; easing.type: Easing.OutCubic } }
            Behavior on y { enabled: !Theme.reduceMotion; NumberAnimation { duration: 360; easing.type: Easing.OutCubic } }
        }
    }

    ColumnLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 65
        spacing: 1
        DirectionAwareLabel {
            Layout.alignment: Qt.AlignHCenter
            text: root.available ? LocaleFormat.formatInteger(Math.round(root.index)) : "—"
            forceLeftToRight: true
            color: Theme.textPrimary
            font.pixelSize: Theme.typeTitle1Size + 4
            font.weight: Theme.weightBold
        }
        DirectionAwareLabel {
            Layout.alignment: Qt.AlignHCenter
            text: root.available ? root.classification : qsTrId("genycaster.market.unavailable")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
        }
    }
}
