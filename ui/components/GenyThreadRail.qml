import QtQuick

// Shared conversation connector: a thin, theme-aware line and a
// Superellipse node that connects nested replies without fake hierarchy.
Item {
    id: root
    property bool compact: false
    property bool highlighted: false
    property int depth: 1

    implicitWidth: compact ? 18 : 24
    implicitHeight: compact ? 108 : 126

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.compact ? 16 : 19
        anchors.bottomMargin: -Theme.space1
        width: root.highlighted ? 3 : 2
        radius: width / 2
        color: root.depth <= 1 ? Theme.accent : Theme.borderStrong
        opacity: root.highlighted ? 0.9 : 0.55
    }

    Superellipse {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.compact ? 10 : 12
        width: root.compact ? 12 : 14
        height: width
        fillColor: root.highlighted ? Theme.accent : Theme.surfaceRaised
        borderWidth: 2
        borderColor: Theme.accent
        curveDepth: 5
    }
}
