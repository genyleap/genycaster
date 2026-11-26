import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Farcaster.Client

Superellipse {
    id: root

    property var embed: ({})
    property bool activated: false
    MediaResourceScope {
        id: resourceScope
        owner: root
        onEligibleChanged: if (!eligible) root.activated = false
    }
    function coordinator() {
        return typeof mediaPlayback !== "undefined" && mediaPlayback !== null ? mediaPlayback : null
    }
    function suspendFromPlaybackCoordinator() {
        root.activated = false
    }
    onActivatedChanged: {
        const session = root.coordinator()
        if (!activated && session) session.release(root)
    }
    Component.onDestruction: {
        const session = root.coordinator()
        if (session) session.release(root)
    }
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
    readonly property var activePlayer: playbackLoader.item ? playbackLoader.item.player : null
    readonly property real progress: root.activePlayer && root.activePlayer.duration > 0
        ? Math.max(0, Math.min(1, root.activePlayer.position / root.activePlayer.duration)) : 0
    // Recycled/offscreen delegates must not keep decoder or audio-device work.
    onTargetUrlChanged: root.activated = false
    onVisibleChanged: {
        if (!root.visible)
            root.activated = false
    }

    signal openRequested(string url)

    function togglePlayback() {
        if (!resourceScope.eligible) return
        if (!root.localPlaybackAvailable) {
            root.openRequested(root.targetUrl)
            return
        }
        if (!root.activated) {
            // Loading the player creates a platform audio device: only do it
            // after explicit Play, never while creating/recycling feed rows.
            root.activated = true
            return
        }
        const player = root.activePlayer
        if (!player)
            return
        if (player.playbackState === MediaPlayer.PlayingState)
            player.pause()
        else {
            const session = root.coordinator()
            if (session) session.claimPrimary(root)
            player.play()
        }
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

    Loader {
        id: playbackLoader
        objectName: "audioPreviewPlaybackLoader"
        active: root.activated && root.localPlaybackAvailable && resourceScope.eligible
                && (typeof nativeAudioOutputEnabled === "undefined" || nativeAudioOutputEnabled)
        asynchronous: true
        onLoaded: {
            if (root.activated && resourceScope.eligible && item && item.player) {
                const session = root.coordinator()
                if (session) session.claimPrimary(root)
                item.player.play()
            }
        }
        sourceComponent: Item {
            property alias player: playback
            AudioOutput { id: audioOutput }
            MediaPlayer {
                id: playback
                source: root.targetUrl
                audioOutput: audioOutput
            }
        }
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
            iconGlyph: root.localPlaybackAvailable && root.activePlayer
                       && root.activePlayer.playbackState === MediaPlayer.PlayingState
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
                    enabled: root.activePlayer && root.activePlayer.duration > 0
                    onTapped: eventPoint => {
                        if (root.activePlayer)
                            root.activePlayer.position = Math.round(
                                (eventPoint.position.x / width) * root.activePlayer.duration)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Qt.LeftToRight

                Label {
                    text: root.durationLabel(root.activePlayer ? root.activePlayer.position : 0)
                    color: Theme.textTertiary
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeCaptionSize
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.durationLabel(root.activePlayer ? root.activePlayer.duration : 0)
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
