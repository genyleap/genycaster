import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Stable, metadata-aware link preview. EmbedPreview can provide already-resolved
 * metadata so one cast URL is never resolved twice by sibling presentation layers.
 */
Superellipse {
    id: root

    property var embed: ({})
    property bool compact: false
    property bool suppressArtwork: false
    property var externalPreview: ({})
    property var browserMiniAppPreview: ({})
    property var browserLinkPreview: ({})
    property var pendingBrowserLinkPreview: ({})
    property var pendingBrowserMiniAppPreview: ({})
    property string browserLinkRequestedUrl: ""
    property string browserPreviewRequestUrl: ""
    property bool metadataManagedExternally: false
    property var resolvedPreview: ({})
    property var pendingResolvedPreview: ({})
    property bool resolvedPreviewPending: false
    property bool metadataTimedOut: false
    property var layoutReservation: null
    // A moving Flickable cannot change the media family. Previously the
    // !scrollMoving guard made a rendered Mini App collapse into a web card
    // on the very first drag, then reappear when the gesture ended.
    readonly property var reservedLayout: (root.knownMiniApp.homeUrl
                                            || root.browserMiniAppPreview.providerKind === "miniapp")
                                          ? ({kind: "miniapp", aspect: 3 / 2, artwork: true})
                                          : root.layoutReservation || root.initialLayout()
    readonly property bool layoutMiniApp: root.reservedLayout.kind === "miniapp"
    // Unknown pages must never reserve an empty thumbnail rectangle. Only
    // an actual preview image may claim image geometry.
    readonly property bool reserveArtwork: !root.suppressArtwork && root.previewSource.length > 0
    readonly property real bodyLineHeight: Math.ceil(Theme.typeBodySize * Theme.typeBodyLineHeight)
    readonly property real metaLineHeight: Math.ceil(Theme.typeMetaSize * Theme.typeMetaLineHeight)
    readonly property real captionLineHeight: Math.ceil(Theme.typeCaptionSize * Theme.typeCaptionLineHeight)
    property var scrollHost: null
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)

    readonly property var previewData: Boolean(root.browserMiniAppPreview.resolved)
                                      ? root.browserMiniAppPreview
                                      : Boolean(root.browserLinkPreview.resolved)
                                        ? root.browserLinkPreview
                                      : root.metadataManagedExternally
                                        ? (root.externalPreview || ({}))
                                        : (root.resolvedPreview || ({}))
    readonly property string targetUrl: String(root.embed.url || root.embed.value || "")
    // The browser cannot crawl arbitrary Cast URLs for fc:miniapp metadata.
    // Reuse only public Mini Apps that the verified discovery catalogue has
    // already identified; a domain match is not proof that an app is online.
    readonly property var knownMiniApp: {
        const match = /^https:\/\/([^\/?#:]+)/i.exec(root.targetUrl)
        if (!match) return ({})
        const host = match[1].toLowerCase()
        for (const catalog of [miniAppCatalog.myApps || [], miniAppCatalog.explorerApps || []]) {
            for (const app of catalog) {
                const home = String(app.homeUrl || "")
                const origin = /^https:\/\/([^\/?#:]+)/i.exec(home)
                if (origin && origin[1].toLowerCase() === host)
                    return app
            }
        }
        return ({})
    }
    readonly property string domainText: String(root.previewData.domain || root.embed.host || root.embed.domain || "")
    readonly property string providerHint: root.providerKindForUrl(root.targetUrl)
    readonly property string providerKind: {
        const resolved = String(root.previewData.providerKind || "")
        return resolved.length > 0 && resolved !== "web" ? resolved
             : Boolean(root.knownMiniApp.homeUrl) ? "miniapp" : root.providerHint
    }
    readonly property bool miniApp: root.providerKind === "miniapp"
    readonly property string miniAppLaunchUrl: String(root.previewData.miniAppLaunchUrl || root.targetUrl)
    readonly property bool miniAppLaunching: {
        miniAppTaskManager.revision
        return root.miniApp && miniAppTaskManager.isLaunchingUrl(root.miniAppLaunchUrl)
    }
    readonly property string siteName: String(root.previewData.siteName || root.knownMiniApp.name || root.domainText)
    readonly property string titleText: String(root.previewData.title || root.knownMiniApp.name
                                               || root.embed.title || root.embed.fileName
                                               || root.siteName || root.domainText)
    readonly property string summaryText: String(root.previewData.summary || root.embed.summary || root.embed.description || "")
    readonly property string previewSource: String(root.previewData.previewImage
                                                   || root.embed.imageSource || root.embed.previewImage
                                                   || root.knownMiniApp.iconSource || "")
    readonly property string animatedPosterUrl: {
        const raw = String(root.previewData.animatedPoster || "")
        return /^https:\/\/[^\s?#]+\.gif(?:\?[^#]*)?$/i.test(raw)
               ? "https://storage-farcaster.genyleap.com/v1/browser/images?url="
                 + encodeURIComponent(raw) : ""
    }
    readonly property string faviconSource: String(root.previewData.favicon || root.embed.favicon
                                                  || root.knownMiniApp.iconSource || "")
    readonly property string authorText: String(root.previewData.author || root.embed.author || "")
    readonly property string dateText: String(root.embed.publicationDate || root.embed.date || "")
    readonly property string durationText: String(root.previewData.duration || root.embed.duration || "")
    readonly property string readingTimeText: String(root.embed.readingTime || "")
    readonly property int previewImageWidth: Number(root.previewData.imageWidth || root.embed.imageWidth || 0)
    readonly property int previewImageHeight: Number(root.previewData.imageHeight || root.embed.imageHeight || 0)
    readonly property real previewImageAspect: root.reservedLayout.aspect
    readonly property string chainText: String(root.previewData.chain || "")
    readonly property string contractAddress: String(root.previewData.contractAddress || "")
    readonly property string tokenId: String(root.previewData.tokenId || "")
    // Waiting for scroll idle and the request debounce is still part of the
    // same metadata load. Keeping this true prevents poster placeholders from
    // swapping spinner -> image icon -> spinner around a fling boundary.
    readonly property bool metadataLoading: root.targetUrl.length > 0
                                            && !root.metadataTimedOut
                                            && !Boolean(root.previewData.resolved)
                                            && !Boolean(root.knownMiniApp.homeUrl)
    readonly property bool collectibleProvider: root.providerKind === "superrare"
                                                || root.providerKind === "opensea"
                                                || root.providerKind === "zora"
                                                || root.providerKind === "foundation"
                                                || root.providerKind === "manifold"
                                                || root.providerKind === "rarible"
                                                || root.providerKind === "magiceden"
                                                || root.providerKind === "highlight"
                                                || root.providerKind === "mint"
                                                || root.providerKind === "objkt"
                                                || root.providerKind === "transient"
    readonly property bool mediaRichProvider: root.providerKind === "youtube"
                                              || root.providerKind === "spotify"
                                              || root.providerKind === "apple-music"
                                              || root.providerKind === "soundcloud"
                                              || root.providerKind === "video"
    // Only genuinely media-rich destinations receive the large poster
    // treatment. Ordinary web/GitHub links stay dense even when OpenGraph
    // metadata happens to include a landscape image; promoting every such link
    // created oversized blank poster slots during metadata/image failures.
    readonly property bool preferPosterLayout: !root.suppressArtwork && root.reservedLayout.kind === "poster"
    readonly property color providerColor: root.providerKind === "youtube" ? Theme.danger
                                          : root.providerKind === "spotify" ? Theme.success
                                          : root.providerKind === "github" ? Theme.textPrimary
                                          : root.providerKind === "apple-music" ? Theme.accent
                                          : root.providerKind === "soundcloud" ? Theme.warning
                                          : Theme.accent

    signal openRequested(string url)

    function initialLayout() {
        const metadata = root.previewData
        const hint = root.providerKindForUrl(root.targetUrl)
        const provider = String(metadata.providerKind || (root.knownMiniApp.homeUrl ? 'miniapp' : hint))
        const kind = provider === "miniapp" ? "miniapp"
                   : ["youtube", "spotify", "apple-music", "soundcloud", "video"].indexOf(provider) >= 0
                     || ["youtube", "spotify", "apple-music", "soundcloud"].indexOf(hint) >= 0
                     ? "poster" : "generic"
        const width = Number(metadata.imageWidth || root.embed.imageWidth || 0)
        const height = Number(metadata.imageHeight || root.embed.imageHeight || 0)
        const aspect = width > 0 && height > 0 ? width / height : kind === "miniapp" ? 3 / 2 : 16 / 9
        return { kind: kind, aspect: aspect,
                 artwork: kind !== "generic" || !Boolean(metadata.resolved)
                          || String(metadata.previewImage || root.embed.imageSource || root.embed.previewImage || "").length > 0 }
    }

    // Freeze only geometry, not metadata/actions. A cold URL gets a reserved
    // fallback; cached dimensions may shape its first frame. Late responses,
    // failures and retries cannot change that reservation until URL reuse.
    Timer {
        id: layoutReservationTimer
        interval: 0
        onTriggered: {
            if (root.layoutReservation === null)
                root.layoutReservation = root.initialLayout()
        }
    }

    function providerKindForUrl(value) {
        const url = String(value || "").toLowerCase()
        if (url.indexOf("youtube.com/") >= 0 || url.indexOf("youtu.be/") >= 0)
            return "youtube"
        if (url.indexOf("open.spotify.com/") >= 0 || url.indexOf("spotify.com/") >= 0)
            return "spotify"
        if (url.indexOf("music.apple.com/") >= 0)
            return "apple-music"
        if (url.indexOf("soundcloud.com/") >= 0)
            return "soundcloud"
        if (url.indexOf("github.com/") >= 0)
            return "github"
        return "web"
    }

    function resolveScrollHost() {
        let candidate = root.parent
        while (candidate !== null) {
            if (candidate.moving !== undefined && candidate.contentY !== undefined) {
                root.scrollHost = candidate
                return
            }
            candidate = candidate.parent
        }
        root.scrollHost = null
    }

    function requestBrowserLinkPreview() {
        if (Qt.platform.os !== 'wasm' || root.targetUrl.length === 0
                || root.scrollMoving || Boolean(root.browserLinkPreview.resolved)
                || Boolean(root.browserMiniAppPreview.resolved)
                || root.browserLinkRequestedUrl === root.targetUrl
                || !/^https:\/\//i.test(root.targetUrl)) return
        const target = root.targetUrl
        root.browserLinkRequestedUrl = target
        const xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE || root.targetUrl !== target
                    || xhr.status !== 200) return
            try {
                const result = JSON.parse(xhr.responseText)
                if (result && result.resolved && result.providerKind === 'web'
                        && String(result.domain || '').length > 0) {
                    if (root.scrollMoving) root.pendingBrowserLinkPreview = result
                    else root.browserLinkPreview = result
                }
            } catch (error) { /* Fall back to the original compact link card. */ }
        }
        xhr.open('GET', 'https://storage-farcaster.genyleap.com/v1/browser/link-preview?url='
                 + encodeURIComponent(target), true)
        xhr.send()
    }

    function requestBrowserMiniAppManifest() {
        if (Qt.platform.os !== "wasm" || root.targetUrl.length === 0
                || root.scrollMoving || Boolean(root.knownMiniApp.homeUrl)
                || Boolean(root.browserMiniAppPreview.resolved)
                || root.browserPreviewRequestUrl === root.targetUrl
                || !/^https:\/\//i.test(root.targetUrl)) return
        const target = root.targetUrl
        const origin = /^https:\/\/([^\/?#:]+)/i.exec(target)
        if (!origin) return
        root.browserPreviewRequestUrl = target
        const xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE || root.targetUrl !== target) return
            if (xhr.status !== 200) {
                root.requestBrowserLinkPreview()
                return
            }
            try {
                const metadata = JSON.parse(xhr.responseText)
                if (metadata && metadata.resolved && metadata.providerKind === "miniapp"
                        && String(metadata.domain || "").toLowerCase() === origin[1].toLowerCase()) {
                    metadata.miniAppLaunchUrl = target
                    if (root.scrollMoving) root.pendingBrowserMiniAppPreview = metadata
                    else root.browserMiniAppPreview = metadata
                    return
                }
            } catch (error) { /* Try safe HTML metadata when not a Mini App. */ }
            root.requestBrowserLinkPreview()
        }
        xhr.open("GET", "https://storage-farcaster.genyleap.com/v1/browser/miniapp-preview?url="
                 + encodeURIComponent("https://" + origin[1] + "/"), true)
        xhr.send()
    }

    Timer {
        id: miniAppManifestTimer
        interval: 520
        repeat: false
        onTriggered: root.requestBrowserMiniAppManifest()
    }

    function requestRemoteMetadataIfNeeded() {
        if (root.metadataManagedExternally || root.targetUrl.length === 0 || root.scrollMoving)
            return
        if (typeof presentationBridge !== "undefined" && presentationBridge !== null
                && !presentationBridge.linkPreviewPrefetchEnabled)
            return
        if (Boolean(root.previewData.resolved))
            return
        linkPreviewResolver.requestPreview(root.targetUrl)
    }

    function openPreview() {
        if (root.miniApp) {
            miniAppHost.openWithMetadata(root.miniAppLaunchUrl, root.titleText, root.faviconSource)
        } else {
            // YouTube increasingly gates embedded WebViews behind account/bot
            // challenges that native clients cannot satisfy reliably. Keep a
            // first-class poster preview in Genycaster, but hand playback to the
            // user's normal browser where their real YouTube session lives.
            root.openRequested(root.targetUrl)
        }
    }

    function refreshMetadata() {
        if (root.metadataManagedExternally || root.targetUrl.length === 0)
            return
        root.resolvedPreview = linkPreviewResolver.previewFor(root.targetUrl)
        metadataRequestTimer.restart()
    }

    function synchronizeMetadataState(metadata) {
        if (Boolean(metadata && metadata.resolved)) {
            root.metadataTimedOut = false
            metadataDeadline.stop()
        } else if (root.targetUrl.length > 0 && !root.scrollMoving) {
            metadataDeadline.restart()
        }
    }

    Timer {
        id: metadataDeadline
        interval: 7000
        repeat: false
        onTriggered: root.metadataTimedOut = true
    }

    Timer {
        id: metadataRequestTimer
        interval: 120
        repeat: false
        onTriggered: root.requestRemoteMetadataIfNeeded()
    }

    onTargetUrlChanged: {
        root.pendingBrowserLinkPreview = ({})
        root.pendingBrowserMiniAppPreview = ({})
        root.browserLinkPreview = ({})
        root.browserLinkRequestedUrl = ""
        root.browserMiniAppPreview = ({})
        root.browserPreviewRequestUrl = ""
        miniAppManifestTimer.stop()
        miniAppManifestTimer.restart()
        root.layoutReservation = null
        layoutReservationTimer.restart()
        root.metadataTimedOut = false
        root.pendingResolvedPreview = ({})
        root.resolvedPreviewPending = false
        metadataDeadline.stop()
        root.refreshMetadata()
    }
    // Observe the two writable sources rather than previewData itself. A change
    // handler on the derived var could re-enter its binding while the resolver
    // was assigning a fresh JS object.
    onExternalPreviewChanged: {
        if (root.metadataManagedExternally)
            root.synchronizeMetadataState(root.externalPreview)
    }
    onResolvedPreviewChanged: {
        if (!root.metadataManagedExternally)
            root.synchronizeMetadataState(root.resolvedPreview)
    }
    onMetadataManagedExternallyChanged: root.synchronizeMetadataState(
        root.metadataManagedExternally ? root.externalPreview : root.resolvedPreview)
    onScrollMovingChanged: {
        if (root.scrollMoving) {
            metadataDeadline.stop()
        } else {
            if (root.pendingBrowserMiniAppPreview.resolved) {
                root.browserMiniAppPreview = root.pendingBrowserMiniAppPreview
                root.pendingBrowserMiniAppPreview = ({})
            }
            if (root.pendingBrowserLinkPreview.resolved) {
                root.browserLinkPreview = root.pendingBrowserLinkPreview
                root.pendingBrowserLinkPreview = ({})
            }
            if (root.resolvedPreviewPending) {
                root.resolvedPreviewPending = false
                root.resolvedPreview = root.pendingResolvedPreview
                root.pendingResolvedPreview = ({})
            }
            metadataRequestTimer.restart()
            miniAppManifestTimer.restart()
            if (root.metadataLoading)
                metadataDeadline.restart()
        }
    }
    Component.onCompleted: {
        root.resolveScrollHost()
        root.refreshMetadata()
        miniAppManifestTimer.restart()
        root.layoutReservation = root.initialLayout()
        if (root.targetUrl.length > 0 && !Boolean(root.previewData.resolved) && !root.scrollMoving)
            metadataDeadline.restart()
    }

    Connections {
        target: root.metadataManagedExternally ? null : linkPreviewResolver
        function onPreviewReady(url, preview) {
            const requested = String(root.targetUrl || "").split("#")[0]
            const resolved = String(url || "").split("#")[0]
            if (requested !== resolved)
                return
            if (root.scrollMoving) {
                root.pendingResolvedPreview = preview || ({})
                root.resolvedPreviewPending = true
                return
            }
            root.resolvedPreview = preview || ({})
        }
    }

    Layout.fillWidth: true
    Layout.maximumWidth: Number.POSITIVE_INFINITY
    Layout.alignment: Qt.AlignVCenter
    implicitHeight: content.implicitHeight + Theme.space3 * 2
    fillColor: !root.scrollMoving && hover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
    borderWidth: Theme.strokeHairline
    borderColor: !root.scrollMoving && hover.hovered ? Theme.borderDefault : Theme.borderSubtle
    curveDepth: Theme.shapeMedia

    Behavior on fillColor {
        enabled: !root.scrollMoving
        ColorAnimation { duration: Theme.motionFast }
    }
    Behavior on borderColor {
        enabled: !root.scrollMoving
        ColorAnimation { duration: Theme.motionFast }
    }

    Superellipse {
        width: 3
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: Theme.space3
        anchors.bottomMargin: Theme.space3
        anchors.left: Theme.rtl ? undefined : parent.left
        anchors.right: Theme.rtl ? parent.right : undefined
        anchors.leftMargin: Theme.rtl ? 0 : Theme.space2
        anchors.rightMargin: Theme.rtl ? Theme.space2 : 0
        visible: false
        fillColor: root.providerColor
        borderWidth: 0
        curveDepth: 1.5
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: root.layoutMiniApp ? Theme.space2 : Theme.space3
        anchors.leftMargin: root.layoutMiniApp ? Theme.space2 : Theme.space3
        anchors.rightMargin: root.layoutMiniApp ? Theme.space2 : Theme.space3
        spacing: Theme.space2

        // Mini Apps keep their Farcaster 3:2 discovery artwork and explicit host action.
        ColumnLayout {
            visible: root.layoutMiniApp
            Layout.fillWidth: true
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(22, root.captionLineHeight)
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                SuperellipseImage {
                    visible: root.faviconSource.length > 0
                    Layout.preferredWidth: visible ? 22 : 0
                    Layout.preferredHeight: visible ? 22 : 0
                    source: root.faviconSource
                    mediaCategory: "miniapps"
                    forceAutomaticDownload: true
                    requestedSourceWidth: 64
                    requestedSourceHeight: 64
                    fillMode: Image.PreserveAspectFit
                    fillColor: Theme.surfacePrimary
                    borderWidth: 0
                    curveDepth: Theme.shapeControlSmall
                    showLoadingIndicator: false
                    showManualLoadIndicator: false
                    showFailureIndicator: false
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    forceLeftToRight: true
                    text: root.siteName.length > 0 ? root.siteName : root.domainText
                    color: root.providerColor
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }
            }

            Item {
                id: miniAppPosterFrame
                readonly property real readyHeight: Math.max(root.compact ? 158 : 184,
                                                              Math.min(root.compact ? 420 : 640,
                                                                       width / Math.max(0.5, Math.min(3.0, root.previewImageAspect))))
                readonly property real targetHeight: root.reserveArtwork ? readyHeight : 0
                visible: targetHeight > 0
                Layout.fillWidth: true
                Layout.preferredHeight: targetHeight

                AmbientMediaBackdrop {
                    anchors.fill: parent
                    sourceItem: miniAppPoster.renderItem
                    active: miniAppPoster.ready
                    spread: 12
                    strength: Theme.dark ? 0.78 : 0.66
                    blurStrength: 1.0
                    saturation: 0.0
                    sampleSize: 88
                }

                Superellipse {
                    anchors.fill: parent
                    fillColor: Theme.backgroundSecondary
                    // The card already owns the outline. A second outline under
                    // the clipped poster bleeds through antialiased corners.
                    borderWidth: 0
                    curveDepth: Theme.shapeMedia
                }

                SuperellipseImage {
                    id: miniAppPoster
                    anchors.fill: parent
                    visible: root.previewSource.length > 0
                    source: root.previewSource
                    mediaCategory: "miniapps"
                    forceAutomaticDownload: true
                    requestedSourceWidth: Math.max(640, Math.round(width))
                    requestedSourceHeight: Math.max(426, Math.round(height))
                    // Mini App discovery art can contain UI/text that must remain
                    // legible. Preserve the complete frame; the ambient backdrop
                    // fills any letterbox space without cropping the artwork.
                    fillMode: Image.PreserveAspectFit
                    fillColor: Theme.backgroundSecondary
                    borderWidth: 0
                    curveDepth: Theme.shapeMedia
                    showLoadingIndicator: false
                    showManualLoadIndicator: false
                    showFailureIndicator: true
                }

                // Qt's ordinary image provider renders only the first GIF
                // frame. Keep an actual AnimatedImage for the approved GIF
                // splash, with the normal poster as a network fallback.
                AnimatedImage {
                    id: miniAppAnimation
                    z: 2
                    anchors.fill: parent
                    source: root.animatedPosterUrl
                    sourceSize: Qt.size(Math.max(1, Math.round(width)),
                                        Math.max(1, Math.round(height)))
                    fillMode: Image.PreserveAspectFit
                    cache: true
                    visible: root.animatedPosterUrl.length > 0 && status === AnimatedImage.Ready
                    playing: visible && !root.scrollMoving
                    smooth: true
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.max(120, parent.width - Theme.space4 * 2)
                    visible: !miniAppPoster.ready && !miniAppAnimation.visible
                             && !root.metadataLoading
                             && (root.previewSource.length === 0 || miniAppPoster.loadFailed)
                    spacing: Theme.space2
                    Icon {
                        objectName: "miniAppLinkFallbackIcon"
                        Layout.alignment: Qt.AlignHCenter
                        text: Theme.iconApps
                        color: Theme.textTertiary
                        preferredSize: Theme.iconSizeLarge
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.miniapp.preview_unavailable")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                }

                GenySpinner {
                    objectName: "miniAppLinkLoadingSpinner"
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    visible: miniAppPosterFrame.visible && !miniAppPoster.ready && !miniAppPoster.loadFailed
                             && (root.metadataLoading || miniAppPoster.loading)
                    running: visible && !root.scrollMoving
                    color: Theme.accent
                    strokeWidth: 2
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(44, root.bodyLineHeight + root.captionLineHeight)
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.titleText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeBodySize
                        font.weight: Theme.weightDemiBold
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.domainText
                        forceLeftToRight: true
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideMiddle
                    }
                }

                ActionButton {
                    compact: true
                    prominent: true
                    busy: root.miniAppLaunching
                    enabled: !root.miniAppLaunching
                    iconGlyph: Theme.iconPlay
                    text: root.miniAppLaunching
                          ? qsTrId("genycaster.miniapp.loading")
                          : String(root.previewData.miniAppButtonTitle || qsTrId("genycaster.miniapp.open_internal"))
                    Accessible.description: qsTrId("genycaster.miniapp.open_internal")
                    onClicked: root.openPreview()
                }
            }

            DirectionAwareLabel {
                visible: miniAppHost.errorText.length > 0 && miniAppHost.launchUrl === root.miniAppLaunchUrl
                Layout.fillWidth: true
                text: qsTrId("genycaster.miniapp.launch_failed") + " · " + miniAppHost.errorText
                color: Theme.danger
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }

        // Generic web links are grouped into cleaner feed cards. Rich media
        // destinations (YouTube, Spotify, etc.) get a poster-first treatment,
        // while ordinary links stay dense and scan-friendly.
        Loader {
            id: webPreviewLoader
            visible: !root.layoutMiniApp
            Layout.fillWidth: true
            // Large link cards incubate off the frame-critical path.
            asynchronous: true
            sourceComponent: root.preferPosterLayout ? richMediaLinkCard : genericLinkCard
        }
    }

    Component {
        id: richMediaLinkCard

        ColumnLayout {
            property alias artworkItem: richPoster
            property alias artworkFrameItem: richPosterFrame
            width: root.width - content.anchors.leftMargin - content.anchors.rightMargin
            spacing: Theme.space2

            Item {
                id: richPosterFrame
                readonly property real readyHeight: Math.max(root.compact ? 148 : 176,
                                                              Math.min(root.compact ? 360 : 560,
                                                                       width / Math.max(0.5, Math.min(3.0, root.previewImageAspect))))
                readonly property real targetHeight: readyHeight
                visible: targetHeight > 0
                Layout.fillWidth: true
                Layout.preferredHeight: targetHeight

                AmbientMediaBackdrop {
                    anchors.fill: parent
                    sourceItem: richPoster.renderItem
                    active: richPoster.ready
                    spread: 12
                    strength: Theme.dark ? 0.78 : 0.66
                    blurStrength: 1.0
                    saturation: 0.0
                    sampleSize: 88
                }

                Superellipse {
                    anchors.fill: parent
                    fillColor: Theme.backgroundSecondary
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: Theme.shapeMedia
                }

                SuperellipseImage {
                    id: richPoster
                    anchors.fill: parent
                    visible: root.previewSource.length > 0
                    source: root.previewSource
                    mediaCategory: "link-previews"
                    requestedSourceWidth: Math.max(640, Math.round(width))
                    requestedSourceHeight: Math.max(360, Math.round(height))
                    // Poster-first link previews may contain titles or UI chrome.
                    // Show the complete source and let the ambient backdrop own
                    // any remaining frame area instead of cropping information.
                    fillMode: Image.PreserveAspectFit
                    fillColor: Theme.backgroundSecondary
                    borderWidth: 0
                    curveDepth: Theme.shapeMedia
                    showLoadingIndicator: false
                    showFailureIndicator: true
                }

                Icon {
                    objectName: "richLinkFallbackIcon"
                    anchors.centerIn: parent
                    visible: !richPoster.ready && root.previewSource.length === 0 && !root.metadataLoading
                    text: Theme.iconImage
                    color: Theme.textTertiary
                    preferredSize: Theme.iconSizeLarge
                }

                GenySpinner {
                    objectName: "richLinkLoadingSpinner"
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    visible: richPosterFrame.visible && !richPoster.ready && !richPoster.loadFailed
                             && (root.metadataLoading || richPoster.loading)
                    running: visible && !root.scrollMoving
                    color: root.providerColor
                    strokeWidth: 2.2
                }

                Superellipse {
                    visible: richPoster.ready
                    anchors.centerIn: parent
                    width: root.providerKind === "youtube" ? 58 : 42
                    height: root.providerKind === "youtube" ? 40 : 42
                    fillColor: root.providerKind === "youtube" ? Theme.danger : Theme.mediaOverlay
                    borderWidth: 1
                    borderColor: root.providerKind === "youtube" ? Qt.rgba(1, 1, 1, 0.12)
                                                                  : Theme.mediaOverlayBorder
                    exponent: root.providerKind === "youtube" ? Theme.superellipseExponent : 2.01
                    curveDepth: root.providerKind === "youtube" ? 11 : width / 2

                    Icon {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 1
                        text: Theme.iconPlay
                        color: Theme.inkInverse
                        preferredSize: Theme.typeMetaSize
                    }
                }

                Superellipse {
                    visible: richPoster.ready && root.durationText.length > 0
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: Theme.space2
                    anchors.bottomMargin: Theme.space2
                    width: durationLabel.implicitWidth + Theme.space2
                    height: 24
                    fillColor: Qt.rgba(0, 0, 0, 0.78)
                    borderWidth: 0
                    curveDepth: 6

                    Label {
                        id: durationLabel
                        anchors.centerIn: parent
                        text: root.durationText
                        color: "white"
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(20, root.captionLineHeight)
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                RowLayout {
                    spacing: Theme.space1
                    layoutDirection: Theme.uiDirection

                    SuperellipseImage {
                        visible: root.faviconSource.length > 0
                        Layout.preferredWidth: visible ? 20 : 0
                        Layout.preferredHeight: visible ? 20 : 0
                        source: root.faviconSource
                        mediaCategory: "link-previews"
                        requestedSourceWidth: 48
                        requestedSourceHeight: 48
                        fillMode: Image.PreserveAspectFit
                        fillColor: Theme.surfacePrimary
                        borderWidth: 0
                        curveDepth: Theme.shapeControlSmall
                        showLoadingIndicator: false
                        showFailureIndicator: false
                    }

                    Superellipse {
                        visible: root.faviconSource.length === 0
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        fillColor: Qt.rgba(root.providerColor.r, root.providerColor.g, root.providerColor.b, 0.12)
                        borderWidth: 0
                        curveDepth: 10
                        exponent: 2.01

                        Icon {
                            anchors.centerIn: parent
                            text: root.providerKind === "youtube" ? Theme.iconPlay
                                  : root.providerKind === "spotify" ? Theme.iconPlay
                                  : root.providerKind === "apple-music" ? Theme.iconPlay
                                  : root.providerKind === "github" ? Theme.iconCode
                                  : Theme.iconLink
                            color: root.providerColor
                            preferredSize: Theme.typeCaptionSize
                        }
                    }

                    DirectionAwareLabel {
                        Layout.maximumWidth: root.compact ? 180 : 240
                        forceLeftToRight: true
                        text: root.siteName.length > 0 ? root.siteName : root.domainText
                        color: root.providerColor
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }
                }

                Item { Layout.fillWidth: true }

                Icon {
                    text: Theme.iconArrowUpRight
                    color: Theme.textTertiary
                    preferredSize: Theme.typeCaptionSize
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.preferredHeight: root.bodyLineHeight * 2
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                verticalAlignment: Text.AlignTop
                text: root.titleText
                color: Theme.textPrimary
                font.pixelSize: Theme.typeBodySize
                font.weight: Theme.weightDemiBold
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }

            DirectionAwareLabel {
                Layout.preferredHeight: root.metaLineHeight * (root.compact ? 2 : 3)
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                verticalAlignment: Text.AlignTop
                Layout.fillWidth: true
                text: root.summaryText
                color: Theme.textSecondary
                font.pixelSize: Theme.typeMetaSize
                maximumLineCount: root.compact ? 2 : 3
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(44, root.captionLineHeight * 2)
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DirectionAwareLabel {
                        Layout.preferredHeight: root.captionLineHeight
                        Layout.minimumHeight: Layout.preferredHeight
                        Layout.maximumHeight: Layout.preferredHeight
                        Layout.fillWidth: true
                        text: [root.authorText, root.dateText].filter(value => String(value || "").length > 0).join(" · ")
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        LayoutMirroring.enabled: false
                        text: root.domainText
                        color: Theme.textTertiary
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideMiddle
                    }
                }

                ActionButton {
                    visible: root.mediaRichProvider
                    compact: true
                    prominent: true
                    iconGlyph: Theme.iconPlay
                    text: qsTrId("genycaster.player.play")
                    onClicked: root.openPreview()
                }
            }
        }
    }

    Component {
        id: genericLinkCard

        RowLayout {
            property alias artworkItem: genericThumb
            property alias artworkFrameItem: genericThumbFrame
            width: root.width - content.anchors.leftMargin - content.anchors.rightMargin
            // Telegram-style card: optional small artwork on the trailing
            // edge; descriptive content stays at the leading edge.
            layoutDirection: Qt.RightToLeft
            spacing: Theme.space2

            Item {
                id: genericThumbFrame
                readonly property bool shouldReserve: root.reserveArtwork
                visible: shouldReserve
                Layout.preferredWidth: shouldReserve ? (root.compact ? 66 : 86) : 0
                Layout.preferredHeight: shouldReserve ? (root.compact ? 66 : 86) : 0

                AmbientMediaBackdrop {
                    anchors.fill: parent
                    sourceItem: genericThumb.renderItem
                    active: false
                    spread: 5
                    strength: Theme.dark ? 0.70 : 0.58
                    blurStrength: 1.0
                    saturation: 0.0
                    sampleSize: 72
                }

                Superellipse {
                    anchors.fill: parent
                    fillColor: Theme.backgroundSecondary
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: Theme.shapeMedia
                }

                SuperellipseImage {
                    id: genericThumb
                    anchors.fill: parent
                    visible: root.previewSource.length > 0
                    source: root.previewSource
                    mediaCategory: "link-previews"
                    requestedSourceWidth: Math.max(320, Math.round(width))
                    requestedSourceHeight: Math.max(240, Math.round(height))
                    fillMode: Image.PreserveAspectCrop
                    fillColor: Theme.backgroundSecondary
                    borderWidth: 0
                    curveDepth: Theme.shapeMedia
                    showLoadingIndicator: false
                    showFailureIndicator: true
                }

                Icon {
                    objectName: "genericLinkFallbackIcon"
                    anchors.centerIn: parent
                    visible: !genericThumb.ready && root.previewSource.length === 0 && !root.metadataLoading
                    text: Theme.iconImage
                    color: Theme.textTertiary
                    preferredSize: Theme.iconSizeLarge
                }

                GenySpinner {
                    objectName: "genericLinkLoadingSpinner"
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    visible: genericThumbFrame.visible && !genericThumb.ready && !genericThumb.loadFailed
                             && (root.metadataLoading || genericThumb.loading)
                    running: visible && !root.scrollMoving
                    color: Theme.textTertiary
                    strokeWidth: 1.8
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                layoutDirection: Qt.LeftToRight
                spacing: Theme.space1

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(20, root.captionLineHeight)
                    Layout.minimumHeight: Layout.preferredHeight
                    Layout.maximumHeight: Layout.preferredHeight
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space1

                    SuperellipseImage {
                        visible: root.faviconSource.length > 0
                        Layout.preferredWidth: visible ? 20 : 0
                        Layout.preferredHeight: visible ? 20 : 0
                        source: root.faviconSource
                        mediaCategory: "link-previews"
                        requestedSourceWidth: 48
                        requestedSourceHeight: 48
                        fillMode: Image.PreserveAspectFit
                        fillColor: Theme.surfacePrimary
                        borderWidth: 0
                        curveDepth: Theme.shapeControlSmall
                        showLoadingIndicator: false
                        showFailureIndicator: false
                    }

                    Superellipse {
                        visible: root.faviconSource.length === 0
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        fillColor: Qt.rgba(root.providerColor.r, root.providerColor.g, root.providerColor.b, 0.12)
                        borderWidth: 0
                        curveDepth: 10
                        exponent: 2.01

                        Icon {
                            anchors.centerIn: parent
                            text: root.providerKind === "youtube" ? Theme.iconPlay
                                  : root.providerKind === "spotify" ? Theme.iconPlay
                                  : root.providerKind === "apple-music" ? Theme.iconPlay
                                  : root.providerKind === "github" ? Theme.iconCode
                                  : Theme.iconLink
                            color: root.providerColor
                            preferredSize: Theme.typeCaptionSize
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        forceLeftToRight: true
                        text: root.siteName.length > 0 ? root.siteName : root.domainText
                        color: root.providerColor
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                    }

                    Icon {
                        text: Theme.iconArrowUpRight
                        color: Theme.textTertiary
                        preferredSize: Theme.typeCaptionSize
                    }
                }

                DirectionAwareLabel {
                    visible: root.titleText.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? root.bodyLineHeight * (root.compact ? 1 : 2) : 0
                    Layout.minimumHeight: Layout.preferredHeight
                    Layout.maximumHeight: Layout.preferredHeight
                    verticalAlignment: Text.AlignTop
                    text: root.titleText
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                    maximumLineCount: root.compact ? 1 : 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                DirectionAwareLabel {
                    visible: root.summaryText.length > 0
                    Layout.preferredHeight: visible ? root.metaLineHeight * (root.compact ? 1 : 2) : 0
                    Layout.minimumHeight: Layout.preferredHeight
                    Layout.maximumHeight: Layout.preferredHeight
                    verticalAlignment: Text.AlignTop
                    Layout.fillWidth: true
                    text: root.summaryText
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeMetaSize
                    maximumLineCount: root.compact ? 1 : 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }

                RowLayout {
                    visible: root.authorText.length > 0 || root.dateText.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? root.captionLineHeight : 0
                    Layout.minimumHeight: Layout.preferredHeight
                    Layout.maximumHeight: Layout.preferredHeight
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space1

                    DirectionAwareLabel {
                        visible: root.authorText.length > 0
                        Layout.maximumWidth: root.compact ? 120 : 180
                        text: root.authorText
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }

                    DirectionAwareLabel {
                        visible: root.dateText.length > 0
                        Layout.maximumWidth: root.compact ? 100 : 150
                        text: root.dateText
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        LayoutMirroring.enabled: false
                        Layout.maximumWidth: root.compact ? 140 : 240
                        text: root.domainText
                        color: Theme.textTertiary
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }

    function pointInsideItem(item, point) {
        if (!item || !item.visible)
            return false
        const local = item.mapFromItem(root, point.x, point.y)
        return local.x >= 0 && local.y >= 0 && local.x <= item.width && local.y <= item.height
    }

    function artworkStillLoadingAt(point) {
        if (root.previewSource.length === 0)
            return false
        if (root.layoutMiniApp) {
            return root.pointInsideItem(miniAppPosterFrame, point) && !miniAppPoster.ready
        }
        const loaded = webPreviewLoader.item
        if (!loaded || !loaded.artworkFrameItem || !loaded.artworkItem)
            return false
        return root.pointInsideItem(loaded.artworkFrameItem, point) && !loaded.artworkItem.ready
    }

    HoverHandler {
        id: hover
        enabled: !root.scrollMoving
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        // Mini App actions belong to their own button. A parent TapHandler
        // must not launch a second app when that child button is tapped.
        enabled: root.targetUrl.length > 0 && !root.layoutMiniApp
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: eventPoint => {
            // When automatic media download is disabled, the first click on
            // artwork belongs to SuperellipseImage's manual-load overlay. Do
            // not also launch the destination while the image is still pending.
            if (root.artworkStillLoadingAt(eventPoint.position))
                return
            root.openPreview()
        }
    }

    Accessible.role: Accessible.Link
    Accessible.name: root.titleText.length > 0 ? root.titleText : qsTrId("genycaster.embed.open_link")
    Accessible.description: root.miniApp ? qsTrId("genycaster.miniapp.open_internal")
        : qsTrId("genycaster.embed.open_safely_in_your_default_app_external")
}
