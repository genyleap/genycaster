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

    // The thread stem is a softly terminated capsule, not a sharp rule.
    // Dim nested connectors so the Cast content remains the focal point.
    Superellipse {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.compact ? 12 : 15
        anchors.bottomMargin: Theme.space1
        width: root.highlighted ? 3 : 2
        curveDepth: width / 2
        exponent: 2.01
        fillColor: root.depth <= 1 ? Theme.accent : Theme.borderStrong
        borderWidth: 0
        opacity: root.highlighted ? 0.7 : 0.32
    }

    Superellipse {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.compact ? 9 : 11
        width: root.compact ? 11 : 12
        height: width
        fillColor: root.highlighted ? Theme.surfaceSelected : Theme.surfaceRaised
        borderWidth: root.highlighted ? 2 : 1.5
        borderColor: root.highlighted ? Theme.accent : Theme.borderStrong
        curveDepth: width / 2
        exponent: 2.01
    }
}
