import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Low-cost static placeholders with a single reduced-motion-aware opacity pulse.
ColumnLayout {
    id: root
    objectName: "statsLoadingSkeleton"
    spacing: Theme.space3
    opacity: 1

    SequentialAnimation on opacity {
        running: root.visible && !Theme.reduceMotion
        loops: Animation.Infinite
        NumberAnimation { from: 1; to: 0.72; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { from: 0.72; to: 1; duration: 900; easing.type: Easing.InOutSine }
    }

    GridLayout {
        layoutDirection: Theme.uiDirection
        Layout.fillWidth: true
        columns: root.width < 600 ? 2 : 4
        columnSpacing: Theme.space2
        rowSpacing: Theme.space2
        Repeater {
            model: 4
            delegate: Superellipse {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: 104
                fillColor: Theme.surface
                borderColor: Theme.borderMuted
                borderWidth: 1
                curveDepth: Theme.curveContainer
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space3
                    Rectangle {
                        Layout.preferredWidth: parent.width * 0.44
                        Layout.preferredHeight: 12
                        radius: 6
                        color: Theme.surfaceMuted
                    }
                    Rectangle {
                        Layout.preferredWidth: parent.width * 0.68
                        Layout.preferredHeight: 28
                        radius: 7
                        color: Theme.surfaceMuted
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 9
                        radius: 5
                        color: Theme.surfaceMuted
                    }
                }
            }
        }
    }

    Superellipse {
        Layout.fillWidth: true
        Layout.preferredHeight: 146
        fillColor: Theme.surface
        borderColor: Theme.borderMuted
        borderWidth: 1
        curveDepth: Theme.curveContainer
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space4
            spacing: Theme.space3
            Rectangle {
                Layout.preferredWidth: parent.width * 0.35
                Layout.preferredHeight: 14
                radius: 6
                color: Theme.surfaceMuted
            }
            Repeater {
                model: 3
                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 9
                    radius: 4
                    color: Theme.surfaceMuted
                }
            }
        }
    }

    Superellipse {
        Layout.fillWidth: true
        Layout.preferredHeight: 212
        fillColor: Theme.surface
        borderColor: Theme.borderMuted
        borderWidth: 1
        curveDepth: Theme.curveContainer
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space4
            Rectangle {
                Layout.preferredWidth: parent.width * 0.32
                Layout.preferredHeight: 14
                radius: 6
                color: Theme.surfaceMuted
            }
            Item { Layout.fillWidth: true; Layout.fillHeight: true }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.surfaceMuted
            }
        }
    }
}
