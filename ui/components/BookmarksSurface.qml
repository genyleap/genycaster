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
                text: "New collection"
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
                    text: "New collection"
                    enabled: root.authenticated && !root.collectionsBusy
                    onClicked: root.editCollection("create",null)
                }
                ActionButton {
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconRefresh
                    text: "Refresh collections"
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
                text: "Rename"
                Layout.fillWidth: root.compact
                enabled: !root.collectionsBusy
                onClicked: root.editCollection("rename",root.activeCollectionEntry)
            }
            ActionButton {
                compact: true
                quiet: true
                destructive: true
                text: root.compact ? "Delete" : "Delete collection"
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
                placeholderText: "Collection name"
                onTextChanged: root.collectionEditorName=text
                maximumLength: 80
            }
            DirectionAwareLabel {
                visible: root.collectionEditorMode==="delete"
                Layout.fillWidth: true
                text: "Delete this collection? Saved casts will remain in Bookmarks."
                wrapMode: Text.Wrap
                color: Theme.textSecondary
            }
            ActionButton {
                compact: true
                prominent: true
                text: root.collectionEditorMode==="delete"?"Confirm delete":"Save"
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
                text: "Cancel"
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
                        GenyComboBox {
                            id: addToCollectionPicker
                            objectName: "bookmarkAssignCollectionPicker"
                            visible: root.selectedCollection.length===0
                                     && root.collectionChoices.length>1
                            Layout.fillWidth: true
                            Layout.maximumWidth: root.compact ? 176 : 248
                            model: ["Add to collection…"].concat(
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
                            text: "Remove from collection"
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
