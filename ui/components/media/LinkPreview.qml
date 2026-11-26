import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Farcaster.Client

/**
 * Stable, metadata-aware link preview. EmbedPreview can provide already-resolved
 * metadata so one cast URL is never resolved twice by sibling presentation layers.
 */
Superellipse {
    id: root

    property var embed: ({})
    // Optional short Cast body folded into the same semantic web card.
    property string castCaption: ""
    property bool compact: false
    property bool magazineCard: false
    property bool embeddedInCard: false
    property bool suppressArtwork: false
    property var externalPreview: ({})
    property var browserMiniAppPreview: ({})
    property var browserLinkPreview: ({})
    property var pendingBrowserLinkPreview: ({})
    property var pendingBrowserMiniAppPreview: ({})
    property string browserLinkRequestedUrl: ""
    property string browserPreviewRequestUrl: ""
    // A gateway-origin 403 or timeout is transient; retry twice after its
    // bounded negative cache expires, never on every animation frame.
    property int miniAppRetryCount: 0
    // Inline notification rows must stay lightweight and deterministic; they
    // still link to the original URL but do not launch opportunistic network
    // discovery for every recycled delegate.
    property bool metadataDiscoveryEnabled: true
    property bool metadataManagedExternally: false
    property var resolvedPreview: ({})
    property var pendingResolvedPreview: ({})
    property bool resolvedPreviewPending: false
    property bool metadataTimedOut: false
    property var layoutReservation: null
    // A moving Flickable cannot change the media family. Previously the
    // !scrollMoving guard made a rendered Mini App collapse into a web card
    // on the very first drag, then reappear when the gesture ended.
    // General link metadata cannot change a feed row's media family after its
    // first reservation. A verified Mini App is the one deliberate exception:
    // its discovery artwork and launch affordance must remain accessible.
    // Manifest admission itself is deferred while the Flickable is moving.
    // Geometry reads only the committed reservation. Catalogue/metadata/image
    // bindings may run during child creation and must not re-enter this binding.
    readonly property var reservedLayout: root.layoutReservation !== null
        ? root.layoutReservation : ({kind: "generic", aspect: 16 / 9, artwork: true})
    readonly property bool layoutMiniApp: root.reservedLayout.kind === "miniapp"
    // Reserve the pending artwork slot before asynchronous metadata arrives.
    // The reservation is frozen per URL; removing the thumbnail after a failed
    // OpenGraph lookup would otherwise change row height while scrolling.
    // Latch terminal artwork failure independently from Image geometry. Binding
    // a frame height directly to Image.loadFailed can oscillate: collapsing the
    // frame clears Image.source and resets the error, growing it again.
    property bool miniAppArtworkUnavailable: false
    property bool richArtworkUnavailable: false
    property bool genericArtworkUnavailable: false
    onPreviewSourceChanged: {
        root.miniAppArtworkUnavailable = false
        root.richArtworkUnavailable = false
        root.genericArtworkUnavailable = false
    }
    // Every actionable link keeps the same small artwork/poster slot from its
    // first frame. Waiting for late OpenGraph art to create that slot changes
    // the ListView delegate height in the middle of a flick.
    readonly property bool reserveArtwork: !root.suppressArtwork
                                           && root.targetUrl.length > 0
    readonly property real bodyLineHeight: Math.ceil(Theme.typeBodySize * Theme.typeBodyLineHeight)
    readonly property real metaLineHeight: Math.ceil(Theme.typeMetaSize * Theme.typeMetaLineHeight)
    readonly property real captionLineHeight: Math.ceil(Theme.typeCaptionSize * Theme.typeCaptionLineHeight)
    property var scrollHost: null
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && (Boolean(root.scrollHost.moving)
                                              || Boolean(root.scrollHost.browserWheelInProgress))

    // Keep resolved metadata as one explicitly reconciled snapshot. Selecting
    // between multiple reactive JS objects inside a readonly QML binding
    // re-entered previewData/titleText while recycled delegates updated.
    // One zero-interval reconciliation per event-loop turn avoids that cycle.
    property var previewData: ({})
    function selectedPreviewData() {
        if (Boolean(root.browserMiniAppPreview.resolved))
            return root.browserMiniAppPreview
        if (Boolean(root.browserLinkPreview.resolved))
            return root.browserLinkPreview
        return root.metadataManagedExternally
               ? (root.externalPreview || ({})) : (root.resolvedPreview || ({}))
    }
    Timer {
        id: previewReconcileTimer
        interval: 0
        repeat: false
        onTriggered: root.previewData = root.selectedPreviewData()
    }
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
    readonly property string rawPreviewSource: String(root.previewData.previewImage
                                                      || root.embed.imageSource || root.embed.previewImage
                                                      || root.knownMiniApp.iconSource || "")
    // Browser-resolved OpenGraph and catalogue pictures may be raw HTTPS URLs.
    // Route them through the same bounded remote-image provider as native QML
    // metadata so CORS, cache policy and image decoding behave identically.
    readonly property string previewSource: {
        const source = root.rawPreviewSource.trim()
        if (/^image:\/\/remote\//i.test(source) || /^qrc:\//i.test(source))
            return source
        if (/^https?:\/\//i.test(source))
            return linkPreviewResolver.imageSourceForUrl(source)
        return ""
    }
    readonly property string animatedPosterUrl: {
        const raw = String(root.previewData.animatedPoster || "")
        return /^https:\/\/[^\s?#]+\.gif(?:\?[^#]*)?$/i.test(raw)
               ? (deploymentEndpoints.browserGateway + "/v1/browser/images?url=")
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
        const metadata = root.selectedPreviewData()
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
                 artwork: kind !== "generic" || root.rawPreviewSource.length > 0 }
    }

    // Freeze only geometry, not metadata/actions. A cold URL gets a reserved
    // fallback; cached dimensions may shape its first frame. Late responses,
    // failures and retries cannot change that reservation until URL reuse.
    Timer {
        id: layoutReservationTimer
        interval: 0
        onTriggered: {
            if (root.scrollMoving)
                return
            if (root.knownMiniApp.homeUrl
                    || root.browserMiniAppPreview.providerKind === "miniapp") {
                if (root.layoutReservation === null || root.layoutReservation.kind !== "miniapp")
                    root.layoutReservation = {kind: "miniapp", aspect: 3 / 2, artwork: true}
            } else if (root.layoutReservation === null) {
                root.layoutReservation = root.initialLayout()
            }
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
        if (!root.metadataDiscoveryEnabled || Qt.platform.os !== 'wasm'
                || root.targetUrl.length === 0
                || root.scrollMoving || Boolean(root.browserLinkPreview.resolved)
                || Boolean(root.browserMiniAppPreview.resolved)
                || root.browserLinkRequestedUrl === root.targetUrl
                || !/^https:\/\//i.test(root.targetUrl)) return
        const target = root.targetUrl
        root.browserLinkRequestedUrl = target
        if (typeof browserLinkMetadataController !== "undefined")
            browserLinkMetadataController.requestPreview(target, "link")
    }

    function requestBrowserMiniAppManifest() {
        if (!root.metadataDiscoveryEnabled || Qt.platform.os !== "wasm"
                || root.targetUrl.length === 0 || root.scrollMoving
                || (Boolean(root.browserMiniAppPreview.resolved)
                    && !Boolean(root.browserMiniAppPreview.castMetadataPending))
                || root.browserPreviewRequestUrl === root.targetUrl
                || !/^https:\/\//i.test(root.targetUrl)) return
        const target = root.targetUrl
        root.browserPreviewRequestUrl = target
        if (typeof browserLinkMetadataController !== "undefined")
            browserLinkMetadataController.requestPreview(target, "miniapp")
    }

    Connections {
        target: typeof browserLinkMetadataController !== "undefined"
            ? browserLinkMetadataController : null
        function onPreviewReady(url, kind, preview) {
            if (String(root.targetUrl || "").split("#")[0] !== String(url))
                return
            if (kind === "miniapp") {
                if (preview && preview.resolved && preview.providerKind === "miniapp") {
                    const metadata = Object.assign({}, preview)
                    // Trust the validated action.url from the frame embed.
                    // It can carry a different query than the cast share URL.
                    if (root.scrollMoving) root.pendingBrowserMiniAppPreview = metadata
                    else root.browserMiniAppPreview = metadata
                    if (metadata.castMetadataPending && root.miniAppRetryCount < 2) {
                        ++root.miniAppRetryCount
                        miniAppManifestRetryTimer.restart()
                    }
                } else {
                    root.requestBrowserLinkPreview()
                    if (root.miniAppRetryCount < 2) {
                        ++root.miniAppRetryCount
                        miniAppManifestRetryTimer.restart()
                    }
                }
            } else if (kind === "link" && preview && preview.resolved
                       && preview.providerKind === "web" && String(preview.domain || "").length > 0) {
                if (root.scrollMoving) root.pendingBrowserLinkPreview = preview
                else root.browserLinkPreview = preview
            }
        }
    }

    Timer {
        id: miniAppManifestTimer
        interval: 520
        repeat: false
        onTriggered: root.requestBrowserMiniAppManifest()
    }

    Timer {
        id: miniAppManifestRetryTimer
        interval: 17500
        repeat: false
        onTriggered: {
            if ((root.browserMiniAppPreview.resolved
                    && !root.browserMiniAppPreview.castMetadataPending)
                    || root.targetUrl.length === 0)
                return
            if (root.scrollMoving) {
                miniAppManifestRetryTimer.restart()
                return
            }
            // Release only the request latch. The controller's 15s negative
            // result cache has expired; one new bounded attempt is now allowed.
            root.browserPreviewRequestUrl = ""
            root.requestBrowserMiniAppManifest()
        }
    }

    function requestRemoteMetadataIfNeeded() {
        if (!root.metadataDiscoveryEnabled || root.metadataManagedExternally
                || root.targetUrl.length === 0 || root.scrollMoving)
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
        root.miniAppRetryCount = 0
        miniAppManifestRetryTimer.stop()
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
        previewReconcileTimer.restart()
    }
    onResolvedPreviewChanged: {
        if (!root.metadataManagedExternally)
            root.synchronizeMetadataState(root.resolvedPreview)
        previewReconcileTimer.restart()
    }
    onMetadataManagedExternallyChanged: {
        root.synchronizeMetadataState(
            root.metadataManagedExternally ? root.externalPreview : root.resolvedPreview)
        previewReconcileTimer.restart()
    }
    onKnownMiniAppChanged: layoutReservationTimer.restart()
    onBrowserMiniAppPreviewChanged: {
        previewReconcileTimer.restart()
        layoutReservationTimer.restart()
    }
    onBrowserLinkPreviewChanged: previewReconcileTimer.restart()
    onScrollMovingChanged: {
        if (root.scrollMoving) {
            metadataDeadline.stop()
        } else {
            layoutReservationTimer.restart()
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
        root.previewData = root.selectedPreviewData()
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
    Layout.minimumWidth: 0
    Layout.maximumWidth: Number.POSITIVE_INFINITY
    Layout.alignment: Qt.AlignVCenter
    // The preview is a visual boundary. Its layout must shrink inside
    // the Cast card on narrow native and WASM windows alike.
    clip: true
    // A generic destination has one compact, stable text budget per width
    // class. Late OpenGraph title/summary/footer metadata replaces text inside
    // this reservation instead of reflowing every subsequent feed row.
    readonly property real genericReservedHeight: root.compact ? 142 : 130
    readonly property real cardInset: root.embeddedInCard ? 0 : root.layoutMiniApp ? Theme.space4
        : root.compact ? Theme.space2 : root.preferPosterLayout ? Theme.space3 : Theme.space5
    // Keep the height contract identical to the actual inner gutter.
    // The Mini App poster previously needed 16px on each side but the card
    // only reserved 8px, clipping the launch footer in long feed rows.
    implicitHeight: content.implicitHeight + root.cardInset * 2
    fillColor: root.embeddedInCard ? "transparent"
               : !root.scrollMoving && hover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
    borderWidth: root.embeddedInCard ? 0 : Theme.strokeHairline
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
        // Mini App artwork and its action row must breathe on all four
        // sides, without the poster touching the surrounding cast card.
        // Generic desktop cards use a wider gutter around both columns.
        anchors.margins: root.cardInset
        spacing: Theme.space2

        // Poster/Mini App cards also retain the Cast sentence within the card;
        // ordinary web links place it beside the thumbnail below.
        DirectionAwareLabel {
            visible: root.castCaption.length > 0 && (root.layoutMiniApp || root.preferPosterLayout)
            Layout.fillWidth: true
            // Let a one-line heading stay one line. A forced two-line
            // reservation left an empty band before every Mini App poster.
            Layout.preferredHeight: visible
                ? Math.min(root.bodyLineHeight * 2, implicitHeight) : 0
            text: root.castCaption
            font.pixelSize: Theme.typeBodySize
            font.weight: Theme.weightDemiBold
            color: Theme.textPrimary
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }

        // Mini Apps keep their Farcaster 3:2 discovery artwork and explicit host action.
        Loader {
            id: miniAppPreviewLoader
            active: root.layoutMiniApp
            visible: active
            Layout.fillWidth: true
            sourceComponent: ColumnLayout {
            property alias artworkItem: miniAppPoster
            property alias artworkFrameItem: miniAppPosterFrame
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
                    alignToUi: true
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
                                                                       width / Math.max(0.7, Math.min(3.0, root.previewImageAspect))))
                // Preserve the 3:2 discovery frame through timeout/failure.
                // The error fallback belongs INSIDE that reserved rectangle;
                // removing it would jerk the entire feed upwards.
                readonly property real targetHeight: root.reserveArtwork
                    && root.reservedLayout.artwork ? readyHeight : 0
                visible: targetHeight > 0
                Layout.fillWidth: true
                Layout.preferredHeight: targetHeight
                clip: true

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
                    onLoadFailedChanged: {
                        if (loadFailed && root.previewSource.length > 0)
                            root.miniAppArtworkUnavailable = true
                    }
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
                    clipToSuperellipse: true
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
                    MediaResourceScope { id: animationScope; owner: miniAppPosterFrame }
                    source: animationScope.eligible && !Theme.powerSave ? root.animatedPosterUrl : ""
                    sourceSize: Qt.size(Math.min(1024, Math.max(128, Math.ceil(width / 128) * 128)),
                                        Math.min(1024, Math.max(128, Math.ceil(height / 128) * 128)))
                    fillMode: Image.PreserveAspectFit
                    cache: true
                    visible: root.animatedPosterUrl.length > 0 && status === AnimatedImage.Ready
                    playing: visible && animationScope.eligible && !root.scrollMoving && !Theme.powerSave
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
                    Layout.minimumWidth: 0
                    spacing: 0
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        alignToUi: true
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
                          : qsTrId("genycaster.apps.open")
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

        }
        // Generic web links are grouped into cleaner feed cards. Rich media
        // destinations (YouTube, Spotify, etc.) get a poster-first treatment,
        // while ordinary links stay dense and scan-friendly.
        Loader {
            id: webPreviewLoader
            active: !root.layoutMiniApp
            visible: active
            Layout.fillWidth: true
            // A second asynchronous incubation in each recycled link card
            // can outlive the parent delegate during rapid Back/scroll.
            // Rich artwork remains asynchronously loaded by its Image.
            asynchronous: false
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
                readonly property real readyHeight: Math.max(root.compact ? 116 : 142,
                                                              Math.min(root.compact ? 238 : 328,
                                                                       width / Math.max(0.75, Math.min(3.0, root.previewImageAspect))))
                // A failed or delayed poster must never collapse the reserved
                // frame: the fallback occupies precisely the same geometry.
                readonly property real targetHeight: root.reserveArtwork
                    && root.reservedLayout.artwork ? readyHeight : 0
                visible: targetHeight > 0
                Layout.fillWidth: true
                Layout.preferredHeight: targetHeight
                clip: true

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
                    onLoadFailedChanged: {
                        if (loadFailed && root.previewSource.length > 0)
                            root.richArtworkUnavailable = true
                    }
                    anchors.fill: parent
                    visible: root.previewSource.length > 0
                    source: root.previewSource
                    mediaCategory: "link-previews"
                    forceAutomaticDownload: true
                    requestedSourceWidth: Math.max(640, Math.round(width))
                    requestedSourceHeight: Math.max(360, Math.round(height))
                    // Poster-first link previews may contain titles or UI chrome.
                    // Show the complete source and let the ambient backdrop own
                    // any remaining frame area instead of cropping information.
                    fillMode: Image.PreserveAspectFit
                    fillColor: Theme.backgroundSecondary
                    borderWidth: 0
                    clipToSuperellipse: true
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

        GridLayout {
            property alias artworkItem: genericThumb
            property alias artworkFrameItem: genericThumbFrame
            width: Math.max(0, root.width - content.anchors.leftMargin
                               - content.anchors.rightMargin)
            Layout.minimumWidth: 0
            clip: true
            // Keep one image/text boundary across desktop cards. Artwork fills
            // the row; phone cards retain their full-width image above the text.
            columns: root.compact ? 1 : 2
            columnSpacing: Theme.space5
            rowSpacing: Theme.space2
            layoutDirection: root.castCaption.length > 0
                             && localizationController.isRightToLeftText(root.castCaption)
                             ? Qt.RightToLeft : Qt.LeftToRight

            Item {
                id: genericThumbFrame
                // Choose the artwork slot once per URL. Metadata or a failed
                // image must not insert/remove a full-width thumbnail mid-scroll.
                readonly property bool shouldReserve: root.reserveArtwork
                                                    && Boolean(root.reservedLayout.artwork)
                visible: shouldReserve
                clip: true
                Layout.alignment: Qt.AlignTop
                Layout.fillHeight: !root.compact
                // Phone geometry stays stable; crop within the reserved frame
                // instead of leaving side bands around square/portrait images.
                Layout.fillWidth: root.compact && shouldReserve
                Layout.minimumWidth: 0
                Layout.preferredWidth: !shouldReserve ? 0 : root.compact
                    ? Math.max(0, root.width - content.anchors.leftMargin
                                  - content.anchors.rightMargin)
                    : Math.min(Theme.scaleUi(240),
                               Math.max(0, root.width - root.cardInset * 2) * 0.30)
                Layout.preferredHeight: !shouldReserve ? 0
                    : Math.min(root.compact ? 300 : Theme.scaleUi(192),
                               Layout.preferredWidth / Math.max(0.8,
                                   Math.min(3.0, root.previewImageAspect)))
                Layout.maximumWidth: root.compact ? Number.POSITIVE_INFINITY : Layout.preferredWidth
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: root.compact ? Layout.preferredHeight : Number.POSITIVE_INFINITY

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
                    onLoadFailedChanged: {
                        if (loadFailed && root.previewSource.length > 0)
                            root.genericArtworkUnavailable = true
                    }
                    anchors.fill: parent
                    visible: root.previewSource.length > 0
                    source: root.previewSource
                    mediaCategory: "link-previews"
                    forceAutomaticDownload: true
                    requestedSourceWidth: Math.max(320, Math.round(width))
                    requestedSourceHeight: Math.max(240, Math.round(height))
                    fillMode: Image.PreserveAspectCrop
                    fillColor: Theme.backgroundSecondary
                    borderWidth: 0
                    clipToSuperellipse: true
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
                    objectName: "genericPosterLoadingSpinner"
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

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                // The grid owns wrapping width; the positioner owns height.
                // Do not feed the children's assigned width back as a size hint.
                Layout.preferredWidth: 0
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignTop
                Layout.minimumHeight: root.genericReservedHeight
                Layout.preferredHeight: root.genericReservedHeight
                Layout.maximumHeight: root.genericReservedHeight
                clip: true
                // Keep the phone artwork in its existing slot while moving
                // all text and destination details onto a shared inner edge.
                Layout.leftMargin: root.compact && !root.embeddedInCard ? Theme.space2 : 0
                Layout.rightMargin: root.compact && !root.embeddedInCard ? Theme.space2 : 0
                Layout.topMargin: root.compact ? Theme.space1 : 0
                Layout.bottomMargin: root.compact && !root.embeddedInCard ? Theme.space2 : 0
                spacing: Theme.space2

                DirectionAwareLabel {
                    visible: root.castCaption.length > 0
                    width: parent.width
                    verticalAlignment: Text.AlignTop
                    // A folded caption is a short excerpt. Paragraph breaks
                    // must not reserve blank lines above the destination header.
                    text: root.castCaption.replace(/\s+/g, " ").trim()
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                    color: Theme.textPrimary
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                RowLayout {
                    width: parent.width
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    Superellipse {
                        Layout.preferredWidth: Theme.iconSizeLarge + Theme.space2
                        Layout.preferredHeight: Layout.preferredWidth
                        fillColor: Theme.surfacePrimary
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderSubtle
                        curveDepth: Theme.shapeControlSmall

                        Icon {
                            anchors.centerIn: parent
                            visible: !siteFavicon.ready && !root.metadataLoading
                            text: root.providerKind === "github" ? Theme.iconCode : Theme.iconLink
                            color: root.providerColor
                            preferredSize: Theme.iconSizeSmall
                        }

                        GenySpinner {
                            objectName: "genericLinkLoadingSpinner"
                            anchors.centerIn: parent
                            width: Theme.iconSizeSmall
                            height: width
                            visible: root.metadataLoading && !genericThumbFrame.shouldReserve
                            running: visible && !root.scrollMoving
                            color: Theme.textTertiary
                            strokeWidth: 1.8
                        }

                        SuperellipseImage {
                            id: siteFavicon
                            anchors.centerIn: parent
                            width: Theme.iconSizeNavigation
                            height: width
                            visible: ready
                            source: root.faviconSource
                            mediaCategory: "link-previews"
                            requestedSourceWidth: Math.ceil(width * Math.max(1, Screen.devicePixelRatio))
                            requestedSourceHeight: requestedSourceWidth
                            fillMode: Image.PreserveAspectFit
                            fillColor: "transparent"
                            borderWidth: 0
                            curveDepth: Theme.shapeControlSmall
                            showLoadingIndicator: false
                            showFailureIndicator: false
                            showManualLoadIndicator: false
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        forceLeftToRight: true
                        text: root.siteName.length > 0 ? root.siteName
                              : root.domainText.length > 0 ? root.domainText : root.targetUrl
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    Icon {
                        text: Theme.iconArrowUpRight
                        color: Theme.textSecondary
                        preferredSize: Theme.iconSizeSmall
                    }
                }

                DirectionAwareLabel {
                    visible: root.titleText.length > 0
                    width: parent.width
                    verticalAlignment: Text.AlignTop
                    text: root.titleText
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                DirectionAwareLabel {
                    visible: root.summaryText.length > 0
                    width: parent.width
                    verticalAlignment: Text.AlignTop
                    text: root.summaryText
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeMetaSize
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }

                Flow {
                    visible: root.authorText.length > 0 || root.dateText.length > 0
                             || root.domainText.length > 0
                    width: parent.width
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    DirectionAwareLabel {
                        visible: root.authorText.length > 0
                        width: Math.min(implicitWidth, parent.width, root.compact ? 180 : 240)
                        text: root.authorText
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    DirectionAwareLabel {
                        visible: root.dateText.length > 0
                        width: Math.min(implicitWidth, parent.width)
                        text: root.dateText
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    Label {
                        visible: root.domainText.length > 0
                        LayoutMirroring.enabled: false
                        width: Math.min(implicitWidth, parent.width)
                        text: root.domainText
                        color: Theme.textTertiary
                        font.family: Theme.latinFontFamily
                        font.pixelSize: Theme.typeCaptionSize
                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
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
            const loaded = miniAppPreviewLoader.item
            return loaded && root.pointInsideItem(loaded.artworkFrameItem, point)
                && !loaded.artworkItem.ready
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
