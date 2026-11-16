import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root

    property var rows: []
    property var mutedProfileFids: []
    // Imperatively refreshed by AppShell. Do not bind this state to the
    // same bridge notification signal that also changes rows and pagination.
    property bool loadingNotifications: false
    property bool hasMore: false
    property string errorText: ""
    property bool unsupportedRawNotifications: false
    property bool authenticated: false
    property bool compact: false
    property string filterKind: "all"
    property double accountFid: 0
    property var accountProfile: ({})
    property string viewportAnchorKey: ""
    property real viewportAnchorOffset: 0
    property bool restoringViewport: false
    property bool stableModelSyncPending: false
    property var stablePayloads: ({})
    property string pendingRestoreKey: ""
    property real pendingRestoreOffset: 0
    property bool pendingRestoreCapture: false
    // A local snapshot of verified actor identities; never invent names for
    // counts that the provider reports without their corresponding profiles.
    property var expandedActors: []
    property int expandedActorCount: 0
    property string expandedActorKind: ""
    function inspectActors(row) {
        if (!row || (row.kind !== "reaction" && row.kind !== "recast")) return
        expandedActors = root.asList(row.actors)
        expandedActorCount = Math.max(expandedActors.length, Number(row.aggregateCount || 0))
        expandedActorKind = String(row.kind || "")
        reactionActorsPopup.open()
    }

    signal refreshRequested()
    signal loadMoreRequested()
    signal signInRequested()
    signal profileRequested(double fid)
    signal castRequested(string castId)

    readonly property int gutter: compact ? Theme.mobileGutter : Theme.space5
    readonly property bool showState: !root.authenticated
                                      || (!root.loadingNotifications && root.visibleRows.length === 0)
    readonly property bool stateIsError: root.authenticated && root.errorText.length > 0
    readonly property string stateIcon: !root.authenticated ? Theme.iconNotification
                                        : root.stateIsError ? Theme.iconWarning
                                        : Theme.iconNotification
    readonly property string stateTitle: !root.authenticated
                                         ? qsTrId("genycaster.notifications.sign_in_title")
                                         : root.unsupportedRawNotifications
                                           ? qsTrId("genycaster.nodes.notifications_unavailable")
                                         : root.stateIsError
                                           ? qsTrId("genycaster.notifications.could_not_refresh")
                                           : root.filterKind !== "all" ? qsTrId("genycaster.notifications.filtered_empty")
                                           : qsTrId("genycaster.notifications.empty_title")
    readonly property string stateDescription: !root.authenticated
                                               ? qsTrId("genycaster.notifications.sign_in_description")
                                               : root.unsupportedRawNotifications
                                                 ? qsTrId("genycaster.nodes.raw_limitations")
                                               : root.stateIsError
                                                 ? root.errorText
                                                 : root.filterKind !== "all"
                                                   ? qsTrId("genycaster.notifications.filtered_description")
                                                   : qsTrId("genycaster.notifications.empty_description")

    // Bridge rows and row.actors arrive as C++ QVariantList values. QML exposes
    // those as array-like sequence objects that support .length and indexed
    // reads, but Array.isArray() is false for them. Normalize through a real
    // array so grouping/merging code can treat every source uniformly.
    function asList(value) {
        const result = []
        if (value === null || value === undefined)
            return result
        const length = Number(value.length) || 0
        for (let index = 0; index < length; ++index)
            result.push(value[index])
        return result
    }

    // %%1%% and %%2%% "people" phrases describe the people *other than* the
    // leading actor, mirroring Farcaster's "0xho3ein and 24 others" summaries.
    // aggregateCount therefore carries the total group size; the label subtracts
    // the first actor before formatting.
    function kindTitle(row) {
        const kind = String(row.kind || "")
        const actor = String(row.actorDisplayName || row.actor || qsTrId("genycaster.notifications.someone"))
        const count = Math.max(1, Number(row.aggregateCount || 1))
        const others = count - 1
        if (kind === "reply") return qsTrId("genycaster.notifications.replied").arg(actor)
        if (kind === "mention") return qsTrId("genycaster.notifications.mentioned").arg(actor)
        if (kind === "follow") return qsTrId("genycaster.notifications.followed").arg(actor)
        if (kind === "recast" && others >= 1)
            return qsTrId("genycaster.notifications.recasted_many").arg(actor).arg(LocaleFormat.formatInteger(others))
        if (kind === "recast") return qsTrId("genycaster.notifications.recasted").arg(actor)
        if (kind === "quote") return actor + " · " + qsTrId("genycaster.composer.quote")
        if (kind === "reaction" && others >= 1)
            return qsTrId("genycaster.notifications.reacted_many").arg(actor).arg(LocaleFormat.formatInteger(others))
        if (kind === "reaction") return qsTrId("genycaster.notifications.reacted").arg(actor)
        return qsTrId("genycaster.notifications.activity")
    }

    function actorRows(row) {
        const actors = root.asList(row.actors)
        if (actors.length > 0)
            return actors
        return [{
            fid: Number(row.actorFid || 0),
            username: String(row.actor || ""),
            displayName: String(row.actorDisplayName || row.actor || ""),
            avatarUrl: String(row.actorAvatarUrl || "")
        }]
    }

    function kindColor(kind) {
        // Reaction types are distinguished by glyphs; notification chrome
        // always tracks the user-selected accent (including custom themes).
        return Theme.accent
    }

    function kindGlyph(kind) {
        switch (String(kind || "")) {
        case "reply": return Theme.iconReply
        case "mention": return Theme.iconMention
        case "reaction": return Theme.iconHeart
        case "recast": return Theme.iconRepost
        case "quote": return Theme.iconQuote
        case "follow": return Theme.iconUser
        }
        return Theme.iconNotification
    }

    function notificationImageUrl(value) {
        const url = String(value || "")
        const path = url.split("?")[0].split("#")[0].toLowerCase()
        // Image-delivery CDNs use opaque IDs and transform variants rather
        // than file extensions; treating those assets as generic website
        // links showed a meaningless "imagedelivery.net" chip instead of art.
        const imageDelivery = /^https:\/\/(?:imagedelivery\.net|imagedelivery\.cloudflare\.com)\//i.test(url)
        return /^https:\/\//i.test(url)
               && (imageDelivery || [".png", ".jpg", ".jpeg", ".webp", ".gif", ".avif"]
                   .some(ext => path.endsWith(ext))) ? url : ""
    }

    function notificationLinkHost(value) {
        const url = String(value || "")
        if (!/^https:\/\//i.test(url))
            return ""
        return url.replace(/^https:\/\/(www\.)?/i, "").split(/[/?#]/)[0]
    }

    function kindTint(kind) {
        return Theme.surfaceSelected
    }

    function notificationKey(row, index) {
        const value = row || ({})
        const kind = String(value.kind || "")
        const target = String(value.castId || "")
        if (target.length > 0 && (kind === "reaction" || kind === "recast"))
            return "notif-group:" + kind + ":" + target
        const id = String(value.id || value.castId || "")
        return id.length > 0 ? "notif:" + id : "notif-index:" + index
    }

    function notificationRevision(row) {
        const value = row || ({})
        return [String(value.kind || ""), String(value.observedAtMs || 0),
                String(value.aggregateCount || 0), String(value.castId || ""),
                String(value.castText || ""), String(value.actorFid || 0),
                String(value.castParentId || ""), String(value.parentCastId || ""),
                String(value.quotedCastId || ""), String(value.parentText || ""), String(value.parentAvatarUrl || ""),
                String(value.castEmbedUrl || ""), String(value.parentEmbedUrl || ""),
                String(value.castChannel || ""),
                root.asList(value.actors).map(actor => String(actor.fid || 0)).join(",")].join("\u001f")
    }

    // ---- Farcaster-style aggregation ---------------------------------------
    // The read provider often returns one row per actor ("X reacted to your
    // cast" three times for three reactors). Grouping collates reaction and
    // recast rows that share the same target cast into a single row whose
    // leading avatar stack shows the distinct actors and whose title reports
    // the total, mirroring Farcaster's "X and N others liked your cast".
    // Replies, mentions and follows stay one row each: every reply is a
    // distinct cast with its own content, and each follow is its own event.
    // Muted accounts must not leak through aggregated reactions. Remove
    // muted actors BEFORE grouping, so aggregation does not overstate counts.
    function withoutMutedActors(source) {
        const rows=root.asList(source)
        if(!root.mutedProfileFids || !root.mutedProfileFids.length)return rows
        const visible=[]
        for(const original of rows) {
            if(!original)continue
            const actors=root.asList(original.actors)
            if(actors.length > 0) {
                const filtered=actors.filter(actor=>
                    root.mutedProfileFids.indexOf(String(Number(actor.fid || 0))) < 0)
                if(filtered.length===0)continue
                const copy=root.cloneNotificationRow(original)
                copy.actors=filtered
                copy.aggregateCount=filtered.length
                const first=filtered[0]
                copy.actorFid=Number(first.fid||0)
                copy.actor=String(first.username||"")
                copy.actorDisplayName=String(first.displayName||first.username||"")
                copy.actorAvatarUrl=String(first.avatarUrl||"")
                visible.push(copy)
            } else if(root.mutedProfileFids.indexOf(
                         String(Number(original.actorFid||0)))<0) {
                visible.push(original)
            }
        }
        return visible
    }
    readonly property var displayRows: root.groupNotificationRows(root.withoutMutedActors(root.rows))
    // A reply to one of my casts is an Activity, not automatically an @mention.
    // Only explicit provider mentions or a literal @username directed at the
    // signed-in account belong under Mentions.
    function mentionsViewer(row) {
        const kind = String(row.kind || "")
        if (kind === "mention") return true
        if (kind !== "reply" && kind !== "quote" && kind !== "unknown")
            return false
        const handle = String(root.accountProfile && root.accountProfile.username || "")
            .replace(/^@/, "").toLowerCase()
        if (handle.length === 0 || /^fid:[0-9]+$/i.test(handle)) return false
        const text = String(row.castText || "").toLowerCase()
        const needle = "@" + handle
        let offset = text.indexOf(needle)
        while (offset >= 0) {
            const end = offset + needle.length
            const before = offset === 0 || !/[a-z0-9_.-]/.test(text.charAt(offset - 1))
            const after = end >= text.length || !/[a-z0-9_.-]/.test(text.charAt(end))
            if (before && after) return true
            offset = text.indexOf(needle, offset + 1)
        }
        return false
    }
    function belongsToCategory(row, category) {
        if (category === "all") return true
        if (category === "channels")
            return String(row.castChannel || "").length > 0
        if (category === "mention")
            return root.mentionsViewer(row)
        return false
    }
    readonly property var visibleRows: root.filterKind === "all" ? root.displayRows
        : root.asList(root.displayRows).filter(row => root.belongsToCategory(row, root.filterKind))

    function cloneNotificationRow(row) {
        const source = row || ({})
        // Copy the fields the surface renders explicitly. Bridge rows are
        // QVariantMap-backed snapshots; building a fresh plain object here lets
        // grouping mutate the copy without touching the shared bridge rows.
        const copy = {
            id: source.id,
            kind: source.kind,
            observedAtMs: source.observedAtMs,
            aggregateCount: source.aggregateCount,
            castId: source.castId,
            castAuthorFid: source.castAuthorFid,
            castParentId: source.castParentId,
            castText: source.castText,
            castReplyCount: source.castReplyCount,
            castRecastCount: source.castRecastCount,
            castLikeCount: source.castLikeCount,
            castEmbedCount: source.castEmbedCount,
            castEmbedUrl: source.castEmbedUrl,
            castChannel: source.castChannel,
            parentEmbedUrl: source.parentEmbedUrl,
            parentCastId: source.parentCastId,
            quotedCastId: source.quotedCastId,
            parentText: source.parentText,
            parentReplyCount: source.parentReplyCount,
            parentAuthorFid: source.parentAuthorFid,
            parentAuthor: source.parentAuthor,
            parentAuthorDisplayName: source.parentAuthorDisplayName,
            parentAvatarUrl: source.parentAvatarUrl,
            actorFid: source.actorFid,
            actor: source.actor,
            actorDisplayName: source.actorDisplayName,
            actorAvatarUrl: source.actorAvatarUrl,
            actors: root.asList(source.actors)
        }
        return copy
    }

    function mergeNotificationActors(existing, extra) {
        const result = root.asList(existing)
        const seen = {}
        for (let index = 0; index < result.length; ++index)
            seen[String(result[index] && result[index].fid || 0)] = true
        const additions = root.asList(extra)
        for (let index = 0; index < additions.length; ++index) {
            const actor = additions[index]
            if (!actor)
                continue
            const key = String(actor.fid || 0)
            if (seen[key])
                continue
            seen[key] = true
            result.push(actor)
        }
        return result
    }

    function actorsForMerge(row) {
        const actors = root.asList(row.actors)
        if (actors.length > 0)
            return actors
        if (Number(row.actorFid || 0) > 0 || String(row.actor || "").length > 0)
            return root.actorRows(row)
        return []
    }

    function mergeNotificationInto(target, row, counter) {
        if (!target || !row || !counter)
            return target
        const provided = Math.max(1, Number(row.aggregateCount || 1))
        if (provided > counter.provided)
            counter.provided = provided
        // Preserve the distinct provided actors, bounded for QML list memory.
        // Only the avatar viewport limits how many can appear at once.
        target.actors = root.mergeNotificationActors(target.actors, root.actorsForMerge(row)).slice(0, 32)
        const observed = Number(row.observedAtMs || 0)
        if (observed > Number(target.observedAtMs || 0))
            target.observedAtMs = observed
        if (target.actors.length > 0) {
            const first = target.actors[0]
            target.actorFid = Number(first.fid || 0)
            target.actor = String(first.username || "")
            target.actorDisplayName = String(first.displayName || first.username || "")
            target.actorAvatarUrl = String(first.avatarUrl || "")
        }
        return target
    }

    function finalizeNotificationGroup(row, counter) {
        if (!row || !counter)
            return
        const actorTotal = root.asList(row.actors).length
        const provided = Math.max(1, counter.provided || 1)
        row.aggregateCount = Math.max(1, actorTotal, provided)
    }

    function groupNotificationRows(source) {
        const rows = root.asList(source)
        const groups = []
        // Parallel to groups: groupable rows get a live counter object, every
        // other row a null placeholder, so counters stay index-aligned with the
        // groups array regardless of how replies/follows interleave.
        const counters = []
        const indexByKey = {}
        for (let index = 0; index < rows.length; ++index) {
            const row = rows[index]
            if (!row)
                continue
            const kind = String(row.kind || "")
            const castId = String(row.castId || "")
            const groupable = castId.length > 0 && (kind === "reaction" || kind === "recast")
            if (!groupable) {
                groups.push(row)
                counters.push(null)
                continue
            }
            const key = kind + "\u001f" + castId
            let targetIndex = indexByKey[key]
            if (targetIndex === undefined) {
                targetIndex = groups.length
                indexByKey[key] = targetIndex
                groups.push(root.cloneNotificationRow(row))
                counters.push({ provided: 0 })
            }
            root.mergeNotificationInto(groups[targetIndex], row, counters[targetIndex])
        }
        for (let index = 0; index < groups.length; ++index)
            root.finalizeNotificationGroup(groups[index], counters[index])
        return groups
    }

    // ---- Reply/mention target context --------------------------------------
    // A reply notification answers an earlier cast. The actor row shows who
    // acted; these helpers resolve *what/who was answered* so the hierarchy
    // reads "X replied to Y" instead of an isolated message. The provider may
    // supply the parent cast and author; for direct replies to the viewer the
    // account profile stands in as the target.
    function hasReplyContext(row) {
        if (String(row.kind || "") !== "reply" && String(row.kind || "") !== "mention")
            return false
        if (Number(row.parentAuthorFid || 0) > 0
                || String(row.parentAuthor || "").length > 0
                || String(row.parentAvatarUrl || "").length > 0)
            return true
        return String(row.kind || "") === "reply"
               && String(row.castParentId || "").length > 0
               && root.accountFid > 0
    }

    function hasParentContext(row) {
        return String(row.parentCastId || "").length > 0
               && (String(row.parentText || "").length > 0
                   || String(row.parentEmbedUrl || "").length > 0)
    }

    function replyTargetName(row) {
        const name = String(row.parentAuthorDisplayName || row.parentAuthor || "")
        if (name.length > 0)
            return name
        if (root.accountProfile) {
            const display = String(root.accountProfile.displayName || "")
            if (display.length > 0)
                return display
            const username = String(root.accountProfile.username || "")
            if (username.length > 0 && !/^fid:[0-9]+$/i.test(username))
                return username
        }
        return ""
    }

    function replyTargetAvatar(row) {
        if (String(row.parentAvatarUrl || "") !== "")
            return String(row.parentAvatarUrl)
        if (root.accountProfile)
            return String(root.accountProfile.avatarUrl || "")
        return ""
    }

    function replyTargetText(row) {
        const hasAuthor = Number(row.parentAuthorFid || 0) > 0
                          || String(row.parentAuthor || "").length > 0
                          || String(row.parentAvatarUrl || "").length > 0
        if (hasAuthor)
            return qsTrId("genycaster.cast.replying_to_value").arg(root.replyTargetName(row))
        return qsTrId("genycaster.notifications.in_reply_to_your_cast")
    }

    // Same stable-model + viewport-anchor mechanism as FeedSurface/ProfileSurface:
    // never hand the ListView a freshly allocated array, or it resets contentY to
    // the top whenever a refresh or load-more replaces the rows.
    function captureViewportAnchor() {
        if (root.restoringViewport || notificationModel.count === 0) return
        const probeY = Math.max(0, list.contentY) + 1
        const index = list.indexAt(Math.max(1, list.width / 2), probeY)
        if (index < 0 || index >= notificationModel.count) return
        const item = list.itemAtIndex(index)
        if (!item) return
        root.viewportAnchorKey = String(notificationModel.get(index).stableKey || "")
        root.viewportAnchorOffset = item.y - list.contentY
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
        if (list.moving || root.viewportAnchorKey.length === 0 || root.restoringViewport) return
        root.restoringViewport = true
        root.scheduleViewportRestore(root.viewportAnchorKey, root.viewportAnchorOffset, true)
    }

    function restoreViewportAnchor(anchorKey, anchorOffset) {
        if (anchorKey.length === 0 || notificationModel.count === 0)
            return
        let targetIndex = -1
        for (let index = 0; index < notificationModel.count; ++index) {
            if (String(notificationModel.get(index).stableKey) === anchorKey) {
                targetIndex = index
                break
            }
        }
        if (targetIndex < 0)
            return
        const item = list.itemAtIndex(targetIndex)
        if (!item)
            return
        const minimum = list.originY
        const maximum = Math.max(minimum, list.contentHeight - list.height)
        list.contentY = Math.max(minimum, Math.min(maximum, item.y - anchorOffset))
    }

    function payloadForStableKey(key) {
        return root.stablePayloads[String(key || "")] || ({})
    }

    function requestStableModelSync() {
        if (list.moving) {
            root.stableModelSyncPending = true
            return
        }
        root.stableModelSyncPending = false
        root.synchronizeStableModel()
    }

    function synchronizeStableModel() {
        const source = root.visibleRows || []
        let anchorKey = ""
        let anchorOffset = 0
        let structureChanged = false
        const probeY = Math.max(0, list.contentY) + 1
        const anchorIndex = list.indexAt(Math.max(1, list.width / 2), probeY)
        if (anchorIndex >= 0 && anchorIndex < notificationModel.count) {
            const anchorItem = list.itemAtIndex(anchorIndex)
            anchorKey = String(notificationModel.get(anchorIndex).stableKey)
            anchorOffset = anchorItem ? anchorItem.y - list.contentY : 0
        }

        for (let index = 0; index < source.length; ++index) {
            const key = root.notificationKey(source[index], index)
            const revision = root.notificationRevision(source[index])
            root.stablePayloads[key] = source[index]
            if (index < notificationModel.count
                    && String(notificationModel.get(index).stableKey) === key) {
                if (String(notificationModel.get(index).revisionKey || "") !== revision) {
                    notificationModel.setProperty(index, "revisionKey", revision)
                }
                continue
            }

            let existingIndex = -1
            for (let probe = index + 1; probe < notificationModel.count; ++probe) {
                if (String(notificationModel.get(probe).stableKey) === key) {
                    existingIndex = probe
                    break
                }
            }
            if (existingIndex >= 0) {
                notificationModel.move(existingIndex, index, 1)
                structureChanged = true
                if (String(notificationModel.get(index).revisionKey || "") !== revision) {
                    notificationModel.setProperty(index, "revisionKey", revision)
                }
            } else {
                notificationModel.insert(index, { stableKey: key, revisionKey: revision })
                structureChanged = true
            }
        }

        while (notificationModel.count > source.length) {
            notificationModel.remove(notificationModel.count - 1)
            structureChanged = true
        }

        if (structureChanged && anchorKey.length > 0)
            root.scheduleViewportRestore(anchorKey, anchorOffset, false)
    }

    // The pending callback belongs to this surface. A route/Loader destruction
    // destroys the timer, preventing a stale Qt.callLater from invoking null.
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

    // Queue model updates through an object-owned timer, not Qt.callLater on a
    // QML method reference. NotificationSurface can be destroyed while a
    // sidebar route changes; delayed callbacks must die with their owner.
    Timer {
        id: stableModelSyncTimer
        interval: 0
        repeat: false
        onTriggered: root.requestStableModelSync()
    }
    onRowsChanged: stableModelSyncTimer.restart()
    onFilterKindChanged: stableModelSyncTimer.restart()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            objectName: "notificationTitleRow"
            Layout.fillWidth: true
            Layout.leftMargin: root.gutter
            Layout.rightMargin: root.gutter
            Layout.topMargin: root.compact ? Theme.space3 : Theme.space4
            Layout.bottomMargin: Theme.space3
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.notifications.title")
                    color: Theme.textPrimary
                    font.pixelSize: root.compact ? Theme.typeTitle3Size : Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                }

                DirectionAwareLabel {
                    visible: !root.compact
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.notifications.subtitle")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeMetaSize
                    wrapMode: Text.Wrap
                }
            }

            ActionButton {
                compact: true
                quiet: true
                iconOnly: root.compact
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.profile.refresh")
                enabled: root.authenticated && !root.loadingNotifications
                onClicked: root.refreshRequested()
            }
        }

        // Provider-backed notification tabs: activity (all), channel-tagged
        // events and direct mentions/replies. Filter only actual fetched rows.
        RowLayout {
            objectName: "notificationFilterRow"
            visible: root.authenticated
            Layout.fillWidth: true
            Layout.leftMargin: root.gutter
            Layout.rightMargin: root.gutter
            Layout.bottomMargin: Theme.space3
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: [
                    { kind: "all", title: qsTrId("genycaster.notifications.filter_activity") },
                    { kind: "channels", title: qsTrId("genycaster.notifications.filter_channels") },
                    { kind: "mention", title: qsTrId("genycaster.notifications.filter_mentions") }
                ]
                delegate: Superellipse {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compact ? 36 : 40
                    fillColor: root.filterKind === modelData.kind
                               ? Theme.primarySurface : "transparent"
                    borderWidth: 1
                    borderColor: root.filterKind === modelData.kind
                                 ? Theme.primaryFill : Theme.borderSubtle
                    curveDepth: Theme.curveControl

                    DirectionAwareLabel {
                        anchors.centerIn: parent
                        text: modelData.title
                        color: root.filterKind === modelData.kind
                               ? Theme.accent : Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: root.filterKind === modelData.kind
                                     ? Theme.weightDemiBold : Theme.weightMedium
                    }
                    TapHandler {
                        onTapped: root.filterKind = modelData.kind
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        // The header and category tabs must never participate in the height
        // negotiation between empty, loading and list states. A single body
        // always consumes remaining space while those states overlap it.
        Item {
            id: notificationBody
            objectName: "notificationBody"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0

        Item {
            visible: root.showState
            anchors.fill: parent

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - Theme.space6, 420)
                spacing: Theme.space2

                Superellipse {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    fillColor: root.stateIsError ? Theme.warningSurface : Theme.primarySurface
                    curveDepth: Theme.curveControl

                    Icon {
                        anchors.centerIn: parent
                        text: root.stateIcon
                        color: root.stateIsError ? Theme.warning : Theme.accent
                        preferredSize: Theme.typeTitle3Size
                    }
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.stateTitle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.stateDescription
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeMetaSize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                ActionButton {
                    visible: !root.authenticated
                             || (root.stateIsError && !root.unsupportedRawNotifications)
                             || (root.authenticated && root.filterKind !== "all" && root.hasMore)
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.space2
                    prominent: !root.authenticated
                    quiet: root.authenticated
                    text: !root.authenticated
                          ? qsTrId("genycaster.notifications.sign_in")
                          : root.filterKind !== "all" && root.hasMore && !root.stateIsError
                            ? qsTrId("genycaster.notifications.load_more") : qsTrId("genycaster.empty.retry")
                    onClicked: {
                        if (!root.authenticated) root.signInRequested()
                        else if (root.filterKind !== "all" && root.hasMore && !root.stateIsError)
                            root.loadMoreRequested()
                        else root.refreshRequested()
                    }
                }
            }
        }

        Item {
            visible: root.authenticated && root.loadingNotifications && root.displayRows.length === 0
            anchors.fill: parent

            GenySpinner {
                anchors.centerIn: parent
                running: visible
            }
        }

        Timer {
            id: scrollWorkThrottle
            interval: 32
            repeat: false
            onTriggered: {
                if (!root.restoringViewport)
                    root.captureViewportAnchor()
            }
        }

        ListModel {
            id: notificationModel
        }

        ListView {
            id: list
            objectName: "notificationList"
            // Keep an interactive viewport even for an empty inbox, so a
            // top-edge pull can refresh a currently empty notification list.
            visible: root.authenticated
            // The empty/error actions remain above the transparent ListView.
            // Blank portions still pass drag input through for pull-to-refresh.
            z: root.showState ? -1 : 0
            anchors.fill: parent
            clip: true
            model: notificationModel
            spacing: 0
            leftMargin: root.compact ? 0 : root.gutter
            rightMargin: root.compact ? 0 : root.gutter
            topMargin: 0
            bottomMargin: Theme.space5
            cacheBuffer: Math.max(height, 800)
            reuseItems: true
            boundsBehavior: Flickable.DragOverBounds
            boundsMovement: Flickable.FollowBoundsBehavior
            ScrollBar.vertical: GenyScrollBar {}

            HoverHandler {
                id: notificationListPointer
                enabled: list.visible && Theme.neonEnabled && !list.moving
                blocking: false
            }

            // Overlay is viewport-owned and does not feed back into ListView's
            // footer/contentHeight calculations (the old footer caused a
            // height binding loop under fast pagination).
            GenySpinner {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.space2
                visible: root.loadingNotifications && root.hasMore
                running: visible
                z: 10
            }

            onContentYChanged: {
                if (!list.moving && !root.restoringViewport && !scrollWorkThrottle.running)
                    scrollWorkThrottle.start()
            }
            onMovementEnded: {
                scrollWorkThrottle.stop()
                if (root.stableModelSyncPending)
                    root.requestStableModelSync()
                root.captureViewportAnchor()
                if (!root.loadingNotifications && root.hasMore && contentY + height >= contentHeight - 260)
                    root.loadMoreRequested()
            }
            onContentHeightChanged: {
                if (!list.moving) {
                    root.stabilizeViewportAfterGeometryChange()
                    if (!root.loadingNotifications && root.hasMore && contentY + height >= contentHeight - 260)
                        root.loadMoreRequested()
                }
            }

            delegate: Item {
                id: notificationRow
                required property string stableKey
                required property string revisionKey
                readonly property var modelData: {
                    const revision = notificationRow.revisionKey
                    return root.payloadForStableKey(notificationRow.stableKey)
                }
                readonly property string kindKey: String(modelData.kind || "")
                readonly property var visibleActors: root.actorRows(notificationRow.modelData)
                readonly property int groupTotal: Math.max(visibleActors.length,
                    Number(modelData.aggregateCount || 1))
                readonly property bool groupedReaction:
                    (kindKey === "reaction" || kindKey === "recast") && groupTotal > 1
                readonly property bool latestActivity: root.visibleRows.length > 0
                    && notificationRow.stableKey === root.notificationKey(root.visibleRows[0], 0)
                readonly property color kindColor: root.kindColor(kindKey)
                readonly property string kindGlyph: root.kindGlyph(kindKey)
                width: list.width - list.leftMargin - list.rightMargin
                implicitHeight: content.implicitHeight + (root.compact ? 34 : 42)

                NeonSurface {
                    id: notificationCardSurface
                    anchors.fill: parent
                    interactive: !list.moving
                    proximityPointerActive: notificationListPointer.hovered && !list.moving
                    proximityPointerX: notificationCardSurface.mapFromItem(
                        list, notificationListPointer.point.position.x,
                        notificationListPointer.point.position.y).x
                    proximityPointerY: notificationCardSurface.mapFromItem(
                        list, notificationListPointer.point.position.x,
                        notificationListPointer.point.position.y).y
                    deferredNeon: true
                    // Notifications are real cards, not bare list rows.
                    // Keep their ambient rim while idle, but only for the
                    // currently visible neighborhood of a recycled ListView.
                    ambientNeon: notificationRow.visible && !list.moving
                        && notificationRow.y + notificationRow.height >= list.contentY - 160
                        && notificationRow.y <= list.contentY + list.height + 160
                    neonStrength: Theme.neonControlStrength
                    externalHovered: rowHover.hovered
                    externalPointerX: rowHover.point.position.x - x
                    externalPointerY: rowHover.point.position.y - y
                    anchors.leftMargin: root.compact ? Theme.space1 : Theme.space2
                    anchors.rightMargin: anchors.leftMargin
                    anchors.topMargin: Theme.space1
                    anchors.bottomMargin: Theme.space1
                    fillColor: rowTap.pressed ? Theme.surfacePressed
                               : rowHover.hovered ? Theme.surfaceHover
                               : notificationRow.latestActivity
                                 ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                           Theme.dark ? 0.11 : 0.055)
                                 : Theme.surfaceRaised
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: Theme.curveContainer
                }

                RowLayout {
                    id: content
                    anchors.fill: parent
                    anchors.leftMargin: root.gutter
                    anchors.rightMargin: root.gutter
                    anchors.topMargin: root.compact ? Theme.space3 : Theme.space4
                    anchors.bottomMargin: root.compact ? Theme.space3 : Theme.space4
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    // Single activities keep an actor portrait; group activity
                    // keeps its reaction symbol in the gutter and lays actors
                    // out above the title, without overlapping its text.
                    Item {
                        id: actorPortraitSlot
                        readonly property var firstActor: {
                            const actors = root.actorRows(notificationRow.modelData)
                            return actors.length > 0 ? actors[0] : ({})
                        }
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: root.compact ? 42 : 46
                        Layout.preferredHeight: root.compact ? 42 : 46

                        Avatar {
                            visible: !notificationRow.groupedReaction
                            anchors.left: parent.left
                            anchors.top: parent.top
                            avatarSize: root.compact ? 40 : 44
                            sourceUrl: String(parent.firstActor.avatarUrl || "")
                            displayName: String(parent.firstActor.displayName
                                                || parent.firstActor.username || "")
                        }

                        Superellipse {
                            visible: notificationRow.groupedReaction
                            anchors.centerIn: parent
                            width: root.compact ? 36 : 40
                            height: width
                            fillColor: root.kindTint(notificationRow.kindKey)
                            borderWidth: 0
                            curveDepth: 15
                            Icon {
                                anchors.centerIn: parent
                                text: notificationRow.kindGlyph
                                color: notificationRow.kindColor
                                preferredSize: 21
                            }
                        }

                        // A small reaction chip is useful on singular activities.
                        Superellipse {
                            visible: !notificationRow.groupedReaction
                            width: 19
                            height: 19
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            fillColor: Theme.backgroundPrimary
                            borderWidth: 1
                            borderColor: Theme.borderSubtle
                            curveDepth: 8
                            Icon {
                                anchors.centerIn: parent
                                text: notificationRow.kindGlyph
                                color: notificationRow.kindColor
                                preferredSize: 12
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        // Bounded horizontal actor stack: each portrait is
                        // distinct, fully visible and never overflows its own
                        // slot into the notification title or quoted cast.
                        // One inline actor strip per cast, with a truthful
                        // group count. Portraits are individually hit-testable.
                        Item {
                            id: actorStack
                            readonly property real avatarSpacing: root.compact ? 7 : 10
                            readonly property real avatarDiameter: root.compact ? 32 : 36
                            // Reserve a persistent trailing +N action. Showing
                            // every available avatar made it disappear exactly
                            // when the provider returned a complete group.
                            readonly property int maximumPreview: root.compact ? 4 : 7
                            readonly property int count: Math.min(
                                notificationRow.visibleActors.length,
                                Math.max(1, notificationRow.groupTotal - 1),
                                maximumPreview,
                                Math.max(1, Math.floor((width - Theme.scaleUi(76))
                                               / (avatarDiameter + avatarSpacing))))
                            visible: notificationRow.groupedReaction
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? avatarDiameter + Theme.space1 : 0

                            Repeater {
                                model: actorStack.visible ? actorStack.count : 0
                                delegate: Avatar {
                                    required property int index
                                    readonly property var actor: notificationRow.visibleActors[index] || ({})
                                    x: index * (actorStack.avatarDiameter + actorStack.avatarSpacing)
                                    y: 0
                                    avatarSize: actorStack.avatarDiameter
                                    sourceUrl: String(actor.avatarUrl || "")
                                    displayName: String(actor.displayName || actor.username || "")
                                    Accessible.role: Accessible.Button
                                    Accessible.name: String(actor.displayName || actor.username || "")
                                    Accessible.onPressAction: {
                                        const fid=Number(actor.fid || 0)
                                        if (fid > 0) root.profileRequested(fid)
                                    }
                                }
                            }
                            Superellipse {
                                id: actorCountBadge
                                readonly property int groupCount: notificationRow.groupTotal
                                readonly property int additionalCount: Math.max(0, groupCount - actorStack.count)
                                visible: additionalCount > 0
                                x: actorStack.count * (actorStack.avatarDiameter + actorStack.avatarSpacing)
                                y: 3
                                width: Math.max(34, actorCountText.implicitWidth + Theme.space2)
                                height: actorStack.avatarDiameter - 5
                                fillColor: Theme.surfaceSelected
                                borderWidth: 0
                                curveDepth: height / 2
                                DirectionAwareLabel {
                                    id: actorCountText
                                    anchors.centerIn: parent
                                    text: "+" + LocaleFormat.formatInteger(actorCountBadge.additionalCount)
                                    color: Theme.accent
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightBold
                                }
                                Accessible.role: Accessible.Button
                                Accessible.name: "+" + LocaleFormat.formatInteger(additionalCount)
                                                 + " " + qsTrId("genycaster.notifications.someone")
                                activeFocusOnTab: visible
                                Accessible.onPressAction: root.inspectActors(notificationRow.modelData)
                                Keys.onReturnPressed: root.inspectActors(notificationRow.modelData)
                                Keys.onSpacePressed: root.inspectActors(notificationRow.modelData)
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler {
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: root.inspectActors(notificationRow.modelData)
                                }
                            }
                        }

                        // Give the activity title the full column width. On
                        // narrow phones a trailing timestamp in the same row
                        // squeezed long aggregated names into three lines.
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.kindTitle(notificationRow.modelData)
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                            lineHeight: 1.22
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: (String(notificationRow.modelData.actor || "").length > 0
                                        ? "@" + String(notificationRow.modelData.actor || "").replace(/^@/, "")
                                        : "")
                                      + (String(notificationRow.modelData.castChannel || "").length > 0
                                         ? "  ·  #" + String(notificationRow.modelData.castChannel)
                                         : "")
                                color: Theme.textTertiary
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }

                            DirectionAwareLabel {
                                Layout.alignment: Qt.AlignVCenter
                                text: LocaleFormat.formatTimestamp(Number(notificationRow.modelData.observedAtMs || 0))
                                color: Theme.textTertiary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }

                        RowLayout {
                            id: replyTargetLine
                            visible: root.hasReplyContext(notificationRow.modelData)
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space1
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Avatar {
                                id: replyTargetAvatar
                                avatarSize: 18
                                sourceUrl: root.replyTargetAvatar(notificationRow.modelData)
                                displayName: root.replyTargetName(notificationRow.modelData)
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.replyTargetText(notificationRow.modelData)
                                color: Theme.textTertiary
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.Wrap
                            }
                        }

                        // Distinguish the quoted cast from the activity title
                        // using a quiet surface, not a full-height pink rail.
                        // A rail inside a RowLayout with fillHeight inherited
                        // the entire delegate height, crossing media and text.
                        // A compact quoted excerpt, not a second grey card
                        // occupying the full width of every notification.
                        Item {
                            id: castExcerpt
                            readonly property bool isEngagement: notificationRow.kindKey === "reaction"
                                                                 || notificationRow.kindKey === "recast"
                            visible: String(notificationRow.modelData.castText || "").length > 0
                                     || notificationRow.kindKey === "mention"
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space1
                            implicitHeight: excerptText.implicitHeight + Theme.space1 * 2

                            RowLayout {
                                anchors.fill: parent
                                spacing: Theme.space2
                                layoutDirection: Theme.uiDirection

                                Superellipse {
                                    Layout.preferredWidth: 2
                                    Layout.preferredHeight: Math.max(20, excerptText.implicitHeight)
                                    fillColor: Theme.accent
                                    curveDepth: Theme.curveCompact
                                }

                                Icon {
                                    Layout.alignment: Qt.AlignTop
                                    text: Theme.iconQuote
                                    visible: notificationRow.kindKey === "quote"
                                             || castExcerpt.isEngagement
                                    color: Theme.inkSubtle
                                    preferredSize: 12
                                }

                                DirectionAwareLabel {
                                    id: excerptText
                                    Layout.fillWidth: true
                                    text: String(notificationRow.modelData.castText || "").length > 0
                                          ? String(notificationRow.modelData.castText)
                                          : qsTrId("genycaster.notifications.content_unavailable")
                                    color: castExcerpt.isEngagement ? Theme.textSecondary : Theme.textPrimary
                                    font.pixelSize: Theme.typeMetaSize
                                    wrapMode: Text.Wrap
                                    maximumLineCount: castExcerpt.isEngagement ? 3 : 7
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        // Media is part of the cast, not merely an embed
                        // count. Preserve a bounded picture and tappable link.
                        SuperellipseImage {
                            readonly property string mediaUrl: root.notificationImageUrl(
                                notificationRow.modelData.castEmbedUrl)
                            visible: mediaUrl.length > 0
                            Layout.alignment: Qt.AlignLeft
                            Layout.preferredWidth: visible ? Math.min(240, notificationRow.width - 100) : 0
                            Layout.preferredHeight: visible ? 150 : 0
                            source: visible ? linkPreviewResolver.imageSourceForUrl(mediaUrl) : ""
                            mediaCategory: "photos"
                            requestedSourceWidth: 480
                            requestedSourceHeight: 300
                            fillMode: Image.PreserveAspectCrop
                            curveDepth: Theme.curveCompact
                            borderWidth: 0
                        }

                        // Notifications should show an actionable reference,
                        // not reserve a 172px media/link card for a URL whose
                        // metadata often has no thumbnail. Keep the full link
                        // discoverable without pushing the next activity away.
                        LinkPreview {
                            id: notificationLink
                            readonly property string linkUrl: String(
                                notificationRow.modelData.castEmbedUrl || "")
                            visible: root.notificationLinkHost(linkUrl).length > 0
                                     && root.notificationImageUrl(linkUrl).length === 0
                            Layout.fillWidth: true
                            embed: ({ kind: "external", url: linkUrl })
                            compact: true
                            embeddedInCard: true
                            suppressArtwork: true
                            onOpenRequested: url => Qt.openUrlExternally(url)
                        }

                        Superellipse {
                            id: parentContextCard
                            visible: root.hasParentContext(notificationRow.modelData)
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space1
                            implicitHeight: parentContextContent.implicitHeight + Theme.space2 * 2
                            fillColor: Theme.surfaceMuted
                            borderWidth: Theme.strokeHairline
                            borderColor: Theme.borderSubtle
                            curveDepth: Theme.curveCompact

                            RowLayout {
                                id: parentContextContent
                                anchors.fill: parent
                                anchors.leftMargin: Theme.space2
                                anchors.rightMargin: Theme.space3
                                anchors.topMargin: Theme.space2
                                anchors.bottomMargin: Theme.space2
                                spacing: Theme.space2
                                layoutDirection: Theme.uiDirection

                                Avatar {
                                    avatarSize: 20
                                    sourceUrl: root.replyTargetAvatar(notificationRow.modelData)
                                    displayName: root.replyTargetName(notificationRow.modelData)
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space1

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: root.replyTargetName(notificationRow.modelData)
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.typeCaptionSize
                                        font.weight: Theme.weightDemiBold
                                        wrapMode: Text.Wrap
                                    }

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: String(notificationRow.modelData.parentText || "")
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.typeMetaSize
                                        maximumLineCount: 4
                                        wrapMode: Text.Wrap
                                        elide: Text.ElideRight
                                    }

                                    SuperellipseImage {
                                        readonly property string quotedMediaUrl: root.notificationImageUrl(
                                            notificationRow.modelData.parentEmbedUrl)
                                        visible: quotedMediaUrl.length > 0
                                        Layout.alignment: Qt.AlignLeft
                                        Layout.preferredWidth: visible ? 132 : 0
                                        Layout.preferredHeight: visible ? 96 : 0
                                        source: visible ? linkPreviewResolver.imageSourceForUrl(quotedMediaUrl) : ""
                                        mediaCategory: "photos"
                                        requestedSourceWidth: 264
                                        requestedSourceHeight: 192
                                        fillMode: Image.PreserveAspectCrop
                                        curveDepth: Theme.curveCompact
                                        borderWidth: 0
                                    }
                                }
                            }

                            TapHandler {
                                enabled: String(notificationRow.modelData.parentCastId || "").length > 0
                                gesturePolicy: TapHandler.ReleaseWithinBounds
                                onTapped: root.castRequested(String(notificationRow.modelData.parentCastId || ""))
                            }
                        }

                        // Quote events can arrive before their referenced cast
                        // is hydrated. Keep the quoted target reachable rather
                        // than rendering an inert generic placeholder.
                        RowLayout {
                            visible: notificationRow.kindKey === "quote"
                                     && String(notificationRow.modelData.quotedCastId || "").length > 0
                                     && !root.hasParentContext(notificationRow.modelData)
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space1
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Icon {
                                text: Theme.iconQuote
                                color: Theme.accent
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.embed.view_quoted_cast_next")
                                color: Theme.accent
                                font.pixelSize: Theme.typeMetaSize
                                elide: Text.ElideRight
                            }
                            Icon {
                                text: Theme.iconArrowRight
                                color: Theme.inkSubtle
                                preferredSize: Theme.iconSizeSmall
                            }
                            TapHandler {
                                gesturePolicy: TapHandler.ReleaseWithinBounds
                                onTapped: root.castRequested(String(notificationRow.modelData.quotedCastId || ""))
                            }
                        }

                        RowLayout {
                            visible: String(notificationRow.modelData.castId || "").length > 0
                                     && (Number(notificationRow.modelData.castReplyCount || 0) > 0
                                         || Number(notificationRow.modelData.castRecastCount || 0) > 0
                                         || Number(notificationRow.modelData.castLikeCount || 0) > 0
                                         || Number(notificationRow.modelData.castEmbedCount || 0) > 0)
                            Layout.fillWidth: true
                            spacing: Theme.space3
                            layoutDirection: Theme.uiDirection

                            RowLayout {
                                visible: Number(notificationRow.modelData.castReplyCount || 0) > 0
                                spacing: 3
                                layoutDirection: Theme.uiDirection
                                Icon { text: Theme.iconReply; color: Theme.accent; preferredSize: 11 }
                                DirectionAwareLabel {
                                    text: LocaleFormat.formatInteger(Number(notificationRow.modelData.castReplyCount || 0))
                                    color: Theme.textTertiary
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }

                            RowLayout {
                                visible: Number(notificationRow.modelData.castRecastCount || 0) > 0
                                spacing: 3
                                layoutDirection: Theme.uiDirection
                                Icon { text: Theme.iconRepost; color: Theme.accent; preferredSize: 11 }
                                DirectionAwareLabel {
                                    text: LocaleFormat.formatInteger(Number(notificationRow.modelData.castRecastCount || 0))
                                    color: Theme.textTertiary
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }

                            RowLayout {
                                visible: Number(notificationRow.modelData.castLikeCount || 0) > 0
                                spacing: 3
                                layoutDirection: Theme.uiDirection
                                Icon { text: Theme.iconHeart; color: Theme.accent; preferredSize: 11 }
                                DirectionAwareLabel {
                                    text: LocaleFormat.formatInteger(Number(notificationRow.modelData.castLikeCount || 0))
                                    color: Theme.textTertiary
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }

                            RowLayout {
                                visible: Number(notificationRow.modelData.castEmbedCount || 0) > 0
                                spacing: 3
                                layoutDirection: Theme.uiDirection
                                Icon { text: Theme.iconImage; color: Theme.textTertiary; preferredSize: 11 }
                                DirectionAwareLabel {
                                    text: LocaleFormat.formatInteger(Number(notificationRow.modelData.castEmbedCount || 0))
                                    color: Theme.textTertiary
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                    }

                    Icon {
                        visible: String(notificationRow.modelData.castId || "").length > 0
                        text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                        color: Theme.textTertiary
                        preferredSize: Theme.iconSizeSmall
                    }
                }

                HoverHandler {
                    id: rowHover
                    enabled: !list.moving
                    cursorShape: String(notificationRow.modelData.castId || "").length > 0
                                 || notificationRow.kindKey === "follow"
                                 ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
                TapHandler {
                    id: rowTap
                    enabled: String(notificationRow.modelData.castId || "").length > 0
                             || (notificationRow.kindKey === "follow"
                                 && Number(notificationRow.modelData.actorFid || 0) > 0)
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: point => {
                        const pos = point.position
                        const actorFid = Number(notificationRow.modelData.actorFid || 0)
                        // The left portrait always opens that person's profile.
                        const portrait = actorPortraitSlot.mapFromItem(
                            notificationRow, pos.x, pos.y)
                        if (actorFid > 0 && !actorStack.visible
                                && portrait.x >= 0 && portrait.y >= 0
                                && portrait.x < actorPortraitSlot.width
                                && portrait.y < actorPortraitSlot.height) {
                            root.profileRequested(actorFid)
                            return
                        }
                        if (actorStack.visible) {
                            const badgePoint = actorCountBadge.mapFromItem(notificationRow, pos.x, pos.y)
                            if (actorCountBadge.visible && badgePoint.x >= 0
                                    && badgePoint.y >= 0 && badgePoint.x < actorCountBadge.width
                                    && badgePoint.y < actorCountBadge.height) {
                                root.inspectActors(notificationRow.modelData)
                                return
                            }
                            const local = actorStack.mapFromItem(notificationRow, pos.x, pos.y)
                            if (local.y >= 0 && local.y < actorStack.avatarDiameter
                                    && local.x >= 0) {
                                const stride = actorStack.avatarDiameter + actorStack.avatarSpacing
                                const index = Math.floor(local.x / stride)
                                if (index < actorStack.count
                                        && local.x - index * stride < actorStack.avatarDiameter) {
                                    const actor = notificationRow.visibleActors[index] || ({})
                                    const fid = Number(actor.fid || 0)
                                    if (fid > 0) {
                                        root.profileRequested(fid)
                                        return
                                    }
                                }
                            }
                        }
                        const castId = String(notificationRow.modelData.castId || "")
                        if (castId.length > 0) root.castRequested(castId)
                        else root.profileRequested(Number(notificationRow.modelData.actorFid || 0))
                    }
                }
            }

            // Keep pagination loading outside ListView's managed footer.
            // A footer whose geometry participates in contentHeight can
            // recursively invalidate the view's own height during model sync.
            // bottomMargin reserves room for the overlay indicator.
            footer: null
        }
            PullToRefresh {
                flickable: list
                busy: root.loadingNotifications
                refreshEnabled: root.authenticated && list.visible
                onRefreshRequested: root.refreshRequested()
            }
        } // notificationBody
    }


    Popup {
        id: reactionActorsPopup
        objectName: "notificationReactionActorsPopup"
        parent: Overlay.overlay
        popupType: Popup.Item
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: parent ? Math.min(Theme.scaleUi(430), parent.width - Theme.space4 * 2) : Theme.scaleUi(390)
        height: parent ? Math.min(parent.height - Theme.space5 * 2,
            Theme.scaleUi(145) + Math.min(8, root.expandedActors.length) * Theme.scaleUi(56)
            + (root.expandedActorCount > root.expandedActors.length ? Theme.scaleUi(42) : 0))
            : Theme.scaleUi(410)
        x: parent ? Math.max(Theme.space2, (parent.width - width) / 2) : 0
        y: parent ? Math.max(Theme.space2, (parent.height - height) / 2) : 0
        padding: Theme.space3
        background: PopupSurface { curveDepth: Theme.curveModal }
        contentItem: ColumnLayout {
            spacing: Theme.space3
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                Icon {
                    text: root.expandedActorKind === "reaction" ? Theme.iconHeart : Theme.iconRepost
                    color: Theme.accent
                    preferredSize: Theme.iconSize
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: LocaleFormat.formatInteger(root.expandedActorCount)
                          + " · " + qsTrId("genycaster.notifications.activity")
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                    color: Theme.textPrimary
                }
                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.settings.done")
                    onClicked: reactionActorsPopup.close()
                }
            }
            ListView {
                id: reactionActorsList
                objectName: "notificationReactionActorsList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.space2
                model: root.expandedActors
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: GenyScrollBar {}
                delegate: Item {
                    id: actorEntry
                    required property var modelData
                    width: reactionActorsList.width
                    height: Theme.scaleUi(55)
                    RowLayout {
                        anchors.fill: parent
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space3
                        Avatar {
                            avatarSize: 40
                            sourceUrl: String(actorEntry.modelData.avatarUrl || "")
                            displayName: String(actorEntry.modelData.displayName || actorEntry.modelData.username || "")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 0
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(actorEntry.modelData.displayName
                                             || actorEntry.modelData.username || actorEntry.modelData.fid || "")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeLabelSize
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                readonly property string actualHandle: String(actorEntry.modelData.username || "").trim().replace(/^@/, "")
                                visible: actualHandle.length > 0 && !/^fid:[0-9]+$/i.test(actualHandle)
                                text: visible ? "@" + actualHandle : ""
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }
                        Icon {
                            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                            color: Theme.textTertiary
                            preferredSize: Theme.iconSizeSmall
                        }
                    }
                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: {
                            const fid = Number(actorEntry.modelData.fid || 0)
                            if (fid > 0) {
                                reactionActorsPopup.close()
                                root.profileRequested(fid)
                            }
                        }
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: root.expandedActorCount > root.expandedActors.length
                text: qsTrId("genycaster.notifications.content_unavailable")
                      + " · " + LocaleFormat.formatInteger(
                          root.expandedActorCount - root.expandedActors.length)
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.textSecondary
                wrapMode: Text.Wrap
            }
        }
    }

    ScrollToTopButton {
        flickable: list
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.compact ? Theme.mobileGutter : Theme.space5
        anchors.bottomMargin: root.compact ? Theme.space2 + 12 : Theme.space4
    }
}
