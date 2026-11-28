import QtQuick
import QtQuick.Layouts
import Farcaster.Client

ColumnLayout {
    id: root
    Layout.minimumWidth: 0

    property var visualEmbeds: []
    property var supportingEmbeds: []
    property string castCaption: ""
    property bool compact: false
    property bool magazineCard: false
    property color mediaSurfaceColor: magazineCard ? Theme.surfaceRaised : Theme.backgroundPrimary
    // Detail views are short-lived during Cast -> Profile navigation.
    // Keep their tile incubation synchronous so Back cannot destroy an
    // incomplete GalleryTile context. Scrolling feed cards stay asynchronous.
    property bool asynchronousTiles: true
    property var sharedMediaGeometry: null
    property var scrollHost: null
    readonly property bool mobileViewport: root.compact && !root.magazineCard
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)

    readonly property var rawVisualEmbeds: root.deduplicatedEmbeds(root.visualEmbeds)
    readonly property var rawSupportingEmbeds: root.deduplicatedEmbeds(root.supportingEmbeds)
    readonly property var collectiblePair: root.buildCollectiblePair(root.rawVisualEmbeds, root.rawSupportingEmbeds)
    readonly property var effectiveVisualEmbeds: root.collectiblePair !== null ? [] : root.rawVisualEmbeds
    readonly property var effectiveSupportingEmbeds: root.collectiblePair !== null
        ? root.supportingWithCollectiblePair(root.rawSupportingEmbeds, root.collectiblePair)
        : root.supportingWithoutVisualDuplicates(root.rawSupportingEmbeds, root.effectiveVisualEmbeds)
    readonly property int visualCount: root.effectiveVisualEmbeds.length
    readonly property int supportingCount: root.effectiveSupportingEmbeds.length
    readonly property real gap: Theme.space2
    readonly property bool supportingTokenPair: root.supportingCount === 2
                                                && root.isTokenEmbed(root.effectiveSupportingEmbeds[0])
                                                   !== root.isTokenEmbed(root.effectiveSupportingEmbeds[1])
    // Keep ordinary links/media in one readable vertical rhythm. The only
    // side-by-side composition is the deliberate destination + token pair,
    // where both cards retain a useful minimum width.
    readonly property real supportingBreakpoint: 820
    readonly property bool useSingleColumn: !root.supportingTokenPair
                                            || width < root.supportingBreakpoint
                                            || root.supportingCount <= 1
    readonly property real columnWidth: root.useSingleColumn
                                        ? Math.max(1, width)
                                        : Math.max(1, Math.floor((width - root.gap) / 2))

    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal castRequested(string castId)

    spacing: Theme.space2

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

    function supportingReservation(embed, availableWidth) {
        const value = embed || ({})
        const kind = String(value.kind || "").toLowerCase()
        if (kind === "token-address" || kind === "token" || kind === "collectible" || kind === "nft")
            return root.compact ? 170 : 196
        if (kind === "cast-reference")
            return root.compact ? 118 : 132
        // Generic external links in media posts suppress duplicate artwork and
        // are intentionally compact. Reserving their final neighborhood keeps
        // ListView geometry stable while the heavy preview subtree is incubated
        // only after kinetic scrolling settles.
        return root.compact ? 126 : 146
    }

    Component.onCompleted: root.resolveScrollHost()
    onParentChanged: root.resolveScrollHost()

    function canonicalUrl(value) {
        let text = String(value || "").trim()
        if (!text) return ""
        text = text.split("#")[0]
        const scheme = text.indexOf("://")
        if (scheme < 0) return text
        const rest = text.substring(scheme + 3)
        let boundary = rest.length
        const slash = rest.indexOf("/")
        const question = rest.indexOf("?")
        if (slash >= 0) boundary = Math.min(boundary, slash)
        if (question >= 0) boundary = Math.min(boundary, question)
        const host = rest.substring(0, boundary).toLowerCase()
        const tail = rest.substring(boundary)
        const queryStart = tail.indexOf("?")
        let path = queryStart >= 0 ? tail.substring(0, queryStart) : tail
        while (path.endsWith("/")) path = path.slice(0, -1)
        // Only discard attribution/tracking parameters; keep meaningful IDs.
        const rawQuery = queryStart >= 0 ? tail.substring(queryStart + 1) : ""
        const query = rawQuery.split("&").filter(part => {
            if (!part) return false
            const key = part.split("=")[0].toLowerCase()
            return !/^(?:utm_[a-z0-9_]+|fbclid|gclid|igshid|ref|referrer|sharedby|si|mc_cid|mc_eid)$/.test(key)
        }).sort().join("&")
        return "https://" + host + path + (query ? "?" + query : "")
    }

    function embedKey(embed) {
        const value = embed || ({})
        const kind = String(value.kind || "").toLowerCase()
        if (kind === "cast-reference")
            return "cast:" + String(value.castId || value.hash || "")
        const tokenLocator = TokenSemantic.locatorFromEmbed(value)
        if (tokenLocator.length > 0)
            return "token-address:" + tokenLocator
        const url = root.canonicalUrl(value.url || value.value || value.imageSource || "")
        if (url.length > 0)
            return "url:" + url
        return kind + ":" + String(value.contractAddress || value.tokenId || value.fileName || "")
    }

    function deduplicatedEmbeds(values) {
        const source = values || []
        const result = []
        const seen = ({})
        for (let index = 0; index < source.length; ++index) {
            const value = source[index] || ({})
            const key = root.embedKey(value)
            if (key.length > 0 && seen[key])
                continue
            if (key.length > 0)
                seen[key] = true
            result.push(value)
        }
        return result
    }

    function supportingWithoutVisualDuplicates(values, visualValues) {
        const seen = ({})
        const visual = visualValues || []
        for (let index = 0; index < visual.length; ++index) {
            const key = root.embedKey(visual[index])
            if (key.length > 0)
                seen[key] = true
        }

        const source = values || []
        const result = []
        for (let index = 0; index < source.length; ++index) {
            const value = source[index] || ({})
            const key = root.embedKey(value)
            if (key.length > 0 && seen[key])
                continue
            if (key.length > 0)
                seen[key] = true
            result.push(value)
        }
        return result
    }

    function isTokenEmbed(embed) {
        return TokenSemantic.locatorFromEmbed(embed || ({})).length > 0
    }

    function collectibleMarketplaceInfo(embed) {
        const value = embed || ({})
        const url = String(value.url || value.value || "").toLowerCase()
        const provider = String(value.providerKind || value.provider || "").toLowerCase()
        const candidates = [
            { key: "superrare", hosts: ["superrare.com/"], label: "SuperRare" },
            { key: "objkt", hosts: ["objkt.com/"], label: "objkt" },
            { key: "transient", hosts: ["transient.xyz/"], label: "Transient" },
            { key: "opensea", hosts: ["opensea.io/"], label: "OpenSea" },
            { key: "zora", hosts: ["zora.co/"], label: "Zora" },
            { key: "foundation", hosts: ["foundation.app/"], label: "Foundation" },
            { key: "manifold", hosts: ["manifold.xyz/"], label: "Manifold" },
            { key: "rarible", hosts: ["rarible.com/"], label: "Rarible" },
            { key: "magiceden", hosts: ["magiceden.io/"], label: "Magic Eden" },
            { key: "highlight", hosts: ["highlight.xyz/"], label: "Highlight" },
            { key: "mint", hosts: ["mint.fun/"], label: "mint.fun" }
        ]
        for (let index = 0; index < candidates.length; ++index) {
            const candidate = candidates[index]
            if (provider === candidate.key)
                return candidate
            for (let hostIndex = 0; hostIndex < candidate.hosts.length; ++hostIndex) {
                if (url.indexOf(candidate.hosts[hostIndex]) >= 0)
                    return candidate
            }
        }
        return null
    }

    function buildCollectiblePair(visuals, supporting) {
        const images = visuals || []
        const links = supporting || []
        if (images.length !== 1)
            return null
        for (let index = 0; index < links.length; ++index) {
            const support = links[index] || ({})
            const marketplace = root.collectibleMarketplaceInfo(support)
            if (marketplace === null)
                continue
            const visual = images[0] || ({})
            const paired = ({})
            for (const key in support) paired[key] = support[key]
            paired.kind = "collectible"
            paired.imageSource = String(visual.imageSource || visual.previewImage || visual.url || visual.value || "")
            paired.aspectRatio = Number(visual.aspectRatio || 0)
            paired.width = Number(visual.width || 0)
            paired.height = Number(visual.height || 0)
            paired.providerKind = marketplace.key
            paired.providerName = marketplace.label
            paired.pairedVisual = true
            paired._pairedMarketplaceKey = root.embedKey(support)
            return paired
        }
        return null
    }

    function supportingWithCollectiblePair(values, pair) {
        const source = values || []
        const result = []
        let inserted = false
        const pairedKey = String((pair || {})._pairedMarketplaceKey || "")
        for (let index = 0; index < source.length; ++index) {
            const value = source[index] || ({})
            if (!inserted && pairedKey.length > 0 && root.embedKey(value) === pairedKey) {
                result.push(pair)
                inserted = true
                continue
            }
            result.push(value)
        }
        if (!inserted && pair !== null)
            result.push(pair)
        return root.deduplicatedEmbeds(result)
    }

    function tileWidthFor(index) {
        if (root.useSingleColumn)
            return Math.max(1, width)

        // A common rich Cast is one destination/media card plus one token card.
        // Token analytics need a dependable minimum width for their stats and
        // contract row, while the destination benefits from the larger visual
        // column. Keep the pair proportional on wide desktop and stack it before
        // either side starts clipping.
        if (root.supportingTokenPair) {
            const tokenWidth = Math.max(360, Math.min(410, Math.floor((width - root.gap) * 0.42)))
            const mediaWidth = Math.max(1, width - root.gap - tokenWidth)
            return root.isTokenEmbed(root.effectiveSupportingEmbeds[index]) ? tokenWidth : mediaWidth
        }

        return root.columnWidth
    }

    GalleryLayout {
        visible: root.visualCount > 0
        Layout.fillWidth: true
        embeds: root.effectiveVisualEmbeds
        compact: root.compact
        mobilePresentation: root.mobileViewport
        cornerCoverColor: root.mediaSurfaceColor
        asynchronousTiles: root.asynchronousTiles && !root.magazineCard
        sharedMediaGeometry: root.sharedMediaGeometry
        lockFeedGeometry: root.magazineCard
        // Desktop Timeline keeps its fixed media reservation. On phones a
        // portrait screenshot must instead adopt the decoded natural ratio,
        // after kinetic scrolling settles, so the full-width image is not
        // shrunk inside a shallow rectangle. Magazine discovers dimensions
        // for pre-admission/cache only, never to resize an exposed card.
        dynamicAspectDiscovery: root.magazineCard
                                || (root.visualCount === 1
                                    && (root.mobileViewport || root.scrollHost === null))
        onOpenRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
    }

    Item {
        visible: root.supportingCount > 0
        Layout.fillWidth: true
        implicitHeight: supportingFlow.childrenRect.height

        Flow {
            id: supportingFlow
            width: parent.width
            spacing: root.gap
            layoutDirection: Theme.uiDirection

            Repeater {
                // Keep existing slots when a recycled row receives new URLs.
                // A JS-array model rebuilds every nested preview on assignment.
                model: root.supportingCount

                delegate: Item {
                    id: tile
                    required property int index
                    readonly property var embedData: root.effectiveSupportingEmbeds[index] || ({})
                    property bool admitted: false
                    readonly property real reservedHeight: root.supportingReservation(
                                                               tile.embedData, tile.width)
                    width: root.tileWidthFor(index)
                    height: previewLoader.item && previewLoader.item.implicitHeight > 0
                            ? previewLoader.item.implicitHeight : tile.reservedHeight

                    function scheduleAdmission() {
                        if (root.magazineCard || tile.admitted || previewAdmission.running)
                            return
                        previewAdmission.restart()
                    }

                    Component.onCompleted: tile.scheduleAdmission()
                    Component.onDestruction: {
                        if (previewLoader && previewLoader.status === Loader.Loading)
                            previewLoader.active = false
                    }

                    Timer {
                        id: previewAdmission
                        interval: 24 + tile.index * 28
                        repeat: false
                        onTriggered: {
                            tile.admitted = true
                        }
                    }

                    Loader {
                        id: previewLoader
                        anchors.left: parent.left
                        anchors.right: parent.right
                        // The magazine's outer asynchronous Loader owns the
                        // construction budget. Its actual supporting geometry
                        // must exist before the whole cast is first revealed.
                        active: root.magazineCard || tile.admitted
                        // Timers on several newly admitted casts can fire in
                        // the same frame. Incubate the heavy preview trees too,
                        // rather than constructing that entire burst at once.
                        asynchronous: !root.magazineCard
                        visible: status === Loader.Ready
                        sourceComponent: Component {
                            EmbedPreview {
                                embed: tile.embedData
                                castCaption: root.supportingCount === 1 && root.visualCount === 0
                                             ? root.castCaption : ""
                                compact: root.magazineCard || tile.width < 520
                                magazineCard: root.magazineCard
                                mediaSurfaceColor: root.mediaSurfaceColor
                                sharedMediaGeometry: root.sharedMediaGeometry
                                flatLinkCard: root.magazineCard
                                suppressArtwork: root.visualCount > 0
                                onOpenRequested: url => root.externalRequested(url)
                                onMediaRequested: (mediaItems, itemIndex) => root.mediaRequested(mediaItems, itemIndex)
                                onCastRequested: castId => root.castRequested(castId)
                            }
                        }
                    }
                }
            }
        }
    }
}
