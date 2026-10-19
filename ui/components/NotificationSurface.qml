import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var rows: []
    // Imperatively refreshed by AppShell. Do not bind this state to the
    // same bridge notification signal that also changes rows and pagination.
    property bool loadingNotifications: false
    property bool hasMore: false
    property string errorText: ""
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
                                         : root.stateIsError
                                           ? qsTrId("genycaster.notifications.could_not_refresh")
                                           : root.filterKind !== "all" ? qsTrId("genycaster.notifications.filtered_empty")
                                           : qsTrId("genycaster.notifications.empty_title")
    readonly property string stateDescription: !root.authenticated
                                               ? qsTrId("genycaster.notifications.sign_in_description")
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
        switch (String(kind || "")) {
        case "reply": return Theme.notificationReply
        case "mention": return Theme.notificationMention
        case "reaction": return Theme.notificationReaction
        case "recast": return Theme.notificationRecast
        case "follow": return Theme.notificationFollow
        }
        return Theme.notificationOther
    }

    function kindGlyph(kind) {
        switch (String(kind || "")) {
        case "reply": return Theme.iconReply
        case "mention": return Theme.iconMention
        case "reaction": return Theme.iconHeart
        case "recast": return Theme.iconRepost
        case "follow": return Theme.iconUser
        }
        return Theme.iconNotification
    }

    function notificationImageUrl(value) {
        const url = String(value || "")
        const path = url.split("?")[0].split("#")[0].toLowerCase()
        return url.startsWith("https://") && [".png", ".jpg", ".jpeg", ".webp", ".gif", ".avif"].some(ext => path.endsWith(ext)) ? url : ""
    }

    function kindTint(kind) {
        const base = root.kindColor(kind)
        return Qt.rgba(base.r, base.g, base.b, Theme.dark ? 0.17 : 0.13)
    }

    function notificationKey(row, index) {
        const value = row || ({})
        const id = String(value.id || value.castId || "")
        return id.length > 0 ? "notif:" + id : "notif-index:" + index
    }

    function notificationRevision(row) {
        const value = row || ({})
        return [String(value.kind || ""), String(value.observedAtMs || 0),
                String(value.aggregateCount || 0), String(value.castId || ""),
                String(value.castText || ""), String(value.actorFid || 0),
                String(value.castParentId || ""), String(value.parentCastId || ""),
                String(value.parentText || ""), String(value.parentAvatarUrl || ""),
                String(value.castEmbedUrl || ""), String(value.parentEmbedUrl || ""),
                String(value.castChannel || "")].join("\u001f")
    }

    // ---- Farcaster-style aggregation ---------------------------------------
    // The read provider often returns one row per actor ("X reacted to your
    // cast" three times for three reactors). Grouping collates reaction and
    // recast rows that share the same target cast into a single row whose
    // leading avatar stack shows the distinct actors and whose title reports
    // the total, mirroring Farcaster's "X and N others liked your cast".
    // Replies, mentions and follows stay one row each: every reply is a
    // distinct cast with its own content, and each follow is its own event.
    readonly property var displayRows: root.groupNotificationRows(root.rows)
    readonly property var visibleRows: root.filterKind === "all" ? root.displayRows
        : root.asList(root.displayRows).filter(row =>
            root.filterKind === "reactions"
            ? String(row.kind || "") === "reaction" || String(row.kind || "") === "recast"
            : String(row.kind || "") === root.filterKind)

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
        const kind = String(target.kind || "")
        if (kind === "reaction") {
            const likes = Number(row.castLikeCount || 0)
            if (likes > counter.engagement)
                counter.engagement = likes
        } else if (kind === "recast") {
            const recasts = Number(row.castRecastCount || 0)
            if (recasts > counter.engagement)
                counter.engagement = recasts
        }
        const provided = Math.max(1, Number(row.aggregateCount || 1))
        if (provided > counter.provided)
            counter.provided = provided
        // Keep at most 8 faces; anything beyond that is summarized by the title.
        target.actors = root.mergeNotificationActors(target.actors, root.actorsForMerge(row)).slice(0, 8)
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
        const engagement = counter.engagement || 0
        row.aggregateCount = Math.max(1, actorTotal, provided, engagement)
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
                counters.push({ provided: 0, engagement: 0 })
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
            if (username.length > 0)
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

    function stabilizeViewportAfterGeometryChange() {
        if (list.moving || root.viewportAnchorKey.length === 0 || root.restoringViewport) return
        root.restoringViewport = true
        const key = root.viewportAnchorKey
        const offset = root.viewportAnchorOffset
        Qt.callLater(function() {
            root.restoreViewportAnchor(key, offset)
            root.restoringViewport = false
            root.captureViewportAnchor()
        })
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
            Qt.callLater(function() { root.restoreViewportAnchor(anchorKey, anchorOffset) })
    }

    onRowsChanged: Qt.callLater(root.requestStableModelSync)
    onFilterKindChanged: Qt.callLater(root.requestStableModelSync)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
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

        // These filters operate on the actual, grouped notifications.
        // Never render non-functional "Channels" pills without channel events.
        RowLayout {
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
                    { kind: "reply", title: qsTrId("genycaster.notifications.filter_replies") },
                    { kind: "mention", title: qsTrId("genycaster.notifications.filter_mentions") },
                    { kind: "reactions", title: qsTrId("genycaster.notifications.filter_reactions") }
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

        Item {
            visible: root.showState
            Layout.fillWidth: true
            Layout.fillHeight: true

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
                    visible: !root.authenticated || root.stateIsError
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
            Layout.fillWidth: true
            Layout.fillHeight: true

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
            visible: root.authenticated && root.visibleRows.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: notificationModel
            spacing: 0
            leftMargin: root.compact ? 0 : root.gutter
            rightMargin: root.compact ? 0 : root.gutter
            topMargin: 0
            bottomMargin: Theme.space5
            cacheBuffer: Math.max(height, 800)
            reuseItems: true
            ScrollBar.vertical: GenyScrollBar {}

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
                readonly property color kindColor: root.kindColor(kindKey)
                readonly property string kindGlyph: root.kindGlyph(kindKey)
                width: list.width - list.leftMargin - list.rightMargin
                implicitHeight: content.implicitHeight + (root.compact ? 28 : 36)

                Rectangle {
                    anchors.fill: parent
                    color: rowTap.pressed ? Theme.surfacePressed
                           : rowHover.hovered ? Theme.surfaceHover : "transparent"
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.divider
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

                    Item {
                        readonly property var visibleActors: root.actorRows(notificationRow.modelData)
                        Layout.preferredWidth: visibleActors.length > 1 ? (root.compact ? 52 : 60) : (root.compact ? 44 : 48)
                        Layout.preferredHeight: root.compact ? 44 : 48

                        Repeater {
                            model: parent.visibleActors
                            delegate: Avatar {
                                required property var modelData
                                required property int index
                                readonly property int actorIndex: index
                                avatarSize: root.compact ? 42 : 46
                                x: Math.min(actorIndex, 2) * (root.compact ? 9 : 10)
                                y: Math.min(actorIndex, 2) * 2
                                sourceUrl: String(modelData.avatarUrl || "")
                                displayName: String(modelData.displayName || modelData.username || "")
                                z: 3 - Math.min(actorIndex, 2)

                                TapHandler {
                                    enabled: Number(modelData.fid || 0) > 0
                                    onTapped: root.profileRequested(Number(modelData.fid || 0))
                                }
                            }
                        }

                        // Keep the notification category visible without a
                        // circular badge behind the actual profile picture.
                        Icon {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.rightMargin: -2
                            anchors.bottomMargin: -2
                            text: notificationRow.kindGlyph
                            color: notificationRow.kindColor
                            preferredSize: root.compact ? 15 : 17
                            z: 6
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.kindTitle(notificationRow.modelData)
                                color: Theme.textPrimary
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                                lineHeight: 1.3
                                wrapMode: Text.Wrap
                            }

                            DirectionAwareLabel {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 2
                                text: LocaleFormat.formatTimestamp(Number(notificationRow.modelData.observedAtMs || 0))
                                color: Theme.textTertiary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }

                        DirectionAwareLabel {
                            visible: String(notificationRow.modelData.actor || "").length > 0
                                     || String(notificationRow.modelData.castChannel || "").length > 0
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

                        // Replies/mentions are messages, not previews of
                        // messages. Show the full body inline, as social
                        // activity timelines do. Reactions quote the source.
                        RowLayout {
                            visible: String(notificationRow.modelData.castText || "").length > 0
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space2
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Rectangle {
                                visible: notificationRow.kindKey === "reaction"
                                         || notificationRow.kindKey === "recast"
                                Layout.preferredWidth: visible ? 3 : 0
                                Layout.fillHeight: true
                                color: notificationRow.kindColor
                                radius: 1
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(notificationRow.modelData.castText || "")
                                color: notificationRow.kindKey === "reaction"
                                       || notificationRow.kindKey === "recast"
                                       ? Theme.textSecondary : Theme.textPrimary
                                font.pixelSize: Theme.typeBodySize
                                wrapMode: Text.Wrap
                                // Never turn an actual reply into "tipped..."
                                // merely because it exceeded three lines.
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

                        // Do not instantiate a metadata-heavy preview for
                        // notifications without a genuine external link.
                        Loader {
                            id: notificationLinkLoader
                            readonly property string linkUrl: String(
                                notificationRow.modelData.castEmbedUrl || "")
                            active: linkUrl.length > 0
                                    && root.notificationImageUrl(linkUrl).length === 0
                            visible: active
                            Layout.fillWidth: true
                            Layout.preferredHeight: active && item ? item.implicitHeight : 0
                            sourceComponent: LinkPreview {
                                embed: ({ url: notificationLinkLoader.linkUrl })
                                compact: root.compact
                                scrollHost: list
                            }
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
                                Icon { text: Theme.iconReply; color: Theme.notificationReply; preferredSize: 11 }
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
                                Icon { text: Theme.iconRepost; color: Theme.notificationRecast; preferredSize: 11 }
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
                                Icon { text: Theme.iconHeart; color: Theme.notificationReaction; preferredSize: 11 }
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
                    onTapped: {
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
    }
}
