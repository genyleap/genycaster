import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Shared eight-stage indicator for the welcome page and desktop stage header.
// Each marker has an identical vertical center, regardless of pill/dot height.
Item {
    id: root
    property int stepCount: 8
    property int currentStep: 0
    property int markerSpacing: 7
    readonly property int safeCount: Math.max(1, stepCount)
    readonly property int activeIndex: Math.max(0, Math.min(safeCount - 1, currentStep))

    implicitWidth: markers.implicitWidth
    implicitHeight: 16

    Row {
        id: markers
        anchors.centerIn: parent
        spacing: root.markerSpacing

        Repeater {
            model: root.safeCount
            delegate: Item {
                id: marker
                required property int index
                readonly property bool active: index === root.activeIndex
                readonly property bool completed: index < root.activeIndex
                width: active ? 26 : 12
                height: 16

                Superellipse {
                    anchors.centerIn: parent
                    width: marker.active ? 26 : 7
                    height: marker.active ? 5 : 7
                    curveDepth: height / 2
                    fillColor: marker.active ? Theme.accent
                           : marker.completed ? Theme.inkMuted : Theme.borderMuted

                    Behavior on width {
                        NumberAnimation {
                            duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on height {
                        NumberAnimation {
                            duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on fillColor {
                        ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionStandard }
                    }
                }
            }
        }
    }
}
