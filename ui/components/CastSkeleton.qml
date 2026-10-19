import QtQuick
import QtQuick.Layouts

// Height-bounded, text-shaped placeholder. Every bar declares Layout sizes:
// Qt layouts do not use a child item's plain width/height as its preferred
// size, which previously collapsed the bars into blank white rectangles.
Item {
    id: root
    property bool mediaPreview: false
    implicitHeight: mediaPreview ? 222 : 142

    SequentialAnimation on opacity {
        running: root.visible && !Theme.reduceMotion
        loops: Animation.Infinite
        NumberAnimation { to: 0.72; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { to: 1.0; duration: 900; easing.type: Easing.InOutSine }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space3
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Superellipse {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            curveDepth: 19
            exponent: 2.01
            fillColor: Theme.surfaceMuted
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                Superellipse {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 11
                    fillColor: Theme.surfaceMuted
                    curveDepth: 5
                }
                Superellipse {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 9
                    fillColor: Theme.surfaceMuted
                    curveDepth: 5
                }
                Item { Layout.fillWidth: true }
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: 12
                fillColor: Theme.surfaceMuted
                curveDepth: 6
            }
            Superellipse {
                Layout.preferredWidth: Math.max(120, Math.min(260, parent.width * 0.66))
                Layout.preferredHeight: 12
                fillColor: Theme.surfaceMuted
                curveDepth: 6
            }

            Superellipse {
                visible: root.mediaPreview
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 94 : 0
                fillColor: Theme.surfaceMuted
                curveDepth: Theme.shapeMedia
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection
                Repeater {
                    model: 3
                    delegate: Superellipse {
                        Layout.preferredWidth: 35
                        Layout.preferredHeight: 8
                        fillColor: Theme.surfaceMuted
                        curveDepth: 4
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
