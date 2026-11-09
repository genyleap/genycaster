import QtQuick
import QtQuick.Controls
import QtQml.Models
import Qt.labs.qmlmodels as DelegateModels
import Farcaster.Client

Item {
    id: root

    property var rows
    property var supplementalRows: []
    property bool supplementalBusy: false
    property bool runtimeReady: false
    property bool socialMode: false
    property bool explorerLayoutAvailable: false
    property string explorerLayout: "magazine"
    readonly property bool magazineEnabled: explorerLayoutAvailable && !compact
        && !socialMode && explorerLayout !== "timeline"
    readonly property var readList: magazineEnabled ? magazineView : timelineList
    onMagazineEnabledChanged: layoutSwitchTimer.restart()
    Timer {
        id: layoutSwitchTimer
        interval: 0
        onTriggered: {
            root.cancelViewportCorrections()
            root.readList.positionViewAtBeginning()
        }
    }
    property bool busy: false
    property bool hasMore: false
    // Cached local rows may continue even while the provider is offline.
    // The bridge always consumes them before the independent network cursor.
    property bool cachedMore: false
    property bool offline: false
    property bool sourceUnavailable: false
    property string errorText: ""
    property string activeFeed: "trending"
    property bool communityFallback: false
    property bool localDerivedRecent: false
    property bool localDerivedFollowing: false
    // This is a protocol signer-key filter, NOT a verified application FID.
    // Unattributed casts stay unknown; provider-supplied client badges are
    // never treated as proof for include/exclude decisions.
    property string signerFilterMode: "all"
    property string selectedSignerKey: ""
    readonly property var knownSignerKeys: root.collectSignerKeys()
    property string appFilterMode: "all"
    property string selectedAppFid: ""
    readonly property var knownAppFids: root.collectAppFids()
    property bool identityAvailable: false
    property bool feedStarted: false
    property bool interactionsAvailable: false
    property bool compact: false
    property real bottomOverlayInset: 0
    property bool presentationActive: true
    // A main-navigation Explorer tap requests a new top position even if this
    // reader remained mounted behind an intermediate profile or cast.
    property int scrollRequestRevision: 0
    property int appliedScrollRequestRevision: 0
    // One-shot return coordinate belongs to the tapped Cast, not a guessed
    // scroll offset. It survives the hidden detail route and row revisions.
    property string returnCastKey: ""
    property string returnCastScope: ""
    property real returnCastOffset: 0
    property real returnCastY: 0
    property bool returnCastPending: false
    property int returnCastAttempts: 0
    function rememberCastForReturn(castId) {
        const hash = String(castId || "").trim().toLowerCase()
        if (!hash) return
        root.returnCastKey = "cast:" + hash
        root.returnCastScope = root.activeFeed + ":" + root.presentationMode + ":" + root.socialMode
        root.returnCastY = readList.contentY
        root.returnCastOffset = 0
        root.returnCastAttempts = 0
        root.returnCastPending = true
        for (let index = 0; index < stableRows.count; ++index) {
            if (String(stableRows.get(index).stableKey).toLowerCase() !== root.returnCastKey)
                continue
            const item = readList.itemAtIndex(index)
            if (item) root.returnCastOffset = item.y - readList.contentY
            break
        }
    }
    function restoreCastOnReturn() {
        if (!root.returnCastPending || !root.presentationActive) return
        const scope = root.activeFeed + ":" + root.presentationMode + ":" + root.socialMode
        if (root.returnCastScope !== scope) {
            root.returnCastPending = false
            return
        }
        let index = -1
        for (let i = 0; i < stableRows.count; ++i) {
            if (String(stableRows.get(i).stableKey).toLowerCase() === root.returnCastKey) {
                index = i
                break
            }
        }
        root.applyingViewportRestore = true
        if (index < 0) {
            readList.contentY = root.returnCastY
        } else {
            readList.forceLayout()
            const item = readList.itemAtIndex(index)
            if (!item && root.returnCastAttempts++ < 2) {
                readList.positionViewAtIndex(index, ListView.Beginning)
                root.applyingViewportRestore = false
                castReturnRestoreTimer.restart()
                return
            }
            readList.contentY = item ? item.y - root.returnCastOffset : root.returnCastY
        }
        root.applyingViewportRestore = false
        root.returnCastPending = false
    }
    function honorPrimaryScrollRequest() {
        if (!root.presentationActive || root.scrollRequestRevision === 0
                || root.appliedScrollRequestRevision === root.scrollRequestRevision)
            return
        root.appliedScrollRequestRevision = root.scrollRequestRevision
        root.scrollToBeginning()
    }
    onScrollRequestRevisionChanged: primaryScrollRequestTimer.restart()
    property double accountFid: 0
    property bool loadMorePending: false
    property bool stableModelSyncPending: false
    property bool deferredFeedSync: false
    property bool deferredArtPrime: false
    property bool deferredPageCheck: false
    // A provider/category change may briefly clear the projected model before
    // the bridge reports busy. Reserve the loading surface for that handoff
    // so no empty-state flash appears between selection and real data.
    property bool selectionSettling: false
    // Sparse semantic searches can remain pending or keep prefetching without
    // ever producing a matching cast. Never leave the user staring at an
    // indefinite skeleton: expose a retryable state after a bounded interval
    // while allowing a late successful provider response to populate the feed.
    property bool semanticWaitExpired: false
    function restartSemanticDeadline() {
        semanticLoadDeadline.stop()
        root.semanticWaitExpired = false
        if (root.runtimeReady && root.presentationActive
                && root.presentationMode !== "for-you"
                && root.presentationMode !== "following")
            semanticLoadDeadline.start()
    }
    readonly property bool waitingForFirstRows: root.runtimeReady
        && !root.semanticWaitExpired
        && (root.busy || root.supplementalBusy || root.selectionSettling
            || root.needsFilteredPrefetch() || filterPrefetchDebounce.running)
    Timer {
        id: semanticLoadDeadline
        interval: 12000
        repeat: false
        onTriggered: root.semanticWaitExpired = true
    }
    Timer {
        id: selectionSettleTimer
        interval: 165
        repeat: false
        onTriggered: root.selectionSettling = false
    }
    function beginSelectionHandoff() {
        root.selectionSettling = true
        selectionSettleTimer.restart()
    }
    // Complex QVariantMap/QJSValue payloads must not live inside ListModel roles:
    // Qt fixes a role's type from its first value and later C++ QVariantMap vs
    // JS object/list wrappers can otherwise be rejected. Keep only primitive,
    // type-stable roles in ListModel and store row payloads externally by key.
    property var stablePayloads: ({})
    property string pendingViewportKey: ""
    property real pendingViewportOffset: 0
    property real pendingViewportY: 0
    property string pendingViewportScope: ""
    property bool pendingViewportSecondPass: false
    property string geometryAnchorKey: ""
    property real geometryAnchorOffset: 0
    property bool applyingGeometryAnchor: false
    property bool applyingViewportRestore: false
    property bool previewSyncPending: false
    property string presentationMode: "for-you"
    property bool topicFilterActive: false
    property string topicFilterSlug: ""
    property string visualFilter: "nfts"
    property int forYouPrefetchPages: 0
    property int visualPrefetchPages: 0
    property int conversationPrefetchPages: 0
    property int developerPrefetchPages: 0
    property int automaticPrefetchPages: 0
    readonly property int filterPrefetchPages: (root.presentationMode === "for-you" || root.presentationMode === "following")
                                               ? root.forYouPrefetchPages
                                               : root.presentationMode === "visuals"
                                                 ? root.visualPrefetchPages
                                                 : root.presentationMode === "devs"
                                                   ? root.developerPrefetchPages
                                                   : root.presentationMode === "conversations"
                                                     ? root.conversationPrefetchPages : 0
    property int pendingNewPostCount: 0
    // Explorer presents a complete root-cast timeline in For You and optional
    // semantic subsets in the other tabs. Provider order remains authoritative
    // within every view; QML must not re-rank a paginated timeline.
    readonly property bool curateHomeForYou: false
    readonly property bool presentationFilterActive: !root.socialMode
    readonly property int filterTargetCount: root.compact ? 5 : 8
    readonly property int filterPrefetchLimit: 8
    readonly property int automaticPrefetchLimit: 16
    // One bounded automatic chain between real user scroll gestures. Content
    // geometry and busy-state changes must not replenish the budget.
    property int automaticLastRowCount: -1
    property int automaticNoProgressPages: 0
    readonly property int automaticNoProgressLimit: 2
    property Component headerContent: null
    // Presentation filters preserve provider order; they only select eligible
    // rows. Pagination therefore remains provider-stable across every branch.

    signal castRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
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
            // Some media origins return bare URLs for real NFT photographs
            // (notably imagedelivery.net and IPFS) without a file extension.
            // The selected read route's art/nft channel must still pass the collectible
            // classifier below; only the media candidate detection expands.
            if (directArtwork.length > 0 || kind === "image" || family === "image"
                    || root.looksLikeArtworkUrl(value)
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
        const row = data || ({})
        const text = String(row.text || "")
        const channel = String(row.channelId || "").toLowerCase()
        const explicitNft = /(?:#nft\b|\bnfts\b|\bnft\b|\bmint(?:ed|ing)?\b|\bonchain art\b|\bdigital collectible\b|\berc[- ]?(?:721|1155)\b)/i.test(text)
        const artContext = /(?:\bart(?:work|ist|ists)?\b|\bcryptoart\b|\billustrat(?:ion|or)\b|\bphotograph(?:y|er)\b|\bpaint(?:ing|ed)?\b|\bdigital creation\b|\bonchain\b)/i.test(text)
        // NFTs is a visual/collectible discovery surface, not a keyword search.
        // A bare "NFT" mention or ordinary mint discussion with no artwork or
        // actual collection destination must never qualify as an art piece.
        if (root.containsCollectibleLink(row))
            return true
        if (!root.rowHasImage(row))
            return false
        return root.isArtChannelCast(row) || root.hasArtChannelMediaCandidate(row)
                || explicitNft || artContext
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
        // A conversation is a canonical reply, not a question, mention or
        // an unrelated root post with an engagement counter.
        return Boolean(row.isReply) || String(row.parentCastId || "").length > 0
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
        const channel = String(row.channelId || "").toLowerCase()
        if (["dev", "developers", "build", "builders", "programming",
             "code", "opensource", "engineering", "hackathon"].indexOf(channel) >= 0)
            return true
        return haystack.indexOf("github.com/") >= 0
                || haystack.indexOf("raw.githubusercontent.com/") >= 0
                || haystack.indexOf("gitlab.com/") >= 0
                || haystack.indexOf("npmjs.com/") >= 0
                || haystack.indexOf("crates.io/") >= 0
                || /(?:#(?:dev|build|coding|opensource|programming)\b|\bopen[ -]source\b|\bshipp(?:ed|ing)\b|\bsoftware engineering\b|\bdeveloper tools?\b|\bapi\b|\bsdk\b|\bprogramming\b|\bcodebase\b|\brepository\b|\bsmart contracts?\b)/i.test(haystack)
    }

    function semanticKind(data) {
        // Semantic subsets classify a cast by its strongest evidence; For You
        // intentionally remains the complete stream instead of a leftover bucket.
        // Generic discovery posts stay out of the narrower Conversation tab.
        if (root.hasCollectible(data))
            return "visuals"
        if (root.looksDeveloper(data))
            return "devs"
        if (root.isConversation(data))
            return "conversations"
        return "other"
    }

    function incrementFilterPrefetchPages() {
        if ((root.presentationMode === "for-you" || root.presentationMode === "following"))
            ++root.forYouPrefetchPages
        else if (root.presentationMode === "visuals")
            ++root.visualPrefetchPages
        else if (root.presentationMode === "devs")
            ++root.developerPrefetchPages
        else if (root.presentationMode === "conversations")
            ++root.conversationPrefetchPages
        root.recordAutomaticPage()
    }

    function canAutomaticallyPage() {
        return root.automaticPrefetchPages < root.automaticPrefetchLimit
            && root.automaticNoProgressPages < root.automaticNoProgressLimit
    }

    function recordAutomaticPage() {
        if (root.automaticLastRowCount >= 0
                && stableRows.count <= root.automaticLastRowCount)
            ++root.automaticNoProgressPages
        else
            root.automaticNoProgressPages = 0
        root.automaticLastRowCount = stableRows.count
        ++root.automaticPrefetchPages
    }

    function resetAutomaticPageBudget() {
        root.automaticPrefetchPages = 0
        root.automaticNoProgressPages = 0
        root.automaticLastRowCount = -1
    }

    function resetCurrentFilterPrefetchBudget() {
        if ((root.presentationMode === "for-you" || root.presentationMode === "following"))
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
        root.resetAutomaticPageBudget()
    }

    function collectSignerKeys() {
        const items = root.rows || []
        const seen = ({})
        const keys = []
        for (let index = 0; index < items.length; ++index) {
            const key = String((items[index] || ({})).signerPublicKeyHex || "").toLowerCase()
            if (/^0x[0-9a-f]{64}$/.test(key) && !seen[key]) {
                seen[key] = true
                keys.push(key)
            }
        }
        keys.sort()
        return keys
    }

    function collectAppFids() {
        const found = ({})
        const values = []
        const rows = root.rows || []
        for (let i = 0; i < rows.length; ++i) {
            const fid = String((rows[i] || ({})).sourceAppFid || "")
            if (/^[1-9][0-9]*$/.test(fid) && !found[fid]) {
                found[fid] = true
                values.push(fid)
            }
        }
        values.sort(function(a, b) {
            return a.length === b.length ? (a < b ? -1 : a > b ? 1 : 0)
                : a.length - b.length
        })
        return values
    }

    function rowMatchesAppFilter(row) {
        if (root.appFilterMode === "all")
            return true
        if (!/^[1-9][0-9]*$/.test(root.selectedAppFid))
            return root.appFilterMode === "exclude"
        const actual = String((row || ({})).sourceAppFid || "")
        return root.appFilterMode === "include"
            ? actual === root.selectedAppFid : actual !== root.selectedAppFid
    }

    function rowMatchesSignerFilter(row) {
        if (root.signerFilterMode === "all")
            return true
        const signer = String((row || ({})).signerPublicKeyHex || "").toLowerCase()
        if (!/^0x[0-9a-f]{64}$/.test(root.selectedSignerKey))
            return root.signerFilterMode === "exclude"
        // Unknown signers do NOT match an include filter. They remain
        // visible in exclude mode because there is no proof of equality.
        return root.signerFilterMode === "include"
            ? signer === root.selectedSignerKey : signer !== root.selectedSignerKey
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
        if (root.presentationMode === "visuals" && root.activeFeed !== "nfts") {
            // A topic-backed NFT search is authoritative. Never append stale
            // unrelated artwork discovered for an older generic feed.
            const artRows = root.supplementalRows || []
            for (let index = 0; index < artRows.length; ++index)
                appendUnique(artRows[index])
        }

        const result = []
        for (let index = 0; index < source.length; ++index) {
            const row = source[index]
            if (!root.topicFilterActive
                    && (!root.rowMatchesSignerFilter(row) || !root.rowMatchesAppFilter(row)))
                continue
            if (root.topicFilterActive) {
                result.push({payload: row, presentationKind: "conversation"})
            } else if ((root.presentationMode === "for-you" || root.presentationMode === "following")) {
                // The main Explorer timeline is a complete, provider-ordered
                // stream of recent root casts, not a residual bucket after
                // NFT/Devs/Conversation classification. Filtering those out
                // made new posts disappear from the primary feed.
                if (root.isRootPost(row))
                    result.push({ payload: row, presentationKind: "conversation" })
            } else if (root.presentationMode === "visuals") {
                if (root.hasCollectible(row))
                    result.push({ payload: row, presentationKind: "visual" })
            } else if (root.presentationMode === "devs") {
                if (root.activeFeed === "developers" || root.looksDeveloper(row))
                    result.push({ payload: row, presentationKind: "developer" })
            } else if (root.presentationMode === "conversations") {
                // Following is an audience scope, not a content type. When the
                // user selects Following + Conversations the provider source is
                // "following", but its verified replies still belong here.
                if (root.isConversation(row))
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

        const compactCard = root.compact || root.magazineEnabled
        const rowWidth = Math.max(compactCard ? 320 : 480, Number(availableWidth || 0))
        const horizontalInset = compactCard ? Theme.mobileGutter * 2
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
            const maximumHeight = compactCard ? Math.max(720, contentWidth * 1.6) : 860
            reservation += Math.max(compactCard ? 120 : 140,
                                    Math.min(maximumHeight, contentWidth / aspect)) + Theme.space2
        } else if (layoutKind === "gallery") {
            const visibleCount = Math.min(6, embeds.length)
            const gap = Theme.space2
            if (contentWidth < (compactCard ? 360 : 430)) {
                const tileHeight = Math.max(compactCard ? 118 : 138,
                    Math.min(compactCard ? 350 : 470, contentWidth / (4 / 3)))
                reservation += visibleCount * tileHeight + Math.max(0, visibleCount - 1) * gap
            } else {
                const columnWidth = (contentWidth - gap) / 2
                const tileHeight = Math.max(compactCard ? 118 : 138,
                    Math.min(compactCard ? 350 : 470, columnWidth / (4 / 3)))
                const rows = Math.ceil(visibleCount / 2)
                reservation += rows * tileHeight + Math.max(0, rows - 1) * gap
            }
        } else if (embeds.length > 0 || /https?:\/\//i.test(String(value.text || ""))) {
            // CastItem can infer a link preview directly from cast text even when
            // the protocol embed list is empty. Reserve that card before the
            // async supporting subtree is admitted so a cold link row does not
            // change ListView geometry underneath the scroll gesture.
            reservation += root.magazineEnabled
                ? Math.min(300, contentWidth / (16 / 9)) + 132 : compactCard ? 210 : 230
        }

        if ((layoutKind === "single-image" || layoutKind === "gallery")
                && supportingEmbedCount > 0) {
            // Mixed media + link rows need room for their deferred supporting
            // cards too. Capping at two avoids pathological reservation while
            // keeping the common image+destination case geometry-stable.
            reservation += Math.min(2, supportingEmbedCount)
                         * (compactCard ? 132 : 152)
        }

        if (presentationKind === "visual")
            reservation += compactCard ? Theme.space2 : Theme.space5
        else if (presentationKind === "developer")
            reservation += Theme.space7
        return Math.max(68, Math.ceil(reservation))
    }

    function rowRevision(row) {
        const value = row || ({})
        if (value._genySection)
            return "section:" + String(value._genySection)
        // Viewer evidence can be *unknown*. Verified false must never share
        // a revision with an absent value, especially on account switches.
        function viewerEvidence(field, knowledgeField) {
            if (knowledgeField && value[knowledgeField] !== true)
                return "unknown"
            if (value[field] === undefined || value[field] === null)
                return "unknown"
            return Boolean(value[field]) ? "true" : "false"
        }
        const followRevision = [viewerEvidence("followingByMe", "followingKnown"),
                                viewerEvidence("followsMe", "followsMeKnown")].join(":")
        if (root.socialMode) {
            return [String(value.fid || value.authorFid || 0),
                    String(value.author || ""), String(value.displayName || ""),
                    String(value.avatarUrl || ""), followRevision].join("\u001f")
        }
        // Structural content receives a C++-generated revision. Mutable viewer
        // state invalidates only its stable row, without serializing embeds.
        return [String(value._revision || value.castId || value.hash || value.id || ""),
                String(value._referencedRevision || ""),
                String(value.likeCount || 0), String(value.recastCount || 0),
                String(value.replyCount || 0), viewerEvidence("likedByMe"),
                viewerEvidence("recastedByMe"), followRevision,
                String(value.writeState || "")].join("\u001f")
    }

    function payloadForStableKey(key) {
        return root.stablePayloads[String(key || "")] || ({})
    }

    function rememberVisibleGeometry() {
        if (root.magazineEnabled) return
        if (readList.moving || !root.presentationActive || readList.count === 0
                || root.applyingGeometryAnchor || root.applyingViewportRestore
                || geometryRestoreTimer.running
                || root.pendingViewportScope.length > 0) return
        const snapshot = root.captureViewportForRebuild()
        if (snapshot && snapshot.key.length > 0) {
            root.geometryAnchorKey = snapshot.key
            root.geometryAnchorOffset = snapshot.offset
        } else {
            root.geometryAnchorKey = ""
        }
    }

    function stabilizeGeometryAnchor() {
        if (readList.moving || !root.presentationActive || root.applyingGeometryAnchor
                || root.pendingViewportScope.length > 0
                || root.geometryAnchorKey.length === 0) return
        const key = root.geometryAnchorKey
        let anchorIndex = -1
        for (let i = 0; i < stableRows.count; ++i) {
            if (String(stableRows.get(i).stableKey) === key) {
                anchorIndex = i
                break
            }
        }
        if (anchorIndex < 0) return
        root.applyingGeometryAnchor = true
        readList.forceLayout()
        const item = readList.itemAtIndex(anchorIndex)
        if (!item) {
            root.applyingGeometryAnchor = false
            return
        }
        const shift = item.y - readList.contentY - root.geometryAnchorOffset
        if (Math.abs(shift) < 1) {
            root.applyingGeometryAnchor = false
            return
        }
        // contentHeight is an estimate for uninstantiated variable-height rows.
        // Clamping to that estimate can move the reader by far more than shift.
        readList.contentY += shift
        root.applyingGeometryAnchor = false
    }

    function captureViewportForRebuild() {
        if (readList.moving || stableRows.count === 0
                || readList.contentY - readList.originY < 48)
            return null
        const probe = Math.min(116, Math.max(20, readList.height * 0.17))
        const index = readList.indexAt(Math.max(1, readList.width * 0.5),
                                       readList.contentY + probe)
        const item = index >= 0 ? readList.itemAtIndex(index) : null
        return { key: index >= 0 && item
                      ? String(stableRows.get(index).stableKey || "") : "",
                 offset: item ? item.y - readList.contentY : 0,
                 y: readList.contentY,
                 scope: root.activeFeed + ":" + root.presentationMode + ":" + root.socialMode }
    }

    function queueViewportRestore(snapshot) {
        if (!snapshot || readList.moving) return
        root.pendingViewportKey = snapshot.key
        root.pendingViewportOffset = snapshot.offset
        root.pendingViewportY = snapshot.y
        root.pendingViewportScope = snapshot.scope
        root.pendingViewportSecondPass = false
        stableViewportRestoreTimer.restart()
    }

    function restoreViewportAfterRebuild() {
        const scope = root.activeFeed + ":" + root.presentationMode + ":" + root.socialMode
        if (root.pendingViewportScope !== scope || readList.moving
                || !root.presentationActive) {
            root.pendingViewportKey = ""
            root.pendingViewportScope = ""
            return
        }
        const key = root.pendingViewportKey
        const previousY = root.pendingViewportY
        const offset = root.pendingViewportOffset
        if (!key) {
            root.pendingViewportScope = ""
            // Even when every cast changed in a provider refresh, preserve a
            // useful proportional viewport rather than teleporting to zero.
            readList.contentY = Math.max(readList.originY,
                Math.min(readList.originY + Math.max(0, readList.contentHeight-readList.height), previousY))
            return
        }
        let index = -1
        for (let i = 0; i < stableRows.count; ++i) {
            if (String(stableRows.get(i).stableKey) === key) { index=i;break }
        }
        if (index < 0) {
            root.pendingViewportScope = ""
            root.pendingViewportKey = ""
            readList.contentY = Math.max(readList.originY,
                Math.min(readList.originY + Math.max(0,readList.contentHeight-readList.height),previousY))
            return
        }
        const item = readList.itemAtIndex(index)
        root.applyingViewportRestore = true
        if (item) {
            // A retained delegate already has exact geometry. Do not first
            // snap its top to the viewport and undo that snap a frame later.
            readList.contentY = item.y - offset
        } else if (!root.pendingViewportSecondPass) {
            root.pendingViewportSecondPass = true
            readList.positionViewAtIndex(index, ListView.Beginning)
            root.applyingViewportRestore = false
            stableViewportRestoreTimer.restart()
            return
        }
        root.applyingViewportRestore = false
        root.pendingViewportKey = ""
        root.pendingViewportScope = ""
        root.pendingViewportSecondPass = false
        geometryAnchorDebounce.restart()
    }

    function rebuildStableModel(source) {
        const snapshot = root.captureViewportForRebuild()
        const payloads = ({})
        root.applyingViewportRestore = true
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
        root.applyingViewportRestore = false
        root.queueViewportRestore(snapshot)
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

    function cancelViewportCorrections() {
        geometryAnchorDebounce.stop()
        geometryRestoreTimer.stop()
        stableViewportRestoreTimer.stop()
        positionAtTopTimer.stop()
        root.geometryAnchorKey = ""
        root.pendingViewportKey = ""
        root.pendingViewportScope = ""
        root.pendingViewportSecondPass = false
    }

    // Explicit primary navigation only. Detail -> Back never calls this,
    // so reading position remains continuous during conversations.
    function scrollToBeginning() {
        root.returnCastPending = false
        castReturnRestoreTimer.stop()
        geometryAnchorDebounce.stop()
        geometryRestoreTimer.stop()
        root.geometryAnchorKey = ""
        stableViewportRestoreTimer.stop()
        root.pendingViewportKey = ""
        root.pendingViewportScope = ""
        readList.cancelFlick()
        readList.positionViewAtBeginning()
        positionAtTopTimer.restart()
    }

    function synchronizeStableModel() {
        const source = root.sourceRows()
        // A temporary empty refresh is not an authoritative removal.
        if (source.length === 0 && root.busy && stableRows.count > 0)
            return

        // One transfer of primitive roles into the native model. Every move,
        // insertion, revision and removal is reconciled inside C++ instead
        // of thousands of individual QML -> QObject method calls.
        const next = []
        const liveKeys = Object.create(null)
        for (let index = 0; index < source.length; ++index) {
            const entry = source[index] || ({})
            const payload = entry.payload || ({})
            const key = root.rowKey(payload, index)
            liveKeys[key] = true
            // Mutate the side-store first: role notifications read from it.
            root.stablePayloads[key] = payload
            next.push({stableKey: key,
                       revisionKey: root.rowRevision(payload),
                       presentationKind: String(entry.presentationKind || "conversation"),
                       mediaLayoutKind: root.mediaLayoutKind(payload)})
        }
        stableRows.reconcile(next)
        for (const key of Object.keys(root.stablePayloads)) {
            if (!liveKeys[key])
                delete root.stablePayloads[key]
        }
    }

    function isNearEnd() {
        if (readList.count === 0 || readList.contentHeight <= 0)
            return false
        if (readList.atYEnd)
            return true
        // A Masonry feed has several independently sized columns. Waiting
        // until the tallest column nears its end leaves the shorter columns
        // black/empty after a small 5/10-post response. Preload a bounded next
        // page when the shortest lane no longer covers the visible viewport.
        if (root.magazineEnabled
                && magazineView.shortestColumnBottom - readList.contentY
                    < readList.height + Math.max(240, readList.height * 0.3))
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
                || (!root.cachedMore && (root.offline || root.sourceUnavailable
                                         || root.errorText.length > 0)))
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
        if (!root.cachedMore
                && (root.offline || root.sourceUnavailable || root.errorText.length > 0)) {
            filterPrefetchDebounce.stop()
            loadMoreDebounce.stop()
            return
        }
        if (!root.canAutomaticallyPage())
            return
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

    // All deferred QML work is owned by this Item's timer, not the global
    // Qt.callLater queue. Repeated changes coalesce into one reconciliation;
    // unloading the page destroys the timer and its pending work.
    function deferFeedWork(syncRows, primeArt, checkPage) {
        root.deferredFeedSync = root.deferredFeedSync || syncRows
        root.deferredArtPrime = root.deferredArtPrime || primeArt
        root.deferredPageCheck = root.deferredPageCheck || checkPage
        feedWorkTimer.restart()
    }

    onRowsChanged: root.deferFeedWork(true, false, true)
    onTopicFilterActiveChanged: root.deferFeedWork(true, false, true)
    onSupplementalRowsChanged: {
        if (root.presentationMode === "visuals")
            root.deferFeedWork(true, true, false)
    }
    onSupplementalBusyChanged: {
        if (!root.supplementalBusy && root.presentationMode === "visuals")
            root.deferFeedWork(false, false, true)
    }
    onSocialModeChanged: {
        stableRows.clear()
        root.resetAllFilterPrefetchBudgets()
        root.deferFeedWork(true, false, false)
    }
    onActiveFeedChanged: {
        root.restartSemanticDeadline()
        root.beginSelectionHandoff()
        if (root.activeFeed === "channel" && root.presentationActive)
            channelScrollTopTimer.restart()
        // Following and Trending own independent continuation streams. A real
        // source change starts a fresh bounded semantic-prefetch budget; local
        // Explorer tab switches no longer change activeFeed.
        root.resetAllFilterPrefetchBudgets()
    }
    onPresentationModeChanged: {
        root.restartSemanticDeadline()
        root.beginSelectionHandoff()
        stableViewportRestoreTimer.stop()
        root.pendingViewportKey = ""
        filterPrefetchDebounce.stop()
        // Each semantic tab visit gets a fresh bounded continuation budget.
        // Without this, a sparse Devs/NFTs visit could permanently exhaust its
        // two-page allowance and only ever show old cached matches afterward.
        root.resetCurrentFilterPrefetchBudget()
        // Keep the existing delegate pool alive until the new projection can
        // reconcile keys; clearing the whole model forced expensive remounts.
        root.deferFeedWork(true, root.presentationMode === "visuals", true)
    }
    onAppFilterModeChanged: root.deferFeedWork(true, false, true)
    onSelectedAppFidChanged: root.deferFeedWork(true, false, true)
    onSignerFilterModeChanged: root.deferFeedWork(true, false, true)
    onSelectedSignerKeyChanged: root.deferFeedWork(true, false, true)
    onVisualFilterChanged: {
        if (root.presentationMode !== "visuals")
            return
        filterPrefetchDebounce.stop()
        // Reconcile only rows whose semantic presentation actually changed.
        root.deferFeedWork(true, false, true)
    }
    onHasMoreChanged: {
        if (!hasMore) {
            loadMorePending = false
            loadMoreDebounce.stop()
            loadMoreRelease.stop()
        } else {
            root.deferFeedWork(false, false, true)
        }
    }
    onBusyChanged: {
        if (busy) {
            loadMoreRelease.stop()
        } else {
            loadMorePending = false
            root.deferFeedWork(true, false, true)
        }
    }
    onRuntimeReadyChanged: {
        if (root.runtimeReady) root.restartSemanticDeadline()
    }
    onPresentationActiveChanged: {
        if (!root.presentationActive) {
            semanticLoadDeadline.stop()
            previewSyncDebounce.stop()
            filterPrefetchDebounce.stop()
            loadMoreDebounce.stop()
            feedWorkTimer.stop()
            root.deferredFeedSync = false
            root.deferredArtPrime = false
            root.deferredPageCheck = false
            root.stableModelSyncPending = true
            return
        }
        root.restartSemanticDeadline()
        root.deferFeedWork(true, root.presentationMode === "visuals", true)
        primaryScrollRequestTimer.restart()
    }
    Component.onCompleted: requestStableModelSync()

    Timer {
        id: feedWorkTimer
        interval: 0
        repeat: false
        onTriggered: {
            const synchronize = root.deferredFeedSync
            const prime = root.deferredArtPrime
            const check = root.deferredPageCheck
            root.deferredFeedSync = false
            root.deferredArtPrime = false
            root.deferredPageCheck = false
            if (!root.presentationActive) {
                if (synchronize)
                    root.stableModelSyncPending = true
                return
            }
            if (prime && root.presentationMode === "visuals")
                root.primeArtPreviews()
            if (synchronize)
                root.requestStableModelSync()
            if (root.returnCastPending)
                castReturnRestoreTimer.restart()
            if (check)
                root.scheduleLoadMore()
        }
    }

    Timer {
        id: geometryAnchorDebounce
        interval: 110
        repeat: false
        onTriggered: root.rememberVisibleGeometry()
    }

    Timer {
        id: geometryRestoreTimer
        interval: 0
        repeat: false
        onTriggered: root.stabilizeGeometryAnchor()
    }

    Timer {
        id: stableViewportRestoreTimer
        interval: 0
        repeat: false
        onTriggered: root.restoreViewportAfterRebuild()
    }
    Timer {
        id: castReturnRestoreTimer
        interval: 0
        repeat: false
        onTriggered: root.restoreCastOnReturn()
    }

    // Object-owned, zero-delay timers replace orphan Qt.callLater callbacks.
    // They are destroyed together with this FeedSurface and never invoke a
    // detached QML context after route changes or component incubation.
    Timer {
        id: primaryScrollRequestTimer
        interval: 0
        repeat: false
        onTriggered: root.honorPrimaryScrollRequest()
    }

    Timer {
        id: channelScrollTopTimer
        interval: 0
        repeat: false
        onTriggered: {
            if (root.activeFeed === "channel" && root.presentationActive)
                root.scrollToBeginning()
        }
    }

    Timer {
        id: positionAtTopTimer
        interval: 0
        repeat: false
        onTriggered: readList.positionViewAtBeginning()
    }

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

    FeedRowsModel {
        id: stableRows
    }

    Timer {
        id: filterPrefetchDebounce
        interval: 240
        repeat: false
        onTriggered: {
            if (!root.needsFilteredPrefetch() || !root.canAutomaticallyPage())
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
                    || (!root.cachedMore && (root.offline || root.sourceUnavailable
                                             || root.errorText.length > 0))
                    || !root.canAutomaticallyPage())
                return
            root.recordAutomaticPage()
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
        id: timelineList
        objectName: "readList"
        visible: !root.magazineEnabled
        anchors.fill: parent
        clip: true
        // The mobile dock is an overlay, not an opaque reserved footer.
        // Allow the final cast to scroll clear of its touch targets.
        bottomMargin: root.compact ? 96 : 0
        model: root.magazineEnabled ? null : stableRows
        spacing: 0
        // Pulling past the top is a deliberate refresh gesture; the content
        // remains bounded during ordinary wheel/flick movement.
        boundsBehavior: Flickable.DragOverBounds
        boundsMovement: Flickable.FollowBoundsBehavior
        // Channel hero + scoped author strip belong to the scroll content,
        // not to a pinned desktop overlay; no stale account StoryStrip.
        // Explorer stories and filters belong to the scrolling content on
        // desktop too. An overlay header pinned the entire story strip.
        headerPositioning: ListView.InlineHeader
        reuseItems: true
        // Keep only a shallow reusable runway. Rich embeds, animated emoji and
        // image masks are expensive delegate trees; pagination continuity must
        // come from stable model reconciliation rather than a large offscreen cache.
        // Keep fewer heavyweight offscreen Cast delegates alive on phones.
        // Incubation and delegate reuse still provide continuity while flicking.
        cacheBuffer: root.compact ? Math.min(Math.max(height * 0.33, 240), 440)
                                  : Math.min(Math.max(height * 0.65, 480), 820)
        pixelAligned: false

        header: feedHeaderComponent
        footer: feedFooterComponent

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
    }

    Connections {
        target: root.readList
        function onMovementStarted() { root.cancelViewportCorrections() }
        function onMovementEnded() {
            geometryAnchorDebounce.restart()
            // A genuine scroll gesture is explicit user intent, unlike geometry
            // updates from an empty/duplicate continuation page.
            root.resetAutomaticPageBudget()
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
        function onContentHeightChanged() {
            if (!readList.moving) {
                if (!scrollWorkThrottle.running)
                    scrollWorkThrottle.start()
            }
        }
        function onContentYChanged() {
            // Wheel/browser fallback/scrollbar input can change contentY without
            // moving. It must cancel corrections even while their timer is queued.
            if (!readList.moving && !root.applyingGeometryAnchor
                    && !root.applyingViewportRestore) {
                root.cancelViewportCorrections()
                geometryAnchorDebounce.restart()
            }
        }
        function onHeightChanged() {
            if (!readList.moving && !scrollWorkThrottle.running)
                scrollWorkThrottle.start()
        }
    }

    FeedMasonryView {
        id: magazineView
        objectName: "explorerMagazine"
        anchors.fill: parent
        visible: root.magazineEnabled
        // Keep numeric Magazine geometry warm, but no visual cards while hidden.
        model: stableRows
        header: feedHeaderComponent
        footer: feedFooterComponent
        // The compact editorial card width is the stable Explorer default.
        // Columns reflow automatically as the central reading pane resizes.
        targetCardWidth: Theme.scaleUi(420)
        maximumColumns: 3
        onLayoutStarted: root.applyingGeometryAnchor = true
        onLayoutFinished: {
            root.applyingGeometryAnchor = false
            // Shortest-lane changes may not affect Flickable.contentHeight;
            // trigger the same debounced, budgeted pagination check.
            if (!magazineView.moving && !scrollWorkThrottle.running)
                scrollWorkThrottle.start()
        }
        estimateHeight: function(key, kind, media, width) {
            return root.rowIncubationHeight(root.payloadForStableKey(key), kind, media, width)
        }
        delegate: Component {
            FeedRow {
                magazineRow: true
                sharedMediaGeometry: magazineView
                viewportY: parent.slotY
                width: parent.width
                index: parent.rowIndex
                stableKey: parent.stableKey
                revisionKey: parent.revisionKey
                presentationKind: parent.presentationKind
                mediaLayoutKind: parent.mediaLayoutKind
            }
        }
    }

    Component {
        id: feedHeaderComponent
        Item {
            id: pinnedHeader
            width: readList.width
            // Reserve the new-post affordance in the list header itself so the
            // first cast can never slide underneath it. The visible banner is a
            // viewport overlay that tracks this slot until it reaches the top,
            // then remains pinned while older posts continue scrolling.
            implicitHeight: feedHeaderLoader.implicitHeight + topicFilterNotice.height + communityNotice.height
                            + localRecentNotice.height + signerFilterBar.height + appFilterBar.height
                            + newPostsSlot.height
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

            Item {
                id: topicFilterNotice
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: feedHeaderLoader.bottom
                anchors.leftMargin: Theme.mobileGutter
                anchors.rightMargin: Theme.mobileGutter
                visible: root.topicFilterActive
                // QQC2 Label's implicitHeight can depend on its assigned
                // height while computing elision. A separate wrapper owns
                // the extra padding, avoiding the Qt height binding loop.
                height: visible ? topicFilterText.implicitHeight + Theme.space3 : 0

                DirectionAwareLabel {
                    id: topicFilterText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: qsTrId("genycaster.filters.topic_results") + " · "
                          + root.topicFilterSlug.replace(/-/g, " ")
                    color: Theme.accent
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Theme.textStartAlignment
                    elide: Text.ElideRight
                }
            }

            DirectionAwareLabel {
                id: communityNotice
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: topicFilterNotice.bottom
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
                id: localRecentNotice
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: communityNotice.bottom
                visible: !root.socialMode && (
                    (root.localDerivedRecent && root.activeFeed === "recent") ||
                    (root.localDerivedFollowing && root.activeFeed === "following"))
                height: visible ? localRecentText.implicitHeight + Theme.space2 : 0

                DirectionAwareLabel {
                    id: localRecentText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: root.localDerivedFollowing && root.activeFeed === "following"
                        ? qsTrId("genycaster.feed.following_local_only_notice")
                        : qsTrId("genycaster.feed.recent_local_only_notice")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    horizontalAlignment: Theme.textStartAlignment
                    wrapMode: Text.WordWrap
                }
            }

            Column {
                id: signerFilterBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: localRecentNotice.bottom
                height: visible ? implicitHeight + Theme.space2 : 0
                visible: !root.socialMode && root.knownSignerKeys.length > 0
                spacing: 3
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3

                DirectionAwareLabel {
                    width: parent.width - Theme.space3 * 2
                    text: qsTrId("genycaster.feed.signer_filter_explanation")
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.inkMuted
                    elide: Text.ElideRight
                }
                Row {
                    width: parent.width - Theme.space3 * 2
                    spacing: Theme.space2

                    ComboBox {
                        id: signerModePicker
                        width: Math.max(110, (parent.width - parent.spacing) * 0.45)
                        height: 34
                        font.pixelSize: Theme.typeCaptionSize
                        model: [
                            qsTrId("genycaster.feed.signer_filter_all"),
                            qsTrId("genycaster.feed.signer_filter_include"),
                            qsTrId("genycaster.feed.signer_filter_exclude")
                        ]
                        onActivated: function(index) {
                            root.signerFilterMode = index === 1 ? "include"
                                                  : index === 2 ? "exclude" : "all"
                            if (root.signerFilterMode !== "all"
                                    && !root.selectedSignerKey
                                    && root.knownSignerKeys.length > 0)
                                root.selectedSignerKey = root.knownSignerKeys[0]
                        }
                    }
                    ComboBox {
                        id: signerKeyPicker
                        width: parent.width - signerModePicker.width - parent.spacing
                        height: 34
                        enabled: root.signerFilterMode !== "all"
                        font.pixelSize: Theme.typeCaptionSize
                        model: root.knownSignerKeys.map(function(key) {
                            return { label: key.slice(0, 12) + "…" + key.slice(-8), key: key }
                        })
                        textRole: "label"
                        onActivated: function(index) {
                            const option = model[index]
                            if (option)
                                root.selectedSignerKey = option.key
                        }
                    }
                }
            }

            Column {
                id: appFilterBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: signerFilterBar.bottom
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                height: visible ? implicitHeight + Theme.space2 : 0
                visible: !root.socialMode && root.knownAppFids.length > 0
                spacing: 3
                DirectionAwareLabel {
                    width: parent.width
                    text: qsTrId("genycaster.feed.app_fid_filter_explanation")
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.inkMuted
                    elide: Text.ElideRight
                }
                Row {
                    width: parent.width
                    spacing: Theme.space2
                    ComboBox {
                        id: appModePicker
                        width: Math.max(110, (parent.width - parent.spacing) * 0.45)
                        height: 34
                        font.pixelSize: Theme.typeCaptionSize
                        model: [
                            qsTrId("genycaster.feed.signer_filter_all"),
                            qsTrId("genycaster.feed.signer_filter_include"),
                            qsTrId("genycaster.feed.signer_filter_exclude")
                        ]
                        onActivated: function(index) {
                            root.appFilterMode = index === 1 ? "include"
                                               : index === 2 ? "exclude" : "all"
                            if (root.appFilterMode !== "all" && !root.selectedAppFid)
                                root.selectedAppFid = root.knownAppFids[0]
                        }
                    }
                    ComboBox {
                        id: appFidPicker
                        width: parent.width - appModePicker.width - parent.spacing
                        height: 34
                        enabled: root.appFilterMode !== "all"
                        font.pixelSize: Theme.typeCaptionSize
                        model: root.knownAppFids.map(function(fid) {
                            return { label: "FID " + fid, fid: fid }
                        })
                        textRole: "label"
                        onActivated: function(index) {
                            const option = model[index]
                            if (option) root.selectedAppFid = option.fid
                        }
                    }
                }
            }

            Item {
                id: newPostsSlot
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: appFilterBar.bottom
                height: root.pendingNewPostCount > 0 && !root.socialMode ? 52 : 0
                visible: height > 0
            }
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
        property bool magazineRow: false
        // Reuse the bounded per-feed aspect cache when ListView recycles a
        // mobile portrait. The Magazine viewport need not be visible.
        property var sharedMediaGeometry: magazineView
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
            // A reservation belongs only to an incubating row. Once its
            // content exists, ListView must see its real height even during
            // motion; freezing estimates creates holes or overlapping casts.
            // Qt owns continuity while moving; idle changes can be anchored.
            const nextHeight = !rowDelegate.incubating && rowDelegate.resolvedHeight > 0
                               ? rowDelegate.resolvedHeight : rowDelegate.reservedHeight
            if (Math.abs(rowDelegate.committedHeight - nextHeight) < 1) return
            // Capture immediately before a real geometry mutation, never
            // in response to ListView's fluctuating contentHeight estimate.
            root.rememberVisibleGeometry()
            rowDelegate.committedHeight = nextHeight
            if (!readList.moving && root.geometryAnchorKey.length > 0)
                geometryRestoreTimer.restart()
        }

        property real viewportY: y
        // Magazine's source is a narrow per-lane Loader, not the full Flickable.
        // Measuring the inner cast at full reader width overestimates rich
        // quote/media geometry and leaves gaps between narrow visual cards.
        width: magazineRow && parent ? parent.width : readList.width
        // Reused delegates stay alive in ListView's pool. Explicitly hide
        // pooled rows so inherited visibility pauses animated emoji, spinners,
        // and other presentation-only work until the row is reused.
        ListView.onPooled: rowDelegate.visible = false
        ListView.onReused: {
            rowDelegate.visible = true
            rowDelegate.synchronizeHeight()
        }
        // ListView must never see a zero-height row while Qt spreads the
        // expensive Cast subtree construction across multiple frames.
        // The placeholder is replaced by measured geometry as soon as the
        // loaded content is ready, including on reuse during a flick.
        // Loader computes implicitHeight from its loaded item and exposes it
        // read-only. Reserve the row using the writable height instead.
        height: Math.max(1, committedHeight)
        // Timeline delegates can incubate asynchronously while flicking.
        // Magazine admission is already viewport-bounded; constructing its
        // row synchronously avoids destroying half-incubated delegates when
        // a card leaves the viewport or the user switches views.
        asynchronous: !sectionRow && !magazineRow
        sourceComponent: rowDelegate.sectionRow ? sectionRowComponent
                       : root.socialMode ? profileRowComponent : castRowComponent
        onReservedHeightChanged: rowDelegate.synchronizeHeight()
        onResolvedHeightChanged: rowDelegate.synchronizeHeight()
        onLoaded: rowDelegate.synchronizeHeight()
        onStableKeyChanged: rowDelegate.committedHeight = rowDelegate.reservedHeight

        // An asynchronous delegate may still be incubating when a fast
        // flick brings its slot on screen. Show a real cast-shaped loading
        // state rather than a vacant patch of the feed.
        CastSkeleton {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            visible: rowDelegate.incubating && !rowDelegate.sectionRow
            mediaPreview: rowDelegate.mediaLayoutKind !== "other"
            animate: !readList.moving
        }

        Connections {
            target: readList
            function onMovingChanged() {
                if (!readList.moving) {
                    // Capture the visible cast before deferred video/card
                    // heights settle, then keep that anchor in view.
                    if (root.geometryAnchorKey.length === 0)
                        root.rememberVisibleGeometry()
                    rowDelegate.synchronizeHeight()
                }
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
                compact: root.compact || rowDelegate.magazineRow
                magazineCard: rowDelegate.magazineRow
                sharedMediaGeometry: rowDelegate.sharedMediaGeometry
                interactionsAvailable: root.interactionsAvailable
                // Keep only a small near-viewport margin subscribed.
                enrichmentActive: root.presentationActive && rowDelegate.visible
                    && rowDelegate.viewportY + rowDelegate.height >= readList.contentY - 320
                    && rowDelegate.viewportY <= readList.contentY + readList.height + 320
                accountFid: root.accountFid
                onOpenRequested: castId => {
                    root.rememberCastForReturn(castId)
                    root.castRequested(castId)
                }
                onProfileRequested: fid => root.profileRequested(fid)
                onReplyRequested: cast => root.replyRequested(cast)
                onQuoteRequested: cast => root.quoteRequested(cast)
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

    Component {
        id: feedFooterComponent
        Item {
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
                    root.resetAutomaticPageBudget()
                    root.restartSemanticDeadline()
                    root.scheduleLoadMore()
                }
                anchors.fill: parent
                visible: stableRows.count === 0 && !root.waitingForFirstRows
                runtimeReady: root.runtimeReady
                timedOut: root.semanticWaitExpired && stableRows.count === 0
                busy: root.busy && !root.semanticWaitExpired
                offline: root.offline
                sourceUnavailable: root.sourceUnavailable
                errorText: root.errorText
                activeFeed: root.activeFeed
                identityAvailable: root.identityAvailable
                feedStarted: root.feedStarted
                onRetryRequested: {
                    root.resetAllFilterPrefetchBudgets()
                    root.restartSemanticDeadline()
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

    PullToRefresh {
        flickable: readList
        busy: root.busy
        onRefreshRequested: root.refreshRequested()
    }

    ScrollToTopButton {
        flickable: readList
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space5
        // The mobile inset already includes the dock and its compose button.
        anchors.bottomMargin: Math.max(Theme.space4, root.bottomOverlayInset
                                      + Theme.space3)
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
                positionAtTopTimer.restart()
            }
        }
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: readList.headerItem ? readList.headerItem.implicitHeight : 0
        visible: stableRows.count === 0 && root.waitingForFirstRows
        Repeater {
            model: root.presentationMode === "visuals" ? 2 : 3
            delegate: CastSkeleton {
                width: root.width
                mediaPreview: root.presentationMode === "visuals"
            }
        }
    }


    ContentRefreshIndicator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 50
        active: root.busy && root.rows.length > 0
    }

}
