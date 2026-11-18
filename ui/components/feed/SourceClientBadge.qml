import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Attribution is evidentiary: no client badge if the selected provider did
// not report a human-readable client name. Never infer from profile or node.
Label {
    id: badge
    property string clientName: ""
    readonly property string sanitizedName: String(clientName || "").replace(/[\x00-\x1f\x7f]/g, "").trim().slice(0, 48)
    visible: sanitizedName.length > 0
    text: sanitizedName
    Accessible.name: qsTrId("genycaster.cast.source_client_accessible").arg(sanitizedName)
    font.family: Theme.fontFamily
    font.pixelSize: Math.max(10, Theme.typeCaptionSize - 1)
    color: Theme.inkMuted
    elide: Text.ElideRight
    Layout.maximumWidth: 104
    leftPadding: Theme.space2
    rightPadding: Theme.space2
    topPadding: 2
    bottomPadding: 2
    background: Superellipse {
        curveDepth: height / 2
        fillColor: Theme.surfaceMuted
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
    }
}
