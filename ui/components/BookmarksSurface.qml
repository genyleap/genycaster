import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: root
    property var rows: []
    property bool compact: false
    property bool authenticated: false
    property bool refreshBusy: false
    property string accountError: ""
    property int pendingCount: 0
    property string sortMode: "priority"
    signal castRequested(string castId)
    signal removeRequested(string castId)
    signal priorityRequested(string castId, int priority)
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
            model: root.sortedRows
            delegate: Superellipse {
                id: bookmarkCard
                required property var modelData
                Layout.fillWidth: true
                // The saved cast card is one navigation target. Icon buttons
                // below retain their own action and must not navigate.
                TapHandler {
                    objectName: "bookmarkCardOpenHandler"
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: root.castRequested(String(bookmarkCard.modelData.castId || ""))
                }
                implicitHeight: savedContent.implicitHeight + Theme.space2 * 2
                fillColor: Theme.surfaceRaised
                borderColor: Theme.borderSubtle
                borderWidth: Theme.strokeHairline
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: savedContent
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        Avatar {
                            visible: Number(modelData.authorFid || 0) > 0
                            avatarSize: 35
                            sourceUrl: String(modelData.avatarUrl || "")
                            displayName: String(modelData.displayName || modelData.author || "")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.displayName || modelData.author || "")
                                      || qsTrId("genycaster.bookmarks.saved_cast")
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.username || modelData.author || "")
                                      || String(modelData.castId || "").slice(0, 12) + "…"
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                                LayoutMirroring.enabled: false
                            }
                        }
                    }

                    DirectionAwareLabel {
                        visible: String(modelData.text || "").length > 0
                        Layout.fillWidth: true
                        text: String(modelData.text || "")
                        color: Theme.ink
                        font.pixelSize: Theme.typeBodySize
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        wrapMode: Text.Wrap
                    }
                    ActionButton {
                        visible: String(modelData.text || "").length === 0
                        Layout.alignment: Qt.AlignLeft
                        quiet: true
                        compact: true
                        iconGlyph: Theme.iconArrowUpRight
                        text: qsTrId("genycaster.bookmarks.open_saved_cast")
                        onClicked: root.castRequested(String(modelData.castId || ""))
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space1
                        Item { Layout.fillWidth: true }
                        ActionButton {
                            objectName: "bookmarkPriorityMobileButton"
                            iconOnly: true
                            compact: true
                            quiet: true
                            selected: false
                            customIconColor: Number(modelData.priority || 0) > 0
                                             ? Theme.warning : Theme.inkMuted
                            iconGlyph: Theme.iconSpark
                            text: Number(modelData.priority || 0) > 0
                                  ? qsTrId("genycaster.bookmarks.unpin")
                                  : qsTrId("genycaster.bookmarks.pin")
                            onClicked: root.priorityRequested(String(modelData.castId || ""),
                                Number(modelData.priority || 0) > 0 ? 0 : 1)
                        }
                        ActionButton {
                            objectName: "bookmarkRemoveMobileButton"
                            iconOnly: true
                            compact: true
                            quiet: true
                            destructive: true
                            iconGlyph: Theme.iconTrash
                            text: qsTrId("genycaster.bookmarks.remove")
                            onClicked: root.removeRequested(String(modelData.castId || ""))
                        }
                    }
                }
            }
        }
    }
}
