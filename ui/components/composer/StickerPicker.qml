import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Farcaster.Client
import "StickerCatalog.js" as StickerCatalog

// Telegram-style transparent sticker grid: anchored to the actual trigger on
// desktop, a bottom sheet in compact layouts. Images remain at Njal's IPFS
// origin. No unlicensed artwork is redistributed with the application.
Popup {
    id: root
    objectName: "stickerPicker"

    signal stickerSelected(var sticker)

    property Item anchorItem: null
    property bool canAdd: true
    property string category: "all"
    property string searchQuery: ""

    readonly property bool compact: parent !== null && parent.width < Theme.breakpointMobile
    readonly property real safeMargin: Theme.space3
    readonly property var favorites: StickerCatalog.decodeIds(stickerPrefs.favoritesJson, 32)
    readonly property var recents: StickerCatalog.decodeIds(stickerPrefs.recentsJson, 12)
    readonly property var visibleStickers: StickerCatalog.filtered(
        root.category, root.searchQuery, root.favorites, root.recents)

    Settings {
        id: stickerPrefs
        category: "genycaster-stickers-v1"
        property string favoritesJson: "[]"
        property string recentsJson: "[]"
    }

    modal: root.compact
    focus: true
    dim: root.compact
    padding: Theme.space3
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? Math.max(1, Math.min(root.compact ? 460 : 444,
        parent.width - root.safeMargin * 2)) : 444
    height: parent ? Math.max(1, Math.min(root.compact ? 564 : 518,
        parent.height - root.safeMargin * 2)) : 518
    background: PopupSurface {}

    Overlay.modal: DrawerBackdrop {
        sourceItem: root.parent && root.parent !== Overlay.overlay ? root.parent : null
        snapshotBlurOnBrowser: true
        active: root.visible && root.compact
        addScrim: true
        blockInput: true
        onBackgroundTapped: root.close()
    }

    function reposition() {
        if (!root.parent || root.parent.width <= 0 || root.parent.height <= 0)
            return
        const outerWidth = root.parent.width
        const outerHeight = root.parent.height
        const leftBound = root.safeMargin
        const topBound = root.safeMargin
        const maxX = Math.max(leftBound, outerWidth - root.width - root.safeMargin)
        const maxY = Math.max(topBound, outerHeight - root.height - root.safeMargin)

        if (root.compact) {
            root.x = Math.round(Math.max(leftBound, (outerWidth - root.width) / 2))
            root.y = Math.round(maxY)
            return
        }
        if (root.anchorItem !== null && root.anchorItem.visible) {
            // The caller supplies the actual ActionButton, never the full
            // formatting toolbar or the large editor surface. Map directly
            // into the popup overlay and clamp both coordinates.
            const above = root.anchorItem.mapToItem(root.parent, 0, 0)
            const below = root.anchorItem.mapToItem(root.parent, 0, root.anchorItem.height)
            const preferredX = Theme.rtl
                ? below.x + root.anchorItem.width - root.width : below.x
            const desiredY = below.y + Theme.space2
            const oppositeY = above.y - root.height - Theme.space2
            root.x = Math.round(Math.max(leftBound, Math.min(maxX, preferredX)))
            root.y = Math.round(Math.max(topBound, Math.min(maxY,
                desiredY + root.height <= outerHeight - topBound ? desiredY : oppositeY)))
            return
        }
        root.x = Math.round(Math.max(leftBound, (outerWidth - root.width) / 2))
        root.y = Math.round(Math.max(topBound, (outerHeight - root.height) / 2))
    }

    function isFavorite(id) {
        return root.favorites.indexOf(id) >= 0
    }

    function toggleFavorite(id) {
        stickerPrefs.favoritesJson = JSON.stringify(
            StickerCatalog.toggleFavorite(root.favorites, id))
    }

    function chooseSticker(id) {
        const sticker = StickerCatalog.itemForId(id)
        if (!root.canAdd || !sticker)
            return
        root.stickerSelected(sticker)
        // Record only after the Composer confirms protocol-media insertion.
    }

    function recordUsage(id) {
        stickerPrefs.recentsJson = JSON.stringify(StickerCatalog.recordRecent(root.recents, id))
    }

    function categoryLabel(id) {
        switch (id) {
        case "all": return qsTrId("genycaster.stickers.all")
        case "reactions": return qsTrId("genycaster.stickers.reactions")
        case "web3": return qsTrId("genycaster.stickers.web3")
        case "favorites": return qsTrId("genycaster.stickers.favorites")
        case "recent": return qsTrId("genycaster.stickers.recent")
        }
        return ""
    }

    onAboutToShow: {
        root.category = "all"
        root.searchQuery = ""
        searchField.text = ""
        root.reposition()
    }
    onOpened: {
        // Layout, popup implicit dimensions and overlay geometry settle only
        // after opening. Avoid the previous stale anchor coordinates.
        Qt.callLater(root.reposition)
        stickerGrid.forceActiveFocus()
    }
    onWidthChanged: if (opened) root.reposition()
    onHeightChanged: if (opened) root.reposition()
    onAnchorItemChanged: if (opened) root.reposition()
    onCompactChanged: if (opened) root.reposition()
    Connections {
        target: root.parent
        function onWidthChanged() { if (root.opened) root.reposition() }
        function onHeightChanged() { if (root.opened) root.reposition() }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stickers.title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stickers.subtitle") + " · "
                          + LocaleFormat.formatInteger(StickerCatalog.stickers.length)
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
            ActionButton {
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.shell.close")
                onClicked: root.close()
            }
        }

        DirectionAwareTextField {
            id: searchField
            objectName: "stickerSearch"
            Layout.fillWidth: true
            placeholderText: qsTrId("genycaster.stickers.search")
            onTextChanged: root.searchQuery = text
        }

        Flickable {
            id: packs
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.compactControlHeight + Theme.space1
            contentWidth: Math.max(width, packRow.implicitWidth)
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            RowLayout {
                id: packRow
                width: implicitWidth
                height: packs.height
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection
                Repeater {
                    model: ["all", "reactions", "web3", "favorites", "recent"]
                    delegate: ActionButton {
                        required property string modelData
                        compact: true
                        quiet: true
                        selected: root.category === modelData
                        text: root.categoryLabel(modelData)
                        onClicked: {
                            root.category = modelData
                            stickerGrid.positionViewAtBeginning()
                        }
                    }
                }
            }
        }

        GridView {
            id: stickerGrid
            objectName: "stickerGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            focus: true
            interactive: true
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: width > 0 ? width / Math.max(2, Math.floor(width / 100)) : 100
            cellHeight: cellWidth + Theme.space1
            model: root.visibleStickers
            cacheBuffer: cellHeight

            delegate: Item {
                id: tile
                required property int index
                required property var modelData
                width: stickerGrid.cellWidth
                height: stickerGrid.cellHeight
                readonly property bool favorite: root.isFavorite(modelData.id)

                Superellipse {
                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    curveDepth: Theme.curveControl
                    fillColor: stickerHit.pressed ? Theme.primarySurface
                        : stickerHit.containsMouse ? Theme.surfaceHover : "transparent"
                    borderWidth: 0
                    Behavior on fillColor {
                        enabled: !Theme.reduceMotion
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                Image {
                    id: thumbnail
                    objectName: "stickerThumbnail"
                    anchors.centerIn: parent
                    width: Math.min(tile.width - Theme.space3 * 2,
                        tile.height - Theme.space3 * 2)
                    height: width
                    source: StickerCatalog.previewUrl(tile.modelData.id)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                    cache: true
                }

                Icon {
                    anchors.centerIn: parent
                    visible: thumbnail.status === Image.Loading
                          || thumbnail.status === Image.Error
                    text: thumbnail.status === Image.Error
                        ? Theme.iconWarning : Theme.iconSticker
                    color: Theme.inkMuted
                    preferredSize: Theme.iconSize
                    opacity: 0.54
                }

                MouseArea {
                    id: stickerHit
                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    hoverEnabled: !root.compact
                    enabled: root.canAdd && thumbnail.status === Image.Ready
                    cursorShape: Qt.PointingHandCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTrId("genycaster.stickers.add") + ": " + tile.modelData.title
                    onClicked: root.chooseSticker(tile.modelData.id)
                }

                ActionButton {
                    z: 2
                    visible: tile.favorite || root.compact || stickerHit.containsMouse
                    anchors.top: parent.top
                    anchors.right: parent.right
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconBookmark
                    selected: tile.favorite
                    text: tile.favorite ? qsTrId("genycaster.stickers.remove_favorite")
                                        : qsTrId("genycaster.stickers.add_favorite")
                    onClicked: root.toggleFavorite(tile.modelData.id)
                }
            }

            Keys.onReturnPressed: event => {
                if (currentIndex >= 0 && currentIndex < root.visibleStickers.length) {
                    root.chooseSticker(root.visibleStickers[currentIndex].id)
                    event.accepted = true
                }
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.visibleStickers.length === 0
            text: qsTrId("genycaster.stickers.empty")
            color: Theme.inkMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            ActionButton {
                compact: true
                quiet: true
                iconGlyph: Theme.iconArrowUpRight
                text: qsTrId("genycaster.stickers.originals")
                onClicked: presentationBridge.openExternalUrl(StickerCatalog.publisherUrl)
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.canAdd ? qsTrId("genycaster.stickers.media_slot")
                    : qsTrId("genycaster.stickers.limit")
                color: root.canAdd ? Theme.inkMuted : Theme.warning
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }
    }
}
