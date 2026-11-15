import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Dialogs
import QtCore
import Farcaster.Client

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
    // Navigation must show the cover, even when this Loader is reused.
    property int navigationRevision: 0
    onNavigationRevisionChanged: {
        if (root.visible) root.scrollToBeginning()
    }
    property string profileFeed: "user-posts"
    property string displayLayout: "timeline"
    readonly property bool profileMagazineEnabled: !root.compact && displayLayout !== "timeline"
    readonly property var readList: root.profileMagazineEnabled ? profileMagazineView : profileList
    // ListView exposes originY, but the masonry Flickable may not. An
    // undefined origin must never turn pagination and anchor math into NaN.
    function readOriginY() {
        const origin = Number(root.readList.originY)
        return isFinite(origin) ? origin : 0
    }
    // Both modes use the same stable-keyed model, header and media renderer.
    onProfileMagazineEnabledChanged: profileLayoutSwitch.restart()
    Timer {
        id: profileLayoutSwitch
        interval: 0
        onTriggered: {
            root.viewportAnchorKey = ""
            root.pendingRestoreKey = ""
            root.readList.positionViewAtBeginning()
        }
    }
    property string profileSortMode: "newest"
    property string popularMetric: "all"
    property var topicCatalog: []
    property bool topicFilterActive: false
    property string topicFilterSlug: ""
    property bool topicFilterBusy: false
    property bool topicFilterHasMore: false
    function topicTitleFor(slug) {
        const titles={
            art:qsTrId("genycaster.topic.art"),
            development:qsTrId("genycaster.topic.development"),
            photography:qsTrId("genycaster.topic.photography"),
            design:qsTrId("genycaster.topic.design"),
            technology:qsTrId("genycaster.topic.technology"),
            music:qsTrId("genycaster.topic.music"),
            writing:qsTrId("genycaster.topic.writing"),
            science:qsTrId("genycaster.topic.science"),
            business:qsTrId("genycaster.topic.business"),
            gaming:qsTrId("genycaster.topic.gaming"),
            trading:qsTrId("genycaster.topic.trading")
        }
        return titles[slug] || slug.replace(/-/g," ")
                        .replace(/\b\w/g,character=>character.toUpperCase())
    }
    property bool runtimeReady: false
    property bool compact: false
    property real bottomOverlayInset: 0
    property bool busy: false
    property bool hasMore: false
    property bool offline: false
    property bool sourceUnavailable: false
    property string errorText: ""
    property bool interactionsAvailable: false
    property var mutedProfileFids: []
    property bool muteSettingsReady: false
    property bool muteWriteBusy: false
    // Account-owned data must not be dereferenced after its QML context
    // service has been destroyed. Empty snapshots are explicitly unknown,
    // never fabricated verified entitlements or activity.
    readonly property var proEntitlementsSnapshot: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    // A cached Pro response from another FID or a disconnected cloud session
    // must never appear as the current account's verified membership.
    readonly property var ownProEntitlement: root.isOwnProfile && openProofConnections
        && openProofConnections.cloudAuthorizationReady
        && Number(openProofConnections.activeAccountFid) === Number(root.activeDetail.fid || 0)
        ? root.proEntitlementsSnapshot : ({status: "unavailable"})
    readonly property var streakSnapshot: openProofConnections
        ? (openProofConnections.streak || ({})) : ({})
    readonly property var streakPreferencesSnapshot: openProofConnections
        ? (openProofConnections.streakPreferences || ({})) : ({})
    readonly property bool streakLoadingSnapshot: openProofConnections
        ? Boolean(openProofConnections.streakBusy) : false
    readonly property string streakErrorSnapshot: openProofConnections
        ? String(openProofConnections.streakError || "") : ""
    // Public activity belongs to the exact profile currently displayed.
    // Modal panels can survive the account service during application teardown.
    readonly property bool publicActivityMatchesProfile: Boolean(openProofConnections
        && Number(root.activeDetail.fid || 0) > 0
        && Number(openProofConnections.publicActivityFid)
            === Number(root.activeDetail.fid || 0))
    readonly property string publicActivityGithubLogin: root.publicActivityMatchesProfile
        ? String(openProofConnections.publicActivityGithubLogin || "") : ""
    readonly property string publicActivitySource: root.publicActivityMatchesProfile
        ? String(openProofConnections.publicActivitySource || "casts") : "casts"
    property double accountFid: 0
    property string accountIssuer: ""
    property bool accountIsNativeCustody: false
    // GENY holder status is *never* derived from a user-editable bio, display
    // name, URL or arbitrary Farcaster metadata. A future attestation service
    // may bind this property after verifying the actual wallet and holdings.
    property bool genyHolderAttested: false
    property bool profileLinksExpanded: false
    property string profileExtrasError: ""
    property string protocolProfileError: ""
    property bool protocolProfilePending: false
    property bool protocolProfileConfirmed: false
    property bool editProfileMediaUrls: false
    // Each explicit mood owns its own semantic icon and accessible theme color.
    // No live-presence inference: these are user-chosen profile decorations.
    readonly property var moodOptions: [
        {id:'available',icon:Theme.iconStatusAvailable,color:Theme.success},
        {id:'busy',icon:Theme.iconStatusBusy,color:Theme.warning},
        {id:'idle',icon:Theme.iconStatusIdle,color:Theme.inkMuted},
        {id:'away',icon:Theme.iconStatusAway,color:Theme.accentSecondary},
        {id:'focusing',icon:Theme.iconStatusFocused,color:Theme.swatchViolet},
        {id:'creating',icon:Theme.iconStatusCreating,color:Theme.accent},
        {id:'dnd',icon:Theme.iconStatusDoNotDisturb,color:Theme.danger},
        {id:'custom',icon:Theme.iconStatusCustom,color:Theme.accent},
        {id:'none',icon:Theme.iconStatusClear,color:Theme.inkMuted}
    ]
    function moodAppearance(mood) {
        const entry=root.moodOptions.find(option=>option.id===String(mood||'none'))
        return entry || root.moodOptions[root.moodOptions.length-1]
    }
    readonly property var displayedMood: {
        const value=openProofConnections ? openProofConnections.profileStatus || ({}) : ({})
        return String(value.fid||'')===String(Number(root.activeDetail.fid||0))
            ? value : ({})
    }
    function moodLabel(mood) {
        const names={available:'available',busy:'busy',idle:'idle',away:'away',
            focusing:'focusing',creating:'creating',dnd:'dnd',custom:'custom',none:'none'}
        const key=names[String(mood||'')]
        return key ? qsTrId('genycaster.status.'+key) : ''
    }
    onActiveDetailChanged: {
        profileStatusFetch.restart()
        streakFetch.restart()
        publicActivityFetch.restart()
    }
    Timer {
        id: publicActivityFetch
        interval: 230
        repeat: false
        onTriggered: {
            const fid=Number(root.activeDetail.fid || 0)
            if(fid>0 && !root.isOwnProfile && openProofConnections)
                openProofConnections.fetchPublicActivitySource(fid)
        }
    }
    Timer {
        id: streakFetch
        interval: 250
        repeat: false
        onTriggered: {
            if (!openProofConnections) return
            if (root.isOwnProfile && root.visible &&
                openProofConnections.cloudAuthorizationReady &&
                !openProofConnections.streakBusy)
                openProofConnections.fetchStreak()
            if (root.isOwnProfile && openProofConnections.cloudAuthorizationReady) {
                openProofConnections.fetchStreakPreferences()
                openProofConnections.fetchProEntitlements()
            }
        }
    }
    Timer {
        id: profileStatusFetch
        interval: 180
        repeat: false
        onTriggered: {
            // Never start background GETs while the user is editing or
            // saving: their response could race the revision used by PUT.
            if(profileStatusEditor.visible || profileStatusEditor.awaitingSave)
                return
            const fid=Number(root.activeDetail.fid||0)
            if(fid>0 && openProofConnections && !openProofConnections.profileStatusBusy)
                openProofConnections.fetchProfileStatus(fid)
        }
    }
    Timer {
        interval: 60000
        repeat: true
        running: root.visible && Number(root.activeDetail.fid||0)>0
        onTriggered: profileStatusFetch.restart()
    }
    Connections {
        target: openProofConnections ? openProofConnections : null
        function onStateChanged() {
            if(root.isOwnProfile && openProofConnections
                && openProofConnections.cloudAuthorizationReady) {
                profileStatusFetch.restart()
                streakFetch.restart()
            }
        }
    }
    property int pendingProfileMediaAfterIndex: -1

    function chooseProfileImage(afterIndex) {
        if (!root.isOwnProfile || !mediaUpload.available || mediaUpload.busy)
            return
        root.pendingProfileMediaAfterIndex = afterIndex
        if (Qt.platform.os === "wasm")
            mediaUpload.browseBrowserImage(afterIndex)
        else
            profilePicturePicker.open()
    }
    Connections {
        target: mediaUpload
        function onAssetReady(url, mediaType, managementToken, afterIndex) {
            if (!root.isOwnProfile || (afterIndex !== -1001 && afterIndex !== -1002)) return
            if (!/^https:\/\/media-farcaster\.genyleap\.com\/m\//.test(String(url))) {
                root.protocolProfileError = qsTrId("genycaster.profile.invalid_uploaded_image")
                return
            }
            if (afterIndex === -1001) avatarProtocolEditor.text = String(url)
            else bannerProtocolEditor.text = String(url)
            mediaUpload.clear()
        }
    }
    Connections {
        target: presentationBridge
        function onProtocolProfileWriteCompleted(confirmed, message) {
            if (!root.protocolProfilePending) return
            root.protocolProfilePending = false
            root.protocolProfileConfirmed = Boolean(confirmed)
            root.protocolProfileError = confirmed ? ""
                : (String(message || "") || qsTrId("genycaster.profile.protocol_write_failed"))
        }
    }
    FileDialog {
        id: profilePicturePicker
        title: qsTrId("genycaster.profile.select_image")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTrId("genycaster.media.images") + " (*.jpg *.jpeg *.png *.webp *.gif *.avif)"]
        onAccepted: {
            if (!mediaUpload.beginUpload(selectedFile, root.pendingProfileMediaAfterIndex))
                root.protocolProfileError = qsTrId("genycaster.profile.invalid_uploaded_image")
        }
    }
    // Force UI bindings to re-evaluate after an authoritative account read.
    // QSettings-backed string changes are otherwise not always propagated
    // to a previously constructed ProfileSurface on Qt/WASM.
    property int profileExtrasGeneration: 0

    Settings {
        id: profileExtrasSettings
        objectName: "profileExtrasSettings"
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
            // Force the profile's computed state to read the new pending/clean
            // flag *after* it is written, even where Qt/WASM Settings does not
            // notify an existing binding synchronously.
            root.profileExtrasGeneration++
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
            root.profileExtrasError = qsTrId("genycaster.profile.conflict_notice")
        } catch (_) { }
    }
    // QVariantList travels from native C++ to QML as a sequence. Qt's
    // sequence values are indexable, but Array.isArray() is not guaranteed
    // to recognize them. Accept both without trusting unbounded input.
    function profileSequence(value, limit) {
        if (!value || typeof value.length !== "number"
                || value.length < 0 || value.length > limit) return []
        const items = []
        for (let i = 0; i < value.length; ++i) items.push(value[i])
        return items
    }
    function cloudProfilePayload() {
        const saved = root.profileExtras
        const urls = root.profileSequence(saved.links, 5)
        const labels = root.profileSequence(saved.labels, 5)
        return {
            category: String(saved.category || ""),
            profession: String(saved.profession || ""),
            roles: root.profileSequence(saved.roles, 5).length
                    ? root.profileSequence(saved.roles, 5)
                    : (saved.profession ? [String(saved.profession)] : []),
            secondary_categories: root.profileSequence(saved.secondaryCategories, 3),
            links: urls.map((url, index) => ({
                label: String(labels[index] || "").slice(0, 32), url: url
            })).filter(entry => typeof entry.url === "string"
                        && root.validExtraUrl(entry.url)).slice(0, 5)
        }
    }
    function restoreCloudProfile(item) {
        if (!root.isOwnProfile || !root.profileExtrasOwnerKey
            || !item || typeof item.profession !== "string"
            || typeof item.category !== "string"
            || !item.links || typeof item.links.length !== "number"
            || item.links.length > 5) return
        const links = root.profileSequence(item.links, 5).filter(entry =>
            entry && typeof entry.url === "string"
            && root.validExtraUrl(entry.url))
        try {
            const owners = JSON.parse(profileExtrasSettings.entriesJson || "{}")
            owners[root.profileExtrasOwnerKey] = {
                category: item.category, profession: item.profession,
                roles: root.profileSequence(item.roles, 5),
                secondaryCategories: root.profileSequence(item.secondary_categories, 3),
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
                      && JSON.stringify(root.profileSequence(item.roles, 5))
                         === JSON.stringify(latest.roles)
                      && JSON.stringify(root.profileSequence(item.secondary_categories, 3))
                         === JSON.stringify(latest.secondary_categories)
                      && JSON.stringify(root.profileSequence(item.links, 5))
                         === JSON.stringify(latest.links)
            if (same) root.setCloudFlags(false, Number(item.revision || 0))
            else {
                root.setCloudFlags(true)
                if (openProofConnections && !openProofConnections.cloudBusy)
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
            if (openProofConnections && !openProofConnections.cloudBusy) {
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
                 && openProofConnections && openProofConnections.cloudServiceAvailable
                 && openProofConnections.cloudAuthorizationReady
        onTriggered: {
            if (openProofConnections && !openProofConnections.cloudBusy)
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
    Component.onCompleted: {
        root.migrateLegacyProfileExtras()
        profileStatusFetch.start()
    }
    readonly property var profileExtras: {
        const generation = root.profileExtrasGeneration
        if (!root.profileExtrasOwnerKey) return ({})
        // Live, account-verified API state is authoritative for a clean
        // profile. QSettings remains a durable offline outbox, never a
        // higher-priority display source than a known newer server revision.
        const server = openProofConnections ? openProofConnections.cloudProfileExtras : null
        if (!root.cloudLocalDirty
                && Number(openProofConnections ? openProofConnections.cloudRevision || 0 : 0) > 0
                && server && typeof server.category === "string"
                && typeof server.profession === "string"
                && server.links && typeof server.links.length === "number"
                && server.links.length <= 5) {
            const links = root.profileSequence(server.links, 5)
            return {
                category: server.category, profession: server.profession,
                roles: root.profileSequence(server.roles, 5),
                secondaryCategories: root.profileSequence(server.secondary_categories, 3),
                links: links.map(entry => String(entry.url || "")),
                labels: links.map(entry => String(entry.label || ""))
            }
        }
        try {
            const rows = JSON.parse(profileExtrasSettings.entriesJson || "{}")
            return rows[root.profileExtrasOwnerKey] || ({})
        } catch (_) { return ({}) }
    }
    // X-style professional identity is selected from curated categories and
    // roles. Legacy freeform roles must be explicitly replaced before saving,
    // because the account API validates the same canonical role vocabulary.
    // These are API identifiers, not translated display strings.
    readonly property var profileCategoryOptions: [
        "Select category", "Arts & entertainment", "Business", "Education",
        "Finance", "Food & beverage", "Health & wellness", "Media & news",
        "Science & technology", "Technology", "Professional services",
        "Sports", "Trading", "Travel & transportation", "Other"
    ]
    readonly property var profileRoleOptions: {
        const options = ["Select role", "Artist", "Author", "Creator",
            "Designer", "Developer", "Engineer", "Entrepreneur", "Founder",
            "Investor", "Journalist", "Photographer", "Researcher",
            "Student", "Teacher", "Consultant", "Organization", "Other",
            "Trader", "Analyst", "Product Manager", "Community Manager", "Content Creator"]
        // The account API accepts only curated roles. Never offer a legacy
        // freeform role as a selectable value and then silently fail HTTP 400.
        return options
    }
    function profileOptionLabel(value) {
        const canonical = String(value || "")
        if (!canonical) return ""
        if (root.profileCategoryOptions.indexOf(canonical) < 0
                && root.profileRoleOptions.indexOf(canonical) < 0)
            return canonical
        const key = canonical.toLowerCase().replace(/[^a-z0-9]+/g, "_")
                                      .replace(/^_+|_+$/g, "")
        // The visible copy is localized but canonical API values are untouched.
        return qsTrId("genycaster.profile.option." + key)
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
        const links = root.profileSequence(root.profileExtras.links, 5)
        return root.isOwnProfile
            ? links.filter(s => typeof s === "string" && root.validExtraUrl(s))
            : []
    }
    function profileExtraLabel(index) {
        const labels = root.profileSequence(root.profileExtras.labels, 5)
        return typeof labels[index] === "string" && labels[index].trim()
            ? labels[index].trim().slice(0, 32)
            : qsTrId("genycaster.profile.link_number").arg(index + 1)
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
        const value = String(text || "")
        // Match the account API's hostname, scheme, port and query policy
        // before closing the editor. Otherwise a valid-looking local edit
        // fails with HTTP 400 and the category silently reverts on refresh.
        const match = /^https:\/\/(?![^/]*@)((?:[A-Za-z0-9-]+\.)+[A-Za-z]{2,63})(?:[\/?#][^\s]*)?$/.exec(value)
        if (!match || value.length > 512 || match[1].length > 253) return false
        const query = /\?([^#]*)/.exec(value)
        const hash = /#(.*)$/.exec(value)
        return (!query || query[1].length < 256)
            && (!hash || hash[1].length < 128)
    }
    function saveExtraDetails(profession, rawLinks, category, secondaryCategories, roles) {
        if (!root.isOwnProfile || !root.profileExtrasOwnerKey) return false
        const selectedRoles = root.profileSequence(roles, 5)
        const role = String(selectedRoles[0] || profession || "").trim()
        if (selectedRoles.length > 5
            || new Set(selectedRoles).size !== selectedRoles.length
            || selectedRoles.some(value => root.profileRoleOptions.indexOf(value) <= 0)) {
            root.profileExtrasError = qsTrId("genycaster.profile.selection_error")
            return false
        }
        const categoryName = String(category || "").trim()
        if ((role && root.profileRoleOptions.indexOf(role) <= 0)
                || (categoryName && root.profileCategoryOptions.indexOf(categoryName) <= 0)) {
            root.profileExtrasError = qsTrId("genycaster.profile.selection_error")
            return false
        }
        const additional=root.profileSequence(secondaryCategories, 3)
        if (additional.length !== Number(secondaryCategories?.length || 0)
            || new Set(additional).size !== additional.length
            || additional.some(name => root.profileCategoryOptions.indexOf(name) <= 0
                                   || name === categoryName)) {
            root.profileExtrasError = qsTrId("genycaster.profile.secondary_error")
            return false
        }
        if (role.length > 80) {
            root.profileExtrasError = qsTrId("genycaster.profile.profession_limit")
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
            root.profileExtrasError = qsTrId("genycaster.profile.links_limit")
            return false
        }
        try {
            const wasDirty = root.cloudLocalDirty
            const knownRevision = Number(openProofConnections ? openProofConnections.cloudRevision || 0 : 0)
            const entries = JSON.parse(profileExtrasSettings.entriesJson || "{}")
            const unique = prepared.filter((item,index)=>prepared.findIndex(e=>e.url===item.url)===index)
            entries[root.profileExtrasOwnerKey] = {
                profession: role, category: categoryName,
                roles: selectedRoles,
                secondaryCategories: additional,
                links: unique.map(e=>e.url), labels: unique.map(e=>e.label)
            }
            profileExtrasSettings.entriesJson = JSON.stringify(entries)
            root.profileExtrasError = ""
            root.profileLinksExpanded = false
            // Mark the new record pending before invalidating the display
            // binding. Otherwise an already-fetched server value can win the
            // first binding evaluation and leave the old category on screen.
            root.setCloudFlags(true, knownRevision)
            root.profileExtrasGeneration++
            if (openProofConnections.cloudServiceAvailable
                    && openProofConnections.cloudAuthorizationReady) {
                if (knownRevision > 0 && !wasDirty && !openProofConnections.cloudBusy)
                    openProofConnections.pushCloudProfileExtras(root.cloudProfilePayload())
                else
                    profileRefreshRetry.restart()
            }
            return true
        } catch (_) {
            root.profileExtrasError = qsTrId("genycaster.profile.local_save_failed")
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
    // A new profile can change its header geometry after the ListView first
    // becomes visible. Keep the entry at the true header top until the first
    // layout settles, without disturbing deliberate reading/scrolling.
    property bool profileEntryResetActive: false
    function finishProfileEntryReset() {
        profileEntryResetActive = false
        resetPositionPasses = 0
        resetPositionTimer.stop()
        profileEntrySettleTimer.stop()
    }
    function positionReaderAtHeaderTop() {
        root.readList.cancelFlick()
        root.readList.positionViewAtBeginning()
        if (!root.profileMagazineEnabled && profileList.headerItem) {
            // ListView InlineHeader may start BEFORE originY. Positioning the
            // first delegate, rather than the header, clips the cover.
            const headerY = Number(profileList.headerItem.y)
            if (isFinite(headerY))
                profileList.contentY = Math.min(root.readOriginY(), headerY)
        }
    }
    // Keep QVariantMap payloads out of ListModel roles; the model itself stays
    // type-stable and delegates re-read only when their revision role changes.
    property var stablePayloads: ({})

    signal backRequested()
    signal followersRequested()
    signal followingRequested()
    signal refreshRequested()
    signal profileFeedRequested(string feed)
    signal layoutRequested(string layout)
    signal topicRequested(string slug, double authorFid)
    signal castRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal followingMutationRequested(double targetFid, bool following)
    signal blockingMutationRequested(double targetFid, bool blocked)
    signal muteRequested(double targetFid, bool muted)
    signal removeCastRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()
    signal loadMoreRequested()
    signal proRequested()

    readonly property bool profileMuted: root.mutedProfileFids.indexOf(
        String(Number(root.activeDetail.fid || 0))) >= 0
    readonly property bool isOwnProfile: Number(root.activeDetail.fid || 0) > 0
                                         && Number(root.activeDetail.fid || 0) === Number(root.accountFid)
    readonly property int profileAvatarSize: root.compact ? 84 : 96
    // Match the Stats/ContextRail outside gutter without shrinking card
    // padding. The profile is responsive across the full desktop workspace.
    readonly property int profileMargin: root.compact ? Theme.mobileGutter : Theme.space3
    readonly property int profileContentLimit: root.compact ? 760 : 1280
    readonly property int profileContentWidth: Math.max(0, Math.min(root.profileContentLimit, width - root.profileMargin * 2))
    readonly property int profileContentX: Math.max(root.profileMargin, Math.round((width - root.profileContentWidth) / 2))
    readonly property bool profileContentRtl: Theme.rtl
    readonly property int profileAvatarX: root.profileContentRtl
                                         ? root.profileContentX + root.profileContentWidth
                                           - root.profileAvatarSize - (root.compact ? Theme.space2 : Theme.space3)
                                         : root.profileContentX + (root.compact ? Theme.space2 : Theme.space3)
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
    // Only an OpenProof FID-bound snapshot can establish a linked account.
    // User-editable profile extras, bios and URLs never assert GitHub identity.
    readonly property var verifiedGithubActivityAccount: {
        const result = {linked:false, login:"", id:""}
        if (!root.isOwnProfile || typeof openProofConnections==="undefined"
                || !openProofConnections) return result
        const entries = openProofConnections.connections || []
        for (let i=0; i<entries.length && i<50; ++i) {
            const entry=entries[i] || ({})
            if (String(entry.provider || "").toLowerCase()!=="github") continue
            result.linked=true
            const subject=String(entry.subject || "")
            const githubId=/^(?:github:)?([1-9][0-9]{0,14})$/.exec(subject)
            if(githubId)result.id=githubId[1]
            const names=[entry.username,entry.preferred_username,entry.login,
                         entry.handle]
            const profile=String(entry.profileUrl || entry.provider_profile_url || "")
            const parsed=/^https:\/\/github\.com\/([A-Za-z0-9-]{1,39})\/?$/.exec(profile)
            if(parsed) names.push(parsed[1])
            for(const name of names) {
                const login=String(name || "").replace(/^@/,"").trim()
                if (/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$/.test(login)) {
                    result.login=login
                    break
                }
            }
            return result
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
    readonly property bool showUsername: String(root.activeDetail.username || "").trim().length > 0
                                         && !root.syntheticUsername(root.activeDetail.username)
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
        // Magazine uses CastItem's compact in-card author layout, not the
        // 42px desktop side-avatar. Estimating a 320px card as 480px caused
        // incorrect line/image reservations until the Loader completed.
        const compactCard = root.compact || root.profileMagazineEnabled
        const rowWidth = Math.max(compactCard ? 260 : 480, Number(availableWidth || 0))
        const horizontalInset = compactCard ? Theme.mobileGutter * 2
                                           : Theme.space3 * 2 + 42 + Theme.space3
        const contentWidth = Math.max(180, rowWidth - horizontalInset)
        const lineHeight = Math.ceil(Theme.typeBodySize * Theme.typeBodyLineHeight)
        const charactersPerLine = Math.max(18,
            Math.floor(contentWidth / Math.max(7, Theme.typeBodySize * 0.55)))
        // Long text-only Magazine cards can wrap far beyond eight lines.
        // Use a wider provisional line budget so later hydration does not
        // position an adjacent card inside unresolved text.
        const textLength = Math.min(root.profileMagazineEnabled ? 1200 : 600,
                                    String(value.text || "").length)
        const textLines = textLength > 0
            ? Math.max(1, Math.min(root.profileMagazineEnabled ? 28 : 8,
                                   Math.ceil(textLength / charactersPerLine))) : 0
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
                reservation += Math.max(compactCard ? 120 : 220,
                                        Math.min(compactCard ? Math.max(720, contentWidth * 1.6) : 520,
                                                 contentWidth / aspect))
            } else if (visualCount > 1) {
                const columns = contentWidth < (compactCard ? 360 : 430) ? 1 : 2
                const tileWidth = columns === 1 ? contentWidth : (contentWidth - Theme.space2) / 2
                const rows = Math.ceil(Math.min(6, visualCount) / columns)
                reservation += rows * Math.max(120, Math.min(320, tileWidth / (4 / 3)))
                             + Math.max(0, rows - 1) * Theme.space2
            } else {
                reservation += compactCard ? 132 : 152
            }
            if (visualCount > 0 && supportingCount > 0)
                reservation += Math.min(2, supportingCount) * (compactCard ? 132 : 152)
        } else if (/https?:\/\//i.test(String(value.text || ""))) {
            // Match CastItem's inferred-link behavior even when protocol embeds
            // are absent so Profile rows reserve their preview geometry early.
            reservation += compactCard ? 132 : 152
        }
        return Math.max(88, Math.ceil(reservation))
    }

    // The profile reuses the same native stable-ID row model as Explorer.
    // Feeding ListView a freshly allocated array on every row change formerly
    // reset contentY to the top; targeted inserts/moves now preserve delegates
    // while the heavyweight content remains in our keyed payload store.
    function captureViewportAnchor() {
        if (root.profileEntryResetActive || root.restoringViewport
                || profileRowsModel.count === 0) return
        const headerHeight = root.readList.headerItem ? root.readList.headerItem.implicitHeight : 0
        // indexAt() takes *content* coordinates, not viewport coordinates.
        // Probing near y=0 while scrolled past the header never finds a row.
        const probeY = Math.max(headerHeight + 1, root.readList.contentY + 1)
        const index = root.readList.indexAt(Math.max(1, root.readList.width / 2), probeY)
        if (index < 0 || index >= profileRowsModel.count) {
            // An empty incubation slot is not evidence that the old anchor
            // is still visible. Retaining it could teleport the view later.
            root.viewportAnchorKey = ""
            return
        }
        const item = root.readList.itemAtIndex(index)
        if (!item) {
            root.viewportAnchorKey = ""
            return
        }
        root.viewportAnchorKey = String(profileRowsModel.get(index).stableKey || "")
        root.viewportAnchorOffset = item.y - root.readList.contentY
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
        if (root.profileEntryResetActive || root.readList.moving
                || root.viewportAnchorKey.length === 0 || root.restoringViewport) return
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
        const item = root.readList.itemAtIndex(targetIndex)
        if (!item) {
            // The old row is no longer instantiated. Forcing it back into view
            // during a late image/layout update is an unwanted scroll jump.
            // Let ListView keep the current visible range instead.
            return
        }
        // A cached-but-offscreen delegate is as stale as a pooled one. Never
        // drag the reader multiple viewports to satisfy an obsolete anchor.
        const currentOffset = item.y - root.readList.contentY
        if (Math.abs(currentOffset - anchorOffset) > Math.max(320, root.readList.height))
            return
        const minimum = root.readOriginY()
        const maximum = Math.max(minimum, root.readList.contentHeight - root.readList.height)
        root.readList.contentY = Math.max(minimum, Math.min(maximum, item.y - anchorOffset))
    }

    function payloadForStableKey(key) {
        return root.stablePayloads[String(key || "")] || ({})
    }

    function requestStableModelSync() {
        if (root.readList.moving) {
            root.stableModelSyncPending = true
            return
        }
        root.stableModelSyncPending = false
        root.synchronizeStableModel()
    }

    function synchronizeStableModel() {
        const source = root.displayRows
        if (root.busy && source.length === 0 && profileRowsModel.count > 0)
            return

        const headerHeight = root.readList.headerItem
                ? root.readList.headerItem.implicitHeight : 0
        const probeY = Math.max(headerHeight + 1, root.readList.contentY + 1)
        const anchorIndex = root.readList.indexAt(
            Math.max(1, root.readList.width / 2), probeY)
        let anchorKey = ""
        let anchorOffset = 0
        if (!root.profileEntryResetActive
                && anchorIndex >= 0 && anchorIndex < profileRowsModel.count) {
            const anchorItem = root.readList.itemAtIndex(anchorIndex)
            anchorKey = String(profileRowsModel.get(anchorIndex).stableKey)
            anchorOffset = anchorItem ? anchorItem.y - root.readList.contentY : 0
        }

        const next = []
        const retainedKeys = Object.create(null)
        let structureChanged = profileRowsModel.count !== source.length
        for (let index = 0; index < source.length; ++index) {
            const key = root.rowKey(source[index], index)
            const revision = root.rowRevision(source[index])
            retainedKeys[key] = true
            root.stablePayloads[key] = source[index]
            next.push({stableKey: key, revisionKey: revision,
                       presentationKind: "conversation", mediaLayoutKind: "other"})
            if (!structureChanged && String(profileRowsModel.get(index).stableKey) !== key)
                structureChanged = true
        }
        // Reconcile once; the native adapter keeps stable delegates intact
        // for unchanged rows and batches large identity replacements.
        profileRowsModel.reconcile(next)
        for (const key in root.stablePayloads) {
            if (!retainedKeys[key])
                delete root.stablePayloads[key]
        }
        if (structureChanged && anchorKey.length > 0)
            root.scheduleViewportRestore(anchorKey, anchorOffset, false)
    }

    function isNearEnd() {
        if (root.readList.count === 0 || root.readList.contentHeight <= 0)
            return false
        const normalizedY = Math.max(0, root.readList.contentY - root.readOriginY())
        const preloadDistance = Math.max(520, root.readList.height * 0.7)
        return root.readList.atYEnd
                || normalizedY + root.readList.height >= root.readList.contentHeight - preloadDistance
    }

    function scheduleLoadMore() {
        // Never append while kinetic scrolling is active. Variable-height cast
        // delegates can otherwise move under the pointer when the model grows.
        if (root.readList.moving || root.loadMorePending || root.busy || !root.hasMore
                || !root.isNearEnd())
            return
        root.loadMorePending = true
        scheduledLoadMoreTimer.restart()
    }

    function requestScheduledLoadMore() {
        root.loadMorePending = false
        if (!root.busy && root.hasMore && root.readList.count > 0)
            root.loadMoreRequested()
    }

    // Re-tapping Profile is intentional navigation, unlike row refreshes.
    function scrollToBeginning() {
        viewportRestoreTimer.stop()
        root.pendingRestoreKey = ""
        root.viewportAnchorKey = ""
        root.restoringViewport = false
        root.profileEntryResetActive = true
        profileEntrySettleTimer.restart()
        root.positionReaderAtHeaderTop()
        root.resetPositionPasses = 2
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
        root.profileEntryResetActive = true
        profileEntrySettleTimer.restart()
        // A real identity navigation resets immediately. Reflow of a loaded
        // header does not cancel the follow-up pass.
        root.positionReaderAtHeaderTop()
        resetPositionTimer.restart()
    }

    onVisibleChanged: {
        if (!root.visible) {
            accountConnectionsPopup.close()
            return
        }
        root.scrollToBeginning()
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
        root.profileEntryResetActive = true
        profileEntrySettleTimer.restart()
        // An explicit tab switch returns to the full header, not the first
        // profile post. The magazine and timeline layouts share this contract.
        root.positionReaderAtHeaderTop()
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
        id: profileEntrySettleTimer
        interval: 480
        repeat: false
        onTriggered: root.finishProfileEntryReset()
    }
    Timer {
        id: resetPositionTimer
        interval: 0
        repeat: false
        onTriggered: {
            if (root.resetPositionPasses <= 0)
                return
            // Only a real gesture cancels entry positioning. A delayed
            // cover/header layout shift is not evidence of a user scroll.
            if (root.readList.moving) {
                root.finishProfileEntryReset()
                return
            }
            root.positionReaderAtHeaderTop()
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
        // Preserve trailing scroll clearance beneath the floating nav.
        bottomMargin: root.compact ? 96 : 0
        visible: !root.profileMagazineEnabled
        model: root.profileMagazineEnabled ? null : profileRowsModel
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
            root.finishProfileEntryReset()
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
            if (!root.profileEntryResetActive && !profileList.moving
                    && !root.restoringViewport && !scrollWorkThrottle.running)
                scrollWorkThrottle.start()
        }
        onContentHeightChanged: {
            if (root.profileEntryResetActive) {
                resetPositionTimer.restart()
                return
            }
            if (!profileList.moving) {
                root.stabilizeViewportAfterGeometryChange()
                if (!scrollWorkThrottle.running)
                    scrollWorkThrottle.start()
            }
        }
        Connections {
            target: profileList.headerItem
            ignoreUnknownSignals: true
            function onHeightChanged() {
                if (root.profileEntryResetActive)
                    resetPositionTimer.restart()
            }
        }

        // Retain the cover/bio header across Magazine/Timeline switches.
        // Recreating its image/text tree on each toggle caused a visible flash.
        header: profileHeaderComponent
        Component {
          id: profileHeaderComponent
          ColumnLayout {
            width: root.readList.width
            // No phantom top gutter after the hidden own-profile action row.
            spacing: root.compact && root.isOwnProfile ? 0 : Theme.space2

            RowLayout {
                visible: !(root.compact && root.isOwnProfile)
                Layout.fillWidth: true
                Layout.leftMargin: root.profileContentX
                Layout.rightMargin: root.profileContentX
                spacing: Theme.space2
                layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight

                ActionButton {
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconBack
                    text: qsTrId("genycaster.setup.back")
                    onClicked: root.backRequested()
                }

                Item { Layout.fillWidth: true }

                ActionButton {
                    id: profileShareButton
                    objectName: "profileOptionsButton"
                    // The same profile actions must be discoverable on phones.
                    visible: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconMore
                    iconOnly: true
                    text: qsTrId("genycaster.profile.options")
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
                                onClosed: {
                                    menuCopyCloseTimer.stop()
                                    copyProfileLinkAction.clearCopyFeedback()
                                }
                                Timer {
                                    id: menuCopyCloseTimer
                                    interval: 1100
                                    repeat: false
                                    onTriggered: sharePopup.close()
                                }
                                contentItem: ColumnLayout {
                                    id: shareActions
                                    spacing: 0
                                    ActionButton {
                                        visible: root.isOwnProfile
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: Theme.iconCompose
                                        text: qsTrId("genycaster.profile.edit_profile")
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
                                        text: qsTrId("genycaster.profile.share_profile")
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
                                        id: copyProfileLinkAction
                                        iconGlyph: Theme.iconCopy
                                        text: qsTrId("genycaster.share.copy_profile_link")
                                        onClicked: {
                                            const copied = presentationBridge.copyText(presentationBridge.profileShareUrl(
                                                Number(root.activeDetail.fid || 0)))
                                            copyProfileLinkAction.acknowledgeCopy(copied)
                                            menuCopyCloseTimer.restart()
                                        }
                                    }
                                    ActionButton {
                                        visible: !root.isOwnProfile
                                                 && Number(root.activeDetail.fid || 0) > 0
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: root.profileMuted ? Theme.iconVolume
                                                                   : Theme.iconVolumeMuted
                                        text: root.profileMuted ? qsTrId("genycaster.profile.unmute")
                                                                : qsTrId("genycaster.profile.mute")
                                        enabled: root.muteSettingsReady && !root.muteWriteBusy
                                        busy: root.muteWriteBusy
                                        customForegroundColor: root.profileMuted ? "transparent" : Theme.warning
                                        onClicked: {
                                            if (!root.muteSettingsReady) return
                                            sharePopup.close()
                                            root.muteRequested(Number(root.activeDetail.fid),
                                                               !root.profileMuted)
                                        }
                                    }
                                    ActionButton {
                                        visible: !root.isOwnProfile
                                                 && Number(root.activeDetail.fid || 0) > 0
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: Theme.iconWarning
                                        destructive: !Boolean(root.activeDetail.blockedByMe)
                                        text: Boolean(root.activeDetail.blockedByMe)
                                              ? qsTrId("genycaster.profile.unblock_user")
                                              : qsTrId("genycaster.profile.block_user")
                                        enabled: !presentationBridge.writeBusy
                                                 && !presentationBridge.signerBusy
                                        onClicked: {
                                            sharePopup.close()
                                            if (!root.interactionsAvailable) {
                                                root.authenticationRequested()
                                                return
                                            }
                                            root.blockingMutationRequested(
                                                Number(root.activeDetail.fid),
                                                !Boolean(root.activeDetail.blockedByMe))
                                        }
                                    }
                                    ActionButton {
                                        visible: presentationBridge.farcasterWebProfileUrl(
                                            String(root.activeDetail.username || "")).length > 0
                                        Layout.fillWidth: true
                                        quiet: true
                                        alignStart: true
                                        iconGlyph: Theme.iconArrowUpRight
                                        text: qsTrId("genycaster.share.open_farcaster")
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
                        // A fixed decode request avoids changing the provider
                        // cache key on every pixel of a live desktop resize.
                        // Only the already-decoded texture is scaled.
                        requestedSourceWidth: 1200
                        requestedSourceHeight: 480
                        fillMode: Image.PreserveAspectCrop
                        cache: true
                        asynchronous: true
                        framePacedPresentation: false
                        deferNewLoadsWhileScrolling: false
                        // Profile covers use the direct scene-graph image path.
                        // The large/wide MultiEffect mask can resolve the image
                        // successfully yet produce a blank layer on some render
                        // backends; the surrounding cover frame still owns the
                        // visual boundary and border.
                        clipToSuperellipse: false
                        curveDepth: Theme.shapeMedia
                        fillColor: Theme.surfaceRaised
                        borderWidth: 0
                        showLoadingIndicator: false
                        // A failed cover offers an explicit retry, not a generic
                        // broken-image glyph in the middle of the profile.
                        showFailureIndicator: false
                        // Never expose the previous user's cover during a
                        // new profile navigation or metadata refresh.
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

                    ActionButton {
                        id: retryProfileCover
                        z: 3
                        anchors.centerIn: parent
                        visible: bannerImage.loadFailed && !bannerImage.policyBlocked
                                 && Boolean(root.activeDetail.bannerUrl)
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconRefresh
                        text: qsTrId("genycaster.profile.refresh")
                        onClicked: {
                            // Manual retry intentionally bypasses the media
                            // provider's short negative-cache backoff.
                            bannerImage.manualLoadRequested = false
                            bannerImage.manualLoadRequested = true
                        }
                    }

                    TapHandler {
                        enabled: bannerImage.ready
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
                        x: root.profileContentRtl ? root.profileContentX
                                                  : root.profileContentX + root.profileContentWidth - width
                        anchors.verticalCenter: parent.verticalCenter
                        // Keep Manage Connections accessible even before the
                        // owner has linked any social accounts. Verified badges
                        // remain absent until OpenProof supplies proof.
                        visible: root.isOwnProfile
                        spacing: Theme.space1
                        // Place linked badges opposite the avatar in either
                        // direction; on compact RTL screens the avatar is right.
                        layoutDirection: root.profileContentRtl ? Qt.RightToLeft : Qt.LeftToRight

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
                                        ? (modelData.manualUrl ? qsTrId("genycaster.profile.network_manual_url")
                                          : liveVerified ? qsTrId("genycaster.profile.network_linked_profile") : qsTrId("genycaster.profile.network_previously_linked"))
                                        : canFind ? qsTrId("genycaster.profile.network_search_linkedin")
                                                  : qsTrId("genycaster.profile.network_manage_connection"))
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
                                Accessible.description: hasPublicUrl ? qsTrId("genycaster.profile.network_open_verified")
                                    : canFind ? qsTrId("genycaster.profile.network_find_linkedin") : qsTrId("genycaster.profile.network_manage")
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
                            text: qsTrId("genycaster.profile.manage")
                            Accessible.description: qsTrId("genycaster.profile.manage_connections")
                            onClicked: root.openConnections()
                        }
                    }

                    
                }

                ColumnLayout {
                    width: root.profileContentWidth
                    x: root.profileContentX
                    spacing: Theme.space1

                    GridLayout {
                        objectName: "profileIdentityRow"
                        Layout.fillWidth: true
                        columns: root.profileContentWidth >=
                            profileDisplayName.implicitWidth + identityLinkSlot.fullLinkWidth
                                + Theme.space3 ? 2 : 1
                        columnSpacing: Theme.space3
                        rowSpacing: Theme.space1
                        layoutDirection: Theme.uiDirection

                        Flow {
                            id: profileIdentityFlow
                            objectName: "profileIdentityLeading"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: implicitHeight
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                        DirectionAwareLabel {
                            id: profileDisplayName
                            objectName: "profileDisplayName"
                            width: Math.min(implicitWidth, profileIdentityFlow.width)
                            height: implicitHeight
                            // An FID is unique only within its verified identity namespace.
                            // A large numeric FID is not proof of HyperSnap ownership.
                            // Public profile reads currently expose no verified issuer field;
                            // only the active account's authenticated identity can be labeled.
                            text: root.activeDetail.displayName
                                  || (root.showUsername ? root.activeDetail.username : "")
                                  || (root.isOwnProfile && root.accountIsNativeCustody
                                      ? qsTrId("genycaster.settings.hypersnap_label")
                                      : qsTrId("genycaster.side_nav.profile"))
                            color: Theme.ink
                            font.pixelSize: root.compact ? Theme.typeTitle2Size : Theme.typeTitle1Size
                            font.weight: Theme.weightBold
                            horizontalAlignment: Theme.textStartAlignment
                            wrapMode: Text.NoWrap
                            elide: Text.ElideRight
                        }

                        Item {
                            id: followsYouBadge
                            visible: Boolean(root.activeDetail.followsMeKnown) && Boolean(root.activeDetail.followsMe)
                                     && !root.isOwnProfile
                            implicitWidth: followsYouText.implicitWidth + Theme.space3
                            implicitHeight: Math.max(22, followsYouText.implicitHeight + Theme.space1)
                            width: implicitWidth
                            height: implicitHeight

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

                        // Reserved for a future server-signed, wallet-bound GENY
                        // holding attestation. Never display from editable metadata.
                        Rectangle {
                            id: genyHolderBadge
                            objectName: "verifiedGenyHolderBadge"
                            visible: root.genyHolderAttested
                            implicitWidth: holderBadgeContent.implicitWidth + Theme.space3
                            implicitHeight: Math.max(25, holderBadgeContent.implicitHeight + Theme.space1)
                            width: implicitWidth
                            height: implicitHeight
                            radius: implicitHeight / 2
                            color: Theme.verifiedGenyBadgeBackground
                            border.color: Theme.verifiedGenyBadgeBorder
                            border.width: 1
                            Row {
                                id: holderBadgeContent
                                anchors.centerIn: parent
                                spacing: Theme.space1
                                Icon {
                                    text: Theme.iconCheck
                                    color: Theme.verifiedGenyBadgeInk
                                    preferredSize: Theme.typeCaptionSize
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Label {
                                    text: qsTrId("genycaster.brand.geny_ticker")
                                    color: Theme.verifiedGenyBadgeInk
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightBold
                                }
                            }
                            ToolTip.visible: genyHolderBadgeHover.hovered
                            ToolTip.text: qsTrId("genycaster.profile.verified_geny_holder")
                            HoverHandler { id: genyHolderBadgeHover }
                        }


                        ActionButton {
                            width: implicitWidth
                            height: implicitHeight
                            // Unknown is not a confirmed negative relationship.
                            // Keep the button discoverable without blind protocol writes.
                            visible: Number(root.activeDetail.fid || 0) > 0 && !root.isOwnProfile
                                     && !Boolean(root.activeDetail.blockedByMe)
                            // Do not strand Follow in a disabled state when the
                            // relationship endpoint cannot yet provide evidence.
                            // The protocol writer still performs signer checks.
                            enabled: !root.busy && !Boolean(root.activeDetail.blocksMe)
                            busy: !Boolean(root.activeDetail.followingKnown) && root.busy
                            compact: true
                            implicitHeight: Theme.scaleUi(30)
                            leftPadding: Theme.space2
                            rightPadding: Theme.space2
                            iconSize: Theme.scaleUi(14)
                            iconGlyph: Boolean(root.activeDetail.followingByMe) ? Theme.iconCheck : ""
                            text: Boolean(root.activeDetail.followingByMe) ? qsTrId("genycaster.profile.followed") : qsTrId("genycaster.profile.follow")
                            selected: Boolean(root.activeDetail.followingByMe)
                            prominent: !Boolean(root.activeDetail.followingByMe)
                            Accessible.description: !Boolean(root.activeDetail.followingKnown)
                                                    ? qsTrId("genycaster.search.relationship_unknown")
                                                    : Boolean(root.activeDetail.followingByMe)
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

                        } // Left-aligned name, badges, and follow actions

                        Item {
                            id: identityLinkSlot
                            objectName: "profileIdentityLinksSlot"
                            visible: root.primaryProfileLinkUrl.length > 0
                            // Keep the URL and the "and more" action together.
                            // Reserve the rendered text's intrinsic width plus a small
                            // bearing allowance. A zero minimum width previously let
                            // Grid/RowLayout silently clip even short desktop links.
                            TextMetrics {
                                id: profileLinkMetrics
                                font.family: Theme.latinFontFamily
                                font.pixelSize: Theme.typeCaptionSize
                                text: root.displayUrl(root.primaryProfileLinkUrl)
                            }
                            readonly property real primaryButtonWidth:
                                Math.ceil(identityLinkIcon.implicitWidth + identityLinkContent.spacing
                                    + Math.max(profileLinkMetrics.advanceWidth, identityLinkText.implicitWidth)
                                    + Theme.scaleUi(8))
                            readonly property real fullLinkWidth:
                                primaryButtonWidth
                                + (identityMoreLinks.visible
                                   ? identityMoreLinks.implicitWidth + identityLinks.spacing : 0)
                            Layout.preferredWidth: fullLinkWidth
                            Layout.minimumWidth: Math.min(fullLinkWidth, root.profileContentWidth)
                            Layout.maximumWidth: fullLinkWidth
                            Layout.alignment: Qt.AlignVCenter | (Theme.rtl ? Qt.AlignLeft : Qt.AlignRight)
                            Layout.preferredHeight: Math.max(profileDisplayName.implicitHeight, identityLinks.implicitHeight)

                            RowLayout {
                                id: identityLinks
                                objectName: "profileIdentityLinks"
                                width: parent.width
                                height: implicitHeight
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: Theme.space1
                                layoutDirection: Theme.uiDirection

                                Button {
                                    id: identityPrimaryLink
                                    objectName: "profileIdentityPrimaryLink"
                                    Layout.fillWidth: false
                                    Layout.preferredWidth: identityLinkSlot.primaryButtonWidth
                                    Layout.minimumWidth: Layout.preferredWidth
                                    Layout.maximumWidth: Layout.preferredWidth
                                    flat: true
                                    hoverEnabled: true
                                    padding: 0
                                    background: Item {}
                                    contentItem: RowLayout {
                                        id: identityLinkContent
                                        spacing: Theme.space1
                                        layoutDirection: Theme.uiDirection
                                        Icon {
                                            id: identityLinkIcon
                                            text: Theme.iconLink
                                            color: Theme.accent
                                            preferredSize: Theme.typeCaptionSize
                                        }
                                        Label {
                                            id: identityLinkText
                                            Layout.fillWidth: false
                                            Layout.preferredWidth: Math.ceil(
                                                Math.max(profileLinkMetrics.advanceWidth, implicitWidth)
                                                + Theme.scaleUi(4))
                                            Layout.minimumWidth: Layout.preferredWidth
                                            text: root.displayUrl(root.primaryProfileLinkUrl)
                                            horizontalAlignment: Theme.textStartAlignment
                                            LayoutMirroring.enabled: false
                                            color: Theme.accent
                                            font.family: Theme.latinFontFamily
                                            font.pixelSize: Theme.typeCaptionSize
                                            // URL is one logical line, not clipped or elided.
                                            wrapMode: Text.NoWrap
                                            elide: Text.ElideNone
                                            clip: false
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    ToolTip.visible: hovered
                                        && identityLinkText.width + 1 < profileLinkMetrics.advanceWidth
                                    ToolTip.text: root.displayUrl(root.primaryProfileLinkUrl)
                                    Accessible.role: Accessible.Link
                                    Accessible.name: root.primaryProfileLinkUrl
                                    onClicked: root.externalRequested(root.primaryProfileLinkUrl)
                                }
                                Button {
                                    id: identityMoreLinks
                                    objectName: "profileMoreLinksInline"
                                    visible: root.isOwnProfile && root.additionalProfileLinksCount > 0
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.minimumWidth: implicitWidth
                                    Layout.fillWidth: false
                                    flat: true
                                    hoverEnabled: true
                                    padding: 0
                                    background: Rectangle {
                                        radius: Theme.curveCompact
                                        color: identityMoreLinks.hovered ? Theme.surfaceHover : "transparent"
                                        Behavior on color {
                                            ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
                                        }
                                    }
                                    contentItem: Label {
                                        text: qsTrId("genycaster.profile.more_links").arg(
                                            LocaleFormat.formatInteger(root.additionalProfileLinksCount))
                                        color: Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                        font.weight: Theme.weightDemiBold
                                    }
                                    Accessible.name: qsTrId("genycaster.profile.show_all_links")
                                    onClicked: profileLinksDrawer.open()
                                }
                            }
                        }

                    } // Full-width identity row

                    RowLayout {
                        objectName: "profileUsernameAndLinksRow"
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection

                        Label {
                            id: profileUsernameLabel
                            visible: root.showUsername
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: Math.min(implicitWidth,
                                root.profileContentWidth * (root.compact ? 0.39 : 0.44))
                            Layout.maximumWidth: Layout.preferredWidth
                            LayoutMirroring.enabled: false
                            text: root.ltrHandle(root.activeDetail.username)
                            font.family: Theme.latinFontFamily
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeMetaSize
                            horizontalAlignment: Theme.textStartAlignment
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        objectName: "profileProfessionalCategory"
                        Layout.fillWidth: true
                        visible: root.profileProfession.length > 0 || root.profileCategory.length > 0
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2
                        Layout.topMargin: Theme.space1
                        Icon {
                            text: Theme.iconBusiness
                            color: Theme.inkMuted
                            preferredSize: 16
                            Layout.alignment: Qt.AlignVCenter
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.profileCategory.length > 0 && root.profileProfession.length > 0
                                  ? root.profileOptionLabel(root.profileCategory) + " · "
                                    + root.profileOptionLabel(root.profileProfession)
                                  : root.profileOptionLabel(root.profileCategory || root.profileProfession)
                            alignToUi: true
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightMedium
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        objectName: "profileMoodSummary"
                        Layout.fillWidth: true
                        visible: root.isOwnProfile || String(root.displayedMood.mood||'none')!=='none'
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        activeFocusOnTab: root.isOwnProfile
                        Accessible.role: root.isOwnProfile ? Accessible.Button : Accessible.StaticText
                        Accessible.name: statusSummaryLabel.text
                        Accessible.onPressAction: if (root.isOwnProfile) profileStatusEditor.open()
                        Keys.onReturnPressed: if (root.isOwnProfile) profileStatusEditor.open()
                        Keys.onSpacePressed: if (root.isOwnProfile) profileStatusEditor.open()
                        TapHandler {
                            enabled: root.isOwnProfile
                            onTapped: profileStatusEditor.open()
                        }
                        Icon {
                            text: root.moodAppearance(root.displayedMood.mood).icon
                            preferredSize: 16
                            color: root.displayedMood.mood && root.displayedMood.mood!=='none'
                                   ? root.moodAppearance(root.displayedMood.mood).color
                                   : Theme.inkMuted
                        }
                        DirectionAwareLabel {
                            id: statusSummaryLabel
                            Layout.fillWidth: true
                            color: root.displayedMood.mood && root.displayedMood.mood!=='none'
                                   ? root.moodAppearance(root.displayedMood.mood).color
                                   : Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            text: root.displayedMood.mood && root.displayedMood.mood!=='none'
                                  ? root.moodLabel(root.displayedMood.mood)
                                    + (root.displayedMood.note ? ' · '+String(root.displayedMood.note) : '')
                                  : qsTrId('genycaster.status.set')
                            elide: Text.ElideRight
                        }
                    }

                    DirectionAwareLabel {
                        objectName: "profileSaveStatus"
                        visible: root.isOwnProfile && root.cloudLocalDirty
                        Layout.fillWidth: true
                        color: openProofConnections && openProofConnections.cloudError.length > 0
                               ? Theme.negative : Theme.inkMuted
                        text: openProofConnections && openProofConnections.cloudError.length > 0
                              ? qsTrId("genycaster.profile.save_retry_notice")
                              : qsTrId("genycaster.profile.saving_details")
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.typeCaptionSize
                    }

                    DirectionAwareLabel {
                        objectName: "profileBlockedByMeStatus"
                        visible: root.activeDetail.blockedByMeKnown === true
                                 && Boolean(root.activeDetail.blockedByMe)
                        Layout.fillWidth: true
                        color: Theme.negative
                        text: qsTrId("genycaster.profile.you_blocked_user")
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        objectName: "profileBlockedMeStatus"
                        visible: root.activeDetail.blocksMeKnown === true
                                 && Boolean(root.activeDetail.blocksMe)
                        Layout.fillWidth: true
                        color: Theme.negative
                        text: qsTrId("genycaster.profile.user_blocked_you")
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
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

                    Flow {
                        id: profileStatsFlow
                        objectName: "profileStatsFlow"
                        // Wrap on narrow phones rather than expanding the
                        // heading or overflowing the horizontal profile gutter.
                        Layout.preferredHeight: childrenRect.height
                        // Give the social counters breathing room below the bio
                        // while keeping the badge on exactly the same row.
                        Layout.topMargin: root.compact ? Theme.space1 : Theme.space3
                        flow: Flow.LeftToRight
                        // Unknown profile metadata must never be presented as
                        // verified zero followers/following on an empty header.
                        visible: Number(root.activeDetail.fid || 0) > 0
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        layoutDirection: Theme.uiDirection

                        Button {
                            id: followingCountButton
                            objectName: "profileFollowingCount"
                            implicitHeight: Theme.scaleUi(32)
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
                                    text: qsTrId("genycaster.profile.following")
                                    color: followingCountButton.hovered ? Theme.ink : Theme.inkMuted
                                    font.pixelSize: Theme.typeBodySize
                                }
                            }
                        }

                        Button {
                            id: followersCountButton
                            objectName: "profileFollowersCount"
                            implicitHeight: Theme.scaleUi(32)
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
                                    text: qsTrId("genycaster.profile.followers")
                                    color: followersCountButton.hovered ? Theme.ink : Theme.inkMuted
                                    font.pixelSize: Theme.typeBodySize
                                }
                            }
                        }

                        // Keep both achievements at the far edge of the social
                        // counters, sharing one measured footprint. Flow wraps
                        // the cluster intact on compact screens.
                        Item {
                            objectName: "profileStreakAlignmentSpacer"
                            width: Math.max(0, profileStatsFlow.width -
                                followingCountButton.width - followersCountButton.width -
                                profileBadgesRow.width - profileStatsFlow.spacing * 3)
                            height: Theme.scaleUi(32)
                        }

                        Row {
                            id: profileBadgesRow
                            objectName: "profileAchievementBadges"
                            width: implicitWidth
                            height: Theme.scaleUi(32)
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            ProfileProBadge {
                                id: profileProBadge
                                objectName: "verifiedGenyProBadge"
                                compact: root.compact
                                entitlement: root.ownProEntitlement
                                showUpgrade: true
                                visible: root.isOwnProfile
                                onDetailsRequested: root.proRequested()
                            }

                            Button {
                                id: profileStreakBadge
                                objectName: "profileStreakBadge"
                                visible: root.isOwnProfile
                                hoverEnabled: true
                                implicitHeight: Theme.scaleUi(32)
                                implicitWidth: streakBadgeContent.implicitWidth + Theme.space1 * 2
                                padding: 0
                                Accessible.name: qsTrId("genycaster.streak.open")
                                // The platform ToolTip is unstyled; use the
                                // same popup surface and placement as ActionButton.
                                onHoveredChanged: {
                                    streakHintTimer.stop()
                                    streakHint.close()
                                    if (hovered && !root.compact)
                                        streakHintTimer.restart()
                                }
                                Timer {
                                    id: streakHintTimer
                                    interval: 420
                                    repeat: false
                                    onTriggered: {
                                        if (!profileStreakBadge.hovered || !profileStreakBadge.Overlay.overlay)
                                            return
                                        const overlay = profileStreakBadge.Overlay.overlay
                                        const point = profileStreakBadge.mapToItem(overlay, 0, 0)
                                        streakHint.x = Math.max(Theme.space2,
                                            Math.min(overlay.width-streakHint.implicitWidth-Theme.space2,
                                                point.x+(profileStreakBadge.width-streakHint.implicitWidth)/2))
                                        streakHint.y = Math.max(Theme.space2,
                                            point.y-streakHint.implicitHeight-Theme.space2)
                                        streakHint.open()
                                    }
                                }
                                Popup {
                                    id: streakHint
                                    parent: profileStreakBadge.Overlay.overlay
                                    modal: false
                                    focus: false
                                    closePolicy: Popup.NoAutoClose
                                    padding: Theme.space2
                                    background: PopupSurface {
                                        curveDepth: Theme.curveCompact
                                        elevation: Theme.elevationPopover
                                    }
                                    contentItem: DirectionAwareLabel {
                                        text: qsTrId("genycaster.streak.open")
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.typeCaptionSize
                                    }
                                }
                                onClicked: {
                                    streakHintTimer.stop()
                                    streakHint.close()
                                    streakDrawer.mobilePresentation = root.compact
                                    streakDrawer.open()
                                }
                                readonly property bool streakActive: Number(
                                    root.streakSnapshot.activity?.current || 0) > 0
                                // Match the ring-and-flame on the desktop account
                                // card; no second oval outline around the counter.
                                background: Item {}
                                contentItem: Row {
                                    id: streakBadgeContent
                                    layoutDirection: Theme.uiDirection
                                    spacing: Theme.space1
                                    Item {
                                        id: profileStreakRing
                                        objectName: "profileStreakRing"
                                        width: Theme.scaleUi(28)
                                        height: width
                                        anchors.verticalCenter: parent.verticalCenter
                                        ProgressRing {
                                            anchors.fill: parent
                                            progress: profileStreakBadge.streakActive
                                                && Number(root.streakSnapshot.activity?.next_milestone || 0)>0
                                                ? Math.min(1, Number(root.streakSnapshot.activity.current)
                                                    /Number(root.streakSnapshot.activity.next_milestone)) : 0
                                            strokeWidth: 2.5
                                            inset: 2
                                            trackColor: Theme.borderMuted
                                            progressColor: profileStreakBadge.streakActive
                                                ? Theme.streak : Theme.inkMuted
                                        }
                                        Icon {
                                            anchors.centerIn: parent
                                            text: Theme.iconStreakFlame
                                            preferredSize: Theme.scaleUi(14)
                                            color: profileStreakBadge.streakActive
                                                ? Theme.streak : Theme.inkMuted
                                        }
                                    }
                                    Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: profileStreakBadge.streakActive
                                            ? Theme.streak : Theme.inkMuted
                                        font.pixelSize: Theme.typeCaptionSize
                                        font.weight: Theme.weightDemiBold
                                        text: LocaleFormat.formatInteger(Number(
                                            root.streakSnapshot.activity?.current || 0))
                                    }
                                }
                            }
                            // Public profiles have no authenticated Streak snapshot.
                            // Show a neutral, non-clickable flame instead of claiming
                            // a verified zero or accidentally opening the viewer's streak.
                            Item {
                                id: profileUnverifiedStreak
                                objectName: "profileUnverifiedStreak"
                                visible: !root.isOwnProfile
                                width: Theme.scaleUi(32)
                                height: width
                                ProgressRing {
                                    anchors.fill: parent
                                    progress: 0
                                    strokeWidth: 2.5
                                    inset: 2
                                    trackColor: Theme.borderMuted
                                    progressColor: Theme.inkMuted
                                }
                                Icon {
                                    anchors.centerIn: parent
                                    text: Theme.iconStreakFlame
                                    color: Theme.inkMuted
                                    preferredSize: Theme.scaleUi(16)
                                }
                                Accessible.name: qsTrId("genycaster.streak.title")
                            }
                        }

                    }
                    RowLayout {
                        objectName: "profileOwnerActions"
                        Layout.fillWidth: true
                        visible: root.isOwnProfile && root.compact
                        // The badge and counters must never touch the action row.
                        Layout.topMargin: Theme.space3
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
                            text: qsTrId("genycaster.profile.share_profile")
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
                            text: qsTrId("genycaster.profile.edit_profile")
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

            // Activity is a read-only derived preview, never a claimed full
            // annual record. Profile rows may be paginated or cached.
            GenyPulsePanel {
                objectName: "profileGenyPulse"
                Layout.fillWidth: true
                Layout.leftMargin: root.profileContentX
                Layout.rightMargin: root.profileContentX
                visible: Number(root.activeDetail.fid || 0) > 0
                profileFid: Number(root.activeDetail.fid || 0)
                compact: root.compact
                isOwnProfile: root.isOwnProfile
                // These are the owner-FID-bound OpenProof connections restored
                // by the native account controller, including server-confirmed
                // cross-device snapshots. A fresh OAuth grant is NOT required
                // just to read the verified public GitHub login.
                verifiedGitHub: root.isOwnProfile
                    ? (root.verifiedGithubActivityAccount.login || "")
                    : root.publicActivityMatchesProfile
                        ? root.publicActivityGithubLogin : ""
                githubLinked: root.isOwnProfile
                    ? root.verifiedGithubActivityAccount.linked
                    : root.publicActivityMatchesProfile
                        && root.publicActivityGithubLogin.length > 0
                githubAccountId: root.verifiedGithubActivityAccount.id
                verifiedWallet: {
                    const verified=root.activeDetail.verifiedEthAddresses || []
                    for(let i=0;i<verified.length && i<50;++i) {
                        const address=String(verified[i] || "")
                        if(/^0x[0-9a-fA-F]{40}$/.test(address))return address
                    }
                    return ""
                }
                previewSource: root.isOwnProfile
                    ? String(root.streakPreferencesSnapshot.activity_source || "casts")
                    : root.publicActivityMatchesProfile
                        ? root.publicActivitySource : "casts"
                onExploreRequested: {
                    activityDrawer.mobilePresentation = root.compact
                    activityDrawer.open()
                }
                onConnectionsRequested: root.openConnections()
            }

            // Profile cards use the same display modes and layout engine as
            // Explorer. Keep the selector independent of profile category tabs
            // so five translated tab labels remain legible at desktop widths.
            RowLayout {
                visible: !root.compact
                Layout.fillWidth: true
                Layout.leftMargin: root.profileContentX
                Layout.rightMargin: root.profileContentX
                Layout.preferredHeight: visible ? Theme.compactControlHeight + 8 : 0
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                ExplorerLayoutControls {
                    layoutMode: root.displayLayout
                    condensed: root.profileContentWidth < 620
                    filterAvailable: true
                    filterLabel: qsTrId("genycaster.filters.profile")
                    onLayoutRequested: mode => root.layoutRequested(mode)
                    onFilterRequested: profileFilterSheet.open()
                }
                Item { Layout.fillWidth: true }
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

                        ActionButton {
                            objectName: "mobileProfileFilterTrigger"
                            // The desktop filter lives next to Magazine/Timeline.
                            visible: root.compact
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: visible ? 44 : 0
                            Layout.preferredHeight: visible ? 44 : 0
                            iconOnly: true
                            quiet: true
                            iconGlyph: "filter"
                            text: qsTrId("genycaster.filters.profile")
                            onClicked: profileFilterSheet.open()
                        }
                        FilterDropdown {
                            id: profileSortFilter
                            objectName: "profileSortFilter"
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: implicitWidth
                            visible: false
                            Layout.maximumWidth: 136
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
            DirectionAwareLabel {
                visible: root.topicFilterActive
                Layout.fillWidth: true
                Layout.leftMargin: root.profileContentX
                Layout.rightMargin: root.profileContentX
                text: qsTrId("genycaster.filters.topic_results") + " · "
                      + root.topicTitleFor(root.topicFilterSlug)
                color: Theme.accent
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Theme.textStartAlignment
            }
            RowLayout {
                layoutDirection: Theme.uiDirection
                visible: false
                Layout.fillWidth: true
                Layout.leftMargin: root.profileContentX
                Layout.rightMargin: root.profileContentX
                Layout.preferredHeight: visible ? Theme.controlHeight + Theme.space2 : 0
                spacing: Theme.space2
                TopicFilterPicker {
                    id: profileTopicPicker
                    Layout.preferredWidth: root.compact ? 164 : 204
                    Layout.maximumWidth: root.compact ? 164 : 204
                    Layout.preferredHeight: Theme.controlHeight
                    catalog: root.topicCatalog
                    compact: root.compact
                    onTopicRequested: slug => {
                        if(slug.length > 0)
                            root.topicRequested(slug,Number(root.activeDetail.fid || 0))
                    }
                }
                Item { Layout.fillWidth: true }
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
                        sharedMediaGeometry: profileMagazineView
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

        footer: profileFooterComponent
        Component {
          id: profileFooterComponent
          Item {
            width: root.readList.width
            height: emptyProfileState.visible ? 176 : (root.busy ? 72 : (root.compact ? 112 : 52))

            ColumnLayout {
                id: emptyProfileState
                x: root.profileContentX
                width: root.profileContentWidth
                anchors.verticalCenter: parent.verticalCenter
                visible: (!root.rows || root.rows.length === 0) && !root.busy && root.runtimeReady
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
                running: visible && !root.readList.moving
                Accessible.name: qsTrId("genycaster.profile.loading_more_profile_posts")
            }
          }
        }
    }

    FeedMasonryView {
        id: profileMagazineView
        objectName: "profileMagazine"
        anchors.fill: parent
        visible: root.profileMagazineEnabled
        // The native window model keeps only viewport cards alive when shown.
        model: profileRowsModel
        header: profileHeaderComponent
        footer: profileFooterComponent
        targetCardWidth: Theme.scaleUi(420)
        maximumColumns: 3
        outsideInset: root.profileContentX
        layoutDirection: Theme.uiDirection
        estimateHeight: function(key, kind, media, width) {
            return root.rowIncubationHeight(root.payloadForStableKey(key), width)
        }
        delegate: Component {
            CastItem {
                width: parent.width
                // A Loader cannot infer the real visual height from an Item
                // that only declares implicitHeight. Keep the paint bounds
                // synchronized with rich-text/media layout growth so the
                // following Magazine slot never draws over this card.
                height: implicitHeight
                modelData: {
                    // Native role notification rebinds payload changes even
                    // though the QML stablePayloads map mutates in place.
                    const revision = parent.revisionKey
                    return root.payloadForStableKey(parent.stableKey)
                }
                compact: true
                magazineCard: true
                sharedMediaGeometry: profileMagazineView
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
        onMovementStarted: {
            resetPositionTimer.stop()
            viewportRestoreTimer.stop()
            root.resetPositionPasses = 0
            root.pendingRestoreKey = ""
            root.viewportAnchorKey = ""
        }
        onMovementEnded: {
            if (root.stableModelSyncPending) root.requestStableModelSync()
            root.scheduleLoadMore()
        }
        onContentYChanged: {
            if (!profileMagazineView.moving && !scrollWorkThrottle.running)
                scrollWorkThrottle.start()
        }
    }

    // Shared adaptive filter drawer for touch and pointer-driven windows.
    ContentFilterSheet {
        id: profileFilterSheet
        heading: qsTrId("genycaster.filters.profile")
        sections: {
            const topics = [{id:"", label:qsTrId("genycaster.filters.all_topics")}]
            for (const entry of root.topicCatalog || []) {
                const slug=String(entry.slug || "")
                if (/^[a-z][a-z0-9-]{1,63}$/.test(slug))
                    topics.push({id:slug,label:root.topicTitleFor(slug)})
            }
            return [
                {id:"feed", title:qsTrId("genycaster.filters.content"), value:root.profileFeed, defaultValue:"user-posts",
                 options:[
                    {id:"user-posts",label:qsTrId("genycaster.profile.posts")},
                    {id:"user-quotes",label:qsTrId("genycaster.profile.quotes")},
                    {id:"user-replies",label:qsTrId("genycaster.profile.replies")},
                    {id:"user-likes",label:qsTrId("genycaster.profile.likes")},
                    {id:"user-popular",label:qsTrId("genycaster.profile.popular")}]},
                {id:"order",title:qsTrId("genycaster.filters.sort"),value:root.profileFeed === "user-popular"
                    ? root.popularMetric : root.profileSortMode,
                 defaultValue:root.profileFeed === "user-popular" ? "all" : "newest",
                 options:root.profileFeed === "user-popular"
                   ? [{id:"all",label:qsTrId("genycaster.profile.sort_all")},
                      {id:"likes",label:qsTrId("genycaster.profile.sort_likes")},
                      {id:"recasts",label:qsTrId("genycaster.profile.sort_recasts")},
                      {id:"replies",label:qsTrId("genycaster.profile.sort_replies")}]
                   : [{id:"newest",label:qsTrId("genycaster.profile.sort_newest")},
                      {id:"oldest",label:qsTrId("genycaster.profile.sort_oldest")}]},
                {id:"topic",title:qsTrId("genycaster.topics.label"),
                 value:root.topicFilterSlug,defaultValue:"",options:topics}
            ]
        }
        onApplied: values => {
            const feedChanged = String(values.feed || "user-posts") !== root.profileFeed
            if (feedChanged)
                root.profileFeedRequested(String(values.feed || "user-posts"))
            if (root.profileFeed === "user-popular")
                root.popularMetric = ["all","likes","recasts","replies"].includes(values.order)
                    ? String(values.order) : "all"
            else
                root.profileSortMode = values.order === "oldest" ? "oldest" : "newest"
            if (String(values.topic || "") !== root.topicFilterSlug
                || (feedChanged && Boolean(values.topic)))
                root.topicRequested(String(values.topic || ""),Number(root.activeDetail.fid || 0))
        }
    }

    function openProfileEditor() {
        if (root.isOwnProfile)
            profileExtrasEditor.open()
    }

    // Activity sources are a separate inspector. Only the owner's choice is
    // saved and synchronized; other profiles default to the public Cast source.
    GenyDrawer {
        id: activityDrawer
        objectName: "profileActivityDrawer"
        parent: Overlay.overlay
        property bool mobilePresentation: false
        bottomSheet: mobilePresentation
        fullScreen: mobilePresentation
        dimBackground: true
        interactive: false
        showHandle: false
        contentPadding: Theme.space4
        width: parent ? (mobilePresentation ? parent.width
            : Math.min(520, Math.max(0, parent.width - Theme.space4*2))) : 520
        height: parent ? parent.height : 640

        contentItem: ColumnLayout {
            spacing: Theme.space3
            ModalHeader {
                Layout.fillWidth: true
                title: qsTrId("genycaster.activity.title")
                onCloseRequested: activityDrawer.close()
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.isOwnProfile
                spacing: Theme.space1
                Label {
                    text: qsTrId("genycaster.activity.default_source")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                GenyTabStrip {
                    Layout.fillWidth: true
                    distribute: true
                    segmented: true
                    currentId: String(root.streakPreferencesSnapshot.activity_source || "casts")
                    tabs: [
                        {id:"casts",label:qsTrId("genycaster.activity.casts")},
                        {id:"github",label:"GitHub"}
                    ]
                    onActivated: id => {
                        if (id === "github" && !root.verifiedGithubActivityAccount.linked) return
                        if (openProofConnections) openProofConnections.setStreakPreferences(
                            id, root.streakPreferencesSnapshot.leaderboard_visible === true)
                    }
                }
                Label {
                    visible: !root.verifiedGithubActivityAccount.linked
                    text: qsTrId("genycaster.activity.github_default_requires_link")
                    font.pixelSize: Theme.typeCaptionSize
                    color: Theme.inkMuted
                    wrapMode: Text.WordWrap
                }
            }
            Flickable {
                objectName: "profileActivityDetailsScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: activityDetails.implicitHeight + Theme.space3
                GenyPulsePanel {
                    id: activityDetails
                    objectName: "profileActivityDetails"
                    width: parent.width
                    detailMode: true
                    expanded: true
                    profileFid: Number(root.activeDetail.fid || 0)
                    compact: activityDrawer.mobilePresentation
                    isOwnProfile: root.isOwnProfile
                    verifiedGitHub: root.isOwnProfile
                        ? (root.verifiedGithubActivityAccount.login || "")
                        : root.publicActivityMatchesProfile
                            ? root.publicActivityGithubLogin : ""
                    githubLinked: root.isOwnProfile
                        ? root.verifiedGithubActivityAccount.linked
                        : root.publicActivityMatchesProfile
                            && root.publicActivityGithubLogin.length > 0
                    githubAccountId: root.verifiedGithubActivityAccount.id
                    verifiedWallet: {
                        const addresses = root.activeDetail.verifiedEthAddresses || []
                        for (let i=0;i<addresses.length && i<50;++i) {
                            const address=String(addresses[i] || "")
                            if(/^0x[0-9a-fA-F]{40}$/.test(address))return address
                        }
                        return ""
                    }
                    onConnectionsRequested: root.openConnections()
                }
            }
        }
    }

    // Streak has its own dedicated surface, not an always-expanded card in
    // the profile timeline. Bottom sheet on phones; side panel on desktop.
    GenyDrawer {
        id: streakDrawer
        objectName: "profileStreakDrawer"
        parent: Overlay.overlay
        // Capture the presentation at opening; a live resize must not flip
        // an already-open Drawer between side panel and bottom sheet.
        property bool mobilePresentation: false
        bottomSheet: mobilePresentation
        dimBackground: true
        showHandle: mobilePresentation
        interactive: false
        contentPadding: mobilePresentation ? Theme.space4 : Theme.space5
        width: parent ? (mobilePresentation ? parent.width
                        : Math.min(500, Math.max(0, parent.width - Theme.space4 * 2))) : 500
        // Desktop inspectors are flush with BOTH viewport edges. A
        // content-fitted height previously left a visible gap at the bottom.
        // Mobile remains a bottom-anchored sheet with safe scrolling.
        height: parent ? (mobilePresentation
            ? Math.min(parent.height * 0.92, Math.max(Theme.scaleUi(320),
                streakDetails.implicitHeight + streakHeader.implicitHeight
                + contentPadding * 2 + sheetTopInset + Theme.space5))
            : parent.height) : 600
        onOpened: {
            if (!openProofConnections || !root.isOwnProfile
                || !openProofConnections.cloudAuthorizationReady) return
            openProofConnections.fetchStreak()
            openProofConnections.fetchStreakLeaderboard("activity")
        }

        contentItem: ColumnLayout {
            spacing: Theme.space3

            ModalHeader {
                id: streakHeader
                Layout.fillWidth: true
                title: qsTrId("genycaster.streak.title")
                subtitle: qsTrId("genycaster.streak.subtitle")
                onCloseRequested: streakDrawer.close()
            }

            Flickable {
                id: profileStreakScroll
                objectName: "profileStreakScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                Layout.preferredHeight: Math.min(contentHeight, Theme.scaleUi(500))
                clip: true
                interactive: contentHeight > height
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: Math.max(0, streakDetails.implicitHeight + Theme.space5)
                onContentHeightChanged: contentY = Math.min(contentY,
                    Math.max(0, contentHeight - height))
                ScrollBar.vertical: GenyScrollBar {}

                StreakCard {
                    id: streakDetails
                    objectName: "ownerProfileStreakDetails"
                    width: parent.width
                    compact: streakDrawer.mobilePresentation
                    snapshot: root.streakSnapshot
                    loading: root.streakLoadingSnapshot
                    errorText: root.streakErrorSnapshot
                    onProfileRequested: fid => {
                        streakDrawer.close()
                        root.profileRequested(fid)
                    }
                    onPageChanged: {
                        // Switching between Streak and its peer leaderboard
                        // starts each page at its top.
                        profileStreakScroll.contentY = 0
                    }
                }
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
        fullScreen: mobilePresentation
        property var secondarySelection: []
        property var roleSelection: []
        dimBackground: true
        width: parent ? (mobilePresentation ? parent.width : Math.min(480, parent.width)) : 480
        height: parent ? parent.height : 750
        showHandle: false
        // Forms own touch-scrolling; the explicit Cancel/Close control owns
        // dismissal. No misleading draggable handle or clipped top stage.
        interactive: false
        contentPadding: Theme.space3
        onOpened: {
            openProofConnections.checkCloudService()
            const savedCategory = String(root.profileExtras.category || "")
            const savedRole = String(root.profileExtras.profession || "")
            const inferredCategory = !savedCategory && savedRole === "Developer"
                                     ? "Technology" : savedCategory
            categoryEditor.currentIndex = Math.max(0,
                root.profileCategoryOptions.indexOf(inferredCategory))
            profileExtrasEditor.secondarySelection = root.profileSequence(root.profileExtras.secondaryCategories, 3)
                .filter(value => value !== inferredCategory)
            profileExtrasEditor.roleSelection = root.profileSequence(root.profileExtras.roles, 5)
            if (profileExtrasEditor.roleSelection.length === 0 && savedRole)
                profileExtrasEditor.roleSelection = [savedRole]
            const saved = Array.isArray(root.profileExtras.links) ? root.profileExtras.links : []
            const titles = Array.isArray(root.profileExtras.labels) ? root.profileExtras.labels : []
            linksEditor.text = saved.map((url,index)=>titles[index]
                ? titles[index] + " | " + url : url).join("\n")
            root.profileExtrasError = ""
            root.protocolProfileError = ""
            root.protocolProfilePending = false
            root.protocolProfileConfirmed = false
            root.editProfileMediaUrls = false
            displayProtocolEditor.text = String(root.activeDetail.displayName || "")
            bioProtocolEditor.text = String(root.activeDetail.bio || "")
            websiteProtocolEditor.text = String(root.activeDetail.profileUrl || "")
            avatarProtocolEditor.text = String(root.activeDetail.avatarUrl || "")
            bannerProtocolEditor.text = String(root.activeDetail.bannerUrl || "")
        }
        contentItem: ColumnLayout {
            spacing: Theme.space3
            ModalHeader {
                Layout.fillWidth: true
                title: qsTrId("genycaster.profile.edit_details")
                subtitle: qsTrId("genycaster.profile.edit_sections_subtitle")
                sheetPresentation: false
                onCloseRequested: profileExtrasEditor.close()
            }
            Flickable {
                id: profileEditorScroll
                objectName: "profileEditorScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                interactive: true
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                contentHeight: formColumn.implicitHeight + Theme.space4
                contentWidth: width
                ScrollBar.vertical: GenyScrollBar {}
                ColumnLayout {
                    id: formColumn
                    width: parent.width
                    spacing: Theme.space3
                    ActionButton {
                        objectName: "profileStatusSetting"
                        Layout.fillWidth: true
                        iconGlyph: Theme.iconSpark
                        quiet: true
                        alignStart: true
                        text: qsTrId('genycaster.status.edit')
                        onClicked: {
                            profileExtrasEditor.close()
                            Qt.callLater(function() { profileStatusEditor.open() })
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.protocol_heading")
                        font.weight: Theme.weightBold
                        font.pixelSize: Theme.typeLabelSize
                        color: Theme.ink
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.protocol_description")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.display_name")
                        color: Theme.inkMuted
                    }
                    DirectionAwareTextField {
                        id: displayProtocolEditor
                        objectName: "profileProtocolDisplay"
                        Layout.fillWidth: true
                        maximumLength: 32
                        color: Theme.ink
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.bio")
                        color: Theme.inkMuted
                    }
                    DirectionAwareTextArea {
                        id: bioProtocolEditor
                        objectName: "profileProtocolBio"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.scaleUi(114)
                        wrapMode: TextEdit.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.website")
                        color: Theme.inkMuted
                    }
                    DirectionAwareTextField {
                        id: websiteProtocolEditor
                        objectName: "profileProtocolWebsite"
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhUrlCharactersOnly
                        forceLeftToRight: true
                    }
                        DirectionAwareLabel {
                         Layout.fillWidth: true
                         text: qsTrId("genycaster.profile.preview_label")
                         font.pixelSize: Theme.typeLabelSize
                         font.weight: Theme.weightDemiBold
                         color: Theme.textPrimary
                     }
                     Item {
                         objectName: "profileLivePreview"
                         Layout.fillWidth: true
                         Layout.preferredHeight: 214
                         clip: true

                         Superellipse {
                             anchors.fill: parent
                             fillColor: Theme.surfaceRaised
                             borderColor: Theme.borderSubtle
                             borderWidth: Theme.strokeHairline
                             curveDepth: Theme.curveContainer
                         }
                         SuperellipseImage {
                             x: 1
                             y: 1
                             width: parent.width - 2
                             height: 144
                             fillMode: Image.PreserveAspectCrop
                             source: linkPreviewResolver ? linkPreviewResolver.imageSourceForUrl(bannerProtocolEditor.text.trim()) : ""
                             mediaCategory: "avatars"
                             forceAutomaticDownload: true
                             showLoadingIndicator: false
                             showFailureIndicator: false
                             fillColor: Theme.surfaceMuted
                             curveDepth: Theme.curveContainer
                         }
                         RowLayout {
                             x: Theme.space3
                             y: 108
                             width: parent.width - Theme.space3 * 2
                             height: 98
                             spacing: Theme.space3
                             layoutDirection: Theme.uiDirection
                             Avatar {
                                 avatarSize: 76
                                 Layout.preferredWidth: 76
                                 Layout.preferredHeight: 76
                                 sourceUrl: avatarProtocolEditor.text.trim()
                                 displayName: displayProtocolEditor.text
                                 forceAutomaticDownload: true
                             }
                             ColumnLayout {
                                 Layout.fillWidth: true
                                 Layout.minimumWidth: 0
                                 Layout.alignment: Qt.AlignBottom
                                 spacing: 2
                                 DirectionAwareLabel {
                                     Layout.fillWidth: true
                                     text: displayProtocolEditor.text.trim()
                                           || String(root.activeDetail.displayName || "")
                                     color: Theme.textPrimary
                                     font.pixelSize: Theme.typeBodySize
                                     font.weight: Theme.weightDemiBold
                                     elide: Text.ElideRight
                                 }
                                 DirectionAwareLabel {
                                     Layout.fillWidth: true
                                     visible: root.showUsername
                                     text: root.showUsername ? "@" + String(root.activeDetail.username) : ""
                                     color: Theme.textSecondary
                                     font.pixelSize: Theme.typeCaptionSize
                                     elide: Text.ElideRight
                                 }
                             }
                         }
                     }
                     RowLayout {
                         Layout.fillWidth: true
                         spacing: Theme.space2
                         layoutDirection: Theme.uiDirection
                         ActionButton {
                             Layout.fillWidth: true
                             compact: true
                             quiet: true
                             iconGlyph: Theme.iconImage
                             text: qsTrId("genycaster.profile.upload_avatar")
                             enabled: mediaUpload.available && !mediaUpload.busy
                             onClicked: root.chooseProfileImage(-1001)
                         }
                         ActionButton {
                             Layout.fillWidth: true
                             compact: true
                             quiet: true
                             iconGlyph: Theme.iconImage
                             text: qsTrId("genycaster.profile.upload_banner")
                             enabled: mediaUpload.available && !mediaUpload.busy
                             onClicked: root.chooseProfileImage(-1002)
                         }
                     }
                     DirectionAwareTextField {
                         id: avatarProtocolEditor
                         objectName: "profileProtocolAvatar"
                         visible: root.editProfileMediaUrls
                         Layout.fillWidth: true
                         forceLeftToRight: true
                         inputMethodHints: Qt.ImhUrlCharactersOnly
                         placeholderText: "https://"
                     }
                     DirectionAwareTextField {
                         id: bannerProtocolEditor
                         objectName: "profileProtocolBanner"
                         visible: root.editProfileMediaUrls
                         Layout.fillWidth: true
                         forceLeftToRight: true
                         inputMethodHints: Qt.ImhUrlCharactersOnly
                         placeholderText: "https://"
                     }
                    ActionButton {
                        Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                        compact: true
                        quiet: true
                        text: root.editProfileMediaUrls
                              ? qsTrId("genycaster.profile.hide_image_url")
                              : qsTrId("genycaster.profile.edit_image_url")
                        onClicked: root.editProfileMediaUrls = !root.editProfileMediaUrls
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: mediaUpload.busy || root.protocolProfilePending
                        text: mediaUpload.busy
                              ? qsTrId("genycaster.profile.uploading_image")
                              : qsTrId("genycaster.profile.saving_to_farcaster")
                        color: Theme.inkMuted
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: root.protocolProfileError.length > 0 || mediaUpload.errorText.length > 0
                        text: root.protocolProfileError || mediaUpload.errorText
                        color: Theme.negative
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: root.protocolProfileConfirmed
                        text: qsTrId("genycaster.profile.protocol_confirmed")
                        color: Theme.success
                        wrapMode: Text.Wrap
                    }
                    ActionButton {
                        objectName: "profileProtocolSave"
                        Layout.fillWidth: true
                        prominent: true
                        enabled: presentationBridge.writeReady && !presentationBridge.writeBusy
                                 && !mediaUpload.busy && !root.protocolProfilePending
                        text: qsTrId("genycaster.profile.update_farcaster")
                        onClicked: {
                            const current = root.activeDetail
                            const fields = {
                                display: displayProtocolEditor.text.trim(),
                                bio: bioProtocolEditor.text.trim(),
                                url: websiteProtocolEditor.text.trim(),
                                pfp: avatarProtocolEditor.text.trim(),
                                banner: bannerProtocolEditor.text.trim()
                            }
                            const existing = {
                                display: String(current.displayName || ""),
                                bio: String(current.bio || ""),
                                url: String(current.profileUrl || ""),
                                pfp: String(current.avatarUrl || ""),
                                banner: String(current.bannerUrl || "")
                            }
                            const changes = ({})
                            for (const key of Object.keys(fields)) {
                                if (fields[key] !== existing[key]) changes[key] = fields[key]
                            }
                            if (Object.keys(changes).length === 0) {
                                root.protocolProfileError = qsTrId("genycaster.profile.no_protocol_changes")
                                return
                            }
                            for (const key of Object.keys(changes)) {
                                if (presentationBridge.utf8ByteCount(changes[key])
                                    > (key === "display" ? 32 : 256)) {
                                    root.protocolProfileError = qsTrId("genycaster.profile.value_too_long")
                                    return
                                }
                            }
                            root.protocolProfileError = ""
                            root.protocolProfileConfirmed = false
                            root.protocolProfilePending = presentationBridge.updateProtocolProfile(changes)
                            if (!root.protocolProfilePending)
                                root.protocolProfileError = qsTrId("genycaster.profile.protocol_write_failed")
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.borderMuted
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.extras_heading")
                        color: Theme.ink
                        font.pixelSize: Theme.typeLabelSize
                        font.weight: Theme.weightDemiBold
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.extras_description")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.category_label")
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
                        optionLabelProvider: function(value) { return root.profileOptionLabel(value) }
                        onActivated: index => {
                            if (index > 0)
                                profileExtrasEditor.secondarySelection =
                                    profileExtrasEditor.secondarySelection.filter(
                                        name => name !== root.profileCategoryOptions[index])
                        }
                        Accessible.name: qsTrId("genycaster.profile.category_label")
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.secondary_categories_label")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    GenyMultiSelect {
                        id: secondaryPicker
                        objectName: "profileSecondaryCategoryPicker"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        options: root.profileCategoryOptions.slice(1).filter(
                            category => category !== categoryEditor.currentText)
                        selectedValues: profileExtrasEditor.secondarySelection
                        maximumSelection: 3
                        placeholderText: qsTrId("genycaster.profile.add_secondary")
                        accessibleLabel: qsTrId("genycaster.profile.secondary_categories_label")
                        removeLabel: qsTrId("genycaster.profile.remove_secondary")
                        optionLabelProvider: function(value) { return root.profileOptionLabel(value) }
                        onSelectionEdited: nextValues => {
                            profileExtrasEditor.secondarySelection = nextValues
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.role_label")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                    GenyMultiSelect {
                        id: professionEditor
                        objectName: "profileProfessionPicker"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        options: root.profileRoleOptions.slice(1)
                        selectedValues: profileExtrasEditor.roleSelection
                        maximumSelection: 5
                        placeholderText: qsTrId("genycaster.profile.add_role")
                        accessibleLabel: qsTrId("genycaster.profile.role_accessible")
                        removeLabel: qsTrId("genycaster.profile.remove_role")
                        optionLabelProvider: function(value) { return root.profileOptionLabel(value) }
                        onSelectionEdited: nextValues => {
                            profileExtrasEditor.roleSelection = nextValues
                        }
                    }
                    DirectionAwareLabel {
                        visible: String(root.profileExtras.profession || "").length > 0
                            && root.profileRoleOptions.indexOf(String(root.profileExtras.profession)) < 0
                            && profileExtrasEditor.roleSelection.length === 0
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.previous_role")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.profile.links_editor_hint")
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.typeCaptionSize
                        color: Theme.inkMuted
                    }
                    DirectionAwareTextArea {
                        id: linksEditor
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.scaleUi(180)
                        wrapMode: TextEdit.WrapAnywhere
                        placeholderText: qsTrId("genycaster.profile.links_placeholder")
                        color: Theme.ink
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
                layoutDirection: Theme.uiDirection
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    Layout.fillWidth: true
                    quiet: true
                    text: qsTrId("genycaster.profile.cancel")
                    onClicked: profileExtrasEditor.close()
                }
                ActionButton {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.profile.save_additional_details")
                    onClicked: {
                        if (root.saveExtraDetails(
                                profileExtrasEditor.roleSelection.length
                                   ? String(profileExtrasEditor.roleSelection[0]) : "",
                                linksEditor.text,
                                categoryEditor.currentIndex > 0 ? categoryEditor.currentText : "",
                                profileExtrasEditor.secondarySelection,
                profileExtrasEditor.roleSelection))
                            profileExtrasEditor.close()
                    }
                }
            }
        }
    }


    GenyDrawer {
        id: profileStatusEditor
        objectName: "profileStatusEditor"
        parent: Overlay.overlay
        readonly property bool mobilePresentation:
            parent && parent.width < Theme.breakpointMobile
        bottomSheet: mobilePresentation
        fullScreen: mobilePresentation
        dimBackground: true
        interactive: false
        showHandle: false
        width: parent ? (mobilePresentation ? parent.width : Math.min(430,parent.width)) : 430
        // Fill the viewport while the keyboard resizes it. The note editor
        // scrolls independently and Save stays in the dedicated footer.
        height: parent ? parent.height : 690
        contentPadding: Theme.space3
        property string selectedMood: "none"
        function revealNoteEditor() {
            if (!opened || !statusNoteEditor.activeFocus || statusFormScroll.height <= 0)
                return
            Qt.callLater(function() {
                if (!profileStatusEditor.opened || !statusNoteEditor.activeFocus
                        || statusFormScroll.height <= 0)
                    return
                const margin = Theme.space2
                const top = statusNoteEditor.y
                const bottom = top + statusNoteEditor.height
                const maxY = Math.max(0, statusFormScroll.contentHeight - statusFormScroll.height)
                const currentY = statusFormScroll.contentY
                if (top < currentY + margin)
                    statusFormScroll.contentY = Math.max(0, Math.min(maxY, top - margin))
                else if (bottom > currentY + statusFormScroll.height - margin)
                    statusFormScroll.contentY = Math.max(0, Math.min(
                        maxY, bottom + margin - statusFormScroll.height))
            })
        }
        property bool publicMood: true
        property bool awaitingSave: false
        property bool dirtySelection: false
        property string saveError: ""
        onOpened: {
            const state=root.displayedMood
            selectedMood=String(state.mood||"none")
            if(!root.moodOptions.some(option=>option.id===selectedMood))
                selectedMood="none"
            statusNoteEditor.text=String(state.note||"")
            publicMood=state.visible!==false
            awaitingSave=false
            dirtySelection=false
            saveError=""
            openProofConnections.fetchProfileStatus(Number(root.activeDetail.fid||0))
        }
        Connections {
            target: openProofConnections
            function onProfileStatusChanged() {
                // GET completion must never be mistaken for a confirmed PUT.
                if(profileStatusEditor.visible && !profileStatusEditor.dirtySelection
                          && !profileStatusEditor.awaitingSave
                          && openProofConnections && !openProofConnections.profileStatusBusy) {
                    const state=root.displayedMood
                    profileStatusEditor.selectedMood=String(state.mood||"none")
                    statusNoteEditor.text=String(state.note||"")
                    profileStatusEditor.publicMood=state.visible!==false
                }
            }
            function onProfileStatusSaveFinished(saved,message) {
                if(!profileStatusEditor.awaitingSave)
                    return
                profileStatusEditor.awaitingSave=false
                if(saved) {
                    profileStatusEditor.saveError=""
                    statusNoteEditor.focus=false
                    profileStatusEditor.close()
                } else {
                    // Persist the error even if a 409 triggers a fresh GET.
                    profileStatusEditor.saveError=message
                }
            }
        }
        contentItem: ColumnLayout {
            spacing: Theme.space3
            ModalHeader {
                Layout.fillWidth: true
                title: qsTrId("genycaster.status.set")
                subtitle: qsTrId("genycaster.status.hint")
                sheetPresentation: false
                onCloseRequested: profileStatusEditor.close()
            }
            Flickable {
                id: statusFormScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: statusForm.implicitHeight+Theme.space3
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                onHeightChanged: profileStatusEditor.revealNoteEditor()
                ColumnLayout {
                    id: statusForm
                    width: parent.width
                    spacing: Theme.space2
                    Repeater {
                        model: root.moodOptions
                        delegate: ActionButton {
                            required property var modelData
                            Layout.fillWidth: true
                            elevated: false
                            quiet: profileStatusEditor.selectedMood!==modelData.id
                            selected: profileStatusEditor.selectedMood===modelData.id
                            iconGlyph: modelData.icon
                            customIconColor: modelData.color
                            customSelectedColor: modelData.color
                            customSelectedSurface: Qt.rgba(modelData.color.r,modelData.color.g,
                                                           modelData.color.b,Theme.dark ? 0.19 : 0.11)
                            alignStart: true
                            text: root.moodLabel(modelData.id)
                            onClicked: {
                                profileStatusEditor.selectedMood=modelData.id
                                profileStatusEditor.dirtySelection=true
                                profileStatusEditor.saveError=""
                            }
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.status.note")
                        color: Theme.inkMuted
                    }
                    DirectionAwareTextField {
                        id: statusNoteEditor
                        objectName: "profileStatusCustomNote"
                        Layout.fillWidth: true
                        maximumLength: 72
                        placeholderText: qsTrId("genycaster.status.placeholder")
                        onActiveFocusChanged: if (activeFocus)
                            profileStatusEditor.revealNoteEditor()
                        onTextChanged: if (profileStatusEditor.visible) {
                            profileStatusEditor.dirtySelection=true
                            profileStatusEditor.saveError=""
                        }
                    }
                    GenySwitch {
                        id: statusVisibility
                        Layout.fillWidth: true
                        checked: profileStatusEditor.publicMood
                        text: qsTrId("genycaster.status.visible")
                        onToggled: {
                            profileStatusEditor.publicMood=checked
                            profileStatusEditor.dirtySelection=true
                            profileStatusEditor.saveError=""
                        }
                    }
                }
            }
            DirectionAwareLabel {
                objectName: "profileStatusSaveError"
                Layout.fillWidth: true
                visible: profileStatusEditor.saveError.length>0
                text: profileStatusEditor.saveError
                color: Theme.negative
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeCaptionSize
                Accessible.role: Accessible.StaticText
            }
            ActionButton {
                objectName: "profileStatusSave"
                Layout.fillWidth: true
                prominent: true
                busy: profileStatusEditor.awaitingSave
                enabled: !profileStatusEditor.awaitingSave
                    && openProofConnections && !openProofConnections.profileStatusBusy
                    && openProofConnections.cloudAuthorizationReady
                    && (profileStatusEditor.selectedMood!=="custom"
                        || statusNoteEditor.text.trim().length>0)
                text: qsTrId("genycaster.status.save")
                onClicked: {
                    profileStatusEditor.saveError=""
                    profileStatusEditor.awaitingSave=true
                    openProofConnections.saveProfileStatus(
                        profileStatusEditor.selectedMood,
                        statusNoteEditor.text,
                        profileStatusEditor.publicMood)
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
        dimBackground: true
        showHandle: mobilePresentation
        width: parent ? (mobilePresentation ? parent.width : Math.min(440, parent.width)) : 440
        height: parent ? (mobilePresentation ? Math.min(parent.height * 0.68, 530) : parent.height) : 530
        contentPadding: Theme.space4
        // LinkPreviewResolver is a shared C++/Qt service, not a browser-only
        // metadata bridge. Queue bounded public-link lookups only on open.
        function requestMetadata(url) {
            linkPreviewResolver.requestPreview(url)
            // Single-threaded WebAssembly has no native parser worker.
            // Reuse Genycaster's existing bounded C++ metadata relay there,
            // rather than introducing browser JavaScript or raw QML fetch.
            if (Qt.platform.os === "wasm"
                    && typeof browserLinkMetadataController !== "undefined")
                browserLinkMetadataController.requestPreview(url, "link")
        }
        onOpened: {
            if (root.showProfileUrl)
                requestMetadata(String(root.activeDetail.profileUrl || ""))
            const urls = root.profileExtraLinks
            for (let i = 0; i < urls.length; ++i)
                requestMetadata(String(urls[i]))
        }
        contentItem: ColumnLayout {
            spacing: Theme.space3
            ModalHeader {
                Layout.fillWidth: true
                title: qsTrId("genycaster.profile.links_title")
                subtitle: qsTrId("genycaster.profile.links_subtitle")
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
                    ProfileLinkRow {
                        visible: root.showProfileUrl
                        mediaActive: profileLinksDrawer.opened
                        targetUrl: String(root.activeDetail.profileUrl || "")
                        userLabel: qsTrId("genycaster.profile.farcaster_website")
                        displayUrl: root.displayUrl(targetUrl)
                        onOpenRequested: url => {
                            root.externalRequested(url)
                            profileLinksDrawer.close()
                        }
                    }
                    Repeater {
                        model: root.profileExtraLinks
                        delegate: ProfileLinkRow {
                            mediaActive: profileLinksDrawer.opened
                            required property int index
                            required property var modelData
                            targetUrl: String(modelData)
                            userLabel: root.profileExtraLabel(index)
                            genericLabel: !String(root.profileSequence(root.profileExtras.labels, 5)[index] || "").trim()
                            displayUrl: root.displayUrl(targetUrl)
                            onOpenRequested: url => {
                                root.externalRequested(url)
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
        connections: openProofConnections ? openProofConnections.connections : []
        catalogFallback: openProofConnections ? openProofConnections.catalogFallback : []
        authorized: openProofConnections ? openProofConnections.authorized : false
        cachedSnapshot: openProofConnections ? openProofConnections.cachedSnapshot : ({})
        authorizing: openProofConnections ? openProofConnections.authorizing : false
        approvalUrl: openProofConnections ? openProofConnections.approvalUrl : ""
        actionBusy: openProofConnections ? openProofConnections.actionBusy : false
        pendingProvider: openProofConnections ? openProofConnections.pendingProvider : ""
        manualLinkedInUrl: openProofConnections ? openProofConnections.manualLinkedInUrl : ""
        errorText: openProofConnections ? openProofConnections.error : ""
        onOpenProfileRequested: url => root.externalRequested(url)
        onProDetailsRequested: root.proRequested()
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
        profileStatusFetch.restart()
        streakFetch.restart()
        publicActivityFetch.restart()
        root.profileLinksExpanded = false
        if (root.isOwnProfile && openProofConnections.cloudAuthorizationReady)
            profileRefreshRetry.restart()
        if (!root.isOwnProfile) {
            accountConnectionsPopup.close()
            profileExtrasEditor.close()
            profileStatusEditor.close()
            profileLinksDrawer.close()
            streakDrawer.close()
        }
    }
    PullToRefresh {
        flickable: root.readList
        busy: root.busy
        refreshEnabled: Number(root.activeDetail.fid || 0) > 0
        onRefreshRequested: root.refreshProfileAndAccount()
    }

    ScrollToTopButton {
        flickable: root.readList
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space5
        // The mobile inset already includes the dock and its compose button.
        anchors.bottomMargin: Math.max(Theme.space4, root.bottomOverlayInset
                                      + Theme.space3)
    }
    ContentRefreshIndicator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 50
        active: root.busy && root.rows && root.rows.length > 0
    }

}
