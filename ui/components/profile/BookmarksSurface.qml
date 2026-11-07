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
    property var collections: []
    property var collectionItems: []
    property var membershipItems: []
    property string selectedCollection: ""
    property bool collectionsBusy: false
    property string collectionsError: ""
    property string collectionEditorMode: ""
    property string collectionEditorId: ""
    property string collectionEditorRevision: ""
    property string collectionEditorName: ""
    readonly property var collectionChoices: {
        const result = [{ id: "", name: qsTrId("genycaster.filters.all_bookmarks") }]
        const source = root.collections || []
        for(let i=0;i<Number(source.length || 0);++i) {
            const item=source[i] || ({})
            if(item.id && item.name)result.push({id:String(item.id),name:String(item.name)})
        }
        return result
    }
    readonly property var collectionChoiceLabels: root.collectionChoices.map(item=>item.name)
    function savedCollectionIndex(hash) {
        const key=String(hash || "").toLowerCase()
        for (const membership of root.membershipItems || []) {
            if (String(membership.cast_hash || "").toLowerCase() !== key) continue
            const index=root.collectionChoices.findIndex(entry=>
                String(entry.id || "") === String(membership.collection_id || ""))
            if (index > 0) return index
        }
        return 0
    }
    readonly property var activeCollectionEntry: {
        const items=root.collections || []
        for(let i=0;i<Number(items.length || 0);++i)
            if(String(items[i].id || "")===root.selectedCollection)return items[i]
        return null
    }
    function editCollection(mode, item) {
        root.collectionEditorMode=mode
        root.collectionEditorId=String(item && item.id || "")
        root.collectionEditorRevision=String(item && item.revision || "")
        root.collectionEditorName=mode==="create" ? "" : String(item && item.name || "")
    }
    signal collectionSelected(string id)
    signal collectionCreateRequested(string name)
    signal collectionRenameRequested(string id, string name, string revision)
    signal collectionDeleteRequested(string id, string revision)
    signal membershipRequested(string castHash, string collectionId, bool remove)
    signal collectionsRefreshRequested()
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
        if(!root.selectedCollection)return unique
        const memberships=new Set()
        const stored=root.collectionItems || []
        for(let i=0;i<Number(stored.length || 0);++i) {
            const item=stored[i] || ({})
            memberships.add(String(item.cast_hash || "").toLowerCase())
        }
        return unique.filter(item=>memberships.has(String(item.castId || "").toLowerCase()))
    }
    // Only read the visible portion of the list; scroll and account updates
    // schedule a bounded, cancellable provider read by canonical cast hash.
    function hydrateVisible() {
        if (!root.authenticated) return
        const requested = []
        for (let i = 0; i < root.sortedRows.length; ++i) {
            const card = savedCastRepeater.itemAt(i)
            // Cards live inside an adaptive GridLayout. Their local y is a
            // grid-relative coordinate, not the scroll content coordinate.
            const cardY = card ? card.mapToItem(root.contentItem, 0, 0).y : 0
            if (!card || cardY + card.height < root.contentY - root.height * 0.5
                    || cardY > root.contentY + root.height * 1.5)
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

    ContentFilterSheet {
        id: bookmarkFilterSheet
        heading: qsTrId("genycaster.filters.bookmarks")
        sections: [
            {id:"sort",title:qsTrId("genycaster.filters.sort"),value:root.sortMode,defaultValue:"priority",
             options:[{id:"priority",label:qsTrId("genycaster.bookmarks.sort_priority")},
                      {id:"newest",label:qsTrId("genycaster.bookmarks.sort_newest")},
                      {id:"oldest",label:qsTrId("genycaster.bookmarks.sort_oldest")}]},
            {id:"collection",title:qsTrId("genycaster.filters.collections"),value:root.selectedCollection,defaultValue:"",
             options:root.collectionChoices.map(entry=>({id:entry.id,label:entry.name}))}
        ]
        onApplied: values => {
            root.sortMode = ["priority","newest","oldest"].includes(values.sort)
                ? String(values.sort) : "priority"
            if (String(values.collection || "") !== root.selectedCollection)
                root.collectionSelected(String(values.collection || ""))
        }
    }

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
            ActionButton {
                visible: true
                objectName: "mobileBookmarkFilterTrigger"
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: "filter"
                text: qsTrId("genycaster.filters.bookmarks")
                onClicked: bookmarkFilterSheet.open()
            }
            ActionButton {
                visible: root.compact
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconPlus
                text: qsTrId("genycaster.bookmarks.new_collection")
                enabled: root.authenticated && !root.collectionsBusy
                onClicked: root.editCollection("create",null)
            }
            ActionButton {
                visible: root.compact
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.bookmarks.refresh")
                busy: root.refreshBusy || root.collectionsBusy
                enabled: root.authenticated && !root.refreshBusy && !root.collectionsBusy
                onClicked: { root.refreshRequested(); root.collectionsRefreshRequested() }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.bookmarks.description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeMetaSize
                wrapMode: Text.Wrap
            }
            RowLayout {
                visible: false
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                GenyComboBox {
                    id: bookmarkSortPicker
                    objectName: "bookmarkSortPicker"
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: root.compact ? 144 : 184
                    Layout.maximumWidth: root.compact ? 144 : 184
                    model: [qsTrId("genycaster.bookmarks.sort_priority"), qsTrId("genycaster.bookmarks.sort_newest"), qsTrId("genycaster.bookmarks.sort_oldest")]
                    currentIndex: root.sortMode === "oldest" ? 2
                                : root.sortMode === "newest" ? 1 : 0
                    onActivated: index => root.sortMode =
                        index === 2 ? "oldest" : index === 1 ? "newest" : "priority"
                }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
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
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space2
            DirectionAwareLabel {
                visible: !root.compact
                Layout.fillWidth: true
                text: qsTrId("genycaster.filters.collections")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeMetaSize
            }
            RowLayout {
                visible: false
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                GenyComboBox {
                    id: collectionPicker
                    objectName: "bookmarkCollectionPicker"
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: root.compact ? 168 : 200
                    Layout.maximumWidth: root.compact ? 168 : 240
                    model: root.collectionChoiceLabels
                    currentIndex: Math.max(0,root.collectionChoices.findIndex(
                        entry=>entry.id===root.selectedCollection))
                    enabled: root.authenticated && !root.collectionsBusy
                    onActivated: index=>root.collectionSelected(root.collectionChoices[index].id)
                }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: root.compact
                    iconGlyph: Theme.iconPlus
                    text: qsTrId("genycaster.bookmarks.new_collection")
                    enabled: root.authenticated && !root.collectionsBusy
                    onClicked: root.editCollection("create",null)
                }
                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconRefresh
                    text: qsTrId("genycaster.bookmarks.refresh_collections")
                    busy: root.collectionsBusy
                    enabled: root.authenticated && !root.collectionsBusy
                    onClicked: root.collectionsRefreshRequested()
                }
            }
        }

        DirectionAwareLabel {
            visible: root.compact
            Layout.fillWidth: true
            text: (root.activeCollectionEntry ? String(root.activeCollectionEntry.name)
                   : qsTrId("genycaster.filters.all_bookmarks"))
                  + " · " + (root.sortMode === "priority" ? qsTrId("genycaster.bookmarks.sort_priority")
                          : root.sortMode === "oldest" ? qsTrId("genycaster.bookmarks.sort_oldest")
                                                      : qsTrId("genycaster.bookmarks.sort_newest"))
            color: Theme.textSecondary
            font.pixelSize: Theme.typeMetaSize
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.activeCollectionEntry !== null
            spacing: Theme.space2
            ActionButton {
                compact: true
                quiet: true
                text: qsTrId("genycaster.bookmarks.rename")
                Layout.fillWidth: root.compact
                enabled: !root.collectionsBusy
                onClicked: root.editCollection("rename",root.activeCollectionEntry)
            }
            ActionButton {
                compact: true
                quiet: true
                destructive: true
                text: root.compact ? qsTrId("genycaster.bookmarks.delete")
                                   : qsTrId("genycaster.bookmarks.delete_collection")
                enabled: !root.collectionsBusy
                onClicked: root.editCollection("delete",root.activeCollectionEntry)
            }
            Item { Layout.fillWidth: true }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: root.collectionEditorMode.length>0
            spacing: Theme.space2
            DirectionAwareTextField {
                id: collectionNameEditor
                visible: root.collectionEditorMode!=="delete"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.collectionEditorName
                placeholderText: qsTrId("genycaster.bookmarks.collection_name")
                onTextChanged: root.collectionEditorName=text
                maximumLength: 80
            }
            DirectionAwareLabel {
                visible: root.collectionEditorMode==="delete"
                Layout.fillWidth: true
                text: qsTrId("genycaster.bookmarks.delete_confirmation")
                wrapMode: Text.Wrap
                color: Theme.textSecondary
            }
            ActionButton {
                compact: true
                prominent: true
                text: root.collectionEditorMode==="delete"
                    ? qsTrId("genycaster.bookmarks.confirm_delete")
                    : qsTrId("genycaster.bookmarks.save")
                enabled: !root.collectionsBusy
                         && (root.collectionEditorMode==="delete"
                             || root.collectionEditorName.trim().length>0)
                onClicked: {
                    if(root.collectionEditorMode==="create")
                        root.collectionCreateRequested(root.collectionEditorName.trim())
                    else if(root.collectionEditorMode==="rename")
                        root.collectionRenameRequested(root.collectionEditorId,
                            root.collectionEditorName.trim(),root.collectionEditorRevision)
                    else root.collectionDeleteRequested(root.collectionEditorId,
                            root.collectionEditorRevision)
                    root.collectionEditorMode=""
                }
            }
            ActionButton {
                compact: true
                quiet: true
                text: qsTrId("genycaster.bookmarks.cancel")
                onClicked: root.collectionEditorMode=""
            }
        }
        DirectionAwareLabel {
            visible: root.collectionsError.length>0
            Layout.fillWidth: true
            text: root.collectionsError
            color: Theme.negative
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        DirectionAwareLabel {
            objectName: "bookmarkAccountStatus"
            visible: root.authenticated && (root.accountError.length > 0 || root.pendingCount > 0)
            Layout.fillWidth: true
            color: root.accountError.length > 0 ? Theme.negative : Theme.inkMuted
            text: root.accountError.length > 0 ? root.accountError
                  : qsTrId("genycaster.bookmarks.pending").arg(LocaleFormat.formatInteger(root.pendingCount))
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

        GridLayout {
            id: savedCastsGrid
            objectName: "desktopBookmarkGrid"
            Layout.fillWidth: true
            // Qt Quick Layouts must not derive a delegate's preferred width
            // from its own GridLayout.width: the parent measures the children
            // first, producing a circular dependency and zero-width cards.
            // Use the outer Flickable's already-constrained viewport instead.
            readonly property real gridAvailableWidth: Math.max(0,
                root.width - 2 * (root.compact ? Theme.mobileGutter : Theme.contentGutter))
            readonly property int columnCount: root.compact || gridAvailableWidth < 660
                ? 1 : gridAvailableWidth < 1680 ? 2 : 3
            readonly property real cardWidth: Math.min(520, Math.max(0,
                (gridAvailableWidth - (columnCount - 1) * Theme.space3) / columnCount))
            columns: columnCount
            columnSpacing: Theme.space3
            rowSpacing: Theme.space3

            Repeater {
            id: savedCastRepeater
            model: root.sortedRows
            delegate: Superellipse {
                id: bookmarkCard
                required property var modelData
                // Stable explicit cell geometry, measured from the viewport,
                // not from the grid's own derived width.
                Layout.fillWidth: false
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                Layout.margins: 0
                Layout.minimumWidth: savedCastsGrid.cardWidth
                Layout.preferredWidth: savedCastsGrid.cardWidth
                Layout.maximumWidth: savedCastsGrid.cardWidth
                Layout.preferredHeight: implicitHeight
                implicitHeight: Math.max(110,
                    savedContent.implicitHeight + Theme.space3 * 2)
                fillColor: Theme.surfaceRaised
                borderColor: Theme.borderSubtle
                borderWidth: Theme.strokeHairline
                curveDepth: Theme.curveContainer
                // Keep delayed media textures and date labels inside their
                // own rounded bookmark card, even during WASM layout updates.
                clip: true

                // Render bookmarked content through the exact same rich cast
                // presentation as feed/profile, including parent context,
                // images, videos, link cards and reaction controls.
                ColumnLayout {
                    id: savedContent
                    anchors.fill: parent
                    anchors.margins: Theme.space3
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

                    // A bookmark can exist on another device before its
                    // canonical cast is fetched. Keep navigation available
                    // even if its provider is offline or returns no preview.
                    ActionButton {
                        visible: castSkeletonFallback.visible
                        Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                        compact: true
                        quiet: true
                        iconGlyph: Theme.iconArrowUpRight
                        text: qsTrId("genycaster.bookmarks.open_saved_cast")
                        onClicked: root.castRequested(
                            String(bookmarkCard.modelData.castId || ""))
                    }

                    CastItem {
                        id: savedCast
                        objectName: "bookmarkRichCast"
                        visible: root.compact && !Boolean(castSkeletonFallback.visible)
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? Math.max(1, implicitHeight) : 0
                        modelData: bookmarkCard.modelData
                        // Dense desktop columns use the same compact cast
                        // spacing as mobile without removing rich content.
                        compact: root.compact || savedCastsGrid.columns >= 3
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

                    // Desktop bookmarks are an index into the saved casts, not
                    // a second full feed. Keep each card bounded and open the
                    // complete rich cast in its dedicated detail surface.
                    ColumnLayout {
                        objectName: "desktopBookmarkSummary"
                        visible: !root.compact && !castSkeletonFallback.visible
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: Theme.space2

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Avatar {
                                avatarSize: 36
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                sourceUrl: String(bookmarkCard.modelData.avatarUrl || "")
                                displayName: String(bookmarkCard.modelData.displayName
                                                    || bookmarkCard.modelData.author || "")
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 0
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(bookmarkCard.modelData.displayName
                                                 || bookmarkCard.modelData.author || "")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeLabelSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: "@" + String(bookmarkCard.modelData.username
                                                      || bookmarkCard.modelData.author || "")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                    elide: Text.ElideRight
                                }
                            }
                            DirectionAwareLabel {
                                visible: Number(bookmarkCard.modelData.createdAtMs || 0) > 0
                                text: LocaleFormat.formatRelativeTime(
                                    Number(bookmarkCard.modelData.createdAtMs || 0))
                                color: Theme.textTertiary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }
                        DirectionAwareLabel {
                            objectName: "desktopBookmarkExcerpt"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            visible: text.length > 0
                            text: String(bookmarkCard.modelData.text || "")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection
                            Icon {
                                visible: Number(bookmarkCard.modelData.embedCount || 0) > 0
                                text: Theme.iconImage
                                color: Theme.textSecondary
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                visible: Number(bookmarkCard.modelData.embedCount || 0) > 0
                                text: LocaleFormat.formatInteger(
                                    Number(bookmarkCard.modelData.embedCount || 0))
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                            ActionButton {
                                objectName: "desktopBookmarkOpenCast"
                                quiet: true
                                compact: true
                                iconGlyph: Theme.iconArrowRight
                                text: qsTrId("genycaster.bookmarks.open_saved_cast")
                                onClicked: root.castRequested(
                                    String(bookmarkCard.modelData.castId || ""))
                            }
                        }
                    }

                    RowLayout {
                        id: bookmarkActions
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space1
                        GenyComboBox {
                            id: addToCollectionPicker
                            objectName: "bookmarkAssignCollectionPicker"
                            visible: root.selectedCollection.length===0
                                     && root.collectionChoices.length>1
                            Layout.fillWidth: true
                            Layout.maximumWidth: root.compact ? 176 : 248
                            model: [qsTrId("genycaster.bookmarks.add_to_collection")].concat(
                                root.collectionChoices.slice(1).map(item=>item.name))
                            currentIndex: root.savedCollectionIndex(
                                String(bookmarkCard.modelData.castId || ""))
                            enabled: !root.collectionsBusy
                            onActivated: index => {
                                if (index > 0)
                                    root.membershipRequested(
                                        String(bookmarkCard.modelData.castId || ""),
                                        root.collectionChoices[index].id,false)
                            }
                        }
                        ActionButton {
                            visible: root.selectedCollection.length>0
                            compact: true
                            quiet: true
                            destructive: true
                            text: qsTrId("genycaster.bookmarks.remove_from_collection")
                            enabled: !root.collectionsBusy
                            onClicked: root.membershipRequested(
                                String(bookmarkCard.modelData.castId || ""),
                                root.selectedCollection,true)
                        }
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
}
