import QtQuick
import QtQuick.Layouts
import Farcaster.Client

// Authoritative dispatcher for canonical embed presentation.
//
// Classification (image/video/audio/file vs a web page) never happens through
// per-file URL lists. The presentation read model and this dispatcher share
// the same rule base (farcaster.presentation.media, surfaced through the
// `mediaClassifier` context object). Metadata refresh can promote a signed or
// extensionless direct-media URL once a trusted resolver observes its body;
// web pages related to video (YouTube & co.) deliberately stay link previews.
Item {
    id: root

    property var embed: ({})
    property string castCaption: ""
    property bool compact: false
    // Magazine rows are already viewport-admitted; avoid canceling a nested
    // incubator when the parent card is recycled or the view switches.
    property bool magazineCard: false
    property color mediaSurfaceColor: magazineCard ? Theme.surfaceRaised : Theme.backgroundPrimary
    property var sharedMediaGeometry: null
    readonly property bool mobileViewport: root.compact && !root.magazineCard
    property bool flatLinkCard: false
    property bool suppressArtwork: false
    readonly property string rawKind: String(root.embed.kind || "").toLowerCase()
    readonly property string targetUrl: String(root.embed.url || root.embed.value || "")
    readonly property string lowerUrl: root.targetUrl.toLowerCase().split("?")[0].split("#")[0]
    readonly property bool collectible: root.rawKind === "collectible" || root.rawKind === "nft" || root.rawKind === "token"
    // Token semantics are resolved from explicit locators and canonical token
    // URLs only. OpenGraph titles/summaries are intentionally not used because
    // asynchronous metadata promotion can create a second token card next to an
    // already-inferred one for the same cast.
    readonly property string explicitTokenLocator: TokenSemantic.locatorFromEmbed(root.embed)
    property var linkMetadata: ({})
    property var pendingLinkMetadata: ({})
    property string browserMarketplaceRequestedUrl: ""
    property bool metadataUpdatePending: false
    property var scrollHost: null
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)
    // Classification is an imperative snapshot. Keeping token, marketplace, and
    // resolved kind in one update prevents asynchronous preview refreshes from
    // re-entering a QML binding chain while a recycled feed delegate is changing.
    property string tokenLocator: ""
    property bool collectibleMarketplace: false
    property string resolvedKind: "link"
    property bool classificationReady: false
    property bool componentReady: false
    property real promotedMediaHeight: 0
    property string classifiedUrl: ""

    readonly property var classified: {
        const fallback = { family: "web", visual: false, directPlayable: false }
        if (typeof mediaClassifier !== "undefined" && mediaClassifier !== null)
            return mediaClassifier.classify(root.embed, root.linkMetadata)
        return fallback
    }
    readonly property string classifiedFamily: {
        const family = String(root.classified.family || "web")
        if (family === "image" || family === "video" || family === "audio" || family === "file")
            return family
        return "web"
    }
    readonly property bool classifiedDirectMedia: root.classifiedFamily !== "web"

    readonly property string safeImageSource: {
        const direct = String(root.embed.imageSource || "")
        // Never send an HTML marketplace page through the remote-image provider.
        // Prefer hydrated OpenGraph artwork, and accept a direct image only when
        // it is distinct from the marketplace page itself.
        if (root.collectibleMarketplace) {
            const preview = String(root.linkMetadata.previewImage || "")
            if (preview.length > 0)
                return preview
            if (direct.length > 0 && direct !== root.targetUrl)
                return direct
            return ""
        }
        if (direct.length > 0)
            return direct
        if (typeof linkPreviewResolver !== "undefined" && linkPreviewResolver !== null)
            return linkPreviewResolver.imageSourceForUrl(root.targetUrl)
        return root.classifiedFamily === "image" ? root.targetUrl : ""
    }

    readonly property var collectibleEmbed: ({
        kind: "collectible",
        url: root.targetUrl,
        imageSource: String(Boolean(root.embed.pairedVisual)
                            ? (root.embed.imageSource || "")
                            : root.collectibleMarketplace
                              ? (root.linkMetadata.previewImage || root.embed.imageSource
                                 || root.embed.artwork || root.embed.previewImage || "")
                              : (root.embed.imageSource || root.embed.artwork || root.embed.previewImage
                                 || root.linkMetadata.previewImage || "")),
        title: String(root.embed.title || root.linkMetadata.title || root.linkMetadata.siteName || ""),
        collection: String(root.embed.collection || root.linkMetadata.siteName || ""),
        edition: String(root.embed.edition || ""),
        chain: String(root.embed.chain || root.linkMetadata.chain || ""),
        price: String(root.embed.price || root.embed.lastSale || ""),
        creator: String(root.embed.creator || root.embed.author || ""),
        providerKind: String(root.embed.providerKind || root.linkMetadata.providerKind || ""),
        providerName: String(root.embed.providerName || root.linkMetadata.siteName || ""),
        domain: String(root.linkMetadata.domain || root.embed.host || ""),
        pairedVisual: Boolean(root.embed.pairedVisual),
        metadataResolved: Boolean(root.linkMetadata.resolved),
        aspectRatio: Number(root.embed.aspectRatio || 0)
    })

    signal openRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal castRequested(string castId)

    Layout.minimumWidth: 0
    implicitHeight: root.promotedMediaHeight > 0 ? root.promotedMediaHeight
                    : loader.item ? loader.item.implicitHeight : 0
    // A viewport resize may compute a fresh responsive plan.
    onWidthChanged: root.promotedMediaHeight = 0

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

    function requestRemoteMetadataIfNeeded() {
        if (!root.needsRemoteMetadata || root.targetUrl.length === 0 || root.scrollMoving)
            return
        if (typeof presentationBridge !== "undefined" && presentationBridge !== null
                && !presentationBridge.linkPreviewPrefetchEnabled)
            return
        if (Boolean(root.linkMetadata.resolved))
            return
        if (typeof linkPreviewResolver !== "undefined" && linkPreviewResolver !== null)
            linkPreviewResolver.requestPreview(root.targetUrl)
    }

    function computeCollectibleMarketplace(metadata, normalizedUrl) {
        const provider = String((metadata || ({})).providerKind || "").toLowerCase()
        if (provider === "superrare" || provider === "opensea" || provider === "zora"
                || provider === "foundation" || provider === "manifold" || provider === "rarible"
                || provider === "magiceden" || provider === "highlight" || provider === "mint"
                || provider === "objkt" || provider === "transient")
            return true

        const host = String(normalizedUrl || "")
        return host.indexOf("superrare.com/") >= 0 || host.indexOf("opensea.io/") >= 0
                || host.indexOf("zora.co/") >= 0 || host.indexOf("foundation.app/") >= 0
                || host.indexOf("manifold.xyz/") >= 0 || host.indexOf("rarible.com/") >= 0
                || host.indexOf("magiceden.io/") >= 0 || host.indexOf("highlight.xyz/") >= 0
                || host.indexOf("mint.fun/") >= 0 || host.indexOf("objkt.com/") >= 0
                || host.indexOf("transient.xyz/") >= 0
    }

    function classify(metadata) {
        const snapshot = metadata || ({})
        const marketplace = root.computeCollectibleMarketplace(snapshot, root.lowerUrl)
        const nextToken = root.explicitTokenLocator

        let nextKind = "link"
        if (root.rawKind === "cast-reference")
            nextKind = "cast-reference"
        else if (nextToken.length > 0)
            nextKind = "token-address"
        else if (root.collectible || marketplace)
            nextKind = "collectible"
        else if (root.rawKind === "image" || root.rawKind === "video"
                 || root.rawKind === "audio" || root.rawKind === "file")
            nextKind = root.rawKind
        else if (root.classifiedFamily === "image")
            nextKind = "image"
        else if (root.classifiedFamily === "video")
            nextKind = "video"
        else if (root.classifiedFamily === "audio")
            nextKind = "audio"
        else if (root.classifiedFamily === "file")
            nextKind = "file"

        root.collectibleMarketplace = marketplace
        const tokenChanged = nextToken !== root.tokenLocator
        root.tokenLocator = nextToken
        if (root.classificationReady && root.classifiedUrl === root.targetUrl && root.resolvedKind === "link"
                && (nextKind === "image" || nextKind === "video") && loader.item) {
            // Extensionless/signed URLs can be recognized only after metadata
            // resolution. Fit direct media into the original link's slot so
            // changing the renderer cannot move the rows below it.
            root.promotedMediaHeight = loader.item.implicitHeight
        }
        root.classifiedUrl = root.targetUrl
        root.resolvedKind = nextKind
        root.classificationReady = true

        if (tokenChanged && nextToken.length > 0
                && typeof liveInsights !== "undefined" && liveInsights !== null)
            liveInsights.requestToken(nextToken)
    }

    function applyLinkMetadata(metadata) {
        root.linkMetadata = metadata || ({})
        root.classify(root.linkMetadata)
    }

    function acceptLinkMetadata(metadata) {
        const nextMetadata = metadata || ({})
        if (root.scrollMoving) {
            // Metadata may promote a generic card into image/video/collectible
            // presentation. Never replace that Loader subtree while a Flickable
            // is moving; commit the newest snapshot once the gesture settles.
            root.pendingLinkMetadata = nextMetadata
            root.metadataUpdatePending = true
            return
        }
        root.metadataUpdatePending = false
        root.pendingLinkMetadata = ({})
        root.applyLinkMetadata(nextMetadata)
    }

    // WASM cannot scrape cross-origin NFT pages. Use the browser-safe metadata
    // service when native resolution has no artwork; never treat an HTML mint
    // page itself as an image. Respect scroll admission and URL reuse.
    function requestBrowserMarketplacePreview() {
        if (Qt.platform.os !== "wasm" || root.scrollMoving
                || root.browserMarketplaceRequestedUrl === root.targetUrl
                || !/^https:\/\//i.test(root.targetUrl)
                || !root.computeCollectibleMarketplace(root.linkMetadata, root.lowerUrl)
                || String(root.linkMetadata.previewImage || "").length > 0
                || (root.embed.pairedVisual && String(root.embed.imageSource || "").length > 0))
            return
        const target = root.targetUrl
        root.browserMarketplaceRequestedUrl = target
        if (typeof browserLinkMetadataController !== "undefined")
            browserLinkMetadataController.requestPreview(target, "link")
    }

    Connections {
        target: typeof browserLinkMetadataController !== "undefined"
            ? browserLinkMetadataController : null
        function onPreviewReady(url, kind, preview) {
            if (kind !== "link" || String(url) !== root.targetUrl.split("#")[0]
                    || !preview || !preview.resolved
                    || !root.computeCollectibleMarketplace(root.linkMetadata, root.lowerUrl)
                    || String(preview.previewImage || "").length === 0
                    || String(root.linkMetadata.previewImage || "").length > 0)
                return
            root.acceptLinkMetadata(preview)
        }
    }

    Timer {
        id: marketplacePreviewTimer
        interval: 360
        repeat: false
        onTriggered: root.requestBrowserMarketplacePreview()
    }

    function refreshLinkMetadata(requestRemote) {
        let nextMetadata = ({})
        if (root.targetUrl.length > 0
                && typeof linkPreviewResolver !== "undefined" && linkPreviewResolver !== null) {
            nextMetadata = linkPreviewResolver.previewFor(root.targetUrl) || ({})
        }
        root.applyLinkMetadata(nextMetadata)

        // Only generic web pages need a remote resolution round-trip. Direct
        // media (video/image/audio/file) is already playable without one.
        if (requestRemote)
            metadataRequestTimer.restart()
    }

    readonly property bool needsRemoteMetadata: root.explicitTokenLocator.length === 0
                                                && !root.classifiedDirectMedia
                                                && /^https?:\/\//i.test(root.targetUrl)

    Timer {
        id: metadataRequestTimer
        interval: 120
        repeat: false
        onTriggered: root.requestRemoteMetadataIfNeeded()
    }

    Component.onCompleted: {
        root.componentReady = true
        root.resolveScrollHost()
        root.refreshLinkMetadata(root.needsRemoteMetadata)
        marketplacePreviewTimer.restart()
    }
    onTargetUrlChanged: {
        if (!root.componentReady)
            return
        // Preserve a compatible renderer on reuse. Its URL bindings reset the
        // media state; toggling Loader.active would destroy the entire card.
        root.promotedMediaHeight = 0
        root.browserMarketplaceRequestedUrl = ""
        root.linkMetadata = ({})
        root.pendingLinkMetadata = ({})
        root.metadataUpdatePending = false
        root.tokenLocator = ""
        root.refreshLinkMetadata(root.needsRemoteMetadata)
        marketplacePreviewTimer.restart()
    }
    onScrollMovingChanged: {
        if (!root.scrollMoving) {
            if (root.metadataUpdatePending)
                root.acceptLinkMetadata(root.pendingLinkMetadata)
            if (root.needsRemoteMetadata && !Boolean(root.linkMetadata.resolved))
                metadataRequestTimer.restart()
            marketplacePreviewTimer.restart()
        }
    }
    onEmbedChanged: {
        if (!root.componentReady)
            return
        // Same-URL metadata refresh must not unload the card and briefly report
        // zero height. classify() replaces the component only if its kind changes.
        classificationRefresh.restart()
    }

    // The timer belongs to the delegate lifecycle and never captures a JS closure.
    // It covers title/kind/metadata changes where the URL itself stays unchanged.
    Timer {
        id: classificationRefresh
        interval: 0
        repeat: false
        onTriggered: root.classify(root.linkMetadata)
    }
    Connections {
        target: typeof linkPreviewResolver !== "undefined" ? linkPreviewResolver : null
        function onPreviewReady(url, preview) {
            if (String(url) === root.targetUrl) {
                // A late weaker resolver response must not erase a browser
                // preview image already displayed on the marketplace card.
                if (!root.linkMetadata.previewImage || String(preview.previewImage || "").length > 0)
                    root.acceptLinkMetadata(preview)
                marketplacePreviewTimer.restart()
            }
        }
    }

    Loader {
        id: loader
        anchors.left: parent.left
        anchors.right: parent.right
        active: root.classificationReady
        // Rapid navigation can recycle a link delegate while an outer
        // asynchronous incubation is still creating LinkPreview. Construct
        // the lightweight link shell synchronously; heavier media stays
        // asynchronously incubated outside the admitted Magazine view.
        asynchronous: !root.magazineCard && root.resolvedKind !== "link"
        sourceComponent: root.resolvedKind === "cast-reference" ? castReferenceComponent
                       : root.resolvedKind === "token-address" ? tokenAddressComponent
                       : root.resolvedKind === "collectible" ? collectibleComponent
                       : root.resolvedKind === "image" ? imageComponent
                       : root.resolvedKind === "video" ? videoComponent
                       : root.resolvedKind === "audio" ? audioComponent
                       : root.resolvedKind === "file" ? fileComponent
                       : linkComponent
    }

    Component {
        id: castReferenceComponent
        EmbedCard {
            embed: root.embed
            scrollMoving: root.scrollMoving
            onOpenRequested: url => root.openRequested(url)
            onCastRequested: castId => root.castRequested(castId)
        }
    }

    Component {
        id: tokenAddressComponent
        TokenAddressPreview {
            locator: root.tokenLocator
            targetUrl: root.targetUrl
            compact: root.compact
            enrichmentActive: root.visible && !root.scrollMoving
            onOpenRequested: url => root.openRequested(url)
        }
    }

    Component {
        id: collectibleComponent
        CollectiblePreview {
            embed: root.collectibleEmbed
            compact: root.compact
            scrollMoving: root.scrollMoving
            onOpenRequested: url => root.openRequested(url)
            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
        }
    }

    Component {
        id: imageComponent
        GalleryLayout {
            id: promotedGallery
            singleImageHeight: root.promotedMediaHeight > 0
                               ? Math.max(1, root.promotedMediaHeight - promotedGallery.shadowTail) : 0
            embeds: [{
                kind: "image",
                url: root.targetUrl,
                imageSource: root.safeImageSource,
                aspectRatio: Number(root.embed.aspectRatio || 0),
                width: Number(root.embed.width || 0),
                height: Number(root.embed.height || 0)
            }]
            compact: root.compact
            mobilePresentation: root.mobileViewport
            cornerCoverColor: root.mediaSurfaceColor
            asynchronousTiles: !root.magazineCard
            sharedMediaGeometry: root.sharedMediaGeometry
            lockFeedGeometry: root.magazineCard
                || (root.sharedMediaGeometry !== null && !root.mobileViewport)
            dynamicAspectDiscovery: root.magazineCard || root.mobileViewport
            onOpenRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
        }
    }

    Component {
        id: videoComponent
        VideoPreview {
            id: promotedVideo
            implicitHeight: root.promotedMediaHeight > 0 ? root.promotedMediaHeight : promotedVideo.preferredMediaHeight
            embed: root.embed
            compact: root.compact
            cornerCoverColor: root.mediaSurfaceColor
            onOpenRequested: url => root.openRequested(url)
        }
    }

    Component {
        id: audioComponent
        AudioPreview {
            embed: root.embed
            onOpenRequested: url => root.openRequested(url)
        }
    }

    Component {
        id: fileComponent
        FilePreview {
            embed: root.embed
            onOpenRequested: url => root.openRequested(url)
        }
    }

    Component {
        id: linkComponent
        LinkPreview {
            magazineCard: root.magazineCard
            embeddedInCard: root.flatLinkCard
            embed: root.embed
            castCaption: root.castCaption
            compact: root.compact
            suppressArtwork: root.suppressArtwork
            metadataManagedExternally: true
            externalPreview: root.linkMetadata
            onOpenRequested: url => root.openRequested(url)
        }
    }
}
