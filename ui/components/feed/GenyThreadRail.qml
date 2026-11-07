import QtQuick
import Farcaster.Client

// Shared conversation connector: a thin, theme-aware line and a
// Superellipse node that connects nested replies without fake hierarchy.
Item {
    id: root
    property bool compact: false
    property bool highlighted: false
    property int depth: 1

    implicitWidth: compact ? 17 : 22
    implicitHeight: compact ? 108 : 126

    // The thread stem is a softly terminated capsule, not a sharp rule.
    // Dim nested connectors so the Cast content remains the focal point.
    Superellipse {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.compact ? 15 : 17
        anchors.bottomMargin: Theme.space1
        width: root.highlighted ? 3 : 2
        curveDepth: width / 2
        exponent: 2.01
        fillColor: root.highlighted ? Theme.accent : Theme.borderStrong
        borderWidth: 0
        opacity: root.highlighted ? 0.8 : 0.48
    }

    Superellipse {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.compact ? 11 : 13
        width: root.compact ? 8 : 9
        height: width
        fillColor: root.highlighted ? Theme.accent : Theme.borderStrong
        borderWidth: 0
        borderColor: "transparent"
        curveDepth: width / 2
        exponent: 2.01
    }
}
