import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Farcaster.Client
import "StickerCatalog.js" as StickerCatalog

// Shared desktop popover / phone bottom sheet. Preview artwork is bundled in
// the QML module; publication uses only first-party, immutable HTTPS PNG URLs.
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
    readonly property var favorites: StickerCatalog.decodeIds(stickerPrefs.favoritesJson, 12)
    readonly property var recents: StickerCatalog.decodeIds(stickerPrefs.recentsJson, 12)
    readonly property var visibleStickers: StickerCatalog.filtered(
        root.category, root.searchQuery, root.favorites, root.recents)

    Settings {
        id: stickerPrefs
        category: "genycaster-stickers-v1"
        property string favoritesJson: "[]"
        property string recentsJson: "[]"
    }

    modal: true
    focus: true
    dim: true
    padding: Theme.space3
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? Math.min(root.compact ? 480 : 472, parent.width - root.safeMargin * 2) : 472
    height: parent ? Math.min(root.compact ? 560 : 540, parent.height - root.safeMargin * 2) : 540
    background: PopupSurface {}

    Overlay.modal: DrawerBackdrop {
        sourceItem: root.parent && root.parent !== Overlay.overlay ? root.parent : null
        snapshotBlurOnBrowser: true
        active: root.visible
        addScrim: true
        blockInput: true
        onBackgroundTapped: root.close()
    }

    function reposition() {
        if (!root.parent) return
        if (root.compact) {
            root.x = Math.round((root.parent.width - root.width) / 2)
            root.y = Math.round(root.parent.height - root.height - root.safeMargin)
            return
        }
        if (!root.anchorItem) {
            root.x = Math.round((root.parent.width - root.width) / 2)
            root.y = Math.round((root.parent.height - root.height) / 2)
            return
        }
        const bottom = root.anchorItem.mapToItem(root.parent, 0, root.anchorItem.height)
        const top = root.anchorItem.mapToItem(root.parent, 0, 0)
        const desiredX = Theme.rtl ? bottom.x + root.anchorItem.width - root.width : bottom.x
        root.x = Math.round(Math.max(root.safeMargin,
            Math.min(root.parent.width - root.width - root.safeMargin, desiredX)))
        const under = bottom.y + Theme.space2
        const over = top.y - root.height - Theme.space2
        root.y = Math.round(under + root.height < root.parent.height - root.safeMargin
            ? under : Math.max(root.safeMargin, over))
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
        if (!root.canAdd || !sticker) return
        root.stickerSelected(sticker)
        // Recording usage happens in the Composer only after insertion succeeds.
    }

    function recordUsage(id) {
        stickerPrefs.recentsJson = JSON.stringify(StickerCatalog.recordRecent(root.recents, id))
    }

    function categoryLabel(id) {
        switch (id) {
        case "all": return qsTrId("genycaster.stickers.all")
        case "favorites": return qsTrId("genycaster.stickers.favorites")
        case "recent": return qsTrId("genycaster.stickers.recent")
        case "originals": return qsTrId("genycaster.stickers.originals")
        case "reactions": return qsTrId("genycaster.stickers.reactions")
        case "web3": return qsTrId("genycaster.stickers.web3")
        }
        return ""
    }

    onAboutToShow: {
        root.category = "all"
        root.searchQuery = ""
        searchField.text = ""
        root.reposition()
    }
    onOpened: stickerGrid.forceActiveFocus()
    onWidthChanged: if (opened) root.reposition()
    onHeightChanged: if (opened) root.reposition()

    contentItem: ColumnLayout {
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stickers.title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stickers.subtitle")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
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
                    model: ["all", "originals", "reactions", "web3", "favorites", "recent"]
                    delegate: ActionButton {
                        required property string modelData
                        compact: true
                        quiet: true
                        selected: root.category === modelData
                        text: root.categoryLabel(modelData)
                        onClicked: root.category = modelData
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
            cellWidth: width > 0 ? width / Math.max(2, Math.floor(width / 112)) : 112
            cellHeight: cellWidth + Theme.space2
            model: root.visibleStickers

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
                    curveDepth: Theme.curveContainer
                    fillColor: stickerHit.pressed ? Theme.primarySurface
                        : stickerHit.containsMouse ? Theme.surfaceElevated : Theme.surfaceMuted
                    borderWidth: 1
                    borderColor: stickerHit.containsMouse ? Theme.accent : Theme.borderMuted
                    Behavior on fillColor {
                        enabled: !Theme.reduceMotion
                        ColorAnimation { duration: Theme.motionFast }
                    }
                }

                Image {
                    id: thumbnail
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: Theme.space2
                    width: Math.min(tile.width - Theme.space5 * 2, tile.height - Theme.space5 * 2)
                    height: width
                    source: StickerCatalog.previewUrl(tile.modelData.id)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                    mipmap: false
                    cache: true
                }

                MouseArea {
                    id: stickerHit
                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    hoverEnabled: !root.compact
                    enabled: root.canAdd
                    cursorShape: Qt.PointingHandCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTrId("genycaster.stickers.add") + ": " + tile.modelData.title
                    onClicked: root.chooseSticker(tile.modelData.id)
                }

                ActionButton {
                    id: favoriteAction
                    z: 2
                    anchors.top: parent.top
                    anchors.topMargin: Theme.space1
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space1
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

            Icon {
                text: Theme.iconImage
                color: Theme.inkMuted
                preferredSize: Theme.typeCaptionSize
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
