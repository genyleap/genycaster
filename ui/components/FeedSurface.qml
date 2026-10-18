import QtQuick
import QtQuick.Controls
import QtQml.Models
import Qt.labs.qmlmodels as DelegateModels

Item {
    id: root

    property var rows
    property var supplementalRows: []
    property bool supplementalBusy: false
    property bool runtimeReady: false
    property bool socialMode: false
    property bool busy: false
    property bool hasMore: false
    property bool offline: false
    property bool sourceUnavailable: false
    property string errorText: ""
    property string activeFeed: "trending"
    property bool communityFallback: false
    property bool identityAvailable: false
    property bool feedStarted: false
    property bool interactionsAvailable: false
    property bool compact: false
    property bool presentationActive: true
    property double accountFid: 0
    property bool loadMorePending: false
    property bool stableModelSyncPending: false
    // Complex QVariantMap/QJSValue payloads must not live inside ListModel roles:
    // Qt fixes a role's type from its first value and later C++ QVariantMap vs
    // JS object/list wrappers can otherwise be rejected. Keep only primitive,
    // type-stable roles in ListModel and store row payloads externally by key.
    property var stablePayloads: ({})
    property bool previewSyncPending: false
    property string presentationMode: "for-you"
    property string visualFilter: "nfts"
    property int forYouPrefetchPages: 0
    property int visualPrefetchPages: 0
    property int conversationPrefetchPages: 0
    property int developerPrefetchPages: 0
    property int automaticPrefetchPages: 0
    readonly property int filterPrefetchPages: root.presentationMode === "for-you"
                                               ? root.forYouPrefetchPages
                                               : root.presentationMode === "visuals"
                                                 ? root.visualPrefetchPages
                                                 : root.presentationMode === "devs"
                                                   ? root.developerPrefetchPages
                                                   : root.presentationMode === "conversations"
                                                     ? root.conversationPrefetchPages : 0
    property int pendingNewPostCount: 0
    // Explorer partitions one stable personalized source into mutually exclusive
    // presentation branches. A cast belongs to exactly one branch, while provider
    // order remains authoritative inside that branch. Never rank/reorder the
    // already-materialized timeline inside QML while pagination is active.
    readonly property bool curateHomeForYou: false
    readonly property bool presentationFilterActive: !root.socialMode
    readonly property int filterTargetCount: root.compact ? 5 : 8
    readonly property int filterPrefetchLimit: 6
    readonly property int automaticPrefetchLimit: 12
    property Component headerContent: null
    // Presentation filters preserve provider order; they only select eligible
    // rows. Pagination therefore remains provider-stable across every branch.

    signal castRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal removeCastRequested(var cast)
    signal loadMoreRequested()
    signal refreshRequested()
    signal revealPendingPostsRequested()
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal retryRequested()
    signal identityRequested()
    signal followingRequested()
    signal trendingRequested()
    signal authenticationRequested()

    function embedUrl(embed) {
        return String(embed && (embed.url || embed.value) ? (embed.url || embed.value) : "")
    }

    function isCollectibleMarketplaceUrl(value) {
        const url = String(value || "").toLowerCase()
        if (url.length === 0)
            return false

        // Explicit mint routes are collectible evidence even when the mint is
        // hosted on a project-owned domain rather than a known marketplace.
        if (/https?:\/\/[^\s?#]+\/(?:[^\s?#]+\/)*mint(?:\/|[?#\s]|$)/i.test(url))
            return true

        // Zora serves both NFT collectibles and fungible/social coins. Coin
        // routes must never leak into Explorer's NFT-art branch.
        if (url.indexOf("zora.co/") >= 0) {
            if (/zora\.co\/(?:coin|coins|trade|swap)\//.test(url))
                return false
            return url.indexOf("zora.co/collect/") >= 0
                    || url.indexOf("zora.co/collections/") >= 0
                    || url.indexOf("zora.co/@") >= 0
        }

        return url.indexOf("superrare.com/artwork/") >= 0
                || url.indexOf("opensea.io/assets/") >= 0
                || url.indexOf("opensea.io/item/") >= 0
                || url.indexOf("opensea.io/collection/") >= 0
                || url.indexOf("foundation.app/") >= 0
                || url.indexOf("manifold.xyz/") >= 0
                || url.indexOf("rarible.com/token/") >= 0
                || url.indexOf("rarible.com/collection/") >= 0
                || url.indexOf("magiceden.io/item-details/") >= 0
                || url.indexOf("magiceden.io/marketplace/") >= 0
                || url.indexOf("highlight.xyz/mint/") >= 0
                || url.indexOf("highlight.xyz/collections/") >= 0
                || url.indexOf("mint.fun/") >= 0
                || url.indexOf("objkt.com/tokens/") >= 0
                || url.indexOf("objkt.com/collections/") >= 0
                || url.indexOf("transient.xyz/") >= 0
                || url.indexOf("fxhash.xyz/") >= 0
                || url.indexOf("sound.xyz/") >= 0
                || url.indexOf("rodeo.club/") >= 0
                || url.indexOf("scatter.art/") >= 0
    }

    function isNftAssetLocator(value) {
        const text = String(value || "").trim()
        return /^eip155:\d+\/erc(?:721|1155):0x[0-9a-f]{40}(?:\/[^/?#]+|:[^/?#]+)?(?:$|[/?#])/i.test(text)
    }

    function collectiblePreview(embed) {
        if (typeof linkPreviewResolver === "undefined" || linkPreviewResolver === null)
            return ({})
        return linkPreviewResolver.previewFor(root.embedUrl(embed)) || ({})
    }

    function isFungibleTokenEvidence(embed, metadata) {
        const value = embed || ({})
        const preview = metadata || ({})
        const kind = String(value.kind || "").toLowerCase()
        const provider = String(preview.providerKind || value.providerKind || value.provider || "").toLowerCase()
        const url = root.embedUrl(value).toLowerCase()
        const locator = String(value.tokenLocator || value.locator || url).toLowerCase()

        if (kind === "token" || kind === "token-address" || kind === "fungible-token")
            return true
        if (provider === "geckoterminal" || provider === "coingecko" || provider === "dexscreener")
            return true
        if (url.indexOf("geckoterminal.com/") >= 0 || url.indexOf("coingecko.com/") >= 0
                || url.indexOf("dexscreener.com/") >= 0 || url.indexOf("uniswap.org/") >= 0
                || url.indexOf("aerodrome.finance/") >= 0
                || /zora\.co\/(?:coin|coins|trade|swap)\//.test(url)
                || /farcaster\.xyz\/~\/c\/(?:base|eth|ethereum):0x[0-9a-f]{40}/.test(url))
            return true
        if (/eip155:\d+\/erc20:/i.test(locator))
            return true

        // A bare fungible-token address is not an NFT. An NFT contract only
        // becomes collectible evidence when a token id or artwork metadata is
        // present.
        const tokenAddress = String(value.tokenAddress || "").trim()
        const tokenId = String(value.tokenId || preview.tokenId || "").trim()
        if (tokenAddress.length > 0 && tokenId.length === 0)
            return true
        return false
    }

    function classifiedFamily(embed, knownMetadata) {
        const metadata = knownMetadata !== undefined
                         ? (knownMetadata || ({}))
                         : (typeof linkPreviewResolver !== "undefined" && linkPreviewResolver !== null
                            ? linkPreviewResolver.previewFor(root.embedUrl(embed)) : ({}))
        if (typeof mediaClassifier !== "undefined" && mediaClassifier !== null) {
            const classified = mediaClassifier.classify(embed || {}, metadata)
            return String(classified.family || "web")
        }
        return String(embed && embed.kind ? embed.kind : "web").toLowerCase()
    }

    function looksLikeArtworkUrl(value) {
        const url = String(value || "").toLowerCase()
        return url.indexOf("imagedelivery.net/") >= 0
                || url.indexOf("res.cloudinary.com/") >= 0
                || url.indexOf("i.imgur.com/") >= 0
                || url.indexOf("pbs.twimg.com/") >= 0
                || url.indexOf("arweave.net/") >= 0
                || url.indexOf("gateway.pinata.cloud/") >= 0
                || url.indexOf("nftstorage.link/") >= 0
                || url.indexOf("ipfs.io/") >= 0
                || url.indexOf("wrpcd.net/") >= 0
    }

    function isArtChannelCast(data) {
        const row = data || ({})
        if (Boolean(row._artChannelSource))
            return true
        const channel = String(row.channelId || "").toLowerCase()
        return channel === "art" || channel === "photography" || channel === "basepaint"
                || channel === "design" || channel === "superrare" || channel === "nft"
                || channel === "cryptoart" || channel === "digital-art" || channel === "artists"
                || channel === "illustration"
    }

    function hasArtChannelMediaCandidate(data) {
        const row = data || ({})
        if (!root.isArtChannelCast(row))
            return false
        const embeds = row.embeds || []
        for (let index = 0; index < embeds.length; ++index) {
            const embed = embeds[index] || ({})
            const metadata = root.collectiblePreview(embed)
            if (root.isFungibleTokenEvidence(embed, metadata))
                continue
            const kind = String(embed.kind || "").toLowerCase()
            const value = root.embedUrl(embed)
            if (kind === "image" || kind === "video" || kind === "media"
                    || kind === "collectible" || kind === "nft"
                    || root.looksLikeArtworkUrl(value) || root.isCollectibleMarketplaceUrl(value)
                    || /^https?:\/\//i.test(value))
                return true
        }
        return false
    }

    function rowHasImage(data) {
        const row = data || ({})
        const embeds = row.embeds || []
        for (let index = 0; index < embeds.length; ++index) {
            const embed = embeds[index] || ({})
            const metadata = root.collectiblePreview(embed)
            const kind = String(embed.kind || "").toLowerCase()
            const value = root.embedUrl(embed)
            const family = root.classifiedFamily(embed, metadata)
            const directArtwork = String(embed.imageSource || embed.previewImage
                                         || metadata.previewImage || metadata.imageSource || "")
            if (directArtwork.length > 0 || kind === "image" || family === "image"
                    || /\.(?:avif|gif|jpe?g|png|webp)(?:$|[?#])/i.test(value))
                return true
        }
        return false
    }

    function containsCollectibleLink(data) {
        const row = data || ({})
        if (root.isCollectibleMarketplaceUrl(row.text)
                || root.isCollectibleMarketplaceUrl(row.parentUrl))
            return true

        const embeds = row.embeds || []
        for (let index = 0; index < embeds.length; ++index) {
            const embed = embeds[index] || ({})
            const metadata = root.collectiblePreview(embed)
            if (root.isFungibleTokenEvidence(embed, metadata))
                continue
            const value = root.embedUrl(embed)
            const kind = String(embed.kind || "").toLowerCase()
            const tokenId = String(embed.tokenId || metadata.tokenId || "").trim()
            const contract = String(embed.contractAddress || metadata.contractAddress || "").trim()
            const assetKind = String(metadata.assetKind || embed.assetKind || "").toLowerCase()
            if (root.isNftAssetLocator(value) || root.isCollectibleMarketplaceUrl(value)
                    || ((kind === "collectible" || kind === "nft") && tokenId.length > 0)
                    || (contract.length > 0 && tokenId.length > 0)
                    || assetKind === "artwork" || assetKind === "collectible")
                return true
        }
        return false
    }

    function hasCollectible(data) {
        // NFTs is a live semantic projection of the current feed. Require both
        // visual media and NFT/mint evidence so generic art-channel posts do not
        // keep stale, unrelated content in this tab.
        return root.rowHasImage(data) && root.containsCollectibleLink(data)
    }

    function primeArtPreviews() {
        if (!root.presentationActive || root.presentationMode !== "visuals"
                || typeof linkPreviewResolver === "undefined" || linkPreviewResolver === null)
            return
        const artRows = root.supplementalRows || []
        let budget = 14
        for (let rowIndex = 0; rowIndex < artRows.length && budget > 0; ++rowIndex) {
            const row = artRows[rowIndex] || ({})
            const embeds = row.embeds || []
            for (let embedIndex = 0; embedIndex < embeds.length && budget > 0; ++embedIndex) {
                const embed = embeds[embedIndex] || ({})
                const url = root.embedUrl(embed)
                if (!/^https?:\/\//i.test(url))
                    continue
                const metadata = root.collectiblePreview(embed)
                if (root.isFungibleTokenEvidence(embed, metadata))
                    continue
                if (String(metadata.previewImage || metadata.imageSource || "").length === 0) {
                    linkPreviewResolver.requestPreview(url)
                    --budget
                }
            }
        }
    }

    function isRootPost(data) {
        const row = data || ({})
        return !Boolean(row.isReply) && String(row.parentCastId || "").length === 0
    }

    function isConversation(data) {
        const row = data || ({})
        // A reply/thread may include media; artwork does not disqualify discussion.
        if (Boolean(row.isReply) || String(row.parentCastId || "").length > 0)
            return true
        if (Number(row.replyCount || 0) > 0)
            return true

        // Root casts that explicitly invite discussion belong here too, but an
        // ordinary text-only post is not automatically a conversation.
        const text = String(row.text || "").trim()
        return text.length >= 32 && (/[?؟]/.test(text) || /(^|\s)@[a-z0-9._-]+/i.test(text))
    }

    function looksDeveloper(data) {
        const row = data || ({})
        let haystack = String(row.text || "") + " " + String(row.parentUrl || "")
        const embeds = row.embeds || []
        for (let index = 0; index < embeds.length; ++index)
            haystack += " " + root.embedUrl(embeds[index])
        const parentPreview = row.parentPreview || ({})
        haystack += " " + String(parentPreview.text || "")
        const parentEmbeds = parentPreview.embeds || []
        for (let index = 0; index < parentEmbeds.length; ++index)
            haystack += " " + root.embedUrl(parentEmbeds[index])

        // Devs is evidence-based: a post belongs here when it actually links
        // to GitHub (repository, issue, PR, commit, gist, raw file, etc.).
        haystack = haystack.toLowerCase()
        return haystack.indexOf("github.com/") >= 0
                || haystack.indexOf("raw.githubusercontent.com/") >= 0
    }

    function semanticKind(data) {
        // The active source is partitioned exactly once across semantic branches. Strong
        // developer intent wins over attached media, then collectible/NFT
        // content, then explicit thread/discussion intent. Generic discovery
        // posts stay unclassified instead of leaking into Conversations.
        if (root.hasCollectible(data))
            return "visuals"
        if (root.looksDeveloper(data))
            return "devs"
        if (root.isConversation(data))
            return "conversations"
        return "other"
    }

    function incrementFilterPrefetchPages() {
        if (root.presentationMode === "for-you")
            ++root.forYouPrefetchPages
        else if (root.presentationMode === "visuals")
            ++root.visualPrefetchPages
        else if (root.presentationMode === "devs")
            ++root.developerPrefetchPages
        else if (root.presentationMode === "conversations")
            ++root.conversationPrefetchPages
        ++root.automaticPrefetchPages
        automaticPrefetchCooldown.restart()
    }

    function resetCurrentFilterPrefetchBudget() {
        if (root.presentationMode === "for-you")
            root.forYouPrefetchPages = 0
        else if (root.presentationMode === "visuals")
            root.visualPrefetchPages = 0
        else if (root.presentationMode === "devs")
            root.developerPrefetchPages = 0
        else if (root.presentationMode === "conversations")
            root.conversationPrefetchPages = 0
    }

    function resetAllFilterPrefetchBudgets() {
        root.forYouPrefetchPages = 0
        root.visualPrefetchPages = 0
        root.conversationPrefetchPages = 0
        root.developerPrefetchPages = 0
        root.automaticPrefetchPages = 0
        automaticPrefetchCooldown.stop()
    }

    function sourceRows() {
        const primary = root.rows || []
        if (root.socialMode) {
            const profiles = []
            for (let index = 0; index < primary.length; ++index)
                profiles.push({ payload: primary[index], presentationKind: "profile" })
            return profiles
        }

        const source = []
        const seen = ({})
        function appendUnique(row) {
            const value = row || ({})
            const castId = String(value.castId || value.hash || value.id || "")
            const key = castId.length > 0 ? castId : "index:" + source.length
            if (seen[key])
                return
            seen[key] = true
            source.push(value)
        }
        for (let index = 0; index < primary.length; ++index)
            appendUnique(primary[index])
        if (root.presentationMode === "visuals") {
            const artRows = root.supplementalRows || []
            for (let index = 0; index < artRows.length; ++index)
                appendUnique(artRows[index])
        }

        const result = []
        for (let index = 0; index < source.length; ++index) {
            const row = source[index]
            if (root.presentationMode === "for-you") {
                if (root.isRootPost(row) && root.semanticKind(row) === "other")
                    result.push({ payload: row, presentationKind: "conversation" })
            } else if (root.presentationMode === "visuals") {
                if (root.hasCollectible(row))
                    result.push({ payload: row, presentationKind: "visual" })
            } else if (root.presentationMode === "devs") {
                if (root.semanticKind(row) === "devs")
                    result.push({ payload: row, presentationKind: "developer" })
            } else if (root.presentationMode === "conversations") {
                if (root.semanticKind(row) === "conversations")
                    result.push({ payload: row, presentationKind: "conversation" })
            } else if (root.isRootPost(row)) {
                result.push({ payload: row, presentationKind: "conversation" })
            }
        }
        return result
    }

    function rowKey(row, index) {
        const value = row || ({})
        if (value._genySection)
            return "section:" + String(value._genySection)
        if (root.socialMode) {
            const fid = Number(value.fid || value.authorFid || 0)
            if (fid > 0)
                return "profile:" + fid
            return "profile-index:" + index
        }
        const castId = String(value.castId || value.hash || value.id || "")
        return castId.length > 0 ? "cast:" + castId : "cast-index:" + index
    }

    function mediaLayoutKind(row) {
        if (root.socialMode || Boolean(row && row._genySection))
            return "other"
        const embeds = (row || ({})).embeds || []
        let imageCount = 0
        for (let index = 0; index < embeds.length; ++index) {
            if (root.classifiedFamily(embeds[index], ({})) === "image")
                ++imageCount
        }
        return imageCount === 1 ? "single-image" : imageCount > 1 ? "gallery" : "other"
    }

    function rowIncubationHeight(row, presentationKind, layoutKind, availableWidth) {
        const value = row || ({})
        if (value._genySection)
            return Math.ceil(Theme.typeTitle3Size * Theme.typeBodyLineHeight + Theme.space4 * 2)
        if (root.socialMode || presentationKind === "profile")
            return 68

        const rowWidth = Math.max(root.compact ? 320 : 480, Number(availableWidth || 0))
        const horizontalInset = root.compact ? Theme.mobileGutter * 2
                                             : Theme.space3 * 2 + 42 + Theme.space3
        const contentWidth = Math.max(180, rowWidth - horizontalInset)
        const lineHeight = Math.ceil(Theme.typeBodySize * Theme.typeBodyLineHeight)
        const charactersPerLine = Math.max(18,
            Math.floor(contentWidth / Math.max(7, Theme.typeBodySize * 0.55)))
        const textLength = Math.min(600, String(value.text || "").length)
        const textLines = textLength > 0
            ? Math.max(1, Math.min(8, Math.ceil(textLength / charactersPerLine))) : 0
        const embeds = value.embeds || []
        let supportingEmbedCount = 0
        for (let index = 0; index < embeds.length; ++index) {
            if (root.classifiedFamily(embeds[index], ({})) !== "image")
                ++supportingEmbedCount
        }
        let reservation = 112 + textLines * lineHeight

        if (layoutKind === "single-image") {
            let aspect = 4 / 3
            for (let index = 0; index < embeds.length; ++index) {
                const embed = embeds[index] || ({})
                const declaredAspect = Number(embed.aspectRatio || 0)
                const declaredWidth = Number(embed.width || 0)
                const declaredHeight = Number(embed.height || 0)
                if (declaredAspect > 0) {
                    aspect = declaredAspect
                    break
                }
                if (declaredWidth > 0 && declaredHeight > 0) {
                    aspect = declaredWidth / declaredHeight
                    break
                }
            }
            aspect = Math.max(0.45, Math.min(2.8, aspect))
            const maximumHeight = root.compact ? Math.max(720, contentWidth * 1.6) : 860
            reservation += Math.max(root.compact ? 120 : 140,
                                    Math.min(maximumHeight, contentWidth / aspect)) + Theme.space2
        } else if (layoutKind === "gallery") {
            const visibleCount = Math.min(6, embeds.length)
            const gap = Theme.space2
            if (contentWidth < (root.compact ? 360 : 430)) {
                const tileHeight = Math.max(root.compact ? 118 : 138,
                    Math.min(root.compact ? 350 : 470, contentWidth / (4 / 3)))
                reservation += visibleCount * tileHeight + Math.max(0, visibleCount - 1) * gap
            } else {
                const columnWidth = (contentWidth - gap) / 2
                const tileHeight = Math.max(root.compact ? 118 : 138,
                    Math.min(root.compact ? 350 : 470, columnWidth / (4 / 3)))
                const rows = Math.ceil(visibleCount / 2)
                reservation += rows * tileHeight + Math.max(0, rows - 1) * gap
            }
        } else if (embeds.length > 0 || /https?:\/\//i.test(String(value.text || ""))) {
            // CastItem can infer a link preview directly from cast text even when
            // the protocol embed list is empty. Reserve that card before the
            // async supporting subtree is admitted so a cold link row does not
            // change ListView geometry underneath the scroll gesture.
            reservation += root.compact ? 210 : 230
        }

        if ((layoutKind === "single-image" || layoutKind === "gallery")
                && supportingEmbedCount > 0) {
            // Mixed media + link rows need room for their deferred supporting
            // cards too. Capping at two avoids pathological reservation while
            // keeping the common image+destination case geometry-stable.
            reservation += Math.min(2, supportingEmbedCount)
                         * (root.compact ? 132 : 152)
        }

        if (presentationKind === "visual")
            reservation += root.compact ? Theme.space2 : Theme.space5
        else if (presentationKind === "developer")
            reservation += Theme.space7
        return Math.max(68, Math.ceil(reservation))
    }

    function rowRevision(row) {
        const value = row || ({})
        if (value._genySection)
            return "section:" + String(value._genySection)
        if (root.socialMode) {
            return [String(value.fid || value.authorFid || 0),
                    String(value.author || ""), String(value.displayName || ""),
                    String(value.avatarUrl || "")].join("\u001f")
        }
        // Structural content receives a C++-generated revision. Mutable viewer
        // state remains explicit so optimistic Like/Recast updates refresh only
        // the affected row without serializing nested QVariant trees in JS.
        return [String(value._revision || value.castId || value.hash || value.id || ""),
                String(value._referencedRevision || ""),
                String(value.likeCount || 0), String(value.recastCount || 0),
                String(value.replyCount || 0), String(Boolean(value.likedByMe)),
                String(Boolean(value.recastedByMe)), String(value.writeState || "")].join("\u001f")
    }

    function payloadForStableKey(key) {
        return root.stablePayloads[String(key || "")] || ({})
    }

    function rebuildStableModel(source) {
        const payloads = ({})
        stableRows.clear()
        for (let index = 0; index < source.length; ++index) {
            const entry = source[index] || ({})
            const payload = entry.payload || ({})
            const key = root.rowKey(payload, index)
            payloads[key] = payload
            stableRows.append({ stableKey: key,
                                revisionKey: root.rowRevision(payload),
                                presentationKind: String(entry.presentationKind || "conversation"),
                                mediaLayoutKind: root.mediaLayoutKind(payload) })
        }
        root.stablePayloads = payloads
    }

    function requestStableModelSync() {
        if (!root.presentationActive) {
            root.stableModelSyncPending = true
            return
        }
        if (readList.moving) {
            root.stableModelSyncPending = true
            return
        }
        root.stableModelSyncPending = false
        root.synchronizeStableModel()
    }

    function synchronizeStableModel() {
        const source = root.sourceRows()

        // Keep the model incremental. Tail pagination should only append rows;
        // in-place payload revisions update the existing delegate and never
        // reposition the ListView. Qt ListView owns viewport continuity.
        for (let index = 0; index < source.length; ++index) {
            const entry = source[index] || ({})
            const payload = entry.payload || ({})
            const presentationKind = String(entry.presentationKind || "conversation")
            const key = root.rowKey(payload, index)
            const revision = root.rowRevision(payload)
            // Mutate the side store before changing revisionKey. Delegates bind
            // through revisionKey, so only the affected row re-reads its payload.
            root.stablePayloads[key] = payload
            if (index < stableRows.count && String(stableRows.get(index).stableKey) === key) {
                const revisionChanged = String(stableRows.get(index).revisionKey || "") !== revision
                const presentationChanged = String(stableRows.get(index).presentationKind || "") !== presentationKind
                if (revisionChanged)
                    stableRows.setProperty(index, "revisionKey", revision)
                if (presentationChanged)
                    stableRows.setProperty(index, "presentationKind", presentationKind)
                if (revisionChanged)
                    stableRows.setProperty(index, "mediaLayoutKind", root.mediaLayoutKind(payload))
                continue
            }

            let existingIndex = -1
            for (let probe = index + 1; probe < stableRows.count; ++probe) {
                if (String(stableRows.get(probe).stableKey) === key) {
                    existingIndex = probe
                    break
                }
            }
            if (existingIndex >= 0) {
                stableRows.move(existingIndex, index, 1)
                const revisionChanged = String(stableRows.get(index).revisionKey || "") !== revision
                const presentationChanged = String(stableRows.get(index).presentationKind || "") !== presentationKind
                if (revisionChanged)
                    stableRows.setProperty(index, "revisionKey", revision)
                if (presentationChanged)
                    stableRows.setProperty(index, "presentationKind", presentationKind)
                if (revisionChanged)
                    stableRows.setProperty(index, "mediaLayoutKind", root.mediaLayoutKind(payload))
            } else {
                stableRows.insert(index, { stableKey: key, revisionKey: revision,
                                           presentationKind: presentationKind,
                                           mediaLayoutKind: root.mediaLayoutKind(payload) })
            }
        }

        while (stableRows.count > source.length)
            stableRows.remove(stableRows.count - 1)
    }

    function isNearEnd() {
        if (readList.count === 0 || readList.contentHeight <= 0)
            return false
        if (readList.atYEnd)
            return true
        // ListView originY is not guaranteed to be zero for variable-height
        // delegates. Normalize contentY before comparing with contentHeight.
        const normalizedY = Math.max(0, readList.contentY - readList.originY)
        const preloadDistance = Math.max(720, readList.height * 0.95)
        return normalizedY + readList.height >= readList.contentHeight - preloadDistance
    }

    function needsFilteredPrefetch() {
        if (root.presentationMode === "visuals" && root.supplementalBusy)
            return false
        if (!root.presentationFilterActive || root.busy || root.loadMorePending || !root.hasMore
                || root.offline || root.sourceUnavailable || root.errorText.length > 0)
            return false
        if (root.filterPrefetchPages >= root.filterPrefetchLimit
                || root.automaticPrefetchPages >= root.automaticPrefetchLimit)
            return false
        // stableRows is the already-classified presentation snapshot. Re-running
        // sourceRows() from the scroll hot path repeats media/link classification.
        return stableRows.count < root.filterTargetCount
    }

    function scheduleLoadMore() {
        if (!root.presentationActive)
            return
        // Rich feed delegates may resolve media and change height while a flick is
        // active. Never append/cache-reconcile a page in the middle of that motion;
        // movementEnded schedules the same work once the viewport is stable.
        if (readList.moving && stableRows.count > 0)
            return
        if (root.offline || root.sourceUnavailable || root.errorText.length > 0) {
            filterPrefetchDebounce.stop()
            loadMoreDebounce.stop()
            return
        }
        if (root.needsFilteredPrefetch()) {
            filterPrefetchDebounce.restart()
            return
        }
        if (root.loadMorePending || root.busy || !root.hasMore || !root.isNearEnd())
            return
        loadMoreDebounce.restart()
    }

    function sectionTitle(key) {
        if (key === "visual")
            return qsTrId("genycaster.home.visual_highlights")
        if (key === "developer")
            return qsTrId("genycaster.home.builder_notes")
        return qsTrId("genycaster.home.conversations")
    }

    onRowsChanged: Qt.callLater(function() {
        if (!root.presentationActive) {
            root.stableModelSyncPending = true
            return
        }
        root.requestStableModelSync()
        root.scheduleLoadMore()
    })
    onSupplementalRowsChanged: Qt.callLater(function() {
        if (root.presentationActive && root.presentationMode === "visuals") {
            root.primeArtPreviews()
            root.requestStableModelSync()
        }
    })
    onSupplementalBusyChanged: {
        if (!root.supplementalBusy && root.presentationMode === "visuals")
            Qt.callLater(root.scheduleLoadMore)
    }
    onSocialModeChanged: {
        stableRows.clear()
        root.resetAllFilterPrefetchBudgets()
        Qt.callLater(root.requestStableModelSync)
    }
    onActiveFeedChanged: {
        // Following and Trending own independent continuation streams. A real
        // source change starts a fresh bounded semantic-prefetch budget; local
        // Explorer tab switches no longer change activeFeed.
        root.resetAllFilterPrefetchBudgets()
    }
    onPresentationModeChanged: {
        filterPrefetchDebounce.stop()
        // Each semantic tab visit gets a fresh bounded continuation budget.
        // Without this, a sparse Devs/NFTs visit could permanently exhaust its
        // two-page allowance and only ever show old cached matches afterward.
        root.resetCurrentFilterPrefetchBudget()
        stableRows.clear()
        Qt.callLater(function() {
            if (root.presentationMode === "visuals")
                root.primeArtPreviews()
            root.requestStableModelSync()
            root.scheduleLoadMore()
        })
    }
    onVisualFilterChanged: {
        if (root.presentationMode !== "visuals")
            return
        filterPrefetchDebounce.stop()
        stableRows.clear()
        Qt.callLater(function() {
            root.requestStableModelSync()
            root.scheduleLoadMore()
        })
    }
    onHasMoreChanged: {
        if (!hasMore) {
            loadMorePending = false
            loadMoreDebounce.stop()
            loadMoreRelease.stop()
        } else {
            Qt.callLater(root.scheduleLoadMore)
        }
    }
    onBusyChanged: {
        if (busy) {
            loadMoreRelease.stop()
        } else {
            loadMorePending = false
            Qt.callLater(root.scheduleLoadMore)
        }
    }
    onPresentationActiveChanged: {
        if (!root.presentationActive) {
            previewSyncDebounce.stop()
            filterPrefetchDebounce.stop()
            loadMoreDebounce.stop()
            return
        }
        Qt.callLater(function() {
            if (root.presentationMode === "visuals")
                root.primeArtPreviews()
            root.requestStableModelSync()
            root.scheduleLoadMore()
        })
    }
    Component.onCompleted: requestStableModelSync()

    Timer {
        id: previewSyncDebounce
        interval: 80
        repeat: false
        onTriggered: {
            if (readList.moving) {
                root.previewSyncPending = true
                return
            }
            root.previewSyncPending = false
            root.requestStableModelSync()
        }
    }

    Connections {
        target: typeof linkPreviewResolver !== "undefined" ? linkPreviewResolver : null
        function onPreviewReady(url, preview) {
            // Metadata may change NFT/media classification. Never run the full
            // semantic partition while kinetic scrolling owns the frame budget.
            // Coalesce all completions into one reconciliation after settle.
            if (!root.presentationActive || root.presentationMode !== "visuals")
                return
            if (readList.moving) {
                root.previewSyncPending = true
                return
            }
            previewSyncDebounce.restart()
        }
    }

    ListModel {
        id: stableRows
    }

    Timer {
        id: automaticPrefetchCooldown
        interval: 1400
        repeat: false
        onTriggered: {
            // Rate-limit automatic semantic pagination as a short rolling window
            // rather than permanently starving tabs visited later in the session.
            root.automaticPrefetchPages = 0
            root.scheduleLoadMore()
        }
    }

    Timer {
        id: filterPrefetchDebounce
        interval: 240
        repeat: false
        onTriggered: {
            if (!root.needsFilteredPrefetch())
                return
            root.incrementFilterPrefetchPages()
            root.loadMorePending = true
            root.loadMoreRequested()
            loadMoreRelease.restart()
        }
    }

    Timer {
        id: loadMoreDebounce
        interval: 90
        repeat: false
        onTriggered: {
            if (root.loadMorePending || root.busy || !root.hasMore || !root.isNearEnd()
                    || root.offline || root.sourceUnavailable || root.errorText.length > 0)
                return
            root.loadMorePending = true
            root.loadMoreRequested()
            loadMoreRelease.restart()
        }
    }

    Timer {
        id: loadMoreRelease
        interval: 700
        repeat: false
        onTriggered: {
            if (!root.busy) {
                root.loadMorePending = false
                root.scheduleLoadMore()
            }
        }
    }

    Timer {
        id: scrollWorkThrottle
        interval: 96
        repeat: false
        onTriggered: root.scheduleLoadMore()
    }

    ListView {
        id: readList
        objectName: "readList"
        anchors.fill: parent
        clip: true
        model: stableRows
        spacing: 0
        // A bounded timeline must not stretch beyond the visible viewport:
        // rubber-banding exposed a large empty area below short/filtered feeds.
        // Refresh remains available from the FeedHeader action.
        boundsBehavior: Flickable.StopAtBounds
        boundsMovement: Flickable.StopAtBounds
        headerPositioning: root.compact || stableRows.count === 0
                           ? ListView.InlineHeader : ListView.OverlayHeader
        reuseItems: true
        // Keep only a shallow reusable runway. Rich embeds, animated emoji and
        // image masks are expensive delegate trees; pagination continuity must
        // come from stable model reconciliation rather than a large offscreen cache.
        // Keep fewer heavyweight offscreen Cast delegates alive on phones.
        // Incubation and delegate reuse still provide continuity while flicking.
        cacheBuffer: root.compact ? Math.min(Math.max(height * 0.33, 240), 440)
                                  : Math.min(Math.max(height * 0.65, 480), 820)
        pixelAligned: false

        onMovementEnded: {
            scrollWorkThrottle.stop()
            if (root.stableModelSyncPending)
                root.requestStableModelSync()
            if (root.previewSyncPending) {
                root.previewSyncPending = false
                previewSyncDebounce.restart()
            }
            root.scheduleLoadMore()
        }
        // Kinetic scrolling itself performs no pagination work. MovementEnded
        // is the single scroll-triggered admission point; geometry/viewport changes
        // only schedule work while the view is already at rest. This removes a
        // periodic JavaScript timer from every active flick.
        onContentHeightChanged: {
            if (!readList.moving && !scrollWorkThrottle.running)
                scrollWorkThrottle.start()
        }
        onHeightChanged: {
            if (!readList.moving && !scrollWorkThrottle.running)
                scrollWorkThrottle.start()
        }


        header: Item {
            id: pinnedHeader
            width: readList.width
            // Reserve the new-post affordance in the list header itself so the
            // first cast can never slide underneath it. The visible banner is a
            // viewport overlay that tracks this slot until it reaches the top,
            // then remains pinned while older posts continue scrolling.
            implicitHeight: feedHeaderLoader.implicitHeight + communityNotice.height + newPostsSlot.height
            z: 20

            Rectangle {
                anchors.fill: parent
                color: Theme.background
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Theme.strokeHairline
                color: Theme.divider
            }

            Loader {
                id: feedHeaderLoader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                sourceComponent: root.headerContent
            }

            DirectionAwareLabel {
                id: communityNotice
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: feedHeaderLoader.bottom
                height: visible ? 36 : 0
                visible: root.communityFallback && root.activeFeed === "trending" && !root.socialMode
                text: qsTrId("genycaster.feed.trending_is_temporarily_unavailable_showing_live_community_posts_instead")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Theme.textStartAlignment
                wrapMode: Text.WordWrap
            }

            Item {
                id: newPostsSlot
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: communityNotice.bottom
                height: root.pendingNewPostCount > 0 && !root.socialMode ? 52 : 0
                visible: height > 0
            }
        }

        // Pool structurally similar rows separately. Reusing an image row for
        // a link (and back) synchronously tears down/rebuilds its media subtree
        // inside the Flickable animation step.
        delegate: DelegateModels.DelegateChooser {
            role: "mediaLayoutKind"
            DelegateModels.DelegateChoice {
                roleValue: "single-image"
                delegate: FeedRow {}
            }
            DelegateModels.DelegateChoice {
                roleValue: "gallery"
                delegate: FeedRow {}
            }
            DelegateModels.DelegateChoice {
                delegate: FeedRow {}
            }
        }

        component FeedRow: Loader {
            id: rowDelegate
            objectName: "feedRow"
            required property string presentationKind
            required property string mediaLayoutKind
            required property string stableKey
            required property string revisionKey
            required property int index
            readonly property var payload: {
                const revision = rowDelegate.revisionKey
                return root.payloadForStableKey(rowDelegate.stableKey)
            }
            readonly property bool sectionRow: Boolean(payload && payload._genySection)
            readonly property bool incubating: status === Loader.Loading || item === null
            readonly property real reservedHeight: root.rowIncubationHeight(
                                                       payload, presentationKind,
                                                       mediaLayoutKind, width)
            readonly property real resolvedHeight: item && item.implicitHeight > 0
                                                   ? item.implicitHeight : 0
            property real committedHeight: reservedHeight

            function synchronizeHeight() {
                // Rows may hydrate a quoted parent, link preview or image while
                // the user scrolls. Grow immediately to preserve hit testing;
                // only shrink at rest to avoid avoidable scroll jumps.
                if (rowDelegate.resolvedHeight > rowDelegate.committedHeight
                        || rowDelegate.resolvedHeight > 0 && !readList.moving) {
                    rowDelegate.committedHeight = rowDelegate.resolvedHeight
                } else if (rowDelegate.incubating || rowDelegate.resolvedHeight <= 0) {
                    rowDelegate.committedHeight = rowDelegate.reservedHeight
                }
            }

            width: readList.width
            // Reused delegates stay alive in ListView's pool. Explicitly hide
            // pooled rows so inherited visibility pauses animated emoji, spinners,
            // and other presentation-only work until the row is reused.
            ListView.onPooled: rowDelegate.visible = false
            ListView.onReused: rowDelegate.visible = true
            // ListView must never see a zero-height row while Qt spreads the
            // expensive Cast subtree construction across multiple frames.
            // Commit the measured height only while the viewport is at rest;
            // completing incubation mid-flick cannot move rows under the pointer.
            height: Math.max(1, committedHeight)
            asynchronous: !sectionRow
            sourceComponent: rowDelegate.sectionRow ? sectionRowComponent
                           : root.socialMode ? profileRowComponent : castRowComponent
            onReservedHeightChanged: rowDelegate.synchronizeHeight()
            onResolvedHeightChanged: rowDelegate.synchronizeHeight()
            onStableKeyChanged: rowDelegate.committedHeight = rowDelegate.reservedHeight

            Connections {
                target: readList
                function onMovingChanged() {
                    if (!readList.moving)
                        rowDelegate.synchronizeHeight()
                }
            }

            Component {
                id: sectionRowComponent
                Item {
                    implicitHeight: semanticSection.implicitHeight + Theme.space4

                    DirectionAwareLabel {
                        id: semanticSection
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: root.compact ? Theme.mobileGutter : Theme.space3
                        anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space3
                        anchors.top: parent.top
                        anchors.topMargin: Theme.space4
                        text: root.sectionTitle(String(rowDelegate.payload._genySection || ""))
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightDemiBold
                    }
                }
            }

            Component {
                id: castRowComponent
                SemanticFeedItem {
                    implicitWidth: rowDelegate.width
                    modelData: rowDelegate.payload
                    rowIndex: rowDelegate.index
                    presentationKind: rowDelegate.presentationKind
                    compact: root.compact
                    interactionsAvailable: root.interactionsAvailable
                    accountFid: root.accountFid
                    onOpenRequested: castId => root.castRequested(castId)
                    onProfileRequested: fid => root.profileRequested(fid)
                    onReplyRequested: cast => root.replyRequested(cast)
                    onReactionRequested: (kind, cast, active) => root.reactionRequested(kind, cast, active)
                    onRemoveRequested: cast => root.removeCastRequested(cast)
                    onExternalRequested: url => root.externalRequested(url)
                    onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                    onAuthenticationRequested: root.authenticationRequested()
                }
            }

            Component {
                id: profileRowComponent
                ProfileListItem {
                    implicitWidth: rowDelegate.width
                    modelData: rowDelegate.payload
                    onOpenRequested: fid => root.profileRequested(fid)
                }
            }
        }

        footer: Item {
            width: readList.width
            // The empty state is part of the ListView content, AFTER the
            // inline header, so filters/tabs never overlap its title or CTA.
            // With short feeds keep the footer large enough for the content to
            // be scrolled into view on phones. Populated feeds retain a fixed
            // footer height, independent of asynchronous pagination.
            height: stableRows.count === 0
                    ? Math.max(feedEmptyState.implicitHeight,
                               readList.height - (readList.headerItem
                                  ? readList.headerItem.implicitHeight : 0), 280)
                    : (root.compact ? 116 : 56)

            EmptyState {
                id: feedEmptyState
                filtered: root.presentationFilterActive
                moreFilteredAvailable: root.hasMore
                                       && (root.filterPrefetchPages >= root.filterPrefetchLimit
                                           || root.automaticPrefetchPages >= root.automaticPrefetchLimit)
                onMoreFilteredRequested: {
                    // Explicit user intent may start a new bounded search window. Automatic
                    // tab switching itself never resets the shared source-pressure budget.
                    root.resetCurrentFilterPrefetchBudget()
                    root.automaticPrefetchPages = 0
                    root.scheduleLoadMore()
                }
                anchors.fill: parent
                visible: stableRows.count === 0
                         && !(root.runtimeReady && (root.busy || root.supplementalBusy
                                                       || root.needsFilteredPrefetch() || filterPrefetchDebounce.running))
                runtimeReady: root.runtimeReady
                busy: root.busy
                offline: root.offline
                sourceUnavailable: root.sourceUnavailable
                errorText: root.errorText
                activeFeed: root.activeFeed
                identityAvailable: root.identityAvailable
                feedStarted: root.feedStarted
                onRetryRequested: {
                    root.resetAllFilterPrefetchBudgets()
                    root.retryRequested()
                }
                onIdentityRequested: root.identityRequested()
                onFollowingRequested: root.followingRequested()
                onTrendingRequested: root.trendingRequested()
            }

            GenySpinner {
                anchors.centerIn: parent
                visible: root.busy && stableRows.count > 0
                running: visible && !readList.moving
                Accessible.name: qsTrId("genycaster.feed_surface.loading_more")
            }
        }
    }

    Item {
        id: newPostsBanner
        anchors.left: parent.left
        anchors.right: parent.right
        height: 52
        z: 31
        visible: root.pendingNewPostCount > 0 && !root.socialMode

        // Depend explicitly on contentY so mapToItem() is reevaluated as the
        // ListView scrolls. Before the header leaves the viewport the banner
        // occupies its reserved slot; afterwards it sticks to the feed top.
        y: {
            const scrollRevision = readList.contentY
            // A ListView header is a separately instantiated component: its
            // ids are not in scope from this sibling overlay. Use the actual
            // headerItem geometry and the header's reserved banner slot.
            const header = readList.headerItem
            if (!header || !root.pendingNewPostCount || root.socialMode)
                return 0
            return Math.max(0, header.mapToItem(root, 0, header.height - newPostsBanner.height).y)
        }

        HoverHandler { id: newPostsHover }

        Superellipse {
            anchors.fill: parent
            anchors.leftMargin: root.compact ? Theme.mobileGutter : Theme.space3
            anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space3
            anchors.topMargin: Theme.space1
            anchors.bottomMargin: Theme.space1
            fillColor: newPostsHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
            borderWidth: 0
            curveDepth: Theme.shapeControlSmall
            highQualityEdges: true
        }

        DirectionAwareLabel {
            anchors.centerIn: parent
            text: qsTrId("genycaster.feed_surface.show_new_posts")
                    .arg(LocaleFormat.formatInteger(root.pendingNewPostCount))
            color: Theme.accent
            font.pixelSize: Theme.typeBodySize
            font.weight: Theme.weightDemiBold
        }

        TapHandler {
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: {
                root.revealPendingPostsRequested()
                Qt.callLater(() => readList.positionViewAtBeginning())
            }
        }
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: readList.headerItem ? readList.headerItem.implicitHeight : 0
        visible: stableRows.count === 0 && root.runtimeReady
                 && (root.busy || root.supplementalBusy || root.needsFilteredPrefetch() || filterPrefetchDebounce.running)
        Repeater {
            model: root.presentationMode === "visuals" ? 3 : 4
            delegate: CastSkeleton {
                width: root.width
                mediaPreview: root.presentationMode === "visuals"
            }
        }
    }


}
