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
                    Superellipse {
                        Layout.preferredWidth: parent.width * 0.44
                        Layout.preferredHeight: 12
                        curveDepth: 6
                        fillColor: Theme.surfaceMuted
                    }
                    Superellipse {
                        Layout.preferredWidth: parent.width * 0.68
                        Layout.preferredHeight: 28
                        curveDepth: 7
                        fillColor: Theme.surfaceMuted
                    }
                    Superellipse {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 9
                        curveDepth: 5
                        fillColor: Theme.surfaceMuted
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
            Superellipse {
                Layout.preferredWidth: parent.width * 0.35
                Layout.preferredHeight: 14
                curveDepth: 6
                fillColor: Theme.surfaceMuted
            }
            Repeater {
                model: 3
                delegate: Superellipse {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 9
                    curveDepth: 4
                    fillColor: Theme.surfaceMuted
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
            Superellipse {
                Layout.preferredWidth: parent.width * 0.32
                Layout.preferredHeight: 14
                curveDepth: 6
                fillColor: Theme.surfaceMuted
            }
            Item { Layout.fillWidth: true; Layout.fillHeight: true }
            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                fillColor: Theme.surfaceMuted
            }
        }
    }
}
