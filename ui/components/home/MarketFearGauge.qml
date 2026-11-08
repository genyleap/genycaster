import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Farcaster.Client

// A single analytical QSG Shape keeps all five arcs in one curve-rendered
// geometry. Mirror only the visual scale (red = zero, green = 100); do not
// mirror translated text or swap the meaning of the sentiment value.
Item {
    id: root
    property real index: -1
    property string classification: ""
    readonly property bool available: Number.isFinite(index) && index >= 0 && index <= 100
    readonly property real progress: available ? Math.max(0, Math.min(1, index / 100)) : 0
    readonly property real stroke: 9
    readonly property real inset: 8

    Layout.fillWidth: true
    Layout.preferredHeight: 138
    implicitHeight: 138
    Accessible.role: Accessible.Indicator
    Accessible.name: qsTrId("genycaster.market.fear") + ": "
                     + (available ? LocaleFormat.formatInteger(Math.round(index))
                                   + "/" + LocaleFormat.formatInteger(100)
                                   + " " + classification : "—")

    Item {
        id: dial
        width: Math.max(80, Math.min(218, root.width - 16, (root.height - 18) * 2))
        height: width
        anchors.horizontalCenter: parent.horizontalCenter
        y: 1
        readonly property real arcRadius: width / 2 - root.inset - root.stroke / 2

        Item {
            anchors.fill: parent
            // RTL is an actual reflected semicircle, including the marker.
            transform: Scale {
                origin.x: dial.width / 2
                xScale: Theme.rtl ? -1 : 1
            }

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                asynchronous: false
                antialiasing: true

                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#E64F53"
                    strokeWidth: root.stroke
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathAngleArc { centerX: dial.width/2; centerY: dial.height/2; radiusX: dial.arcRadius; radiusY: dial.arcRadius; startAngle: 184; sweepAngle: 28 }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#F58A2D"
                    strokeWidth: root.stroke
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathAngleArc { centerX: dial.width/2; centerY: dial.height/2; radiusX: dial.arcRadius; radiusY: dial.arcRadius; startAngle: 220; sweepAngle: 28 }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#F9CA3A"
                    strokeWidth: root.stroke
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathAngleArc { centerX: dial.width/2; centerY: dial.height/2; radiusX: dial.arcRadius; radiusY: dial.arcRadius; startAngle: 256; sweepAngle: 28 }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#8BCD34"
                    strokeWidth: root.stroke
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathAngleArc { centerX: dial.width/2; centerY: dial.height/2; radiusX: dial.arcRadius; radiusY: dial.arcRadius; startAngle: 292; sweepAngle: 28 }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#26B98B"
                    strokeWidth: root.stroke
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathAngleArc { centerX: dial.width/2; centerY: dial.height/2; radiusX: dial.arcRadius; radiusY: dial.arcRadius; startAngle: 328; sweepAngle: 28 }
                }
            }

            Rectangle {
                visible: root.available
                readonly property real angle: (180 + root.progress * 180) * Math.PI / 180
                width: 12
                height: width
                radius: width / 2
                color: Theme.textPrimary
                border.color: Theme.surface
                border.width: 2.5
                antialiasing: true
                x: dial.width/2 + Math.cos(angle)*dial.arcRadius - width/2
                y: dial.height/2 + Math.sin(angle)*dial.arcRadius - height/2
                Behavior on x {
                    enabled: !Theme.reduceMotion
                    NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
                }
                Behavior on y {
                    enabled: !Theme.reduceMotion
                    NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
                }
            }
        }
    }

    ColumnLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        y: dial.y + dial.height/2 - 24
        spacing: 2
        DirectionAwareLabel {
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: root.available ? LocaleFormat.formatInteger(Math.round(root.index)) : "—"
            font.pixelSize: Theme.typeTitle1Size + 4
            font.weight: Theme.weightBold
            color: Theme.textPrimary
        }
        DirectionAwareLabel {
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: root.available ? root.classification : qsTrId("genycaster.market.unavailable")
            color: Theme.textSecondary
            font.pixelSize: Theme.typeCaptionSize
        }
    }
}
