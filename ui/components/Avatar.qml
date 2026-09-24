import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string sourceUrl: ""
    property string displayName: ""
    property int avatarSize: 44
    property real avatarCurveDepth: Math.round(avatarSize * 0.30)
    property real avatarExponent: 3.8

    readonly property string initial: displayName.length > 0
                                      ? displayName.substring(0, 1).toUpperCase()
                                      : "?"
    readonly property bool hasLocalImageSource: sourceUrl.startsWith("image://")
                                                 || sourceUrl.startsWith("qrc:/")

    implicitWidth: avatarSize
    implicitHeight: avatarSize

    Superellipse {
        anchors.fill: parent
        curveDepth: root.avatarCurveDepth
        exponent: root.avatarExponent
        fillColor: Theme.primarySurface

        Label {
            anchors.centerIn: parent
            text: root.initial
            color: Theme.accent
            font.pixelSize: Math.max(13, Math.round(root.avatarSize * 0.34))
            font.weight: Theme.weightDemiBold
        }
    }

    SuperellipseImage {
        id: avatarImage
        anchors.fill: parent
        source: root.hasLocalImageSource ? root.sourceUrl : ""
        mediaCategory: "avatars"
        requestedSourceWidth: Math.max(1, root.avatarSize)
        requestedSourceHeight: Math.max(1, root.avatarSize)
        fillMode: Image.PreserveAspectCrop
        curveDepth: root.avatarCurveDepth
        exponent: root.avatarExponent
        fillColor: "transparent"
        borderWidth: 1
        borderColor: Theme.borderMuted
        showLoadingIndicator: false
        showFailureIndicator: false
    }
}
