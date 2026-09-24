import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property bool mediaPreview: false
    implicitHeight: mediaPreview ? 318 : 164

    SequentialAnimation on opacity {
        running: root.visible && !Theme.reduceMotion
        loops: Animation.Infinite
        NumberAnimation { to: 0.56; duration: 720; easing.type: Easing.InOutSine }
        NumberAnimation { to: 1.0; duration: 720; easing.type: Easing.InOutSine }
    }

    RowLayout {
        anchors.left: parent.left
        layoutDirection: Theme.uiDirection
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space2
        spacing: Theme.space2

        Superellipse {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: Theme.space1
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            curveDepth: 21
            exponent: 2.01
            fillColor: Theme.surfaceMuted
        }

        Superellipse {
            Layout.fillWidth: true
            implicitHeight: skeletonContent.implicitHeight + Theme.space3 * 2
            fillColor: Theme.surface
            borderWidth: 1
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveContainer

            ColumnLayout {
                id: skeletonContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space3
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection
                    Superellipse { width: 116; height: 11; curveDepth: 5; fillColor: Theme.surfaceMuted }
                    Superellipse { width: 72; height: 9; curveDepth: 4; fillColor: Theme.surfaceMuted }
                    Item { Layout.fillWidth: true }
                }

                Superellipse { Layout.fillWidth: true; height: 11; curveDepth: 5; fillColor: Theme.surfaceMuted }
                Superellipse { Layout.preferredWidth: Math.max(140, parent.width * 0.72); height: 11; curveDepth: 5; fillColor: Theme.surfaceMuted }

                Superellipse {
                    visible: root.mediaPreview
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 142 : 0
                    curveDepth: Theme.shapeMedia
                    fillColor: Theme.surfaceMuted
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space4
                    layoutDirection: Theme.uiDirection
                    Superellipse { width: 42; height: 9; curveDepth: 4; fillColor: Theme.surfaceMuted }
                    Superellipse { width: 42; height: 9; curveDepth: 4; fillColor: Theme.surfaceMuted }
                    Superellipse { width: 42; height: 9; curveDepth: 4; fillColor: Theme.surfaceMuted }
                    Item { Layout.fillWidth: true }
                    Superellipse { width: 54; height: 8; curveDepth: 4; fillColor: Theme.surfaceMuted }
                }
            }
        }
    }
}
