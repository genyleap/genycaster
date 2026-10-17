import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import QtMultimedia

Item {
    id: root

    component PlayerIconButton : Item {
        id: playerButton

        property string iconGlyph: ""
        property string accessibleText: ""
        property bool selected: false
        property bool filled: false
        property real iconSize: Theme.mediaPlayerControlIconSize
        signal clicked()

        implicitWidth: Theme.mediaPlayerControlSize
        implicitHeight: implicitWidth
        activeFocusOnTab: true
        scale: !Theme.reduceMotion && playerButtonTap.pressed ? 0.96 : 1

        Behavior on scale {
            NumberAnimation {
                duration: Theme.reduceMotion ? 0 : Theme.motionFast
                easing.type: Easing.OutCubic
            }
        }

        Superellipse {
            anchors.fill: parent
            fillColor: playerButton.filled
                ? (playerButtonHover.hovered ? Theme.mediaPlayerChromeHover : Theme.mediaPlayerChrome)
                : playerButton.selected
                ? Theme.primarySurfaceHover
                : (playerButtonHover.hovered ? Theme.mediaPlayerChromeHover : "transparent")
            borderWidth: Theme.keyboardFocusVisible(playerButton) ? Theme.strokeEmphasis : 0
            borderColor: Theme.accent
            curveDepth: width / 2

            Behavior on fillColor {
                ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
            }
        }

        Icon {
            anchors.centerIn: parent
            text: playerButton.iconGlyph
            color: playerButton.selected ? Theme.accent : Theme.mediaPlayerInk
            preferredSize: playerButton.iconSize
        }

        HoverHandler {
            id: playerButtonHover
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            id: playerButtonTap
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: {
                playerButton.forceActiveFocus()
                playerButton.clicked()
            }
        }

        Keys.onReturnPressed: event => {
            playerButton.clicked()
            event.accepted = true
        }
        Keys.onEnterPressed: event => {
            playerButton.clicked()
            event.accepted = true
        }
        Keys.onSpacePressed: event => {
            playerButton.clicked()
            event.accepted = true
        }

        Accessible.role: Accessible.Button
        Accessible.name: playerButton.accessibleText
        Accessible.onPressAction: playerButton.clicked()
    }

    // Shared visual grammar for the primary action, acknowledgement and loading.
    // The icon owns the centered square; captions never displace its center.
    component PlayerStateBadge : Superellipse {
        id: badge
        property alias iconGlyph: badgeIcon.text
        property alias caption: badgeLabel.text
        property bool busy: false
        property bool highlighted: false
        property bool focused: false
        property real maximumWidth: 260
        property real actionSize: Theme.mediaPlayerActionHeight
        property real captionSpace: 0
        implicitWidth: Math.min(maximumWidth, actionSize)
        implicitHeight: implicitWidth
        fillColor: highlighted ? Theme.mediaPlayerActionHover : Theme.mediaPlayerAction
        borderWidth: focused ? Theme.strokeEmphasis : 0
        borderColor: Theme.accent
        curveDepth: Theme.curveControl

        Behavior on fillColor {
            ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }

        Icon {
            id: badgeIcon
            anchors.centerIn: parent
            visible: !badge.busy
            color: Theme.mediaPlayerInk
            preferredSize: Math.min(32, badge.width * 0.5)
        }
        GenySpinner {
            anchors.centerIn: parent
            visible: badge.busy
            running: badge.busy && badge.visible
            Accessible.ignored: true
        }
        DirectionAwareLabel {
            id: badgeLabel
            anchors.top: parent.bottom
            anchors.topMargin: Theme.space2
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(badge.maximumWidth, 260)
            visible: badge.captionSpace >= implicitHeight + Theme.space2
            color: Theme.mediaPlayerInk
            font.pixelSize: Theme.typeLabelSize
            font.weight: Theme.weightDemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            style: Text.Outline
            styleColor: Theme.mediaPlayerBackground
            Accessible.ignored: true
        }
    }

    component SeekControl : Item {
        id: seekRoot

        property real position: 0
        property real buffered: 0
        property color fillColor: Theme.mediaViewerControl
        property color accentColor: Theme.accent
        property color handleColor: Theme.mediaViewerInk
        property bool seeking: false
        property real previewPosition: 0

        signal seekRequested(real fraction)

        readonly property real trackWidth: Math.max(1, width)
        property real dragBase: 0

        function clampFraction(value) {
            return Math.max(0, Math.min(1, value))
        }

        function updatePreview() {
            if (!seekRoot.seeking)
                return
            const delta = seekDrag.translation.x / seekRoot.trackWidth
            seekRoot.previewPosition = clampFraction(seekRoot.dragBase + delta)
        }

        function commitPreview() {
            if (!seekRoot.seeking)
                return
            seekRoot.seeking = false
            seekRoot.seekRequested(seekRoot.previewPosition)
        }

        implicitWidth: 120
        implicitHeight: 32
        activeFocusOnTab: enabled

        onPositionChanged: {
            if (!seekRoot.seeking)
                seekRoot.previewPosition = seekRoot.position
        }

        Superellipse {
            id: trackSurface
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: seekRoot.seeking || seekHover.hovered || seekRoot.activeFocus ? 6 : 3
            fillColor: seekRoot.fillColor
            borderWidth: 0
            curveDepth: height / 2
            Behavior on height { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
        }

        Superellipse {
            id: bufferedSurface
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: trackSurface.width * clampFraction(seekRoot.buffered)
            height: trackSurface.height
            fillColor: Qt.lighter(seekRoot.fillColor, 1.3)
            borderWidth: 0
            curveDepth: 2
            visible: seekRoot.buffered > 0.01
        }

        Superellipse {
            id: progressSurface
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: trackSurface.width * clampFraction(seekRoot.previewPosition)
            height: trackSurface.height
            fillColor: seekRoot.accentColor
            borderWidth: 0
            curveDepth: 2
        }

        Superellipse {
            id: handle
            width: seekRoot.seeking ? 14 : 10
            height: width
            anchors.verticalCenter: parent.verticalCenter
            x: trackSurface.x + progressSurface.width - width / 2
            fillColor: seekRoot.handleColor
            borderWidth: 0
            curveDepth: width / 2
            exponent: 2
            opacity: (seekRoot.seeking || seekHover.hovered || seekRoot.activeFocus) ? 1 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }

        HoverHandler { id: seekHover; cursorShape: Qt.PointingHandCursor }

        TapHandler {
            id: seekTap
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: eventPoint => {
                seekRoot.forceActiveFocus()
                const local = seekRoot.mapToItem(trackSurface, eventPoint.position)
                seekRoot.seekRequested(clampFraction(local.x / trackSurface.width))
            }
        }

        DragHandler {
            id: seekDrag
            target: null
            yAxis.enabled: false
            onActiveChanged: {
                if (seekDrag.active) {
                    seekRoot.forceActiveFocus()
                    seekRoot.dragBase = seekRoot.position
                    seekRoot.seeking = true
                    seekRoot.previewPosition = seekRoot.position
                } else {
                    seekRoot.commitPreview()
                }
            }
            onTranslationChanged: seekRoot.updatePreview()
        }

        Accessible.role: Accessible.Slider
        Accessible.name: qsTrId("genycaster.player.seek")
        Accessible.onIncreaseAction: seekRoot.seekRequested(clampFraction(seekRoot.position + 0.05))
        Accessible.onDecreaseAction: seekRoot.seekRequested(clampFraction(seekRoot.position - 0.05))
        Keys.onLeftPressed: event => { seekRoot.seekRequested(clampFraction(seekRoot.position - 0.05)); event.accepted = true }
        Keys.onRightPressed: event => { seekRoot.seekRequested(clampFraction(seekRoot.position + 0.05)); event.accepted = true }
        // Keys has no dedicated Home/End signals; handle them through pressed.
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Home) {
                seekRoot.seekRequested(0)
                event.accepted = true
            } else if (event.key === Qt.Key_End) {
                seekRoot.seekRequested(1)
                event.accepted = true
            } else {
                event.accepted = false
            }
        }
    }

    property url source: ""
    // Optional adaptive master manifest. When this is HLS, Qt Multimedia owns
    // bandwidth-driven rendition switching inside the backend.
    property url adaptiveSource: ""
    // Optional progressive rendition descriptors: { url/source, height, bitrate }.
    // They enable explicit quality selection and conservative client-side
    // down/up shifting when a service cannot provide an adaptive manifest.
    property var renditions: []
    property string contentType: ""
    property url posterSource: ""
    property string mediaLabel: ""
    property url openOriginalUrl: root.source
    property real curveDepth: Theme.shapeMedia
    property color cornerMaskColor: Theme.background
    property bool compact: false
    property bool interactive: true
    property bool showChrome: true
    property bool fullscreenEnabled: true
    property bool openOriginalEnabled: false
    property bool forceMuted: false
    property bool primeFrame: false
    // Eager consumers (composer/upload tiles) intentionally bind the source
    // before user activation so a first frame can prime. Feed surfaces keep
    // this false: scanning must never open decoder/network sessions.
    property bool eager: false
    property real presetAspectRatio: 16 / 9
    property real maximumImplicitHeight: 520

    // Public read-only state --------------------------------------------------
    readonly property var normalizedRenditions: root.normalizeRenditions(root.renditions)
    readonly property string adaptiveManifestUrl: root.resolveAdaptiveManifest()
    readonly property bool hasNativeAdaptive: root.adaptiveManifestUrl.length > 0
    readonly property bool hasSource: root.source.toString().length > 0
                                      || root.hasNativeAdaptive
                                      || root.normalizedRenditions.length > 0
    readonly property bool hasPoster: root.posterSource.toString().length > 0
    readonly property bool userActivated: activation.activated
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property bool paused: player.playbackState === MediaPlayer.PausedState
    readonly property bool ended: player.mediaStatus === MediaPlayer.EndOfMedia
                                  || (player.duration > 0 && player.position > 0
                                      && player.position >= player.duration - 120)
    // `BufferingMedia` means Qt already has enough buffered data for immediate
    // playback. Treat only an actual backend stall as blocking rebuffering; a
    // partially-filled buffer must never freeze the chrome or hide live video.
    readonly property bool buffering: player.mediaStatus === MediaPlayer.StalledMedia
    // Covers remote preflight and first-frame startup as well as a real backend
    // stall, allowing every embedding surface to show honest progress.
    readonly property bool loading: activation.starting || root.buffering
    readonly property bool failed: (activation.activated && !activation.recovering
                                    && player.error !== MediaPlayer.NoError)
                                   || activation.stalled
    readonly property bool fullscreen: root.fullscreenEnabled && fullscreenPopup.opened
    readonly property int playbackState: player.playbackState
    readonly property int mediaStatus: player.mediaStatus
    readonly property real position: player.position
    readonly property real duration: player.duration
    readonly property bool primed: activation.primed
    readonly property string preferredQuality: root.coordinator() !== null
                                               ? String(root.coordinator().preferredQuality || "auto")
                                               : "auto"
    readonly property var qualityOptions: root.buildQualityOptions()
    readonly property string activeQualityId: activation.qualityId
    readonly property bool automaticRenditionMode: root.preferredQuality === "auto"
                                                    && !root.hasNativeAdaptive
                                                    && root.normalizedRenditions.length > 1

    // Session semantics --------------------------------------------------------
    // The coordinator owns the one audible-session policy: when a video starts,
    // the previously active session suspends. Mute is a session-wide preference
    // so no Cast surface invents its own volume policy.
    readonly property bool sessionMuted: typeof mediaPlayback !== "undefined"
                                         && mediaPlayback !== null
                                         ? mediaPlayback.muted : false
    readonly property bool effectiveMuted: root.forceMuted || root.sessionMuted

    // Video controls keep one high-contrast dark language in both app themes.
    readonly property color chromeInk: Theme.mediaPlayerInk
    readonly property color chromeFill: Theme.mediaPlayerChrome
    readonly property color chromeBorder: Theme.mediaPlayerBorder
    readonly property color chromeHandle: Theme.mediaPlayerInk
    readonly property bool mediaLoaded: player.mediaStatus === MediaPlayer.LoadedMedia
                                        || player.mediaStatus === MediaPlayer.BufferedMedia
                                        || player.mediaStatus === MediaPlayer.EndOfMedia
    readonly property real playedFraction: player.duration > 0
                                           ? Math.max(0, Math.min(1, player.position / player.duration))
                                           : 0

    signal openOriginalRequested(string url)
    signal playbackFailed(string errorText)

    function timeLabel(milliseconds) {
        const seconds = Math.max(0, Math.floor(Number(milliseconds || 0) / 1000))
        const minutes = Math.floor(seconds / 60)
        const remainder = seconds % 60
        const localizedZero = LocaleFormat.formatInteger(0)
        const localizedRemainder = LocaleFormat.formatInteger(remainder)
        const paddedRemainder = remainder < 10 ? localizedZero + localizedRemainder : localizedRemainder
        return LocaleFormat.formatInteger(minutes) + ":" + paddedRemainder
    }

    function isAdaptiveUrl(value) {
        const text = String(value || "").toLowerCase()
        const clean = text.split("?")[0].split("#")[0]
        return clean.endsWith(".m3u8")
    }

    function contentTypeIsAdaptive() {
        return String(root.contentType || "").toLowerCase().indexOf("mpegurl") >= 0
    }

    function isProgressiveVideoUrl(value) {
        const text = String(value || "").toLowerCase()
        const clean = text.split("?")[0].split("#")[0]
        return clean.endsWith(".mp4") || clean.endsWith(".webm")
            || clean.endsWith(".mov") || clean.endsWith(".m4v")
            || clean.endsWith(".mkv") || String(root.contentType || "").toLowerCase().startsWith("video/")
    }

    function normalizeRenditions(values) {
        const input = Array.isArray(values) ? values : []
        const byHeight = ({})
        for (let index = 0; index < input.length; ++index) {
            const entry = input[index] || ({})
            const url = String(entry.url || entry.source || entry.src || "")
            const height = Math.max(0, parseInt(entry.height || entry.quality || entry.verticalResolution || 0, 10) || 0)
            if (url.length === 0 || height <= 0)
                continue
            byHeight[String(height)] = {
                url: url,
                height: height,
                bitrate: Math.max(0, Number(entry.bitrate || entry.bandwidth || 0))
            }
        }
        const result = []
        for (const key in byHeight)
            result.push(byHeight[key])
        result.sort((left, right) => left.height - right.height)
        return result
    }

    function resolveAdaptiveManifest() {
        const explicit = String(root.adaptiveSource || "")
        if (explicit.length > 0)
            return explicit
        const primary = String(root.source || "")
        return root.isAdaptiveUrl(primary) || root.contentTypeIsAdaptive() ? primary : ""
    }

    function qualityLabel(id) {
        if (id === "auto")
            return qsTrId("genycaster.player.quality_auto")
        if (id === "original")
            return qsTrId("genycaster.player.quality_original")
        const height = parseInt(String(id || ""), 10) || 0
        return height > 0 ? LocaleFormat.formatInteger(height) + "p" : String(id || "")
    }

    function buildQualityOptions() {
        const options = []
        if (root.hasNativeAdaptive || root.normalizedRenditions.length > 1)
            options.push({ id: "auto", label: root.qualityLabel("auto") })
        for (let index = root.normalizedRenditions.length - 1; index >= 0; --index) {
            const height = root.normalizedRenditions[index].height
            options.push({ id: String(height) + "p", label: root.qualityLabel(String(height) + "p") })
        }
        const primary = String(root.source || "")
        if (primary.length > 0 && !root.isAdaptiveUrl(primary))
            options.push({ id: "original", label: root.qualityLabel("original") })
        if (options.length === 0 && root.hasSource)
            options.push({ id: "original", label: root.qualityLabel("original") })
        return options
    }

    function initialAutoIndex() {
        const values = root.normalizedRenditions
        if (values.length === 0)
            return -1
        let selected = 0
        for (let index = 0; index < values.length; ++index) {
            if (values[index].height <= 720)
                selected = index
            else
                break
        }
        return selected
    }

    function descriptorForPreference(preference) {
        const requested = String(preference || "auto").toLowerCase()
        const values = root.normalizedRenditions
        if (requested === "auto") {
            if (root.hasNativeAdaptive)
                return { url: root.adaptiveManifestUrl, qualityId: "auto", autoIndex: -1 }
            if (values.length > 0) {
                if (activation.autoIndex < 0 || activation.autoIndex >= values.length)
                    activation.autoIndex = root.initialAutoIndex()
                const automatic = values[Math.max(0, activation.autoIndex)]
                return { url: automatic.url, qualityId: String(automatic.height) + "p", autoIndex: activation.autoIndex }
            }
        }
        if (requested !== "original") {
            const requestedHeight = parseInt(requested, 10) || 0
            for (let index = 0; index < values.length; ++index) {
                if (values[index].height === requestedHeight)
                    return { url: values[index].url, qualityId: requested, autoIndex: index }
            }
        }
        const primary = String(root.source || "")
        if (primary.length > 0)
            return { url: primary, qualityId: root.isAdaptiveUrl(primary) ? "auto" : "original", autoIndex: -1 }
        if (root.hasNativeAdaptive)
            return { url: root.adaptiveManifestUrl, qualityId: "auto", autoIndex: -1 }
        if (values.length > 0) {
            const fallback = values[values.length - 1]
            return { url: fallback.url, qualityId: String(fallback.height) + "p", autoIndex: values.length - 1 }
        }
        return { url: "", qualityId: "original", autoIndex: -1 }
    }

    function tryRecoverPlayback() {
        if (!root.userActivated || activation.recovering)
            return false

        // Progressive Auto can recover from a rendition-specific decoder/CDN
        // failure by dropping one real rendition before surfacing an error.
        if (root.automaticRenditionMode && activation.autoIndex > 0) {
            const nextIndex = activation.autoIndex - 1
            const entry = root.normalizedRenditions[nextIndex]
            activation.recovering = true
            activation.autoIndex = nextIndex
            root.queueSource({
                url: entry.url,
                qualityId: String(entry.height) + "p",
                autoIndex: nextIndex
            }, player.position, true, true)
            return true
        }

        // If an optional adaptive master fails but the embed also supplied a
        // real progressive file, fall back once. Never feed a Cloudflare watch
        // page or another generic web URL to FFmpeg as a pretend media file.
        const primary = String(root.source || "")
        if (!activation.fallbackAttempted && primary.length > 0
                && primary !== activation.playbackUrl
                && root.isProgressiveVideoUrl(primary)) {
            activation.fallbackAttempted = true
            activation.recovering = true
            root.queueSource({ url: primary, qualityId: "original", autoIndex: -1 },
                             player.position, true, true)
            return true
        }
        return false
    }

    function coordinator() {
        return typeof mediaPlayback !== "undefined" && mediaPlayback !== null
            ? mediaPlayback : null
    }

    function requestPrimaryPlayback() {
        const session = root.coordinator()
        if (session !== null)
            session.claimPrimary(root)
    }

    function releasePrimaryPlayback() {
        const session = root.coordinator()
        if (session !== null)
            session.release(root)
    }

    // Source management ---------------------------------------------------------
    // User-triggered source assignment is deferred by one frame. This lets the
    // pressed/loading state reach the scene graph before Qt Multimedia performs
    // backend/decoder setup, avoiding a synchronous-feeling tap stall.
    function queueSource(descriptor, positionMs, resumeAfterLoad, switching, forceReload) {
        const target = descriptor || ({ url: "", qualityId: "original", autoIndex: -1 })
        if (String(target.url || "").length === 0)
            return
        activation.pendingUrl = String(target.url)
        activation.pendingQualityId = String(target.qualityId || "original")
        activation.pendingPosition = Math.max(0, Number(positionMs || 0))
        activation.resumeAfterLoad = Boolean(resumeAfterLoad)
        activation.switching = Boolean(switching)
        activation.forceReload = Boolean(forceReload)
        activation.starting = true
        if (Number(target.autoIndex) >= 0)
            activation.autoIndex = Number(target.autoIndex)
        startupTimer.restart()
    }

    function prepareBackendForSource(target) {
        const session = root.coordinator()
        if (session !== null && typeof session.prepareForSource === "function")
            session.prepareForSource(player, String(target || ""))
    }

    function startPendingSource(target) {
        if (String(target || "").length === 0) {
            activation.starting = false
            startupWatchdog.stop()
            return
        }
        root.prepareBackendForSource(target)
        if (String(player.source) !== String(target))
            player.source = target
        activation.playbackUrl = String(target)
        activation.qualityId = activation.pendingQualityId
        startupWatchdog.restart()
        Qt.callLater(() => {
            if (root.userActivated || root.eager)
                player.play()
        })
    }

    function applyPreparedSource(target) {
        if (activation.forceReload && String(player.source) === String(target)) {
            // Retry is the only path that tears down the same URL. Splitting
            // clear/re-assign keeps backend teardown out of the click handler.
            activation.forceReload = false
            player.source = ""
            Qt.callLater(() => root.startPendingSource(target))
            return
        }
        activation.forceReload = false
        root.startPendingSource(target)
    }

    function preparePendingSource(target) {
        const session = root.coordinator()
        if (session !== null && typeof session.prepareSource === "function") {
            activation.preparationId = Number(session.prepareSource(String(target || "")))
            return
        }
        root.applyPreparedSource(target)
    }

    function syncSource() {
        if (!root.eager || !root.hasSource) {
            if (!root.userActivated)
                player.source = ""
            return
        }
        const descriptor = root.descriptorForPreference(root.preferredQuality)
        activation.playbackUrl = String(descriptor.url || "")
        activation.qualityId = String(descriptor.qualityId || "original")
        if (activation.playbackUrl.length > 0) {
            root.prepareBackendForSource(activation.playbackUrl)
            player.source = activation.playbackUrl
        }
    }

    function resetPlayback() {
        startupTimer.stop()
        startupWatchdog.stop()
        adaptiveDownshift.stop()
        adaptiveUpgrade.stop()
        player.stop()
        activation.activated = false
        activation.priming = false
        activation.primed = false
        activation.stalled = false
        activation.starting = false
        activation.switching = false
        activation.recovering = false
        activation.fallbackAttempted = false
        activation.forceReload = false
        activation.preparationId = 0
        activation.pendingUrl = ""
        activation.playbackUrl = ""
        activation.qualityId = "original"
        activation.autoIndex = -1
        activation.pendingPosition = 0
        stallWatchdog.stop()
        if (primeMute.hold) {
            primeMute.hold = false
            audioOutput.muted = root.sessionMuted
        }
        root.releasePrimaryPlayback()
        root.syncSource()
    }

    function activate() {
        if (!root.hasSource || activation.activated)
            return
        activation.activated = true
        activation.stalled = false
        activation.recovering = false
        activation.fallbackAttempted = false
        if (root.primeFrame) {
            activation.priming = true
            if (!root.forceMuted) {
                primeMute.hold = true
                audioOutput.muted = true
            }
        }
        root.requestPrimaryPlayback()
        root.queueSource(root.descriptorForPreference(root.preferredQuality), 0, true, false)
    }

    function resumePlayback() {
        if (!root.hasSource || root.failed)
            return
        if (!activation.activated) {
            root.activate()
            return
        }
        if (activation.starting) {
            activation.resumeAfterLoad = true
            return
        }
        if (root.ended && !root.playing)
            player.position = 0
        root.requestPrimaryPlayback()
        player.play()
    }

    function showTransportFeedback(nowPlaying) {
        if (!root.showChrome || root.failed)
            return
        transportFeedback.iconGlyph = nowPlaying ? Theme.iconPlay : Theme.iconPause
        transportFeedback.caption = nowPlaying
            ? qsTrId("genycaster.player.playing")
            : qsTrId("genycaster.player.paused")
        transportFeedbackAnimation.stop()
        transportFeedback.opacity = 1
        transportFeedback.scale = Theme.reduceMotion ? 1 : 0.96
        transportFeedbackAnimation.restart()
    }

    function togglePlayback() {
        if (!root.interactive || !root.hasSource || root.failed)
            return
        if (!root.userActivated) {
            root.activate()
            return
        }
        if (activation.starting) {
            activation.resumeAfterLoad = !activation.resumeAfterLoad
            root.showTransportFeedback(activation.resumeAfterLoad)
            return
        }
        if (root.playing) {
            player.pause()
            root.showTransportFeedback(false)
        } else {
            root.resumePlayback()
            root.showTransportFeedback(true)
        }
    }

    function pausePlayback() {
        activation.resumeAfterLoad = false
        player.pause()
    }

    /** @brief Invoked by the coordinator when another video claims the session. */
    function suspendFromPlaybackCoordinator() {
        activation.resumeAfterLoad = false
        if (player.playbackState === MediaPlayer.PlayingState)
            player.pause()
    }

    function retryPlayback() {
        if (!root.hasSource)
            return
        activation.activated = true
        activation.stalled = false
        activation.recovering = false
        activation.fallbackAttempted = false
        root.requestPrimaryPlayback()
        root.queueSource(root.descriptorForPreference(root.preferredQuality), 0, true, true, true)
    }

    function applyPreferredQuality() {
        if (!root.userActivated || !root.hasSource)
            return
        if (root.preferredQuality !== "auto")
            activation.autoIndex = -1
        const descriptor = root.descriptorForPreference(root.preferredQuality)
        const targetUrl = String(descriptor.url || "")
        const currentUrl = activation.starting ? activation.pendingUrl : activation.playbackUrl
        if (targetUrl === currentUrl) {
            if (activation.starting)
                activation.pendingQualityId = String(descriptor.qualityId || activation.pendingQualityId)
            else
                activation.qualityId = String(descriptor.qualityId || activation.qualityId)
            return
        }
        const shouldResume = activation.starting ? activation.resumeAfterLoad : root.playing
        const restorePosition = activation.starting ? activation.pendingPosition : player.position
        root.queueSource(descriptor, restorePosition, shouldResume,
                         activation.switching || root.mediaLoaded)
    }

    function selectQuality(qualityId) {
        const session = root.coordinator()
        if (session !== null)
            session.preferredQuality = String(qualityId || "auto")
        else {
            // Coordinator-less previews still honor the selection locally for
            // this activation without pretending it can persist app-wide.
            const descriptor = root.descriptorForPreference(String(qualityId || "auto"))
            root.queueSource(descriptor, player.position, root.playing, true)
        }
        root.showChromeBriefly()
    }

    function switchAutomaticRendition(targetIndex) {
        const values = root.normalizedRenditions
        if (!root.automaticRenditionMode || activation.switching || values.length < 2)
            return
        const index = Math.max(0, Math.min(values.length - 1, Number(targetIndex)))
        if (index === activation.autoIndex)
            return
        activation.autoIndex = index
        const entry = values[index]
        root.queueSource({
            url: entry.url,
            qualityId: String(entry.height) + "p",
            autoIndex: index
        }, player.position, root.playing, true)
    }

    function toggleMute() {
        const session = root.coordinator()
        if (session === null)
            return
        session.setMuted(!session.muted)
    }

    function setVolume(value) {
        audioOutput.volume = Math.max(0, Math.min(1, Number(value || 0)))
        if (audioOutput.volume > 0 && root.sessionMuted) {
            const session = root.coordinator()
            if (session !== null)
                session.setMuted(false)
        }
    }

    function enterFullscreen() {
        if (!root.fullscreenEnabled || !root.hasSource || root.fullscreen)
            return
        player.videoOutput = fullscreenOutput
        fullscreenPopup.open()
    }

    function exitFullscreen() {
        if (!root.fullscreen)
            return
        fullscreenPopup.close()
    }

    function openOriginal() {
        const target = root.openOriginalUrl.toString().length > 0
                       ? root.openOriginalUrl : root.source
        root.openOriginalRequested(String(target))
    }

    QtObject {
        id: activation
        property bool activated: false
        property bool priming: false
        property bool primed: false
        property bool starting: false
        property bool switching: false
        property bool recovering: false
        property bool fallbackAttempted: false
        property bool forceReload: false
        property real preparationId: 0
        property bool resumeAfterLoad: true
        property real pendingPosition: 0
        property string pendingUrl: ""
        property string pendingQualityId: "original"
        property string playbackUrl: ""
        property string qualityId: "original"
        property int autoIndex: -1
        // Set by the stall watchdog: playback was requested but no decoder
        // progress is being made (dead stream, hung connection). Prevents an
        // eternal buffering spinner.
        property bool stalled: false
    }

    QtObject {
        id: primeMute
        property bool hold: false
    }

    onSourceChanged: root.resetPlayback()
    onAdaptiveSourceChanged: root.resetPlayback()
    onEagerChanged: {
        if (!activation.activated)
            root.resetPlayback()
    }
    onRenditionsChanged: root.resetPlayback()
    onPreferredQualityChanged: root.applyPreferredQuality()

    AudioOutput {
        id: audioOutput
        volume: 1.0
        muted: root.effectiveMuted
    }

    MediaPlayer {
        id: player
        audioOutput: audioOutput
        // The inline player must own a real sink from its first frame. Previous
        // revisions assigned a VideoOutput only after closing fullscreen, which
        // allowed decode to run with no visible inline sink.
        videoOutput: stageOutput

        onPlaybackStateChanged: {
            if (root.primeFrame && activation.priming && !activation.primed
                    && playbackState === MediaPlayer.PlayingState) {
                // MediaStatus can become Loaded before play() transitions to Playing.
                // Arm the one-frame stop from the playback transition as well so a
                // passive feed preview can never continue as muted background video.
                activation.primed = true
                primePause.restart()
            }
            if (root.userActivated && playbackState === MediaPlayer.PlayingState
                    && !activation.priming)
                root.requestPrimaryPlayback()
            if (root.userActivated && playbackState === MediaPlayer.PlayingState
                    && !root.buffering) {
                chromePinned = false
                chromeRefresh.restart()
            }
            if (root.userActivated && playbackState === MediaPlayer.PlayingState
                    && !root.failed)
                stallWatchdog.restart()
            if (playbackState === MediaPlayer.PausedState
                    || playbackState === MediaPlayer.StoppedState)
                stallWatchdog.stop()
            if (root.userActivated && playbackState !== MediaPlayer.PlayingState
                    && mediaStatus !== MediaPlayer.EndOfMedia)
                chromePinned = true
            if (root.automaticRenditionMode && playbackState === MediaPlayer.PlayingState
                    && !root.buffering && !activation.switching)
                adaptiveUpgrade.restart()
            else if (playbackState !== MediaPlayer.PlayingState)
                adaptiveUpgrade.stop()
        }

        onPositionChanged: {
            // Any decoder progress proves the stream is alive; re-arm the
            // stall watchdog for the next quiet window and stop treating the
            // session as first-frame startup even if the backend did not emit a
            // distinct LoadedMedia transition.
            if (position > 0) {
                activation.starting = false
                startupWatchdog.stop()
            }
            if (root.userActivated && playbackState === MediaPlayer.PlayingState
                    && !root.failed)
                stallWatchdog.restart()
        }

        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia
                    || mediaStatus === MediaPlayer.BufferingMedia
                    || mediaStatus === MediaPlayer.BufferedMedia) {
                activation.starting = false
                startupWatchdog.stop()
                const hasPendingSource = activation.pendingUrl.length > 0
                activation.playbackUrl = hasPendingSource ? activation.pendingUrl : String(player.source)
                if (hasPendingSource)
                    activation.qualityId = activation.pendingQualityId
                const seekPosition = activation.pendingPosition
                const resume = activation.resumeAfterLoad
                activation.pendingUrl = ""
                activation.pendingPosition = 0
                Qt.callLater(() => {
                    if (seekPosition > 0 && player.duration > 0)
                        player.position = Math.min(seekPosition, player.duration)
                    if (!resume)
                        player.pause()
                    activation.switching = false
                    activation.recovering = false
                })
                adaptiveDownshift.stop()
                if (root.automaticRenditionMode && player.playbackState === MediaPlayer.PlayingState)
                    adaptiveUpgrade.restart()
            } else if (mediaStatus === MediaPlayer.StalledMedia) {
                if (root.automaticRenditionMode && player.playbackState === MediaPlayer.PlayingState
                        && !activation.switching)
                    adaptiveDownshift.restart()
                adaptiveUpgrade.stop()
            }
            if (root.primeFrame && activation.priming && !activation.primed
                    && (mediaStatus === MediaPlayer.LoadedMedia
                        || mediaStatus === MediaPlayer.BufferingMedia
                        || mediaStatus === MediaPlayer.BufferedMedia)) {
                if (player.playbackState === MediaPlayer.PlayingState) {
                    activation.primed = true
                    primePause.restart()
                }
            } else if (root.primeFrame && root.eager && !activation.activated
                       && (mediaStatus === MediaPlayer.LoadedMedia
                           || mediaStatus === MediaPlayer.BufferingMedia
                           || mediaStatus === MediaPlayer.BufferedMedia)) {
                // Eager composer/upload tiles prime a muted first frame as soon
                // as the media is decodable, matching the historical behavior.
                activation.activated = true
                activation.priming = true
                if (!root.forceMuted) {
                    primeMute.hold = true
                    audioOutput.muted = true
                }
                // Passive first-frame extraction must not steal the app-wide
                // primary playback lease from a video the user is already watching.
                player.play()
            }
            if (mediaStatus === MediaPlayer.EndOfMedia) {
                chromePinned = true
                stallWatchdog.stop()
            }
        }

        onErrorOccurred: (error, errorString) => {
            stallWatchdog.stop()
            startupWatchdog.stop()
            if (!root.userActivated || error === MediaPlayer.NoError)
                return
            if (root.tryRecoverPlayback())
                return
            activation.recovering = false
            chromePinned = true
            root.playbackFailed(String(errorString))
        }
    }

    Connections {
        target: root.coordinator()

        function onSourcePrepared(requestId, source) {
            if (Number(requestId) !== activation.preparationId
                    || String(source) !== activation.pendingUrl)
                return
            activation.preparationId = 0
            root.applyPreparedSource(String(source))
        }

        function onSourcePreparationFailed(requestId, source, errorText) {
            if (Number(requestId) !== activation.preparationId
                    || String(source) !== activation.pendingUrl)
                return
            activation.preparationId = 0
            activation.starting = false
            activation.stalled = true
            root.releasePrimaryPlayback()
            chromePinned = true
            root.playbackFailed(String(errorText || ""))
        }
    }

    Timer {
        id: startupTimer
        interval: 16
        repeat: false
        onTriggered: {
            if ((!root.userActivated && !root.eager) || activation.pendingUrl.length === 0) {
                activation.starting = false
                return
            }
            root.preparePendingSource(activation.pendingUrl)
        }
    }

    // First-frame watchdog covers the state before QMediaPlayer reaches
    // PlayingState. The old watchdog started only after playback began, so a
    // dead manifest/decoder open could leave the player apparently buffering
    // forever.
    Timer {
        id: startupWatchdog
        interval: 12000
        repeat: false
        onTriggered: {
            if (!root.userActivated || root.failed || !activation.starting)
                return
            activation.starting = false
            if (root.tryRecoverPlayback())
                return
            activation.stalled = true
            player.pause()
            root.releasePrimaryPlayback()
            chromePinned = true
        }
    }

    // Progressive fallback ABR. Native HLS never enters these timers because
    // Qt/FFmpeg can make bandwidth decisions without restarting the stream.
    Timer {
        id: adaptiveDownshift
        interval: 1800
        repeat: false
        onTriggered: {
            if (root.automaticRenditionMode && root.buffering && activation.autoIndex > 0)
                root.switchAutomaticRendition(activation.autoIndex - 1)
        }
    }

    Timer {
        id: adaptiveUpgrade
        interval: 30000
        repeat: false
        onTriggered: {
            if (root.automaticRenditionMode && root.playing && !root.buffering
                    && activation.autoIndex >= 0
                    && activation.autoIndex < root.normalizedRenditions.length - 1)
                root.switchAutomaticRendition(activation.autoIndex + 1)
        }
    }

    // Stall watchdog: fires only when playback was requested but the decoder
    // made zero progress for the whole window (dead stream, hung connection,
    // HTTP/2 session reset). Converts an eternal buffering spinner into the
    // recoverable failure surface.
    Timer {
        id: stallWatchdog
        interval: 15000
        repeat: false
        onTriggered: {
            if (!root.userActivated || root.failed || !root.hasSource)
                return
            if (player.playbackState !== MediaPlayer.PlayingState)
                return
            activation.stalled = true
            player.pause()
            root.releasePrimaryPlayback()
            chromePinned = true
        }
    }

    Timer {
        id: primePause
        interval: 140
        repeat: false
        onTriggered: {
            if (root.primeFrame && player.playbackState === MediaPlayer.PlayingState) {
                // Keep the decoded frame resident in the sink. Seeking back to zero
                // can transiently clear VideoOutput on some backends and recreate the
                // black idle surface this prime is intended to avoid.
                player.pause()
                activation.priming = false
                root.releasePrimaryPlayback()
            }
            if (primeMute.hold) {
                primeMute.hold = false
                audioOutput.muted = root.sessionMuted
            }
        }
    }

    // Main surface. In fullscreen this stage is reparented into the overlay
    // popup so the same session (position, pause/play state, volume) continues
    // without restarting playback.
    Item {
        id: stage
        anchors.fill: root
        focus: true
        readonly property real controlInset: root.compact || width < 400
            ? Theme.space3 : Theme.mediaPlayerInset
        readonly property real actionSize: Math.min(width, height,
            width < 400 || height < 260 ? Theme.mediaPlayerActionHeight : 64)
        // Reserve caption space, never offset the actual transport icon.
        readonly property real actionCaptionSpace: height / 2 - actionSize / 2
            - (root.showChrome && root.hasSource ? chrome.height : 0) - controlInset
        // Keep active video frames on the direct scene-graph path. Applying an
        // offscreen MultiEffect mask here forces every decoded frame through an
        // extra texture pass and causes visible playback stalls on busy feeds.

        Superellipse {
            id: backdrop
            anchors.fill: parent
            // In fullscreen the underlying popup VideoOutput presents the media;
            // an opaque inline backdrop would otherwise hide it.
            visible: !root.fullscreen
            fillColor: Theme.mediaPlayerBackground
            curveDepth: root.curveDepth
            borderWidth: 0
        }

        SuperellipseImage {
            id: poster
            anchors.fill: parent
            visible: !root.fullscreen
                     && (!root.userActivated
                         || (root.failed && root.hasPoster)
                         || (!root.playing && !root.mediaLoaded && !root.failed))
            source: root.posterSource
            mediaCategory: "video-previews"
            // Keep the poster composition identical to the live VideoOutput:
            // media is content, so never crop the preview before playback starts.
            fillMode: Image.PreserveAspectFit
            fillColor: Theme.mediaPlayerBackground
            borderWidth: 0
            borderColor: "transparent"
            curveDepth: root.curveDepth
        }

        // The decoder remains connected to one direct VideoOutput for its whole
        // lifetime. No duplicate sink, shader mask or offscreen effect is created.
        VideoOutput {
            id: stageOutput
            anchors.fill: parent
            visible: root.userActivated && !root.failed && !root.fullscreen
            fillMode: VideoOutput.PreserveAspectFit
        }

        // Keep the active VideoOutput on the direct scene-graph path while
        // restoring the same rounded media boundary as posters and images.
        // This inverse vector mask only paints the four outside corners; it
        // never routes decoded frames through an offscreen texture/effect pass.
        Shape {
            id: videoCornerMask
            anchors.fill: parent
            z: 4
            visible: !root.fullscreen && root.curveDepth > 0
            preferredRendererType: Shape.CurveRenderer
            asynchronous: false

            readonly property real cornerRadius: Math.max(0, Math.min(root.curveDepth, width / 2, height / 2))
            readonly property real cubicFactor: {
                const exponent = Math.max(2.0, Theme.superellipseExponent)
                const diagonal = Math.pow(0.5, 1 / exponent)
                return Math.max(0.5522847498, Math.min(0.985, (8 * diagonal - 4) / 3))
            }

            ShapePath {
                fillColor: root.cornerMaskColor
                fillRule: ShapePath.OddEvenFill
                strokeColor: "transparent"
                strokeWidth: -1

                startX: 0
                startY: 0
                PathLine { x: videoCornerMask.width; y: 0 }
                PathLine { x: videoCornerMask.width; y: videoCornerMask.height }
                PathLine { x: 0; y: videoCornerMask.height }
                PathLine { x: 0; y: 0 }

                PathMove { x: videoCornerMask.cornerRadius; y: 0 }
                PathLine { x: videoCornerMask.width - videoCornerMask.cornerRadius; y: 0 }
                PathCubic {
                    x: videoCornerMask.width
                    y: videoCornerMask.cornerRadius
                    control1X: videoCornerMask.width - videoCornerMask.cornerRadius
                               + videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                    control1Y: 0
                    control2X: videoCornerMask.width
                    control2Y: videoCornerMask.cornerRadius
                               - videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                }
                PathLine { x: videoCornerMask.width; y: videoCornerMask.height - videoCornerMask.cornerRadius }
                PathCubic {
                    x: videoCornerMask.width - videoCornerMask.cornerRadius
                    y: videoCornerMask.height
                    control1X: videoCornerMask.width
                    control1Y: videoCornerMask.height - videoCornerMask.cornerRadius
                               + videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                    control2X: videoCornerMask.width - videoCornerMask.cornerRadius
                               + videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                    control2Y: videoCornerMask.height
                }
                PathLine { x: videoCornerMask.cornerRadius; y: videoCornerMask.height }
                PathCubic {
                    x: 0
                    y: videoCornerMask.height - videoCornerMask.cornerRadius
                    control1X: videoCornerMask.cornerRadius
                               - videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                    control1Y: videoCornerMask.height
                    control2X: 0
                    control2Y: videoCornerMask.height - videoCornerMask.cornerRadius
                               + videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                }
                PathLine { x: 0; y: videoCornerMask.cornerRadius }
                PathCubic {
                    x: videoCornerMask.cornerRadius
                    y: 0
                    control1X: 0
                    control1Y: videoCornerMask.cornerRadius
                               - videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                    control2X: videoCornerMask.cornerRadius
                               - videoCornerMask.cubicFactor * videoCornerMask.cornerRadius
                    control2Y: 0
                }
            }
        }

        // Lightweight scene-graph scrim: no shader, blur or offscreen texture.
        // It gives controls consistent contrast without touching the video sink.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.min(parent.height, chrome.height + Theme.space8)
            visible: root.userActivated && !root.failed && opacity > 0
            color: "transparent"
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 0.5; color: Theme.mediaPlayerChrome }
                GradientStop { position: 1; color: Theme.mediaPlayerScrim }
            }
            opacity: root.controlsRowVisible ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                    easing.type: Easing.OutCubic
                }
            }
        }

        // First-frame shimmer: while the decoder warms up (activated but no
        // pixels yet) a gentle accent pulse replaces the old static buffer
        // spinner, matching the app's skeleton motion language.
        Superellipse {
            anchors.fill: parent
            visible: root.userActivated && !root.failed && activation.starting
                     && !root.buffering
            fillColor: root.fullscreen ? "transparent" : Theme.mediaPlayerBackground
            borderWidth: 0
            curveDepth: root.curveDepth
            opacity: 0.6

            SequentialAnimation on opacity {
                running: root.userActivated && activation.starting && !root.buffering && !Theme.reduceMotion
                loops: Animation.Infinite
                NumberAnimation {
                    from: 0.35
                    to: 0.75
                    duration: Theme.reduceMotion ? 0 : 900
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    from: 0.75
                    to: 0.35
                    duration: Theme.reduceMotion ? 0 : 900
                    easing.type: Easing.InOutSine
                }
            }
        }

        // Fullscreen quick-scrub timeline hint.
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Theme.space3
            width: Math.min(420, parent.width - Theme.space8)
            height: 3
            visible: root.scrubbing && !root.controlsRowVisible

            Superellipse {
                anchors.fill: parent
                fillColor: root.chromeBorder
                borderWidth: 0
                curveDepth: 1.5
            }

            Superellipse {
                width: parent.width * root.scrubPreview
                height: parent.height
                visible: root.scrubPreview > 0.001
                fillColor: Theme.accent
                borderWidth: 0
                curveDepth: 1.5
            }
        }

        // Loading / buffering indication -----------------------------------------
        PlayerStateBadge {
            id: bufferingOverlay
            anchors.centerIn: parent
            actionSize: stage.actionSize
            captionSpace: stage.actionCaptionSpace
            maximumWidth: Math.max(1, stage.width - stage.controlInset * 2)
            width: implicitWidth
            height: implicitHeight
            busy: true
            caption: qsTrId("genycaster.player.buffering")
            visible: root.showChrome && root.userActivated && root.hasSource && !root.failed
                     && root.loading && !transportFeedback.visible
            scale: bufferingOverlay.visible ? 1 : 0.96

            Behavior on scale {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionFast
                    easing.type: Easing.OutCubic
                }
            }

            Accessible.role: Accessible.StaticText
            Accessible.name: caption
        }

        // Failure surface ----------------------------------------------------------
        Superellipse {
            id: failureSurface
            visible: root.failed && root.showChrome
            anchors.fill: parent
            fillColor: root.fullscreen ? "transparent" : Theme.mediaPlayerBackground
            borderWidth: 0
            borderColor: "transparent"
            curveDepth: root.curveDepth

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - Theme.space6 * 2, 320)
                spacing: Theme.space3

                Superellipse {
                    Layout.alignment: Qt.AlignHCenter
                    width: 56
                    height: 56
                    fillColor: root.chromeFill
                    borderWidth: 1
                    borderColor: root.chromeBorder
                    curveDepth: 28

                    Icon {
                        anchors.centerIn: parent
                        text: Theme.iconWarning
                        color: Theme.warning
                        preferredSize: Theme.typeTitle2Size
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.player.video_unavailable")
                    color: root.chromeInk
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                ActionButton {
                    Layout.alignment: Qt.AlignHCenter
                    prominent: true
                    text: qsTrId("genycaster.player.retry")
                    iconGlyph: Theme.iconRefresh
                    onClicked: root.retryPlayback()
                }

                ActionButton {
                    visible: root.openOriginalEnabled
                    Layout.alignment: Qt.AlignHCenter
                    selected: true
                    text: qsTrId("genycaster.player.open_original")
                    iconGlyph: Theme.iconArrowUpRight
                    onClicked: root.openOriginal()
                }
            }
        }

        // Short transport acknowledgement. It confirms what the surface tap did
        // without leaving another continuously animated layer over the video.
        PlayerStateBadge {
            id: transportFeedback
            anchors.centerIn: parent
            actionSize: stage.actionSize
            captionSpace: stage.actionCaptionSpace
            maximumWidth: Math.max(1, stage.width - stage.controlInset * 2)
            width: implicitWidth
            height: implicitHeight
            z: 14
            enabled: false
            visible: opacity > 0.001
            opacity: 0
            scale: 0.96
            Accessible.role: Accessible.StaticText
            Accessible.name: caption

            SequentialAnimation {
                id: transportFeedbackAnimation
                ParallelAnimation {
                    NumberAnimation {
                        target: transportFeedback
                        property: "scale"
                        to: 1
                        duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: transportFeedback
                        property: "opacity"
                        to: 1
                        duration: Theme.reduceMotion ? 0 : Theme.motionFast
                    }
                }
                PauseAnimation { duration: Theme.reduceMotion ? 420 : 300 }
                ParallelAnimation {
                    NumberAnimation {
                        target: transportFeedback
                        property: "opacity"
                        to: 0
                        duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                        easing.type: Easing.InCubic
                    }
                    NumberAnimation {
                        target: transportFeedback
                        property: "scale"
                        to: Theme.reduceMotion ? 1 : 1.025
                        duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                        easing.type: Easing.InCubic
                    }
                }
            }
        }

        // Persistent center action for idle, paused and replay states. The icon
        // communicates the available action; the caption removes ambiguity.
        PlayerStateBadge {
            id: bigPlay
            readonly property bool shown: root.showChrome && root.interactive && root.hasSource
                                          && !root.failed && !root.loading
                                          && (!root.userActivated || !root.playing || root.ended)
                                          && !transportFeedback.visible
            visible: opacity > 0.001
            enabled: shown
            z: 12
            anchors.centerIn: parent
            actionSize: stage.actionSize
            captionSpace: stage.actionCaptionSpace
            maximumWidth: Math.max(1, stage.width - stage.controlInset * 2)
            width: implicitWidth
            height: implicitHeight
            opacity: shown ? 1 : 0
            highlighted: bigPlayHover.hovered || bigPlayTap.pressed
            focused: activeFocus
            activeFocusOnTab: shown
            iconGlyph: root.ended ? Theme.iconReplay : Theme.iconPlay
            caption: root.ended ? qsTrId("genycaster.player.replay")
                               : root.userActivated ? qsTrId("genycaster.player.resume")
                                                    : qsTrId("genycaster.player.play")
            scale: Theme.reduceMotion ? 1 : !shown ? 0.96 : bigPlayTap.pressed ? 0.97 : 1

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on scale {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionFast
                    easing.type: Easing.OutCubic
                }
            }

            HoverHandler {
                id: bigPlayHover
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                id: bigPlayTap
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.togglePlayback()
            }

            Accessible.role: Accessible.Button
            Accessible.name: caption
            Accessible.onPressAction: root.togglePlayback()
            Keys.onSpacePressed: event => { root.togglePlayback(); event.accepted = true }
            Keys.onReturnPressed: event => { root.togglePlayback(); event.accepted = true }
            Keys.onEnterPressed: event => { root.togglePlayback(); event.accepted = true }
        }

        // Open-original secondary action (idle state) ------------------------------
        PlayerIconButton {
            visible: root.showChrome && root.openOriginalEnabled && !root.userActivated
                     && !root.failed
            anchors.top: parent.top
            anchors.topMargin: Theme.space3
            anchors.right: Theme.rtl ? undefined : parent.right
            anchors.left: Theme.rtl ? parent.left : undefined
            anchors.rightMargin: Theme.space3
            anchors.leftMargin: Theme.space3
            iconGlyph: Theme.iconArrowUpRight
            accessibleText: qsTrId("genycaster.player.open_original")
            filled: true
            onClicked: root.openOriginal()
        }

        // Bottom chrome ---------------------------------------------------------------
        Item {
            id: chrome
            property real reveal: root.controlsRowVisible ? 1 : 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: seekControl.height + controlRow.height + stage.controlInset
            visible: root.showChrome && root.userActivated && !root.failed && root.hasSource
            enabled: root.interactive && root.controlsRowVisible
            Behavior on reveal {
                NumberAnimation {
                    duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                    easing.type: Easing.OutCubic
                }
            }

            // Thin persistent progress line while controls are auto-hidden.
            Item {
                id: progressLine
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 0
                height: 3
                opacity: 1 - chrome.reveal
                visible: opacity > 0.001

                Superellipse {
                    id: progressTrack
                    anchors.fill: parent
                    fillColor: Theme.mediaPlayerBorder
                    borderWidth: 0
                    curveDepth: 1.5
                }

                Superellipse {
                    width: parent.width * root.playedFraction
                    height: parent.height
                    visible: root.playedFraction > 0.001
                    fillColor: Theme.accent
                    borderWidth: 0
                    curveDepth: 1.5
                }
            }

            SeekControl {
                id: seekControl
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: stage.controlInset
                anchors.rightMargin: stage.controlInset
                anchors.bottom: controlRow.top
                height: 32
                opacity: chrome.reveal
                visible: opacity > 0.001
                transform: Translate { y: (1 - chrome.reveal) * Theme.space1 }
                enabled: root.interactive && player.seekable && player.duration > 0
                position: root.playedFraction
                buffered: player.bufferProgress
                fillColor: Theme.mediaPlayerBorder
                accentColor: Theme.accent
                handleColor: root.chromeHandle
                onSeekRequested: fraction => root.scrubTo(fraction)
                onSeekingChanged: {
                    if (seeking) {
                        root.chromePinned = true
                        chromeRefresh.stop()
                    } else {
                        root.showChromeBriefly()
                    }
                }
            }

            // Full control row.
            RowLayout {
                id: controlRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: stage.controlInset
                anchors.rightMargin: stage.controlInset
                anchors.bottomMargin: stage.controlInset
                height: Theme.mediaPlayerControlSize
                spacing: Theme.space1
                layoutDirection: Qt.LeftToRight
                LayoutMirroring.enabled: false
                LayoutMirroring.childrenInherit: true
                opacity: chrome.reveal
                visible: opacity > 0.001
                transform: Translate { y: (1 - chrome.reveal) * Theme.space1 }

                PlayerIconButton {
                    id: playbackButton
                    iconGlyph: root.ended ? Theme.iconReplay : root.playing ? Theme.iconPause : Theme.iconPlay
                    accessibleText: root.playing ? qsTrId("genycaster.player.pause")
                        : root.ended ? qsTrId("genycaster.player.replay")
                        : qsTrId("genycaster.player.play")
                    onClicked: root.togglePlayback()
                }

                DirectionAwareLabel {
                    visible: stage.width >= 240
                    text: root.timeLabel(seekControl.seeking ? seekControl.previewPosition * player.duration : player.position)
                        + (player.duration > 0 && stage.width >= 320 ? " / " + root.timeLabel(player.duration) : "")
                    color: Theme.mediaPlayerInk
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    font.features: ({ "tnum": 1 })
                    horizontalAlignment: Text.AlignHCenter
                    forceLeftToRight: true
                }

                Item {
                    Layout.fillWidth: true
                }

                PlayerIconButton {
                    id: muteButton
                    visible: !root.forceMuted
                    iconGlyph: root.effectiveMuted ? Theme.iconVolumeMuted : Theme.iconVolume
                    accessibleText: root.effectiveMuted ? qsTrId("genycaster.player.unmute")
                                                        : qsTrId("genycaster.player.mute")
                    onClicked: root.toggleMute()
                }

                Slider {
                    id: volumeSlider
                    visible: !root.forceMuted && chrome.width >= 560
                    Layout.preferredWidth: 72
                    Layout.fillHeight: true
                    leftPadding: Theme.space2
                    rightPadding: Theme.space2
                    from: 0
                    to: 1
                    value: audioOutput.volume
                    Accessible.name: qsTrId("genycaster.player.volume")
                    onMoved: root.setVolume(value)

                    background: Item {
                        x: volumeSlider.leftPadding
                        y: Math.round((volumeSlider.height - height) / 2)
                        width: volumeSlider.availableWidth
                        height: 12

                        Superellipse {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 4
                            fillColor: Theme.mediaPlayerBorder
                            borderWidth: 0
                            curveDepth: 2
                        }

                        Superellipse {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width * volumeSlider.visualPosition
                            height: 4
                            fillColor: Theme.mediaPlayerInk
                            borderWidth: 0
                            curveDepth: 2
                        }
                    }

                    handle: Superellipse {
                        x: volumeSlider.leftPadding
                           + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                        y: Math.round((volumeSlider.height - height) / 2)
                        width: volumeSlider.pressed ? 12 : 8
                        height: width
                        fillColor: volumeSlider.pressed ? Qt.lighter(Theme.accent, 1.1) : root.chromeHandle
                        curveDepth: width / 2
                        exponent: 2
                        borderWidth: 0
                    }
                }

                Item {
                    id: qualityButton
                    visible: root.qualityOptions.length > 1 && stage.width >= 280
                    Layout.preferredWidth: stage.width < 400 ? Theme.mediaPlayerControlSize
                        : Math.max(Theme.mediaPlayerControlSize, qualityValue.implicitWidth + Theme.space3 * 2)
                    Layout.preferredHeight: Theme.mediaPlayerControlSize
                    activeFocusOnTab: true

                    function openMenu() {
                        const point = qualityButton.mapToItem(Overlay.overlay, 0, 0)
                        qualityPopup.x = Math.max(Theme.space2,
                                                  Math.min(Overlay.overlay.width - qualityPopup.width - Theme.space2,
                                                           point.x + qualityButton.width - qualityPopup.width))
                        qualityPopup.y = Math.max(Theme.space2, point.y - qualityPopup.height - Theme.space2)
                        qualityPopup.open()
                    }

                    Superellipse {
                        anchors.fill: parent
                        fillColor: qualityPopup.opened || qualityHover.hovered
                            ? Theme.mediaPlayerChromeHover : "transparent"
                        borderWidth: Theme.keyboardFocusVisible(qualityButton) ? Theme.strokeEmphasis : 0
                        borderColor: Theme.accent
                        curveDepth: height / 2
                    }

                    DirectionAwareLabel {
                        id: qualityValue
                        anchors.centerIn: parent
                        visible: stage.width >= 400
                        text: root.qualityLabel(root.preferredQuality)
                        color: qualityPopup.opened ? Theme.accent : Theme.mediaPlayerInk
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }

                    Icon {
                        anchors.centerIn: parent
                        visible: stage.width < 400
                        text: Theme.iconSettings
                        color: Theme.mediaPlayerInk
                        preferredSize: Theme.mediaPlayerControlIconSize
                    }

                    HoverHandler {
                        id: qualityHover
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: {
                            qualityButton.forceActiveFocus()
                            qualityButton.openMenu()
                        }
                    }

                    Keys.onReturnPressed: event => {
                        qualityButton.openMenu()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: event => {
                        qualityButton.openMenu()
                        event.accepted = true
                    }
                    Keys.onSpacePressed: event => {
                        qualityButton.openMenu()
                        event.accepted = true
                    }

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTrId("genycaster.player.quality")
                    Accessible.onPressAction: qualityButton.openMenu()
                }

                PlayerIconButton {
                    id: fullscreenButton
                    visible: root.fullscreenEnabled
                    iconGlyph: root.fullscreen ? Theme.iconCompress : Theme.iconExpand
                    accessibleText: root.fullscreen ? qsTrId("genycaster.player.exit_fullscreen")
                                                    : qsTrId("genycaster.player.fullscreen")
                    onClicked: root.fullscreen ? root.exitFullscreen() : root.enterFullscreen()
                }
            }
        }

        // One inset optical frame follows the exact same contour as the mask.
        Superellipse {
            anchors.fill: parent
            anchors.margins: Theme.strokeHairline / 2
            visible: !root.fullscreen
            z: 20
            fillColor: "transparent"
            borderWidth: Theme.strokeHairline
            borderColor: Theme.mediaPlayerFrame
            curveDepth: root.curveDepth
            Accessible.ignored: true
        }

        // Pointer interaction ---------------------------------------------------------
        HoverHandler {
            id: hoverHandler
            enabled: root.interactive
            onHoveredChanged: {
                if (!root.userActivated)
                    return
                chromePinned = hoverHandler.hovered
                if (root.userActivated && root.playing && !root.buffering)
                    chromeRefresh.restart()
            }
        }

        TapHandler {
            id: surfaceTap
            enabled: root.interactive && root.hasSource && !root.failed
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: {
                stage.forceActiveFocus()
                root.showChromeBriefly()
                root.togglePlayback()
            }
        }

        Keys.onSpacePressed: event => {
            if (root.userActivated && root.interactive) {
                root.togglePlayback()
                event.accepted = true
            }
        }
        Keys.onLeftPressed: event => {
            if (root.mediaLoaded && player.duration > 0) {
                player.position = Math.max(0, player.position - 5000)
                event.accepted = true
            }
        }
        Keys.onRightPressed: event => {
            if (root.mediaLoaded && player.duration > 0) {
                player.position = Math.min(player.duration, player.position + 5000)
                event.accepted = true
            }
        }
        Keys.onPressed: event => {
            if (event.key === Qt.Key_M) {
                root.toggleMute()
                event.accepted = true
            } else if (event.key === Qt.Key_F) {
                if (root.fullscreen)
                    root.exitFullscreen()
                else
                    root.enterFullscreen()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape && root.fullscreen) {
                root.exitFullscreen()
                event.accepted = true
            }
        }

        Accessible.role: Accessible.Button
        Accessible.name: root.playing ? qsTrId("genycaster.player.pause")
                                      : qsTrId("genycaster.player.play")
        Accessible.description: root.mediaLabel.length > 0
                                ? root.mediaLabel
                                : qsTrId("genycaster.player.video_controls")
    }

    // Chrome visibility -----------------------------------------------------------
    property bool chromePinned: false
    readonly property bool hoverActive: hoverHandler.hovered
    readonly property bool controlFocused: playbackButton.activeFocus || seekControl.activeFocus
        || muteButton.activeFocus || volumeSlider.activeFocus || qualityButton.activeFocus
        || fullscreenButton.activeFocus || qualityPopup.opened
    readonly property bool chromeWanted: root.showChrome && root.userActivated
                                         && root.hasSource && !root.failed
                                         && (root.hoverActive || root.chromePinned || root.controlFocused
                                             || root.buffering || !root.playing
                                             || root.ended)
    readonly property bool controlsRowVisible: root.chromeWanted
                                               && (root.chromePinned || root.buffering || root.controlFocused
                                                   || !root.playing || root.ended)

    function showChromeBriefly() {
        chromePinned = true
        if (root.playing && !root.buffering)
            chromeRefresh.restart()
    }

    function scrubTo(fraction) {
        const clamped = Math.max(0, Math.min(1, Number(fraction || 0)))
        if (player.duration <= 0)
            return
        player.position = Math.round(clamped * player.duration)
    }

    // Fullscreen quick-scrub: horizontal drag anywhere on the video seeks with
    // a thin timeline hint; taps still toggle chrome/playback.
    DragHandler {
        id: surfaceScrub
        enabled: root.interactive && root.userActivated && root.mediaLoaded
                 && player.duration > 0
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
        onActiveChanged: {
            if (surfaceScrub.active) {
                scrubBase = root.playedFraction
                chromePinned = true
                chromeRefresh.stop()
            } else {
                root.scrubTo(scrubPreview)
                if (root.playing && !root.buffering)
                    chromeRefresh.restart()
            }
        }
        onTranslationChanged: {
            scrubPreview = Math.max(0, Math.min(1,
                scrubBase + surfaceScrub.translation.x / Math.max(1, stage.width)))
        }
    }

    // Scrub state lives on the root so the popup and chrome can read it.
    property real scrubBase: 0
    property real scrubPreview: 0
    readonly property bool scrubbing: surfaceScrub.active

    Timer {
        id: chromeRefresh
        interval: 2400
        repeat: false
        onTriggered: {
            if (root.playing && !root.buffering && !root.hoverActive && !root.controlFocused
                    && !seekControl.seeking) {
                chromePinned = false
                stage.forceActiveFocus()
            }
        }
    }

    Popup {
        id: qualityPopup
        parent: Overlay.overlay
        width: 180
        implicitHeight: qualityColumn.implicitHeight + padding * 2
        padding: Theme.space1
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface {
            curveDepth: Theme.curveCompact
            elevation: Theme.elevationPopover
        }

        contentItem: ColumnLayout {
            id: qualityColumn
            spacing: 0

            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space2
                Layout.rightMargin: Theme.space2
                Layout.topMargin: Theme.space1
                Layout.bottomMargin: Theme.space1
                text: qsTrId("genycaster.player.quality")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }

            Repeater {
                model: root.qualityOptions
                delegate: ItemDelegate {
                    id: qualityOption
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 34
                    hoverEnabled: true
                    leftPadding: Theme.space2
                    rightPadding: Theme.space2
                    topPadding: 0
                    bottomPadding: 0

                    contentItem: RowLayout {
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: String(qualityOption.modelData.label || "")
                            color: root.preferredQuality === qualityOption.modelData.id
                                   || (root.preferredQuality === "auto"
                                       && root.activeQualityId === qualityOption.modelData.id)
                                   ? Theme.accent : Theme.textSecondary
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: root.preferredQuality === qualityOption.modelData.id
                                         ? Theme.weightDemiBold : Theme.weightMedium
                        }

                        Icon {
                            visible: root.preferredQuality === qualityOption.modelData.id
                            text: Theme.iconCheck
                            color: Theme.accent
                            preferredSize: Theme.iconSizeSmall
                        }
                    }

                    background: Superellipse {
                        fillColor: qualityOption.hovered ? Theme.surfaceHover : "transparent"
                        curveDepth: Theme.curveCompact
                    }

                    onClicked: {
                        root.selectQuality(String(qualityOption.modelData.id || "auto"))
                        qualityPopup.close()
                    }
                }
            }
        }
    }

    // Fullscreen overlay ------------------------------------------------------------
    Popup {
        id: fullscreenPopup
        objectName: "videoFullscreen"
        parent: Overlay.overlay
        x: 0
        y: 0
        width: parent ? parent.width : 1000
        height: parent ? parent.height : 720
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape

        Overlay.modal: Rectangle { color: "transparent" }
        background: Rectangle { color: Theme.mediaViewerBackground }

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis
                easing.type: Easing.OutCubic
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                easing.type: Easing.OutCubic
            }
        }

        onOpened: {
            stage.parent = fullscreenHost
            stage.anchors.fill = fullscreenHost
            fullscreenOutput.visible = true
            chromePinned = true
            Qt.callLater(() => stage.forceActiveFocus())
        }

        onClosed: {
            player.videoOutput = stageOutput
            fullscreenOutput.visible = false
            if (stage.parent !== root) {
                stage.parent = root
                stage.anchors.fill = root
            }
            chromePinned = false
        }

        contentItem: Item {
            id: fullscreenHost
            anchors.fill: parent

            VideoOutput {
                id: fullscreenOutput
                anchors.fill: parent
                visible: false
                fillMode: VideoOutput.PreserveAspectFit
            }

            PlayerIconButton {
                anchors.top: fullscreenHost.top
                anchors.topMargin: Theme.space4
                anchors.right: fullscreenHost.right
                anchors.rightMargin: Theme.space4
                z: 30
                iconGlyph: Theme.iconCompress
                accessibleText: qsTrId("genycaster.player.exit_fullscreen")
                filled: true
                onClicked: root.exitFullscreen()
            }
        }
    }

    // Cleanup: recycled/deleted delegates never leak decoders, audio outputs
    // or coordinator ownership.
    Component.onDestruction: {
        startupTimer.stop()
        startupWatchdog.stop()
        stallWatchdog.stop()
        adaptiveDownshift.stop()
        adaptiveUpgrade.stop()
        if (qualityPopup.opened)
            qualityPopup.close()
        if (fullscreenPopup.opened)
            fullscreenPopup.close()
        player.stop()
        activation.activated = false
        activation.preparationId = 0
        root.releasePrimaryPlayback()
    }
}
