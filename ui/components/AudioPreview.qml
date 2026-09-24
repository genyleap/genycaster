import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Superellipse {
    id: root

    property var embed: ({})
    property bool activated: false
    readonly property string targetUrl: String(root.embed.url || root.embed.value || "")
    readonly property string fileName: {
        const provided = String(root.embed.fileName || "")
        if (provided.length > 0)
            return provided
        const clean = root.targetUrl.split("?")[0].split("#")[0]
        const pieces = clean.split("/")
        const leaf = pieces.length > 0 ? pieces[pieces.length - 1] : ""
        return leaf.length > 0 ? decodeURIComponent(leaf) : String(root.embed.host || root.embed.domain || root.targetUrl)
    }
    readonly property bool localPlaybackAvailable: root.targetUrl.startsWith("file:") || root.targetUrl.startsWith("qrc:")
    readonly property real progress: player.duration > 0 ? Math.max(0, Math.min(1, player.position / player.duration)) : 0

    signal openRequested(string url)

    function togglePlayback() {
        if (!root.activated) {
            root.activated = true
            Qt.callLater(function() { player.play() })
            return
        }
        if (player.playbackState === MediaPlayer.PlayingState)
            player.pause()
        else
            player.play()
    }

    function durationLabel(milliseconds) {
        const seconds = Math.max(0, Math.floor(Number(milliseconds || 0) / 1000))
        const minutes = Math.floor(seconds / 60)
        const remainder = seconds % 60
        const localizedZero = LocaleFormat.formatInteger(0)
        const localizedRemainder = LocaleFormat.formatInteger(remainder)
        const paddedRemainder = remainder < 10 ? localizedZero + localizedRemainder : localizedRemainder
        return LocaleFormat.formatInteger(minutes) + ":" + paddedRemainder
    }

    implicitHeight: 84
    fillColor: Theme.surfaceSecondary
    borderWidth: 1
    borderColor: Theme.borderSubtle
    curveDepth: Theme.shapeSurface

    AudioOutput { id: audioOutput }
    MediaPlayer {
        id: player
        source: root.activated && root.localPlaybackAvailable ? root.targetUrl : ""
        audioOutput: audioOutput
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.space3
        layoutDirection: Theme.uiDirection
        spacing: Theme.space3

        ActionButton {
            compact: true
            iconOnly: true
            quiet: false
            iconGlyph: root.localPlaybackAvailable && player.playbackState === MediaPlayer.PlayingState
                       ? Theme.iconPause : Theme.iconPlay
            text: root.localPlaybackAvailable ? qsTrId("genycaster.embed.play_video")
                                              : qsTrId("genycaster.embed.open_link")
            onClicked: root.localPlaybackAvailable ? root.togglePlayback() : root.openRequested(root.targetUrl)
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.fileName
                color: Theme.textPrimary
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideMiddle
            }

            Rectangle {
                Layout.fillWidth: true
                height: 3
                color: Theme.borderMuted

                Rectangle {
                    width: parent.width * root.progress
                    height: parent.height
                    color: Theme.accent
                }

                TapHandler {
                    enabled: root.activated && player.duration > 0
                    onTapped: eventPoint => player.position = Math.round((eventPoint.position.x / width) * player.duration)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Qt.LeftToRight

                Label {
                    text: root.durationLabel(player.position)
                    color: Theme.textTertiary
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeCaptionSize
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.durationLabel(player.duration)
                    color: Theme.textTertiary
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
        }

        ActionButton {
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconArrowUpRight
            text: qsTrId("genycaster.embed.open_link")
            onClicked: root.openRequested(root.targetUrl)
        }
    }
}
