import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Attribution is evidentiary: no client badge if the selected provider did
// not report a human-readable client name. Never infer from profile or node.
Label {
    id: badge
    property string clientName: ""
    readonly property string sanitizedName: String(clientName || "").replace(/[\\u0000-\\u001f\\u007f]/g, "").trim().slice(0, 48)
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
    background: Rectangle {
        radius: height / 2
        color: Theme.surfaceMuted
        border.width: Theme.strokeHairline
        border.color: Theme.borderMuted
    }
}
