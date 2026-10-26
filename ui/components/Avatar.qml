import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string sourceUrl: ""
    property string displayName: ""
    property int avatarSize: 44
    // UI-critical channel artwork must not be suppressed by the optional
    // feed-media automatic-download preference.
    property bool forceAutomaticDownload: false
    property real avatarCurveDepth: Math.round(avatarSize * 0.30)
    property real avatarExponent: 3.8

    readonly property string initial: displayName.length > 0
                                      ? displayName.substring(0, 1).toUpperCase()
                                      : "?"
    // Search results and some Mini App/profile projections contain an HTTPS
    // PFP rather than the image-provider URL already used by feed profiles.
    // Normalize through the existing remote image provider (with its cache,
    // sizing, and automatic-download policy) instead of discarding the URL.
    readonly property string resolvedSource: sourceUrl.startsWith("image://")
                                          || sourceUrl.startsWith("qrc:/")
                                          || sourceUrl.startsWith("file:/")
                                          ? sourceUrl
                                          : sourceUrl.length > 0
                                            && typeof linkPreviewResolver !== "undefined"
                                            && linkPreviewResolver !== null
                                            ? linkPreviewResolver.imageSourceForUrl(sourceUrl)
                                            : ""

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
        source: root.resolvedSource
        mediaCategory: "avatars"
        forceAutomaticDownload: root.forceAutomaticDownload
        requestedSourceWidth: Math.max(1, root.avatarSize)
        requestedSourceHeight: Math.max(1, root.avatarSize)
        fillMode: Image.PreserveAspectCrop
        curveDepth: root.avatarCurveDepth
        exponent: root.avatarExponent
        fillColor: "transparent"
        borderWidth: 0
        showLoadingIndicator: false
        showFailureIndicator: false
    }
}
