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
                text: qsTr("Your saved casts, available on your signed-in devices")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeMetaSize
                wrapMode: Text.Wrap
            }
            GenyComboBox {
                id: bookmarkSortPicker
                objectName: "bookmarkSortPicker"
                Layout.preferredWidth: root.compact ? 116 : 160
                model: [qsTr("Priority"), qsTr("Newest"), qsTr("Oldest")]
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
                text: qsTr("Refresh saved casts")
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
                  : qsTr("%1 saved cast changes are being added to your account.").arg(root.pendingCount)
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
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: savedContent.implicitHeight + Theme.space3 * 2
                fillColor: Theme.surfaceRaised
                borderColor: Theme.borderSubtle
                borderWidth: Theme.strokeHairline
                curveDepth: Theme.curveContainer

                ColumnLayout {
                    id: savedContent
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        Avatar {
                            avatarSize: 35
                            sourceUrl: String(modelData.avatarUrl || "")
                            displayName: String(modelData.displayName || modelData.author || "")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.displayName || modelData.author || "Farcaster")
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: String(modelData.author || modelData.username || "")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }
                        ActionButton {
                            objectName: "bookmarkPriorityButton"
                            compact: true
                            quiet: true
                            iconOnly: true
                            iconGlyph: Theme.iconLocation
                            text: Number(modelData.priority || 0) > 0
                                  ? qsTr("Remove priority") : qsTr("Prioritize bookmark")
                            onClicked: root.priorityRequested(String(modelData.castId || ""),
                                Number(modelData.priority || 0) > 0 ? 0 : 1)
                        }
                        ActionButton {
                            objectName: "bookmarkRemoveButton"
                            compact: true
                            quiet: true
                            iconOnly: true
                            iconGlyph: Theme.iconTrash
                            text: qsTr("Remove bookmark")
                            onClicked: root.removeRequested(String(modelData.castId || ""))
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: String(modelData.text || "") || qsTr("Cast details unavailable. Tap to open.")
                        color: Theme.ink
                        font.pixelSize: Theme.typeBodySize
                        maximumLineCount: 5
                        elide: Text.ElideRight
                        wrapMode: Text.Wrap
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: root.castRequested(String(modelData.castId || ""))
                        }
                    }
                }
            }
        }
    }
}
