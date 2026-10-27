import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: root
    property var rows: []
    property bool compact: false
    property bool authenticated: false
    property bool interactionsAvailable: false
    property double accountFid: 0
    property bool refreshBusy: false
    property string accountError: ""
    property int pendingCount: 0
    property string sortMode: "priority"
    signal castRequested(string castId)
    signal removeRequested(string castId)
    signal priorityRequested(string castId, int priority)
    signal hydrationRequested(var castIds)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal removeCastRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal refreshRequested()
    signal authenticationRequested()

    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight + Theme.space5 * 2
    boundsBehavior: Flickable.DragOverBounds
    boundsMovement: Flickable.FollowBoundsBehavior
    readonly property var sortedRows: {
        // QML exposes C++ QVariantList as a sequence, which is not always a
        // JavaScript Array. Array.isArray() can silently discard every saved
        // cast even though the native account model contains bookmarks.
        const sequence = root.rows || []
        const items = []
        for (let i = 0; i < Number(sequence.length || 0); ++i)
            items.push(sequence[i])
        // Sorting never collapses distinct Farcaster hashes into one item.
        const seen = new Set()
        const unique = items.filter(item => {
            const id = String(item.castId || "").toLowerCase()
            if (!/^0x[0-9a-f]{40}$/.test(id) || seen.has(id)) return false
            seen.add(id)
            return true
        })
        unique.sort((a, b) => {
            if (root.sortMode === "priority") {
                const byPriority = Number(b.priority || 0) - Number(a.priority || 0)
                if (byPriority !== 0) return byPriority
            }
            const timeA = Number(a.savedAtMs || a.createdAtMs || 0)
            const timeB = Number(b.savedAtMs || b.createdAtMs || 0)
            return root.sortMode === "oldest" ? timeA - timeB : timeB - timeA
        })
        return unique
    }
    // Only read the visible portion of the list; scroll and account updates
    // schedule a bounded, cancellable provider read by canonical cast hash.
    function hydrateVisible() {
        if (!root.authenticated) return
        const requested = []
        for (let i = 0; i < root.sortedRows.length; ++i) {
            const card = savedCastRepeater.itemAt(i)
            if (!card || card.y + card.height < root.contentY - root.height * 0.5
                    || card.y > root.contentY + root.height * 1.5)
                continue
            const item = root.sortedRows[i]
            if (!item || Boolean(item.richHydrated)) continue
            const hash = String(item.castId || "")
            if (/^0x[0-9a-fA-F]{40}$/.test(hash)) requested.push(hash)
            if (requested.length >= 8) break
        }
        if (requested.length > 0) root.hydrationRequested(requested)
    }
    Timer {
        id: hydrationTimer
        interval: 260
        repeat: false
        onTriggered: root.hydrateVisible()
    }
    Component.onCompleted: hydrationTimer.start()
    onContentYChanged: hydrationTimer.restart()
    onSortedRowsChanged: hydrationTimer.restart()
    onHeightChanged: hydrationTimer.restart()

    PullToRefresh {
        flickable: root
        busy: root.refreshBusy
        refreshEnabled: root.authenticated
        onRefreshRequested: root.refreshRequested()
    }

    ColumnLayout {
        id: column
        x: root.compact ? Theme.mobileGutter : Theme.contentGutter
        width: Math.max(0, root.width - 2 * x)
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space3
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            Icon { text: Theme.iconBookmark; color: Theme.accent; preferredSize: Theme.typeTitle2Size }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.side_nav.bookmarks")
                color: Theme.ink
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightBold
            }
            Label {
                text: LocaleFormat.formatInteger(root.sortedRows.length)
                color: Theme.inkMuted
                font.pixelSize: Theme.typeMetaSize
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.bookmarks.description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeMetaSize
                wrapMode: Text.Wrap
            }
            GenyComboBox {
                id: bookmarkSortPicker
                objectName: "bookmarkSortPicker"
                Layout.preferredWidth: root.compact ? 116 : 160
                model: [qsTrId("genycaster.bookmarks.sort_priority"), qsTrId("genycaster.bookmarks.sort_newest"), qsTrId("genycaster.bookmarks.sort_oldest")]
                currentIndex: root.sortMode === "oldest" ? 2
                            : root.sortMode === "newest" ? 1 : 0
                onActivated: index => root.sortMode =
                    index === 2 ? "oldest" : index === 1 ? "newest" : "priority"
            }
            ActionButton {
                iconOnly: true
                compact: true
                quiet: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.bookmarks.refresh")
                busy: root.refreshBusy
                enabled: root.authenticated && !root.refreshBusy
                onClicked: root.refreshRequested()
            }
        }

        DirectionAwareLabel {
            objectName: "bookmarkAccountStatus"
            visible: root.authenticated && (root.accountError.length > 0 || root.pendingCount > 0)
            Layout.fillWidth: true
            color: root.accountError.length > 0 ? Theme.negative : Theme.inkMuted
            text: root.accountError.length > 0 ? root.accountError
                  : qsTrId("genycaster.bookmarks.pending").arg(root.pendingCount)
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        DirectionAwareLabel {
            visible: !root.authenticated || root.sortedRows.length === 0
            Layout.fillWidth: true
            Layout.topMargin: Theme.space6
            text: !root.authenticated
                  ? qsTrId("genycaster.notifications.sign_in_required")
                  : qsTrId("genycaster.empty.nothing_here_yet")
            color: Theme.inkMuted
            font.pixelSize: Theme.typeBodySize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Repeater {
            id: savedCastRepeater
            model: root.sortedRows
            delegate: Superellipse {
                id: bookmarkCard
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: savedContent.implicitHeight + Theme.space2 * 2
                fillColor: Theme.surfaceRaised
                borderColor: Theme.borderSubtle
                borderWidth: Theme.strokeHairline
                curveDepth: Theme.curveContainer

                // Render bookmarked content through the exact same rich cast
                // presentation as feed/profile, including parent context,
                // images, videos, link cards and reaction controls.
                ColumnLayout {
                    id: savedContent
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    spacing: Theme.space1

                    CastSkeleton {
                        id: castSkeletonFallback
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        visible: !Boolean(bookmarkCard.modelData.authorFid)
                                 && !String(bookmarkCard.modelData.text || "").length
                                 && !Boolean(bookmarkCard.modelData.richHydrated)
                        animate: visible && !root.moving
                    }

                    CastItem {
                        id: savedCast
                        objectName: "bookmarkRichCast"
                        visible: !Boolean(castSkeletonFallback.visible)
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? implicitHeight : 0
                        modelData: bookmarkCard.modelData
                        compact: root.compact
                        interactionsAvailable: root.interactionsAvailable
                        accountFid: root.accountFid
                        enrichmentActive: bookmarkCard.visible && !root.moving
                        onOpenRequested: castId => root.castRequested(castId)
                        onProfileRequested: fid => root.profileRequested(fid)
                        onReplyRequested: cast => root.replyRequested(cast)
                        onQuoteRequested: cast => root.quoteRequested(cast)
                        onReactionRequested: (kind, cast, active) =>
                            root.reactionRequested(kind, cast, active)
                        onRemoveRequested: cast => root.removeCastRequested(cast)
                        onExternalRequested: url => root.externalRequested(url)
                        onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
                        onAuthenticationRequested: root.authenticationRequested()
                    }

                    RowLayout {
                        id: bookmarkActions
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space1
                        Item { Layout.fillWidth: true }
                        ActionButton {
                            objectName: "bookmarkPriorityMobileButton"
                            iconOnly: true
                            compact: true
                            quiet: true
                            selected: Number(bookmarkCard.modelData.priority || 0) > 0
                            customIconColor: selected ? Theme.warning : Theme.inkMuted
                            iconGlyph: Theme.iconPriorityStar
                            text: selected ? qsTrId("genycaster.bookmarks.unpin")
                                           : qsTrId("genycaster.bookmarks.pin")
                            onClicked: root.priorityRequested(
                                String(bookmarkCard.modelData.castId || ""),
                                selected ? 0 : 1)
                        }
                        ActionButton {
                            objectName: "bookmarkRemoveMobileButton"
                            iconOnly: true
                            compact: true
                            quiet: true
                            destructive: true
                            iconGlyph: Theme.iconTrash
                            text: qsTrId("genycaster.bookmarks.remove")
                            onClicked: root.removeRequested(
                                String(bookmarkCard.modelData.castId || ""))
                        }
                    }
                }
            }
        }
    }
}
