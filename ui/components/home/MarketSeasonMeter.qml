import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// The track, endpoints and index marker share one logical axis.
// RTL is a true mirror: Bitcoin (0) is at the right; altcoins (100) at the left.
Item {
    id: root
    property real index: -1
    readonly property bool available: Number.isFinite(index) && index >= 0 && index <= 100
    readonly property real progress: available ? Math.max(0, Math.min(1, index / 100)) : 0
    implicitHeight: 68
    Layout.preferredHeight: implicitHeight
    Layout.fillWidth: true

    Accessible.role: Accessible.Indicator
    Accessible.name: qsTrId("genycaster.market.season") + ": "
                     + (available ? LocaleFormat.formatInteger(Math.round(index))
                                   + "/" + LocaleFormat.formatInteger(100) : "—")

    RowLayout {
        id: endpoints
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        layoutDirection: Theme.uiDirection
        spacing: Theme.space2

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            alignToUi: true
            text: qsTrId("genycaster.market.bitcoin_season")
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            horizontalAlignment: Theme.textEndAlignment
            text: qsTrId("genycaster.market.altcoin_season")
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
    }

    Item {
        id: track
        anchors.top: endpoints.bottom
        anchors.topMargin: Theme.space3
        anchors.left: parent.left
        anchors.right: parent.right
        height: 10
        clip: false

        Row {
            anchors.fill: parent
            layoutDirection: Theme.uiDirection
            spacing: 4
            Repeater {
                model: [Theme.marketSeasonWeak, Theme.marketSeasonCooling, Theme.marketSeasonNeutral, Theme.marketSeasonWarming, Theme.accent]
                delegate: Superellipse {
                    required property var modelData
                    width: Math.max(0, (track.width - 16) / 5)
                    height: track.height
                    curveDepth: height / 2
                    exponent: 2
                    fillColor: modelData
                }
            }
        }

        Superellipse {
            visible: root.available
            width: 17
            height: width
            curveDepth: width / 2
            exponent: 2
            y: (track.height - height) / 2
            readonly property real logicalFraction: Theme.rtl ? 1-root.progress : root.progress
            x: Math.max(0, Math.min(track.width - width,
                   track.width * logicalFraction - width/2))
            fillColor: Theme.textPrimary
            borderColor: Theme.surface
            borderWidth: 3
            Behavior on x {
                enabled: !Theme.reduceMotion
                NumberAnimation { duration: 380; easing.type: Easing.OutCubic }
            }
        }
    }
}
