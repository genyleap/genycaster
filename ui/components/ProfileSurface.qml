import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import QtCore

Item {
    id: root

    property var detail
    // Metadata refreshes can briefly publish an empty profile. Keep the
    // visible header and its height until a resolved identity arrives.
    property var retainedDetail: ({})
    readonly property var activeDetail: {
        const current = root.detail || ({})
        const retained = root.retainedDetail || ({})
        return Number(current.fid || 0) > 0 ? current : retained
    }
    property var rows
    property string profileFeed: "user-posts"
    property string profileSortMode: "newest"
    property string popularMetric: "all"
    property bool runtimeReady: false
    property bool compact: false
    property bool busy: false
    property bool hasMore: false
    property bool offline: false
    property bool sourceUnavailable: false
    property string errorText: ""
    property bool interactionsAvailable: false
    property double accountFid: 0
    property string accountIssuer: ""
    // GENY holder status is *never* derived from a user-editable bio, display
    // name, URL or arbitrary Farcaster metadata. A future attestation service
    // may bind this property after verifying the actual wallet and holdings.
    property bool genyHolderAttested: false
    property bool profileLinksExpanded: false
    property string profileExtrasError: ""
    // Force UI bindings to re-evaluate after an authoritative account read.
    // QSettings-backed string changes are otherwise not always propagated
    // to a previously constructed ProfileSurface on Qt/WASM.
    property int profileExtrasGeneration: 0

    Settings {
        id: profileExtrasSettings
        category: "genycaster-profile-extras"
        property string entriesJson: "{}"
    }
    // Internal offline outbox. No sync controls are exposed to the user:
    // editing profile details is the only action they need to perform.
    Settings {
        id: profileCloudSettings
        category: "genycaster-profile-cloud"
        property string perAccountJson: "{}"
    }
    readonly property var cloudAccountFlags: {
        if (!root.profileExtrasOwnerKey) return ({})
        try {
            return JSON.parse(profileCloudSettings.perAccountJson || "{}")
                [root.profileExtrasOwnerKey] || ({})
        } catch (_) { return ({}) }
    }
    readonly property bool cloudLocalDirty: root.cloudAccountFlags.dirty === true
    function setCloudFlags(dirty, baseRevision) {
        if (!root.profileExtrasOwnerKey) return
        try {
            const owners = JSON.parse(profileCloudSettings.perAccountJson || "{}")
            const old = owners[root.profileExtrasOwnerKey] || ({})
            // Only an explicit user edit records its known server revision.
            // Existing legacy "dirty" flags without a revision are NOT proof
            // that the cache may replace a newer database record.
            const base = baseRevision === undefined
                ? old.baseRevision : baseRevision
            owners[root.profileExtrasOwnerKey] = {
                dirty: dirty, baseRevision: base === undefined ? null : base,
                localBackups: old.localBackups || []
            }
            profileCloudSettings.perAccountJson = JSON.stringify(owners)
        } catch (_) { }
    }
    function preserveConflictingProfile(item) {
        if (!root.profileExtrasOwnerKey) return
        try {
            const owners = JSON.parse(profileCloudSettings.perAccountJson || "{}")
            const previous = owners[root.profileExtrasOwnerKey] || ({})
            const snapshots = previous.localBackups || []
            snapshots.push({
                value: root.profileExtras,
                baseRevision: previous.baseRevision === undefined
                    ? null : previous.baseRevision,
                serverRevision: Number(item.revision || 0)
            })
            // Bounded backups preserve offline user work without letting stale
            // device caches silently replace the account database.
            owners[root.profileExtrasOwnerKey] = {
                dirty: false, baseRevision: Number(item.revision || 0),
                localBackups: snapshots.slice(-5)
            }
            profileCloudSettings.perAccountJson = JSON.stringify(owners)
            root.profileExtrasError = qsTr("Another device has newer profile details. Your previous local edit was preserved.")
        } catch (_) { }
    }
    function cloudProfilePayload() {
        const saved = root.profileExtras
        const urls = Array.isArray(saved.links) ? saved.links : []
        const labels = Array.isArray(saved.labels) ? saved.labels : []
        return {
            category: String(saved.category || ""),
            profession: String(saved.profession || ""),
            links: urls.map((url, index) => ({
                label: String(labels[index] || "").slice(0, 32), url: url
            })).filter(entry => typeof entry.url === "string"
                        && root.validExtraUrl(entry.url)).slice(0, 5)
        }
    }
    function restoreCloudProfile(item) {
        if (!root.isOwnProfile || !root.profileExtrasOwnerKey
            || !item || typeof item.profession !== "string"
            || typeof item.category !== "string" || !Array.isArray(item.links)) return
        const links = item.links.filter(entry => entry && typeof entry.url === "string"
            && root.validExtraUrl(entry.url)).slice(0, 5)
        try {
            const owners = JSON.parse(profileExtrasSettings.entriesJson || "{}")
            owners[root.profileExtrasOwnerKey] = {
                category: item.category, profession: item.profession,
                links: links.map(entry => entry.url),
                labels: links.map(entry => String(entry.label || "").slice(0, 32))
            }
            profileExtrasSettings.entriesJson = JSON.stringify(owners)
            root.profileExtrasGeneration++
            root.setCloudFlags(false, Number(item.revision || 0))
        } catch (_) { }
    }
    Connections {
        target: openProofConnections
        function onCloudProfileExtrasReceived(item) {
            if (!root.isOwnProfile) return
            if (root.cloudLocalDirty) {
                const remoteRevision = Number(item && item.revision || 0)
                const baseRevision = root.cloudAccountFlags.baseRevision
                // A legacy device cache has no CAS base. It may be imported
                // only when the database has no record. A contemporary edit
                // may be uploaded only when its known base is current.
                if (remoteRevision === 0 ||
                        (typeof baseRevision === "number" &&
                         baseRevision === remoteRevision)) {
                    openProofConnections.pushCloudProfileExtras(root.cloudProfilePayload())
                } else {
                    root.preserveConflictingProfile(item)
                    root.restoreCloudProfile(item)
                }
            } else if (item && typeof item.category === "string") {
                root.restoreCloudProfile(item)
            } else if (Object.keys(root.profileExtras).length > 0) {
                // One-time migration of previously saved local profile data.
                root.setCloudFlags(true)
                openProofConnections.pushCloudProfileExtras(root.cloudProfilePayload())
            }
        }
        function onCloudProfileExtrasSaved(item) {
            if (!root.isOwnProfile || !item || typeof item.category !== "string") return
            // Never acknowledge an older request if another local save
            // happened while it was in flight. Keep retrying latest edits.
            const latest = root.cloudProfilePayload()
            const same = item.category === latest.category
                      && item.profession === latest.profession
                      && JSON.stringify(item.links || []) === JSON.stringify(latest.links)
            if (same) root.setCloudFlags(false, Number(item.revision || 0))
            else {
                root.setCloudFlags(true)
                if (!openProofConnections.cloudBusy)
                    openProofConnections.fetchCloudProfileExtras()
            }
        }
    }
    function refreshProfileAndAccount() {
        root.refreshRequested()
        if (root.isOwnProfile && openProofConnections.cloudAuthorizationReady) {
            profileRefreshRetry.restart()
            openProofConnections.refreshAccountDraft()
        }
    }
    Timer {
        id: profileRefreshRetry
        interval: 350
        repeat: true
        onTriggered: {
            if (!root.isOwnProfile || !openProofConnections.cloudAuthorizationReady
                    || !openProofConnections.cloudServiceAvailable) {
                stop()
                return
            }
            if (!openProofConnections.cloudBusy) {
                stop()
                openProofConnections.fetchCloudProfileExtras()
            }
        }
    }
    Timer {
        id: profileBackgroundRefresh
        interval: 45000
        repeat: true
        triggeredOnStart: true
        running: root.visible && root.isOwnProfile
                 && openProofConnections.cloudServiceAvailable
                 && openProofConnections.cloudAuthorizationReady
        onTriggered: {
            if (!openProofConnections.cloudBusy)
                openProofConnections.fetchCloudProfileExtras()
        }
    }
    // Genycaster stores account-authored profile data under the verified FID,
    // regardless of whether the browser entered through OpenProof or Farcaster.
    // Older route-specific local settings are imported without deleting them.
    readonly property string profileExtrasOwnerKey: root.isOwnProfile
        ? "farcaster:" + String(root.accountFid) : ""
    function migrateLegacyProfileExtras() {
        if (!root.profileExtrasOwnerKey) return
        try {
            const canonical = root.profileExtrasOwnerKey
            const fid = String(root.accountFid)
            const oldKeys = [
                String(root.accountIssuer || "local") + ":" + fid,
                "https://openproof.genyleap.com:" + fid,
                "genycaster:device-local:" + fid,
                "local:" + fid
            ]
            const entries = JSON.parse(profileExtrasSettings.entriesJson || "{}")
            const flags = JSON.parse(profileCloudSettings.perAccountJson || "{}")
            // An unsent edit from another sign-in route wins over an older
            // acknowledged local cache. Both legacy copies remain untouched.
            if (entries[canonical] && flags[canonical]?.dirty === true) return
            const validLegacy = key => entries[key] && typeof entries[key] === "object"
                                        && !Array.isArray(entries[key])
            let source = oldKeys.find(key => validLegacy(key)
                                    && flags[key]?.dirty === true)
            if (!source && entries[canonical]) return
            if (!source) source = oldKeys.find(key => validLegacy(key))
            if (!source) return
            entries[canonical] = entries[source]
            profileExtrasSettings.entriesJson = JSON.stringify(entries)
            root.profileExtrasGeneration++
            if (flags[source]?.dirty === true) {
                flags[canonical] = {dirty: true}
                profileCloudSettings.perAccountJson = JSON.stringify(flags)
            }
        } catch (_) {
            // Offline settings may be unavailable; never erase legacy data.
        }
    }
    onProfileExtrasOwnerKeyChanged: root.migrateLegacyProfileExtras()
    onAccountIssuerChanged: root.migrateLegacyProfileExtras()
    Component.onCompleted: root.migrateLegacyProfileExtras()
    readonly property var profileExtras: {
        const generation = root.profileExtrasGeneration
        if (!root.profileExtrasOwnerKey) return ({})
        // Live, account-verified API state is authoritative for a clean
        // profile. QSettings remains a durable offline outbox, never a
        // higher-priority display source than a known newer server revision.
        const server = openProofConnections.cloudProfileExtras
        if (!root.cloudLocalDirty
                && Number(openProofConnections.cloudRevision || 0) > 0
                && server && typeof server.category === "string"
                && typeof server.profession === "string"
                && Array.isArray(server.links)) {
            return {
                category: server.category, profession: server.profession,
                links: server.links.map(entry => String(entry.url || "")),
                labels: server.links.map(entry => String(entry.label || ""))
            }
        }
        try {
            const rows = JSON.parse(profileExtrasSettings.entriesJson || "{}")
            return rows[root.profileExtrasOwnerKey] || ({})
        } catch (_) { return ({}) }
    }
    // X-style professional identity is selected from curated categories and
    // roles. Previously saved freeform roles remain available in the selector.
    // These values are canonical API identifiers, not translated display
    // strings. Translation must not change sync payloads across devices.
    readonly property var profileCategoryOptions: [
        "Select category", "Arts & entertainment", "Business", "Education",
        "Finance", "Food & beverage", "Health & wellness", "Media & news",
        "Science & technology", "Technology", "Professional services",
        "Sports", "Travel & transportation", "Other"
    ]
    readonly property var profileRoleOptions: {
        const options = ["Select role", "Artist", "Author", "Creator",
            "Designer", "Developer", "Engineer", "Entrepreneur", "Founder",
            "Investor", "Journalist", "Photographer", "Researcher",
            "Student", "Teacher", "Consultant", "Organization", "Other"]
        const saved = String(root.profileExtras.profession || "")
        if (saved && options.indexOf(saved) < 0) options.push(saved)
        return options
    }
    readonly property string profileCategory: {
        return root.isOwnProfile ? String(root.profileExtras.category || "") : ""
    }
    readonly property string profileProfession: {
        const fromSource = String(root.activeDetail.profession || root.activeDetail.category || "")
        return (root.isOwnProfile && root.profileExtras.profession)
            ? String(root.profileExtras.profession) : fromSource.slice(0, 80)
    }
    readonly property var profileExtraLinks: {
        return root.isOwnProfile && Array.isArray(root.profileExtras.links)
            ? root.profileExtras.links.filter(s=>typeof s === "string" && root.validExtraUrl(s)).slice(0, 5)
            : []
    }
    function profileExtraLabel(index) {
        const labels = root.profileExtras.labels
        return Array.isArray(labels) && typeof labels[index] === "string" && labels[index].trim()
            ? labels[index].trim().slice(0, 32) : qsTr("Link %1").arg(index + 1)
    }
    readonly property int profileLinkTotal: root.profileExtraLinks.length + (root.showProfileUrl ? 1 : 0)
    readonly property string primaryProfileLinkUrl: root.showProfileUrl
        ? String(root.activeDetail.profileUrl || "")
        : (root.isOwnProfile && root.profileExtraLinks.length > 0
            ? String(root.profileExtraLinks[0]) : "")
    readonly property int additionalProfileLinksCount: Math.max(
        0, root.profileLinkTotal - (root.primaryProfileLinkUrl.length > 0 ? 1 : 0))
    function openProfileLinks() {
        if (root.profileLinkTotal > 0) profileLinksDrawer.open()
    }
    function validExtraUrl(text) {
        return text.length <= 512
            && /^https:\/\/(?![^/]*@)[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+(?:\:[0-9]{2,5})?(?:[\/?#][^\s]*)?$/.test(text)
    }
    function saveExtraDetails(profession, rawLinks, category) {
        if (!root.isOwnProfile || !root.profileExtrasOwnerKey) return false
        const role = String(profession || "").trim()
        const categoryName = String(category || "").trim()
        if ((role && root.profileRoleOptions.indexOf(role) <= 0)
                || (categoryName && root.profileCategoryOptions.indexOf(categoryName) <= 0)) {
            root.profileExtrasError = qsTr("Choose a category and role from the lists.")
            return false
        }
        if (role.length > 80) {
            root.profileExtrasError = qsTr("Profession must be 80 characters or fewer.")
            return false
        }
        const lines = String(rawLinks || "").split(/[\r\n]+/).map(s=>s.trim()).filter(s=>s.length>0)
        const prepared = lines.map(line => {
            const separator = line.indexOf("|")
            const label = separator >= 0 ? line.slice(0, separator).trim() : ""
            const url = separator >= 0 ? line.slice(separator + 1).trim() : line
            return {label: label, url: url}
        })
        if (prepared.length > 5 || prepared.some(s=>s.label.length>32 || !root.validExtraUrl(s.url))) {
            root.profileExtrasError = qsTr("Use up to five HTTPS links. Optional format: Personal | https://example.com")
            return false
        }
        try {
            const entries = JSON.parse(profileExtrasSettings.entriesJson || "{}")
            const unique = prepared.filter((item,index)=>prepared.findIndex(e=>e.url===item.url)===index)
            entries[root.profileExtrasOwnerKey] = {
                profession: role, category: categoryName,
                links: unique.map(e=>e.url), labels: unique.map(e=>e.label)
            }
            profileExtrasSettings.entriesJson = JSON.stringify(entries)
            root.profileExtrasGeneration++
            root.profileExtrasError = ""
            root.profileLinksExpanded = false
            root.setCloudFlags(true, Number(openProofConnections.cloudRevision || 0))
            if (openProofConnections.cloudServiceAvailable
                    && openProofConnections.cloudAuthorizationReady)
                profileRefreshRetry.restart()
            return true
        } catch (_) {
            root.profileExtrasError = qsTr("Could not save local profile details.")
            return false
        }
    }
    property bool loadMorePending: false
    property double positionedProfileFid: 0
    property string viewportAnchorKey: ""
    property real viewportAnchorOffset: 0
    property bool restoringViewport: false
    property bool stableModelSyncPending: false
    property string pendingRestoreKey: ""
    property real pendingRestoreOffset: 0
    property bool pendingRestoreCapture: false
    property int resetPositionPasses: 0
    // Keep QVariantMap payloads out of ListModel roles; the model itself stays
    // type-stable and delegates re-read only when their revision role changes.
    property var stablePayloads: ({})

    signal backRequested()
    signal followersRequested()
    signal followingRequested()
    signal refreshRequested()
    signal profileFeedRequested(string feed)
    signal castRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal followingMutationRequested(double targetFid, bool following)
    signal removeCastRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()
    signal loadMoreRequested()

    readonly property bool isOwnProfile: Number(root.activeDetail.fid || 0) > 0
                                         && Number(root.activeDetail.fid || 0) === Number(root.accountFid)
    readonly property int profileAvatarSize: root.compact ? 84 : 96
    readonly property int profileMargin: root.compact ? Theme.mobileGutter : Theme.space5
    readonly property int profileContentLimit: root.compact ? 760 : 880
    readonly property int profileContentWidth: Math.max(0, Math.min(root.profileContentLimit, width - root.profileMargin * 2))
    readonly property int profileContentX: Math.max(root.profileMargin, Math.round((width - root.profileContentWidth) / 2))
    readonly property bool profileContentRtl: Theme.rtl
    readonly property int profileAvatarX: root.compact
                                           ? root.profileContentX + Theme.space2
                                           : root.profileContentRtl
                                             ? root.profileContentX + root.profileContentWidth - root.profileAvatarSize - Theme.space3
                                             : root.profileContentX + Theme.space3
    // Never infer social proof from a Farcaster bio, display name or vanity URL.
    // These labels originate from OpenProof's verified, FID-bound connection
    // response (or a clearly marked owner-local last-known snapshot).
    readonly property var ownerSocialLinks: {
        if (!root.isOwnProfile || typeof openProofConnections === "undefined"
                || openProofConnections === null) return []
        const result = []
        for (const item of openProofConnections.connections) {
            const provider = String(item.provider || "")
            if (!["x", "github", "linkedin", "telegram"].includes(provider)) continue
            const username = String(item.username || "").replace(/^@/, "")
            let href = ""
            let manuallyEntered = false
            if (provider === "x" && /^[A-Za-z0-9_]{1,15}$/.test(username))
                href = "https://x.com/" + username
            else if (provider === "github"
                     && /^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/.test(username))
                href = "https://github.com/" + username
            else if (provider === "telegram"
                     && /^[A-Za-z][A-Za-z0-9_]{4,31}$/.test(username))
                href = "https://t.me/" + username
            else if (provider === "linkedin") {
                const official = String(item.profileUrl || "")
                if (/^https:\/\/www\.linkedin\.com\/(?:in\/[A-Za-z0-9_-]{3,100}\/?|profile-thirdparty-redirect\/[-A-Za-z0-9_%.]{1,1024})$/.test(official))
                    href = official
                else {
                    const manual = String(item.manualProfileUrl || "")
                    if (/^https:\/\/www\.linkedin\.com\/in\/[A-Za-z0-9_-]{3,100}\/?$/.test(manual)) {
                        href = manual
                        manuallyEntered = true
                    }
                }
            }
            const label = String(item.displayName || username || provider).slice(0, 128)
            // A name-based LinkedIn search is a discovery action, not an
            // authenticated public profile URL. Never label it as verified.
            const searchHref = provider === "linkedin" && !href && label
                    ? "https://www.linkedin.com/search/results/people/?keywords="
                      + encodeURIComponent(label) : ""
            result.push({ provider: provider, href: href, searchHref: searchHref,
                          manualUrl: manuallyEntered,
                          label: label })
        }
        return result
    }
    function socialIcon(provider) {
        const rootPath = "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/providers/"
        return rootPath + provider + ".svg"
    }
    function openConnections() {
        if (typeof openProofConnections === "undefined" || openProofConnections === null)
            return
        openProofConnections.refresh()
        accountConnectionsPopup.open()
    }
    readonly property bool showUsername: Boolean(root.activeDetail.username) && !root.syntheticUsername(root.activeDetail.username)
    readonly property string normalizedBio: String(root.activeDetail.bio || "").replace(/\s+/g, " ").trim()
    readonly property string displayLocation: {
        const geoAvailable = typeof geoLocationResolver !== "undefined" && geoLocationResolver !== null
        const geoRevision = geoAvailable ? geoLocationResolver.revision : 0
        if (!geoAvailable)
            return ""
        return geoLocationResolver.displayLabel(
            String(root.activeDetail.location || ""), localizationController.effectiveLanguageTag)
    }

    function ltrText(value) {
        const isolate = String.fromCharCode(0x2066)
        const pop = String.fromCharCode(0x2069)
        return isolate + String(value || "") + pop
    }

    function ltrHandle(value) {
        return root.ltrText(String.fromCharCode(0x40) + String(value || ""))
    }

    function compactCount(value) {
        const localeRevision = localizationController.revision
        if (root.activeDetail.socialCountsKnown === false)
            return "—"
        return LocaleFormat.formatCompactNumber(Number(value || 0))
    }

    function syntheticUsername(value) {
        return /^fid:[0-9]+$/i.test(String(value || "").trim())
    }

    function displayUrl(value) {
        let text = String(value || "")
        text = text.replace(/^https?:\/\//i, "")
        return text.replace(/\/$/, "")
    }

    function metadataKey(value) {
        let text = String(value || "").trim().toLowerCase()
        text = text.replace(/^https?:\/\//i, "")
        text = text.replace(/^www\./i, "")
        text = text.replace(/^@/, "")
        text = text.replace(/\/$/, "")
        return text
    }

    readonly property string usernameMetadataKey: root.showUsername ? root.metadataKey(root.activeDetail.username) : ""
    readonly property string urlMetadataKey: root.metadataKey(root.activeDetail.profileUrl)
    readonly property string locationMetadataKey: root.metadataKey(root.activeDetail.location)
    readonly property bool showProfileUrl: root.urlMetadataKey.length > 0
                                           && root.urlMetadataKey !== root.usernameMetadataKey
    readonly property bool showLocation: root.locationMetadataKey.length > 0
                                         && root.locationMetadataKey !== root.usernameMetadataKey
                                         && root.locationMetadataKey !== root.urlMetadataKey

    function rowTime(row) {
        const value = row || ({})
        const numeric = Number(value.createdAtMs || value.time || 0)
        return isFinite(numeric) ? numeric : 0
    }

    function popularityScore(row) {
        const value = row || ({})
        const likes = Number(value.likeCount || 0)
        const recasts = Number(value.recastCount || 0)
        const replies = Number(value.replyCount || 0)
        if (root.popularMetric === "likes")
            return likes
        if (root.popularMetric === "recasts")
            return recasts
        if (root.popularMetric === "replies")
            return replies
        return likes + recasts * 2 + replies * 1.5
    }

    function sortedRows() {
        const source = root.rows || []
        const result = []
        for (let index = 0; index < source.length; ++index)
            result.push(source[index])

        if (root.profileFeed === "user-popular") {
            result.sort(function(left, right) {
                const scoreDelta = root.popularityScore(right) - root.popularityScore(left)
                return scoreDelta !== 0 ? scoreDelta : root.rowTime(right) - root.rowTime(left)
            })
            return result
        }

        const direction = root.profileSortMode === "oldest" ? 1 : -1
        result.sort(function(left, right) {
            return (root.rowTime(left) - root.rowTime(right)) * direction
        })
        return result
    }

    readonly property var displayRows: root.sortedRows()

    function rowKey(row, index) {
        const value = row || ({})
        const castId = String(value.castId || value.hash || value.id || "")
        return castId.length > 0 ? "cast:" + castId : "cast-index:" + index
    }

    function rowRevision(row) {
        const value = row || ({})
        // Viewer state is tri-state: a missing field means not yet hydrated,
        // not a verified negative. A recycled profile cast must be updated
        // when the account changes or follow/reaction evidence arrives.
        function viewerEvidence(field, knowledgeField) {
            if (knowledgeField && value[knowledgeField] !== true)
                return "unknown"
            if (value[field] === undefined || value[field] === null)
                return "unknown"
            return Boolean(value[field]) ? "true" : "false"
        }
        return [String(value._revision || value.castId || value.hash || value.id || ""),
                String(value._referencedRevision || ""),
                String(value.likeCount || 0), String(value.recastCount || 0),
                String(value.replyCount || 0), viewerEvidence("likedByMe"),
                viewerEvidence("recastedByMe"),
                viewerEvidence("followingByMe", "followingKnown"),
                viewerEvidence("followsMe", "followsMeKnown"),
                String(value.writeState || "")].join("\u001f")
    }

    function rowIncubationHeight(row, availableWidth) {
        const value = row || ({})
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
        let reservation = 112 + textLines * lineHeight

        if (embeds.length > 0) {
            let visualCount = 0
            let supportingCount = 0
            let firstVisual = null
            for (let index = 0; index < embeds.length; ++index) {
                const embed = embeds[index] || ({})
                const kind = String(embed.kind || "").toLowerCase()
                if (kind === "image" || kind === "video" || kind === "media"
                        || String(embed.imageSource || embed.previewImage || "").length > 0) {
                    ++visualCount
                    if (firstVisual === null)
                        firstVisual = embed
                } else {
                    ++supportingCount
                }
            }
            if (visualCount === 1 && firstVisual !== null) {
                let aspect = Number(firstVisual.aspectRatio || 0)
                const declaredWidth = Number(firstVisual.width || 0)
                const declaredHeight = Number(firstVisual.height || 0)
                if (!(aspect > 0) && declaredWidth > 0 && declaredHeight > 0)
                    aspect = declaredWidth / declaredHeight
                if (!(aspect > 0))
                    aspect = 16 / 9
                aspect = Math.max(0.5, Math.min(2.8, aspect))
                reservation += Math.max(root.compact ? 180 : 220,
                                        Math.min(root.compact ? 420 : 520, contentWidth / aspect))
            } else if (visualCount > 1) {
                const columns = contentWidth < (root.compact ? 360 : 430) ? 1 : 2
                const tileWidth = columns === 1 ? contentWidth : (contentWidth - Theme.space2) / 2
                const rows = Math.ceil(Math.min(6, visualCount) / columns)
                reservation += rows * Math.max(120, Math.min(320, tileWidth / (4 / 3)))
                             + Math.max(0, rows - 1) * Theme.space2
            } else {
                reservation += root.compact ? 132 : 152
            }
            if (visualCount > 0 && supportingCount > 0)
                reservation += Math.min(2, supportingCount) * (root.compact ? 132 : 152)
        } else if (/https?:\/\//i.test(String(value.text || ""))) {
            // Match CastItem's inferred-link behavior even when protocol embeds
            // are absent so Profile rows reserve their preview geometry early.
            reservation += root.compact ? 132 : 152
        }
        return Math.max(88, Math.ceil(reservation))
    }

    // The profile reuses the same native stable-ID row model as Explorer.
    // Feeding ListView a freshly allocated array on every row change formerly
    // reset contentY to the top; targeted inserts/moves now preserve delegates
    // while the heavyweight content remains in our keyed payload store.
    function captureViewportAnchor() {
        if (root.restoringViewport || profileRowsModel.count === 0) return
        const headerHeight = profileList.headerItem ? profileList.headerItem.implicitHeight : 0
        // indexAt() takes *content* coordinates, not viewport coordinates.
        // Probing near y=0 while scrolled past the header never finds a row.
        const probeY = Math.max(headerHeight + 1, profileList.contentY + 1)
        const index = profileList.indexAt(Math.max(1, profileList.width / 2), probeY)
        if (index < 0 || index >= profileRowsModel.count) {
            // An empty incubation slot is not evidence that the old anchor
            // is still visible. Retaining it could teleport the view later.
            root.viewportAnchorKey = ""
            return
        }
        const item = profileList.itemAtIndex(index)
        if (!item) {
            root.viewportAnchorKey = ""
            return
        }
        root.viewportAnchorKey = String(profileRowsModel.get(index).stableKey || "")
        root.viewportAnchorOffset = item.y - profileList.contentY
    }

    function scheduleViewportRestore(key, offset, captureAfter) {
        if (key.length === 0)
            return
        root.pendingRestoreKey = key
        root.pendingRestoreOffset = offset
        root.pendingRestoreCapture = root.pendingRestoreCapture || captureAfter
        viewportRestoreTimer.restart()
    }

    function stabilizeViewportAfterGeometryChange() {
        if (profileList.moving || root.viewportAnchorKey.length === 0 || root.restoringViewport) return
        root.restoringViewport = true
        root.scheduleViewportRestore(root.viewportAnchorKey, root.viewportAnchorOffset, true)
    }

    function restoreViewportAnchor(anchorKey, anchorOffset) {
        if (anchorKey.length === 0 || profileRowsModel.count === 0)
            return
        let targetIndex = -1
        for (let index = 0; index < profileRowsModel.count; ++index) {
            if (String(profileRowsModel.get(index).stableKey) === anchorKey) {
                targetIndex = index
                break
            }
        }
        if (targetIndex < 0)
            return
        const item = profileList.itemAtIndex(targetIndex)
        if (!item) {
            // The old row is no longer instantiated. Forcing it back into view
            // during a late image/layout update is an unwanted scroll jump.
            // Let ListView keep the current visible range instead.
            return
        }
        // A cached-but-offscreen delegate is as stale as a pooled one. Never
        // drag the reader multiple viewports to satisfy an obsolete anchor.
        const currentOffset = item.y - profileList.contentY
        if (Math.abs(currentOffset - anchorOffset) > Math.max(320, profileList.height))
            return
        const minimum = profileList.originY
        const maximum = Math.max(minimum, profileList.contentHeight - profileList.height)
        profileList.contentY = Math.max(minimum, Math.min(maximum, item.y - anchorOffset))
    }

    function payloadForStableKey(key) {
        return root.stablePayloads[String(key || "")] || ({})
    }

    function requestStableModelSync() {
        if (profileList.moving) {
            root.stableModelSyncPending = true
            return
        }
        root.stableModelSyncPending = false
        root.synchronizeStableModel()
    }

    function synchronizeStableModel() {
        const source = root.displayRows
        // A refreshing provider can temporarily expose an empty list. Keep
        // the last page and its scroll range until the refresh completes.
        if (root.busy && source.length === 0 && profileRowsModel.count > 0)
            return
        let anchorKey = ""
        let anchorOffset = 0
        let structureChanged = false
        const retainedKeys = Object.create(null)
        const headerHeight = profileList.headerItem ? profileList.headerItem.implicitHeight : 0
        // indexAt() takes *content* coordinates, not viewport coordinates.
        // Probing near y=0 while scrolled past the header never finds a row.
        const probeY = Math.max(headerHeight + 1, profileList.contentY + 1)
        const anchorIndex = profileList.indexAt(Math.max(1, profileList.width / 2), probeY)
        if (anchorIndex >= 0 && anchorIndex < profileRowsModel.count) {
            const anchorItem = profileList.itemAtIndex(anchorIndex)
            anchorKey = String(profileRowsModel.get(anchorIndex).stableKey)
            anchorOffset = anchorItem ? anchorItem.y - profileList.contentY : 0
        }

        for (let index = 0; index < source.length; ++index) {
            const key = root.rowKey(source[index], index)
            const revision = root.rowRevision(source[index])
            retainedKeys[key] = true
            root.stablePayloads[key] = source[index]
            if (index < profileRowsModel.count
                    && String(profileRowsModel.get(index).stableKey) === key) {
                if (String(profileRowsModel.get(index).revisionKey || "") !== revision) {
                    profileRowsModel.setProperty(index, "revisionKey", revision)
                }
                continue
            }

            let existingIndex = -1
            for (let probe = index + 1; probe < profileRowsModel.count; ++probe) {
                if (String(profileRowsModel.get(probe).stableKey) === key) {
                    existingIndex = probe
                    break
                }
            }
            if (existingIndex >= 0) {
                profileRowsModel.move(existingIndex, index, 1)
                structureChanged = true
                if (String(profileRowsModel.get(index).revisionKey || "") !== revision) {
                    profileRowsModel.setProperty(index, "revisionKey", revision)
                }
            } else {
                profileRowsModel.insert(index, { stableKey: key, revisionKey: revision })
                structureChanged = true
            }
        }

        while (profileRowsModel.count > source.length) {
            profileRowsModel.remove(profileRowsModel.count - 1)
            structureChanged = true
        }

        // ProfileSurface can stay instantiated while navigating between
        // identities. Release former casts' large media/embed payloads after
        // their stable delegate keys have left this one authoritative model.
        for (const key in root.stablePayloads) {
            if (!retainedKeys[key])
                delete root.stablePayloads[key]
        }

        if (structureChanged && anchorKey.length > 0)
            root.scheduleViewportRestore(anchorKey, anchorOffset, false)
    }

    function isNearEnd() {
        if (profileList.count === 0 || profileList.contentHeight <= 0)
            return false
        const normalizedY = Math.max(0, profileList.contentY - profileList.originY)
        const preloadDistance = Math.max(520, profileList.height * 0.7)
        return profileList.atYEnd
                || normalizedY + profileList.height >= profileList.contentHeight - preloadDistance
    }

    function scheduleLoadMore() {
        // Never append while kinetic scrolling is active. Variable-height cast
        // delegates can otherwise move under the pointer when the model grows.
        if (profileList.moving || root.loadMorePending || root.busy || !root.hasMore
                || !root.isNearEnd())
            return
        root.loadMorePending = true
        scheduledLoadMoreTimer.restart()
    }

    function requestScheduledLoadMore() {
        root.loadMorePending = false
        if (!root.busy && root.hasMore && profileList.count > 0)
            root.loadMoreRequested()
    }

    // Re-tapping Profile is intentional navigation, unlike row refreshes.
    function scrollToBeginning() {
        viewportRestoreTimer.stop()
        root.pendingRestoreKey = ""
        root.viewportAnchorKey = ""
        root.restoringViewport = false
        profileList.cancelFlick()
        profileList.positionViewAtBeginning()
        root.resetPositionPasses = 1
        resetPositionTimer.restart()
    }

    function positionNewProfileAtBeginning() {
        const fid = Number(root.activeDetail.fid || 0)
        if (!root.visible || fid <= 0 || fid === root.positionedProfileFid)
            return
        root.positionedProfileFid = fid
        root.viewportAnchorKey = ""
        // Two event turns permit the ListView header to settle without a
        // delayed closure outliving a recycled/destroyed profile surface.
        root.resetPositionPasses = 2
        // A real identity navigation resets immediately. Only the follow-up
        // layout pass is deferred and may be cancelled by a user scroll.
        profileList.positionViewAtBeginning()
        resetPositionTimer.restart()
    }

    onVisibleChanged: {
        if (!root.visible)
            accountConnectionsPopup.close()
        root.positionNewProfileAtBeginning()
    }
    onDetailChanged: {
        const incomingFid = Number((root.detail || ({})).fid || 0)
        if (incomingFid > 0)
            root.retainedDetail = root.detail
        // A transient empty detail during metadata refresh is not a new
        // profile; clearing the ListView here teleports the reader to the top.
        if (incomingFid > 0 && incomingFid !== root.positionedProfileFid) {
            profileRowsModel.clear()
            root.viewportAnchorKey = ""
            root.stableModelSyncPending = false
        }
        root.positionNewProfileAtBeginning()
    }
    onRowsChanged: stableModelSyncTimer.restart()
    onProfileFeedChanged: {
        profileSortMode = "newest"
        popularMetric = "all"
        root.viewportAnchorKey = ""
        stableModelSyncTimer.restart()
        root.resetPositionPasses = 1
        // A selected profile tab is an explicit route change, unlike metadata
        // reflow. Reset synchronously before any new wheel event can intervene.
        profileList.positionViewAtBeginning()
        resetPositionTimer.restart()
    }
    onProfileSortModeChanged: stableModelSyncTimer.restart()
    onPopularMetricChanged: stableModelSyncTimer.restart()

    onHasMoreChanged: {
        if (!hasMore)
            loadMorePending = false
    }

    onBusyChanged: {
        if (!busy) {
            loadMorePending = false
            stableModelSyncTimer.restart()
        }
    }

    Timer {
        id: scheduledLoadMoreTimer
        interval: 0
        repeat: false
        onTriggered: root.requestScheduledLoadMore()
    }

    Timer {
        id: viewportRestoreTimer
        interval: 0
        repeat: false
        onTriggered: {
            const key = root.pendingRestoreKey
            const offset = root.pendingRestoreOffset
            const recapture = root.pendingRestoreCapture
            root.pendingRestoreKey = ""
            root.pendingRestoreCapture = false
            root.restoreViewportAnchor(key, offset)
            if (recapture) {
                root.restoringViewport = false
                root.captureViewportAnchor()
            }
        }
    }

    Timer {
        id: stableModelSyncTimer
        interval: 0
        repeat: false
        onTriggered: root.requestStableModelSync()
    }

    Timer {
        id: resetPositionTimer
        interval: 0
        repeat: false
        onTriggered: {
            if (root.resetPositionPasses <= 0)
                return
            // A queued reset belongs to navigation, never to the next wheel
            // frame. Desktop wheel input can change contentY without entering
            // ListView.moving; do not override a reader already scrolling.
            if (profileList.moving
                    || profileList.contentY - profileList.originY
                        > Math.max(100, profileList.height * 0.4)) {
                root.resetPositionPasses = 0
                return
            }
            profileList.positionViewAtBeginning()
            --root.resetPositionPasses
            if (root.resetPositionPasses > 0)
                resetPositionTimer.restart()
        }
    }

    Timer {
        id: scrollWorkThrottle
        interval: 96
        repeat: false
        onTriggered: {
            if (!root.restoringViewport)
                root.captureViewportAnchor()
        }
    }

    FeedRowsModel {
        id: profileRowsModel
        objectName: "profileRowsModel"
    }

    ListView {
        id: profileList
        objectName: "profileList"
        anchors.fill: parent
        clip: true
        model: profileRowsModel
        spacing: 0
        // Only a deliberate pull is allowed past the top; wheel scrolling
        // stays bounded and the shared indicator requests real profile reload.
        boundsBehavior: Flickable.DragOverBounds
        boundsMovement: Flickable.FollowBoundsBehavior
        reuseItems: true
        cacheBuffer: Math.min(Math.max(height * 0.75, 560), 960)
        pixelAligned: false

        onMovementStarted: {
            // User gestures outrank a pending initial-position timer.
            root.resetPositionPasses = 0
            resetPositionTimer.stop()
            viewportRestoreTimer.stop()
            root.restoringViewport = false
            root.pendingRestoreKey = ""
            root.viewportAnchorKey = ""
        }
        onMovementEnded: {
            scrollWorkThrottle.stop()
            if (root.stableModelSyncPending)
                root.requestStableModelSync()
            root.captureViewportAnchor()
            root.scheduleLoadMore()
        }
        // Do not probe indexAt()/itemAtIndex() every ~96 ms during a flick.
        // The anchor is captured once when movement ends; geometry stabilization
        // is only meaningful while the viewport is stationary.
        onContentYChanged: {
            if (root.resetPositionPasses > 0
                    && profileList.contentY - profileList.originY
                        > Math.max(100, profileList.height * 0.4)) {
                root.resetPositionPasses = 0
                resetPositionTimer.stop()
            }
            if (!profileList.moving && !root.restoringViewport && !scrollWorkThrottle.running)
                scrollWorkThrottle.start()
        }
        onContentHeightChanged: {
            if (!profileList.moving) {
                root.stabilizeViewportAfterGeometryChange()
                if (!scrollWorkThrottle.running)
                    scrollWorkThrottle.start()
            }
        }

        header: ColumnLayout {
            width: profileList.width
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.profileContentX
                Layout.rightMargin: root.profileContentX
                spacing: Theme.space2
                layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight

                ActionButton {
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconBack
                    text: qsTrId("genycaster.profile.back_back_to_feed")
                    onClicked: root.backRequested()
                }

                Item { Layout.fillWidth: true }

                ActionButton {
                    id: profileShareButton
                    objectName: "profileOptionsButton"
                    visible: !root.compact
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconMore
                    iconOnly: true
                    text: qsTr("Profile options")
                    enabled: Number(root.activeDetail.fid || 0) > 0
                    onClicked: {
                        profileShareMenu.active = true
                        const menu = profileShareMenu.item
                        if (!menu) return
                        const point = profileShareButton.mapToItem(Overlay.overlay, 0, profileShareButton.height + Theme.space1)
                        menu.x = Math.max(Theme.space2, Math.min(Overlay.overlay.width - menu.width - Theme.space2, point.x))
                        menu.y = Math.max(Theme.space2, Math.min(Overlay.overlay.height - menu.height - Theme.space2, point.y))
                        menu.open()
                    }

                    Loader {
                        id: profileShareMenu
                        active: false
                        sourceComponent: Component {
                            Popup {
                                id: sharePopup
                                objectName: "profileOptionsMenu"
                                parent: Overlay.overlay
                                width: 250
                                implicitHeight: shareActions.implicitHeight + Theme.space2
                                padding: Theme.space1
                                modal: true
                                dim: false
                                focus: true
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                background: PopupSurface { curveDepth: Theme.curveContainer }
                                contentItem: ColumnLayout {
                                    id: shareActions
                                    spacing: 0
                                    ActionButton {
                                        visible: root.isOwnProfile
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: Theme.iconCompose
                                        text: qsTr("Edit profile")
                                        onClicked: {
                                            sharePopup.close()
                                            profileExtrasEditor.open()
                                        }
                                    }
                                    ActionButton {
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: Theme.iconShare
                                        text: qsTr("Share profile")
                                        onClicked: {
                                            sharePopup.close()
                                            presentationBridge.shareUrl(presentationBridge.profileShareUrl(
                                                Number(root.activeDetail.fid || 0)))
                                        }
                                    }
                                    ActionButton {
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: Theme.iconCopy
                                        text: qsTr("Copy profile link")
                                        onClicked: {
                                            presentationBridge.copyText(presentationBridge.profileShareUrl(
                                                Number(root.activeDetail.fid || 0)))
                                            sharePopup.close()
                                        }
                                    }
                                    ActionButton {
                                        visible: presentationBridge.farcasterWebProfileUrl(
                                            String(root.activeDetail.username || "")).length > 0
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: Theme.iconArrowUpRight
                                        text: qsTr("Open in Farcaster")
                                        onClicked: {
                                            sharePopup.close()
                                            presentationBridge.openFarcasterProfile(
                                                String(root.activeDetail.username || ""))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconRefresh
                    text: root.busy ? qsTrId("genycaster.profile.refreshing") : qsTrId("genycaster.profile.refresh")
                    // ActionButton substitutes its continuously rotating
                    // GenySpinner for the icon while a refresh is pending.
                    busy: root.busy
                    enabled: !root.busy
                    onClicked: root.refreshProfileAndAccount()
                }
            }

            Column {
                id: profileHeader
                Layout.fillWidth: true
                spacing: 0

                Item {
                    id: bannerFrame
                    x: root.profileContentX
                    width: root.profileContentWidth
                    readonly property real bannerAspectRatio: root.compact ? 2.8 : 3.2
                    readonly property real coverRadius: Math.max(0, Math.min(Theme.shapeMedia, width / 2, height / 2))
                    readonly property real coverCubicFactor: {
                        const exponent = Math.max(2.0, Theme.superellipseExponent)
                        const diagonal = Math.pow(0.5, 1 / exponent)
                        return Math.max(0.5522847498, Math.min(0.985, (8 * diagonal - 4) / 3))
                    }
                    height: Boolean(root.activeDetail.bannerUrl)
                            ? Math.max(root.compact ? 132 : 156,
                                       Math.min(root.compact ? 196 : 280,
                                                Math.round(width / bannerAspectRatio)))
                            : (root.compact ? 56 : 64)

                    Superellipse {
                        anchors.fill: parent
                        curveDepth: Theme.shapeMedia
                        fillColor: Theme.surfaceRaised
                        borderWidth: Boolean(root.activeDetail.bannerUrl) ? 0 : Theme.strokeHairline
                        borderColor: Theme.borderMuted

                        // Keep the no-banner state deliberately quiet. It must read
                        // as an intentional empty cover, never as a loading skeleton.
                        Superellipse {
                            anchors.fill: parent
                            anchors.margins: 1
                            curveDepth: Theme.shapeMedia
                            fillColor: Theme.primarySurface
                            opacity: 0.18
                            borderWidth: 0
                        }
                    }

                    SuperellipseImage {
                        id: bannerImage
                        anchors.fill: parent
                        visible: Boolean(root.activeDetail.bannerUrl)
                        source: root.activeDetail.bannerUrl || ""
                        mediaCategory: "photos"
                        requestedSourceWidth: Math.max(1, Math.round(width))
                        requestedSourceHeight: Math.max(1, Math.round(height))
                        fillMode: Image.PreserveAspectCrop
                        cache: true
                        // Profile covers use the direct scene-graph image path.
                        // The large/wide MultiEffect mask can resolve the image
                        // successfully yet produce a blank layer on some render
                        // backends; the surrounding cover frame still owns the
                        // visual boundary and border.
                        clipToSuperellipse: false
                        curveDepth: Theme.shapeMedia
                        fillColor: Theme.surfaceRaised
                        borderWidth: 0
                        showLoadingIndicator: true
                        showFailureIndicator: true
                        retainPreviousWhileLoading: false
                    }

                    // Keep the cover on the direct Image path, but mask only the
                    // four outside corner patches. A full-frame odd/even mask
                    // shares its inner path with the image edge and CurveRenderer
                    // can antialias that coincidence into a visible hairline.
                    Shape {
                        anchors.fill: parent
                        z: 2
                        visible: bannerImage.visible && bannerImage.ready
                        preferredRendererType: Shape.CurveRenderer
                        asynchronous: false

                        // Top-left outside patch.
                        ShapePath {
                            fillColor: Theme.background
                            strokeColor: "transparent"
                            strokeWidth: -1
                            startX: 0
                            startY: 0
                            PathLine { x: bannerFrame.coverRadius; y: 0 }
                            PathCubic {
                                x: 0
                                y: bannerFrame.coverRadius
                                control1X: bannerFrame.coverRadius
                                           - bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                                control1Y: 0
                                control2X: 0
                                control2Y: bannerFrame.coverRadius
                                           - bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                            }
                            PathLine { x: 0; y: 0 }
                        }

                        // Top-right outside patch.
                        ShapePath {
                            fillColor: Theme.background
                            strokeColor: "transparent"
                            strokeWidth: -1
                            startX: bannerFrame.width
                            startY: 0
                            PathLine { x: bannerFrame.width - bannerFrame.coverRadius; y: 0 }
                            PathCubic {
                                x: bannerFrame.width
                                y: bannerFrame.coverRadius
                                control1X: bannerFrame.width - bannerFrame.coverRadius
                                           + bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                                control1Y: 0
                                control2X: bannerFrame.width
                                control2Y: bannerFrame.coverRadius
                                           - bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                            }
                            PathLine { x: bannerFrame.width; y: 0 }
                        }

                        // Bottom-right outside patch.
                        ShapePath {
                            fillColor: Theme.background
                            strokeColor: "transparent"
                            strokeWidth: -1
                            startX: bannerFrame.width
                            startY: bannerFrame.height
                            PathLine { x: bannerFrame.width; y: bannerFrame.height - bannerFrame.coverRadius }
                            PathCubic {
                                x: bannerFrame.width - bannerFrame.coverRadius
                                y: bannerFrame.height
                                control1X: bannerFrame.width
                                control1Y: bannerFrame.height - bannerFrame.coverRadius
                                           + bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                                control2X: bannerFrame.width - bannerFrame.coverRadius
                                           + bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                                control2Y: bannerFrame.height
                            }
                            PathLine { x: bannerFrame.width; y: bannerFrame.height }
                        }

                        // Bottom-left outside patch.
                        ShapePath {
                            fillColor: Theme.background
                            strokeColor: "transparent"
                            strokeWidth: -1
                            startX: 0
                            startY: bannerFrame.height
                            PathLine { x: bannerFrame.coverRadius; y: bannerFrame.height }
                            PathCubic {
                                x: 0
                                y: bannerFrame.height - bannerFrame.coverRadius
                                control1X: bannerFrame.coverRadius
                                           - bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                                control1Y: bannerFrame.height
                                control2X: 0
                                control2Y: bannerFrame.height - bannerFrame.coverRadius
                                           + bannerFrame.coverCubicFactor * bannerFrame.coverRadius
                            }
                            PathLine { x: 0; y: bannerFrame.height }
                        }
                    }

                    TapHandler {
                        enabled: Boolean(root.activeDetail.bannerUrl)
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: root.mediaRequested([
                            { imageSource: String(root.activeDetail.bannerUrl || ""), kind: "image" }
                        ], 0)
                    }
                }

                Item {
                    width: parent.width
                    height: Math.round(root.profileAvatarSize * 0.60) + Theme.space2

                    Superellipse {
                        x: root.profileAvatarX - 4
                        y: -Math.round(root.profileAvatarSize * 0.50) - 4
                        width: root.profileAvatarSize + 8
                        height: root.profileAvatarSize + 8
                        curveDepth: Math.round(width * 0.30)
                        exponent: 3.8
                        fillColor: Theme.surface
                    }

                    Avatar {
                        id: profileAvatar
                        x: root.profileAvatarX
                        y: -Math.round(root.profileAvatarSize * 0.50)
                        avatarSize: root.profileAvatarSize
                        sourceUrl: root.activeDetail.avatarUrl || ""
                        displayName: root.activeDetail.displayName || root.activeDetail.username || ""
                    }

                    TapHandler {
                        parent: profileAvatar
                        enabled: Boolean(root.activeDetail.avatarUrl)
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: root.mediaRequested([
                            { imageSource: String(root.activeDetail.avatarUrl || ""), kind: "image" }
                        ], 0)
                    }

                    RowLayout {
                        id: ownerSocialBadgeStrip
                        objectName: "ownerVerifiedSocialBadgeStrip"
                        width: Math.min(implicitWidth, Math.max(0, root.profileContentWidth - root.profileAvatarSize - Theme.space2))
                        x: root.profileContentX + root.profileContentWidth - width
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.isOwnProfile && root.ownerSocialLinks.length > 0
                        spacing: Theme.space1
                        // Keep the social proof group at the physical right edge
                        // on desktop. On phones hide the verbose heading to fit.
                        layoutDirection: Qt.LeftToRight

                        Label {
                            visible: !root.compact
                            text: qsTr("Verified networks")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightDemiBold
                        }

                        Repeater {
                            model: root.ownerSocialLinks
                            delegate: Button {
                                id: socialBadge
                                required property var modelData
                                readonly property bool liveVerified: openProofConnections.authorized
                                readonly property bool hasPublicUrl: String(modelData.href || "").length > 0
                                readonly property bool canFind: String(modelData.searchHref || "").length > 0
                                readonly property string hintText: String(modelData.label)
                                    + (hasPublicUrl
                                        ? (modelData.manualUrl ? qsTr(" · URL entered by you (not LinkedIn-verified)")
                                          : liveVerified ? qsTr(" · linked profile") : qsTr(" · previously linked"))
                                        : canFind ? qsTr(" · search LinkedIn by name")
                                                  : qsTr(" · manage connection"))
                                Layout.preferredWidth: Theme.scaleUi(root.compact ? 29 : 34)
                                Layout.preferredHeight: Theme.scaleUi(root.compact ? 29 : 34)
                                hoverEnabled: !root.compact
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                display: AbstractButton.IconOnly
                                icon.source: root.socialIcon(String(modelData.provider))
                                icon.width: Theme.scaleUi(root.compact ? 18 : 21)
                                icon.height: Theme.scaleUi(root.compact ? 18 : 21)
                                background: Rectangle {
                                    radius: Theme.curveCompact
                                    color: socialBadge.down ? Theme.surfacePressed
                                           : socialBadge.hovered ? Theme.surfaceHover : Theme.surface
                                    border.color: Theme.keyboardFocusVisible(socialBadge) ? Theme.focus
                                                  : socialBadge.hovered ? Theme.accent : Theme.borderMuted
                                    border.width: socialBadge.hovered || Theme.keyboardFocusVisible(socialBadge) ? 2 : 1
                                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                                }
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.rightMargin: -2
                                    anchors.bottomMargin: -2
                                    width: Theme.scaleUi(13)
                                    height: width
                                    radius: width / 2
                                    // Blue indicates a previously linked OpenProof
                                    // account; green indicates current live proof.
                                    // The tooltip still discloses cached provenance.
                                    color: socialBadge.liveVerified ? Theme.success : Theme.accent
                                    border.color: Theme.surface
                                    border.width: 1
                                    Icon {
                                        anchors.centerIn: parent
                                        text: Theme.iconCheck
                                        preferredSize: 9
                                        color: Theme.contrastInk(socialBadge.liveVerified ? Theme.success : Theme.accent)
                                    }
                                }
                                ToolTip {
                                    visible: socialBadge.hovered && !root.compact
                                    delay: 500
                                    timeout: 3500
                                    x: (socialBadge.width - width) / 2
                                    y: socialBadge.height + Theme.space2
                                    width: Theme.scaleUi(248)
                                    padding: Theme.space2
                                    contentItem: Text {
                                        text: socialBadge.hintText
                                        color: Theme.ink
                                        font.pixelSize: Theme.typeCaptionSize
                                        wrapMode: Text.Wrap
                                    }
                                    background: Rectangle {
                                        radius: Theme.curveCompact
                                        color: Theme.surfaceElevated
                                        border.color: Theme.borderMuted
                                        border.width: 1
                                    }
                                }
                                Accessible.name: hintText
                                Accessible.description: hasPublicUrl ? qsTr("Open verified network profile")
                                    : canFind ? qsTr("Find the linked account on LinkedIn") : qsTr("Manage connection")
                                onClicked: {
                                    if (hasPublicUrl) root.externalRequested(String(modelData.href))
                                    else if (canFind) root.externalRequested(String(modelData.searchHref))
                                    else root.openConnections()
                                }
                            }
                        }
                        ActionButton {
                            objectName: "profileConnectionsButton"
                            compact: true
                            quiet: true
                            iconGlyph: Theme.iconLink
                            text: qsTr("Manage")
                            Accessible.description: qsTr("Manage verified social connections")
                            onClicked: root.openConnections()
                        }
                    }

                    ActionButton {
                        x: root.profileContentRtl ? root.profileContentX : root.profileContentX + root.profileContentWidth - width
                        anchors.top: parent.top
                        anchors.topMargin: Theme.space2
                        visible: Boolean(root.activeDetail.followingKnown)
                                 && Number(root.activeDetail.fid || 0) > 0
                                 && !root.isOwnProfile
                        compact: true
                        iconGlyph: Boolean(root.activeDetail.followingByMe) ? Theme.iconCheck : ""
                        text: Boolean(root.activeDetail.followingByMe) ? qsTrId("genycaster.profile.followed") : qsTrId("genycaster.profile.follow")
                        selected: Boolean(root.activeDetail.followingByMe)
                        prominent: !Boolean(root.activeDetail.followingByMe)
                        Accessible.description: Boolean(root.activeDetail.followingByMe)
                                                ? qsTrId("genycaster.profile.unfollow") : text
                        onClicked: {
                            if (!root.interactionsAvailable) {
                                root.authenticationRequested()
                                return
                            }
                            root.followingMutationRequested(
                                Number(root.activeDetail.fid), !Boolean(root.activeDetail.followingByMe))
                        }
                    }
                }

                ColumnLayout {
                    width: root.profileContentWidth
                    x: root.profileContentX
                    spacing: Theme.space1

                    RowLayout {
                        objectName: "profileIdentityRow"
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            Layout.minimumWidth: Theme.scaleUi(root.compact ? 76 : 125)
                            text: root.activeDetail.displayName || (root.showUsername ? root.activeDetail.username : "")
                                  || qsTrId("genycaster.profile.farcaster_profile")
                            color: Theme.ink
                            font.pixelSize: root.compact ? Theme.typeTitle2Size : Theme.typeTitle1Size
                            font.weight: Theme.weightBold
                            horizontalAlignment: Theme.textStartAlignment
                            wrapMode: root.compact ? Text.NoWrap : Text.Wrap
                            elide: root.compact ? Text.ElideRight : Text.ElideNone
                        }

                        // Reserved for a future server-signed, wallet-bound GENY
                        // holding attestation. Never display from editable metadata.
                        Rectangle {
                            id: genyHolderBadge
                            objectName: "verifiedGenyHolderBadge"
                            visible: root.genyHolderAttested
                            implicitWidth: holderBadgeContent.implicitWidth + Theme.space3
                            implicitHeight: Math.max(25, holderBadgeContent.implicitHeight + Theme.space1)
                            radius: implicitHeight / 2
                            color: Theme.dark ? "#342812" : "#FFF6DE"
                            border.color: "#D6A435"
                            border.width: 1
                            Row {
                                id: holderBadgeContent
                                anchors.centerIn: parent
                                spacing: Theme.space1
                                Icon {
                                    text: Theme.iconCheck
                                    color: Theme.dark ? "#F5CD66" : "#956A0B"
                                    preferredSize: Theme.typeCaptionSize
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Label {
                                    text: qsTr("GENY")
                                    color: Theme.dark ? "#F5CD66" : "#956A0B"
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightBold
                                }
                            }
                            ToolTip.visible: genyHolderBadgeHover.hovered
                            ToolTip.text: qsTr("Verified GENY holder")
                            HoverHandler { id: genyHolderBadgeHover }
                        }

                        Item {
                            id: followsYouBadge
                            visible: Boolean(root.activeDetail.followsMeKnown) && Boolean(root.activeDetail.followsMe)
                                     && !root.isOwnProfile
                            implicitWidth: followsYouText.implicitWidth + Theme.space3
                            implicitHeight: Math.max(24, followsYouText.implicitHeight + Theme.space1)

                            Superellipse {
                                anchors.fill: parent
                                fillColor: Theme.surfaceRaised
                                borderWidth: 1
                                borderColor: Theme.borderStrong
                                curveDepth: Math.round(height / 2)
                            }

                            Label {
                                id: followsYouText
                                anchors.centerIn: parent
                                text: qsTrId("genycaster.profile.follows_you")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
                        }

                        RowLayout {
                            id: identityLinks
                            objectName: "profileIdentityLinks"
                            visible: root.primaryProfileLinkUrl.length > 0
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            Layout.preferredWidth: Math.min(root.profileContentWidth * 0.57,
                                Theme.scaleUi(root.compact ? 218 : 260))
                            Layout.maximumWidth: Layout.preferredWidth
                            spacing: Theme.space1
                            layoutDirection: Qt.LeftToRight

                            Button {
                                id: identityPrimaryLink
                                objectName: "profileIdentityPrimaryLink"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                flat: true
                                hoverEnabled: true
                                padding: 0
                                background: Item {}
                                contentItem: RowLayout {
                                    spacing: Theme.space1
                                    Icon {
                                        text: Theme.iconLink
                                        color: Theme.accent
                                        preferredSize: Theme.typeCaptionSize
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: root.displayUrl(root.primaryProfileLinkUrl)
                                        color: Theme.accent
                                        font.family: Theme.latinFontFamily
                                        font.pixelSize: Theme.typeCaptionSize
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                                Accessible.role: Accessible.Link
                                Accessible.name: root.primaryProfileLinkUrl
                                onClicked: root.externalRequested(root.primaryProfileLinkUrl)
                            }
                            Button {
                                id: identityMoreLinks
                                objectName: "profileMoreLinksInline"
                                visible: root.isOwnProfile && root.additionalProfileLinksCount > 0
                                Layout.alignment: Qt.AlignVCenter
                                flat: true
                                hoverEnabled: true
                                padding: Theme.space1
                                background: Rectangle {
                                    radius: Theme.curveCompact
                                    color: identityMoreLinks.hovered ? Theme.surfaceHover : "transparent"
                                    Behavior on color {
                                        ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
                                    }
                                }
                                contentItem: Label {
                                    text: qsTr("and %1 more").arg(root.additionalProfileLinksCount)
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightDemiBold
                                }
                                Accessible.name: qsTr("Show all profile links")
                                onClicked: profileLinksDrawer.open()
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        layoutDirection: Theme.uiDirection

                        Label {
                            visible: root.showUsername
                            Layout.maximumWidth: Math.max(160, parent.width * 0.72)
                            LayoutMirroring.enabled: false
                            text: root.ltrHandle(root.activeDetail.username)
                            font.family: Theme.latinFontFamily
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            horizontalAlignment: Theme.textStartAlignment
                            elide: Text.ElideRight
                        }


                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        objectName: "profileProfessionalCategory"
                        Layout.fillWidth: true
                        visible: root.profileProfession.length > 0 || root.profileCategory.length > 0
                        spacing: Theme.space2
                        Icon {
                            text: Theme.iconBusiness
                            color: Theme.inkMuted
                            preferredSize: Theme.typeCaptionSize
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.profileCategory.length > 0 && root.profileProfession.length > 0
                                  ? root.profileCategory + " · " + root.profileProfession
                                  : (root.profileCategory || root.profileProfession)
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightMedium
                            elide: Text.ElideRight
                        }
                    }

                    RichText {
                        visible: root.normalizedBio.length > 0
                        Layout.fillWidth: true
                        sourceText: root.normalizedBio
                        textSize: Theme.typeBodySize
                        textLineHeight: Theme.typeBodyLineHeight
                        onExternalRequested: url => root.externalRequested(url)
                    }

                    RowLayout {
                        objectName: "profileLocationRow"
                        visible: root.showLocation
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        Icon {
                            text: Theme.iconLocation
                            color: Theme.inkSubtle
                            preferredSize: Theme.typeCaptionSize
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.displayLocation
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        // Unknown profile metadata must never be presented as
                        // verified zero followers/following on an empty header.
                        visible: Number(root.activeDetail.fid || 0) > 0
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        layoutDirection: Theme.uiDirection

                        Button {
                            id: followingCountButton
                            objectName: "profileFollowingCount"
                            flat: true
                            hoverEnabled: true
                            onClicked: root.followingRequested()
                            Accessible.name: qsTrId("genycaster.profile.value_following")
                                .arg(root.compactCount(root.activeDetail.followingCount))
                            background: Item {}
                            padding: Theme.space1
                            contentItem: Row {
                                spacing: Theme.space1
                                Label {
                                    text: root.compactCount(root.activeDetail.followingCount)
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightDemiBold
                                }
                                Label {
                                    text: qsTr("Following")
                                    color: followingCountButton.hovered ? Theme.ink : Theme.inkMuted
                                    font.pixelSize: Theme.typeBodySize
                                }
                            }
                        }

                        Button {
                            id: followersCountButton
                            objectName: "profileFollowersCount"
                            flat: true
                            hoverEnabled: true
                            onClicked: root.followersRequested()
                            Accessible.name: qsTrId("genycaster.profile.value_followers")
                                .arg(root.compactCount(root.activeDetail.followerCount))
                            background: Item {}
                            padding: Theme.space1
                            contentItem: Row {
                                spacing: Theme.space1
                                Label {
                                    text: root.compactCount(root.activeDetail.followerCount)
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightDemiBold
                                }
                                Label {
                                    text: qsTr("Followers")
                                    color: followersCountButton.hovered ? Theme.ink : Theme.inkMuted
                                    font.pixelSize: Theme.typeBodySize
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        objectName: "profileOwnerActions"
                        Layout.fillWidth: true
                        visible: root.isOwnProfile && root.compact
                        spacing: Theme.space2
                        layoutDirection: Qt.LeftToRight
                        // Two equal, gutter-to-gutter touch targets on phones.
                        // Desktop actions live exclusively in profileOptionsMenu.
                        ActionButton {
                            objectName: "profileShareAction"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: Math.max(0, (root.profileContentWidth - Theme.space2) / 2)
                            compact: true
                            quiet: false
                            pill: true
                            elevated: false
                            text: qsTr("Share profile")
                            enabled: Number(root.activeDetail.fid || 0) > 0
                            onClicked: presentationBridge.shareUrl(
                                presentationBridge.profileShareUrl(Number(root.activeDetail.fid || 0)))
                        }
                        ActionButton {
                            objectName: "profileEditAction"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: Math.max(0, (root.profileContentWidth - Theme.space2) / 2)
                            compact: true
                            quiet: false
                            pill: true
                            elevated: false
                            text: qsTr("Edit profile")
                            onClicked: profileExtrasEditor.open()
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: Number(root.activeDetail.fid || 0) > 0
                                 && root.activeDetail.socialCountsKnown === true
                                 && providerProfiles.activeReadContract === "snapchain-v1"
                        text: qsTrId("genycaster.nodes.counts_short")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: Number(root.activeDetail.fid || 0) > 0
                                 && root.activeDetail.socialCountsKnown === false
                        text: qsTrId("genycaster.profile.social_totals_unavailable")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }

                }
            }

            // One compact navigation row, like the X profile timeline:
            // the sort/filter action lives alongside the content tabs rather
            // than occupying an otherwise empty second row above the casts.
            Item {
                Layout.fillWidth: true
                implicitHeight: 48

                Item {
                    x: root.profileContentX
                    width: root.profileContentWidth
                    height: parent.height

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.divider
                    }

                    RowLayout {
                        anchors.fill: parent
                        layoutDirection: Theme.uiDirection
                        spacing: root.compact ? 2 : Theme.space2

                        GenyTabStrip {
                            id: profileTabs
                            objectName: "profileTabs"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumWidth: 0
                            distribute: true
                            segmented: false
                            minimumTabWidth: root.compact ? 52 : 68
                            spacing: 0
                            currentId: root.profileFeed
                            tabs: [
                                { id: "user-posts", label: qsTrId("genycaster.profile.posts") },
                                { id: "user-quotes", label: qsTrId("genycaster.profile.quotes") },
                                { id: "user-replies", label: qsTrId("genycaster.profile.replies") },
                                { id: "user-likes", label: qsTrId("genycaster.profile.likes") },
                                { id: "user-popular", label: qsTrId("genycaster.profile.popular") }
                            ]
                            onActivated: tabId => root.profileFeedRequested(tabId)
                        }

                        FilterDropdown {
                            id: profileSortFilter
                            objectName: "profileSortFilter"
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: implicitWidth
                            Layout.maximumWidth: root.compact ? 98 : 136
                            model: root.profileFeed === "user-popular"
                                   ? [
                                       qsTrId("genycaster.profile.sort_all"),
                                       qsTrId("genycaster.profile.sort_likes"),
                                       qsTrId("genycaster.profile.sort_recasts"),
                                       qsTrId("genycaster.profile.sort_replies")
                                     ]
                                   : [
                                       qsTrId("genycaster.profile.sort_newest"),
                                       qsTrId("genycaster.profile.sort_oldest")
                                     ]
                            currentIndex: root.profileFeed === "user-popular"
                                          ? (root.popularMetric === "likes" ? 1
                                             : root.popularMetric === "recasts" ? 2
                                             : root.popularMetric === "replies" ? 3 : 0)
                                          : (root.profileSortMode === "oldest" ? 1 : 0)
                            onActivated: index => {
                                if (root.profileFeed === "user-popular") {
                                    root.popularMetric = index === 1 ? "likes"
                                                       : index === 2 ? "recasts"
                                                       : index === 3 ? "replies" : "all"
                                } else {
                                    root.profileSortMode = index === 1 ? "oldest" : "newest"
                                }
                            }
                        }
                    }
                }
            }
        }

        delegate: Item {
            id: profileCastDelegate
            required property string stableKey
            required property string revisionKey
            readonly property var modelData: {
                const revision = profileCastDelegate.revisionKey
                return root.payloadForStableKey(profileCastDelegate.stableKey)
            }
            readonly property real reservedHeight: root.rowIncubationHeight(
                                                       profileCastDelegate.modelData,
                                                       profileCastDelegate.width)
            readonly property real resolvedHeight: profileCastLoader.item
                                                   && profileCastLoader.item.implicitHeight > 0
                                                   ? profileCastLoader.item.implicitHeight : 0
            property real committedHeight: reservedHeight

            function synchronizeHeight() {
                // The estimated slot must not outlive the real delegate geometry:
                // keeping an oversized reservation until a flick ends exposes a
                // large empty gap between profile casts.
                if (profileCastLoader.status === Loader.Ready
                        && profileCastDelegate.resolvedHeight > 0)
                    profileCastDelegate.committedHeight = profileCastDelegate.resolvedHeight
                else
                    profileCastDelegate.committedHeight = profileCastDelegate.reservedHeight
            }

            // On mobile the ListView owns the viewport width and CastItem owns
            // the single symmetric content gutter. Keeping profileContentX here
            // produced a second inset that Explorer no longer has.
            x: root.compact ? 0 : root.profileContentX
            width: root.compact ? profileList.width : root.profileContentWidth
            height: Math.max(1, committedHeight)
            ListView.onPooled: profileCastDelegate.visible = false
            ListView.onReused: profileCastDelegate.visible = true
            onReservedHeightChanged: profileCastDelegate.synchronizeHeight()
            onResolvedHeightChanged: profileCastDelegate.synchronizeHeight()
            onStableKeyChanged: profileCastDelegate.committedHeight = profileCastDelegate.reservedHeight

            CastSkeleton {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                visible: profileCastLoader.status !== Loader.Ready
                mediaPreview: profileCastDelegate.reservedHeight > 320
                animate: !profileList.moving
            }

            Loader {
                id: profileCastLoader
                anchors.fill: parent
                asynchronous: true
                sourceComponent: Component {
                    CastItem {
                        implicitWidth: profileCastDelegate.width
                        modelData: profileCastDelegate.modelData
                        compact: root.compact
                        interactionsAvailable: root.interactionsAvailable
                        accountFid: root.accountFid
                        onOpenRequested: castId => root.castRequested(castId)
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
            }

            Connections {
                target: profileList
                function onMovingChanged() {
                    if (!profileList.moving)
                        profileCastDelegate.synchronizeHeight()
                }
            }
        }

        footer: Item {
            width: profileList.width
            height: emptyProfileState.visible ? 176 : (root.busy ? 72 : (root.compact ? 112 : 52))

            ColumnLayout {
                id: emptyProfileState
                x: root.profileContentX
                width: root.profileContentWidth
                anchors.verticalCenter: parent.verticalCenter
                visible: root.rows.length === 0 && !root.busy && root.runtimeReady
                         && root.errorText.length === 0
                spacing: Theme.space2

                Label {
                    Layout.fillWidth: true
                    text: root.profileFeed === "user-quotes"
                          ? qsTrId("genycaster.profile.no_quotes_here_yet")
                          : root.profileFeed === "user-replies"
                            ? qsTrId("genycaster.profile.no_replies_here_yet")
                            : root.profileFeed === "user-likes"
                              ? qsTrId("genycaster.profile.no_liked_casts_here_yet")
                              : root.profileFeed === "user-popular"
                                ? qsTrId("genycaster.profile.no_popular_casts_available_yet")
                                : qsTrId("genycaster.profile.no_posts_here_yet")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    text: root.offline || root.sourceUnavailable
                          ? qsTrId("genycaster.profile.saved_profile_data_is_empty_retry_when_the_live_source")
                          : qsTrId("genycaster.profile.try_another_profile_tab_or_refresh")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            }

            GenySpinner {
                anchors.centerIn: parent
                visible: root.busy
                running: visible && !profileList.moving
                Accessible.name: qsTrId("genycaster.profile.loading_more_profile_posts")
            }
        }
    }

    // All account-owned sheets reuse the application's interactive
    // GenyDrawer motion contract (position animation + reduced-motion support).
    GenyDrawer {
        id: profileExtrasEditor
        objectName: "profileExtrasEditor"
        parent: Overlay.overlay
        readonly property bool mobilePresentation: parent && parent.width < Theme.breakpointMobile
        bottomSheet: mobilePresentation
        edge: mobilePresentation ? Qt.BottomEdge : Qt.RightEdge
        dimBackground: true
        width: parent ? (mobilePresentation ? parent.width : Math.min(480, parent.width)) : 480
        height: parent ? (mobilePresentation ? Math.min(parent.height * 0.92, 750) : parent.height) : 750
        showHandle: mobilePresentation
        contentPadding: Theme.space4
        onOpened: {
            openProofConnections.checkCloudService()
            const savedCategory = String(root.profileExtras.category || "")
            const savedRole = String(root.profileExtras.profession || "")
            const inferredCategory = !savedCategory && savedRole === "Developer"
                                     ? "Technology" : savedCategory
            categoryEditor.currentIndex = Math.max(0,
                root.profileCategoryOptions.indexOf(inferredCategory))
            professionEditor.currentIndex = Math.max(0,
                root.profileRoleOptions.indexOf(savedRole))
            const saved = Array.isArray(root.profileExtras.links) ? root.profileExtras.links : []
            const titles = Array.isArray(root.profileExtras.labels) ? root.profileExtras.labels : []
            linksEditor.text = saved.map((url,index)=>titles[index]
                ? titles[index] + " | " + url : url).join("\n")
            root.profileExtrasError = ""
        }
        contentItem: ColumnLayout {
            spacing: Theme.space3
            ModalHeader {
                Layout.fillWidth: true
                title: qsTr("Edit profile details")
                subtitle: qsTr("Profession and personal links")
                onCloseRequested: profileExtrasEditor.close()
            }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentHeight: formColumn.implicitHeight + Theme.space4
                contentWidth: width
                ColumnLayout {
                    id: formColumn
                    width: parent.width
                    spacing: Theme.space3
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTr("Professional category")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    GenyComboBox {
                        id: categoryEditor
                        objectName: "profileCategoryPicker"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        popupMaximumWidth: width
                        model: root.profileCategoryOptions
                        Accessible.name: qsTr("Professional category")
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTr("Role")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    GenyComboBox {
                        id: professionEditor
                        objectName: "profileProfessionPicker"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        popupMaximumWidth: width
                        model: root.profileRoleOptions
                        Accessible.name: qsTr("Professional role")
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTr("Links — optional title | HTTPS URL, one per line")
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.inkMuted
                    }
                    TextArea {
                        id: linksEditor
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.scaleUi(180)
                        wrapMode: TextEdit.WrapAnywhere
                        placeholderText: qsTr("Personal | https://example.com\nCompany | https://company.com")
                        color: Theme.ink
                        background: Rectangle {
                            color: Theme.surface
                            radius: Theme.curveCompact
                            border.color: linksEditor.activeFocus ? Theme.focus : Theme.borderMuted
                            border.width: 1
                        }
                    }
                    DirectionAwareLabel {
                        visible: root.profileExtrasError.length > 0
                        Layout.fillWidth: true
                        color: Theme.negative
                        text: root.profileExtrasError
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    Layout.fillWidth: true
                    quiet: true
                    text: qsTr("Cancel")
                    onClicked: profileExtrasEditor.close()
                }
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTr("Save changes")
                    onClicked: {
                        if (root.saveExtraDetails(
                                professionEditor.currentIndex > 0 ? professionEditor.currentText : "",
                                linksEditor.text,
                                categoryEditor.currentIndex > 0 ? categoryEditor.currentText : ""))
                            profileExtrasEditor.close()
                    }
                }
            }
        }
    }

    GenyDrawer {
        id: profileLinksDrawer
        objectName: "profileLinksDrawer"
        parent: Overlay.overlay
        readonly property bool mobilePresentation: parent && parent.width < Theme.breakpointMobile
        bottomSheet: mobilePresentation
        edge: mobilePresentation ? Qt.BottomEdge : Qt.RightEdge
        dimBackground: true
        showHandle: mobilePresentation
        width: parent ? (mobilePresentation ? parent.width : Math.min(440, parent.width)) : 440
        height: parent ? (mobilePresentation ? Math.min(parent.height * 0.68, 530) : parent.height) : 530
        contentPadding: Theme.space4
        contentItem: ColumnLayout {
            spacing: Theme.space3
            ModalHeader {
                Layout.fillWidth: true
                title: qsTr("Links")
                subtitle: qsTr("Personal and professional websites")
                onCloseRequested: profileLinksDrawer.close()
            }
            Flickable {
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentHeight: linksSheetRows.implicitHeight + Theme.space4
                ColumnLayout {
                    id: linksSheetRows
                    width: parent.width
                    spacing: Theme.space3
                    Button {
                        visible: root.showProfileUrl
                        Layout.fillWidth: true
                        flat: true
                        background: Rectangle {
                            radius: Theme.curveCompact
                            color: parent.hovered ? Theme.surfaceHover : Theme.surface
                        }
                        padding: Theme.space2
                        contentItem: ColumnLayout {
                            spacing: Theme.space1
                            DirectionAwareLabel {
                                text: qsTr("Farcaster website")
                                color: Theme.ink
                                font.weight: Theme.weightDemiBold
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.displayUrl(root.activeDetail.profileUrl || "")
                                color: Theme.inkMuted
                                elide: Text.ElideRight
                            }
                        }
                        onClicked: {
                            root.externalRequested(root.activeDetail.profileUrl || "")
                            profileLinksDrawer.close()
                        }
                    }
                    Repeater {
                        model: root.profileExtraLinks
                        delegate: Button {
                            id: linkSheetRow
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            flat: true
                            padding: Theme.space2
                            background: Rectangle {
                                radius: Theme.curveCompact
                                color: linkSheetRow.hovered ? Theme.surfaceHover : Theme.surface
                                Behavior on color {
                                    ColorAnimation { duration: Theme.motionFast }
                                }
                            }
                            contentItem: ColumnLayout {
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    text: root.profileExtraLabel(linkSheetRow.index)
                                    color: Theme.ink
                                    font.weight: Theme.weightDemiBold
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: root.displayUrl(linkSheetRow.modelData)
                                    color: Theme.inkMuted
                                    elide: Text.ElideRight
                                }
                            }
                            onClicked: {
                                root.externalRequested(String(modelData))
                                profileLinksDrawer.close()
                            }
                        }
                    }
                }
            }
        }
    }

    AccountConnectionsPopup {
        id: accountConnectionsPopup
        // The drawer must cover the whole application viewport, not only
        // the profile center pane, on both desktop and mobile.
        parent: Overlay.overlay
        providers: typeof openProofConnections !== "undefined"
                   && openProofConnections !== null
                   ? openProofConnections.providers : []
        connections: openProofConnections.connections
        catalogFallback: openProofConnections.catalogFallback
        authorized: openProofConnections.authorized
        cachedSnapshot: openProofConnections.cachedSnapshot
        authorizing: openProofConnections.authorizing
        approvalUrl: openProofConnections.approvalUrl
        actionBusy: openProofConnections.actionBusy
        pendingProvider: openProofConnections.pendingProvider
        manualLinkedInUrl: openProofConnections.manualLinkedInUrl
        errorText: openProofConnections.error
        onOpenProfileRequested: url => root.externalRequested(url)
        onAuthorizeRequested: openProofConnections.authorizeAccount()
        onApprovalRequested: url => Qt.openUrlExternally(url)
        onRefreshRequested: openProofConnections.refreshConnections()
        onConnectRequested: provider => openProofConnections.connectProvider(provider)
        onManualLinkedInRequested: url => openProofConnections.setManualLinkedInUrl(url)
        onOpened: openProofConnections.checkCloudService()
        onDisconnectRequested: (provider, subject) =>
            openProofConnections.disconnectProvider(provider, subject)
        onClosed: {
            accountConnectionsPopup.editAccessRequested = false
            openProofConnections.stopAuthorization()
        }
    }

    onIsOwnProfileChanged: {
        root.profileLinksExpanded = false
        if (root.isOwnProfile && openProofConnections.cloudAuthorizationReady)
            profileRefreshRetry.restart()
        if (!root.isOwnProfile) {
            accountConnectionsPopup.close()
            profileExtrasEditor.close()
            profileLinksDrawer.close()
        }
    }
    PullToRefresh {
        flickable: profileList
        busy: root.busy
        refreshEnabled: Number(root.activeDetail.fid || 0) > 0
        onRefreshRequested: root.refreshProfileAndAccount()
    }

    ScrollToTopButton {
        flickable: profileList
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space5
        anchors.bottomMargin: root.compact ? Theme.space2 + 12 : Theme.space4
    }
    ContentRefreshIndicator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 50
        active: root.busy && root.rows.length > 0
    }

}
