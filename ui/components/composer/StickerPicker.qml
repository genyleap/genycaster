import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Farcaster.Client
import "StickerCatalog.js" as StickerCatalog

// Cross-platform sticker library. Only the verified account's cloud-backed
// pack list enables composer insertion; previewing a pack never installs it.
Popup {
    id: root
    objectName: "stickerPicker"

    signal stickerSelected(var sticker)
    signal proRequested()

    property Item anchorItem: null
    property bool canAdd: true
    property string page: "discover"
    property string selectedPackId: "njal"
    property string category: "all"
    property string searchQuery: ""
    property string pendingInstallId: ""

    readonly property bool compact: parent !== null
        && parent.width < Theme.breakpointMobile
    readonly property bool needsPreviewCompatibility: Qt.platform.os === "wasm"
        || Qt.platform.os === "android"
    function stickerDisplayUrl(id) {
        return StickerCatalog.displayPreviewUrl(id,
            openProofConnections ? String(openProofConnections.stickerPreviewBaseUrl || "") : "",
            root.needsPreviewCompatibility)
    }
    readonly property real safeMargin: Theme.space3
    readonly property bool authorized: openProofConnections
        && openProofConnections.cloudAuthorizationReady
    readonly property bool libraryReady: authorized && openProofConnections.stickerPacksReady
    readonly property bool busy: openProofConnections && openProofConnections.stickerPacksBusy
    readonly property var installedIds: libraryReady
        ? (openProofConnections.stickerPackIds || []) : []
    readonly property bool verifiedPro: openProofConnections
        && openProofConnections.proEntitlements.status === "verified"
        && openProofConnections.proEntitlements.eligible === true
    readonly property var activePack: StickerCatalog.packForId(root.selectedPackId)
    readonly property bool activeInstalled: root.installedIds.indexOf(root.selectedPackId) >= 0
    readonly property bool activePro: root.activePack && root.activePack.tier === "pro"
    readonly property bool activeUsable: root.activeInstalled
        && (!root.activePro || root.verifiedPro)
    readonly property var favorites: StickerCatalog.decodeIds(stickerPrefs.favoritesJson, 32)
    readonly property var recents: StickerCatalog.decodeIds(stickerPrefs.recentsJson, 12)
    readonly property var detailStickers: StickerCatalog.stickersForPack(root.selectedPackId)
    readonly property var activeStickers: StickerCatalog.filtered(
        root.selectedPackId === "njal" ? "njal" : "cream-cat-pro",
        root.searchQuery, root.favorites, root.recents)
    readonly property var visibleStickers: root.category === "favorites"
        || root.category === "recent"
        ? StickerCatalog.filtered(root.category, root.searchQuery, root.favorites, root.recents)
             .filter(function(item) {
                 return root.installedIds.indexOf(item.pack) >= 0
                     && (item.pack !== "cream-cat-pro" || root.verifiedPro)
             })
        : root.activeUsable ? root.activeStickers : []

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
    width: parent ? Math.max(1, Math.min(root.compact ? 500 : 460,
        parent.width - root.safeMargin * 2)) : 460
    height: parent ? Math.max(1, Math.min(root.compact ? 665 : 600,
        parent.height - root.safeMargin * 2)) : 600
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
            // Map the actual button into the overlay, not the drawer/editor.
            // Prefer opening entirely above the editor toolbar.
            const top = root.anchorItem.mapToItem(root.parent, 0, 0)
            const bottom = root.anchorItem.mapToItem(root.parent, 0, root.anchorItem.height)
            const preferredX = Theme.rtl
                ? bottom.x + root.anchorItem.width - root.width : bottom.x
            const below = bottom.y + Theme.space2
            const above = top.y - root.height - Theme.space2
            const hasAbove = above >= topBound
            const hasBelow = below + root.height <= outerHeight - topBound
            const desiredY = hasAbove ? above : hasBelow ? below
                : Math.max(topBound, Math.min(maxY, top.y - root.height / 2))
            root.x = Math.round(Math.max(leftBound, Math.min(maxX, preferredX)))
            root.y = Math.round(Math.max(topBound, Math.min(maxY, desiredY)))
            return
        }
        root.x = Math.round(Math.max(leftBound, (outerWidth - root.width) / 2))
        root.y = Math.round(Math.max(topBound, (outerHeight - root.height) / 2))
    }

    function showPack(id) {
        if (!StickerCatalog.packForId(id))
            return
        root.selectedPackId = id
        root.page = "detail"
    }

    function showLibrary() {
        root.page = "library"
        root.category = "all"
        root.searchQuery = ""
        searchField.text = ""
        if (root.installedIds.indexOf(root.selectedPackId) < 0)
            root.selectedPackId = root.installedIds.length > 0 ? root.installedIds[0] : "njal"
    }

    function togglePack() {
        if (!root.libraryReady || root.busy)
            return
        if (root.activePro && !root.verifiedPro && !root.activeInstalled) {
            root.proRequested()
            return
        }
        root.pendingInstallId = root.activeInstalled ? "" : root.selectedPackId
        openProofConnections.setStickerPackInstalled(root.selectedPackId, !root.activeInstalled)
    }

    function categoryLabel(id) {
        switch (id) {
        case "all": return qsTrId("genycaster.stickers.all")
        case "favorites": return qsTrId("genycaster.stickers.favorites")
        case "recent": return qsTrId("genycaster.stickers.recent")
        }
        return ""
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
        if (!root.canAdd || !root.libraryReady || !sticker
            || root.installedIds.indexOf(sticker.pack) < 0
            || (sticker.pack === "cream-cat-pro" && !root.verifiedPro))
            return
        root.stickerSelected(sticker)
    }

    function recordUsage(id) {
        stickerPrefs.recentsJson = JSON.stringify(StickerCatalog.recordRecent(root.recents, id))
    }

    function errorLabel() {
        if (!root.authorized) return qsTrId("genycaster.stickers.signin")
        const code = String(openProofConnections.stickerPacksError || "")
        switch (code) {
        case "geny_pro_required": return qsTrId("genycaster.stickers.pro_required")
        case "sticker_library_conflict": return qsTrId("genycaster.stickers.conflict")
        case "sticker_library_save_failed": return qsTrId("genycaster.stickers.save_failed")
        }
        if (!root.libraryReady && root.busy) return qsTrId("genycaster.stickers.loading")
        if (!root.libraryReady) return qsTrId("genycaster.stickers.unavailable")
        return ""
    }

    onAboutToShow: {
        root.pendingInstallId = ""
        root.category = "all"
        root.searchQuery = ""
        searchField.text = ""
        root.page = root.installedIds.length > 0 ? "library" : "discover"
        if (root.installedIds.length > 0)
            root.selectedPackId = root.installedIds[0]
        root.reposition()
        if (root.authorized) {
            openProofConnections.fetchStickerPacks()
            openProofConnections.fetchProEntitlements(false)
        }
    }
    onOpened: {
        Qt.callLater(root.reposition)
    }
    onWidthChanged: if (opened) root.reposition()
    onHeightChanged: if (opened) root.reposition()
    onAnchorItemChanged: if (opened) root.reposition()
    onCompactChanged: if (opened) root.reposition()
    // A cloud-session transition is a reason to fetch the private library.
    // Unrelated stateChanged notifications (Pro, identity, profile) are not:
    // repeating that GET flooded the gateway on browser/mobile startup.
    onAuthorizedChanged: {
        if (root.opened && root.authorized && !root.libraryReady)
            openProofConnections.fetchStickerPacks()
    }

    Connections {
        target: root.parent
        function onWidthChanged() { if (root.opened) root.reposition() }
        function onHeightChanged() { if (root.opened) root.reposition() }
    }
    Connections {
        target: openProofConnections
        function onStickerPacksChanged() {
            if (!root.opened || !root.libraryReady)
                return
            if (root.pendingInstallId.length > 0
                    && root.installedIds.indexOf(root.pendingInstallId) >= 0) {
                const id = root.pendingInstallId
                root.pendingInstallId = ""
                root.selectedPackId = id
                root.showLibrary()
            }
        }
        // The popup's onAuthorizedChanged handles the single transition
        // to a verified cloud session, without retrying on every state event.
    }

    contentItem: ColumnLayout {
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            ActionButton {
                visible: root.page === "detail"
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconArrowLeft
                text: qsTrId("genycaster.stickers.back")
                onClicked: root.page = "discover"
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.page === "detail" && root.activePack
                        ? root.activePack.title : qsTrId("genycaster.stickers.title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.page === "detail" && root.activePack
                        ? qsTrId("genycaster.stickers.by_author").arg(root.activePack.author)
                        : qsTrId("genycaster.stickers.subtitle")
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

        RowLayout {
            objectName: "stickerPrimaryTabs"
            visible: root.page !== "detail"
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Theme.scaleUi(32) : 0
            layoutDirection: Theme.uiDirection
            spacing: Theme.space1
            StickerTab {
                objectName: "stickerMyPacksTab"
                selected: root.page === "library"
                text: qsTrId("genycaster.stickers.my_packs")
                onClicked: root.showLibrary()
            }
            StickerTab {
                objectName: "stickerDiscoverTab"
                selected: root.page === "discover"
                text: qsTrId("genycaster.stickers.discover")
                onClicked: root.page = "discover"
            }
            Item { Layout.fillWidth: true }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.errorLabel().length > 0 && root.page !== "library"
            text: root.errorLabel()
            color: root.authorized && root.busy ? Theme.inkMuted : Theme.warning
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }

        // The discover cards look like pack previews: selected art,
        // author, item count and a single prominent affordance to open.
        ColumnLayout {
            visible: root.page === "discover"
            Layout.fillWidth: true
            Layout.fillHeight: visible
            spacing: Theme.space2

            Repeater {
                model: StickerCatalog.packs
                delegate: Item {
                    id: packCard
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180

                    Superellipse {
                        anchors.fill: parent
                        fillColor: Theme.surfaceMuted
                        curveDepth: Theme.curveContainer
                        borderColor: Theme.borderMuted
                        borderWidth: 1
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: packCard.modelData.title
                                font.pixelSize: Theme.typeBodySize
                                font.weight: Theme.weightDemiBold
                                color: Theme.ink
                            }
                            DirectionAwareLabel {
                                text: packCard.modelData.tier === "pro"
                                    ? qsTrId("genycaster.stickers.pro")
                                    : qsTrId("genycaster.stickers.free")
                                color: packCard.modelData.tier === "pro"
                                    ? Theme.accent : Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 86
                            spacing: Theme.space1
                            Repeater {
                                model: StickerCatalog.stickersForPack(packCard.modelData.id).slice(0, 4)
                                delegate: Image {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 82
                                    source: root.stickerDisplayUrl(modelData.id)
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    smooth: true
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.stickers.count")
                                    .arg(LocaleFormat.formatInteger(packCard.modelData.count))
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                            }
                            ActionButton {
                                compact: true
                                positive: true
                                text: qsTrId("genycaster.stickers.view_pack")
                                onClicked: root.showPack(packCard.modelData.id)
                            }
                        }
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }

        ColumnLayout {
            visible: root.page === "library"
            Layout.fillWidth: true
            Layout.fillHeight: visible
            spacing: Theme.space2

            Flickable {
                visible: root.installedIds.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? Theme.scaleUi(32) : 0
                contentWidth: Math.max(width, installedRow.implicitWidth)
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                RowLayout {
                    id: installedRow
                    layoutDirection: Theme.uiDirection
                    height: parent.height
                    width: implicitWidth
                    spacing: Theme.space1
                    Repeater {
                        model: root.installedIds
                        delegate: StickerTab {
                            required property string modelData
                            selected: root.selectedPackId === modelData
                            text: modelData === "njal" ? "Njal" : "Cream Cat"
                            onClicked: {
                                root.selectedPackId = modelData
                                root.category = "all"
                            }
                        }
                    }
                }
            }

            DirectionAwareTextField {
                id: searchField
                visible: root.installedIds.length > 0
                objectName: "stickerSearch"
                Layout.fillWidth: true
                placeholderText: qsTrId("genycaster.stickers.search")
                onTextChanged: root.searchQuery = text
            }

            Flickable {
                visible: root.installedIds.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? Theme.scaleUi(32) : 0
                contentWidth: Math.max(width, filterRow.implicitWidth)
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                RowLayout {
                    id: filterRow
                    layoutDirection: Theme.uiDirection
                    width: implicitWidth
                    height: parent.height
                    spacing: Theme.space1
                    Repeater {
                        model: ["all", "favorites", "recent"]
                        delegate: StickerTab {
                            required property string modelData
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
                visible: root.installedIds.length > 0
                Layout.fillWidth: true
                Layout.fillHeight: visible
                clip: true
                focus: true
                boundsBehavior: Flickable.StopAtBounds
                cellWidth: width > 0 ? width / Math.max(2, Math.floor(width / 104)) : 104
                cellHeight: cellWidth
                model: root.visibleStickers
                cacheBuffer: cellHeight

                delegate: Item {
                    id: stickerTile
                    required property int index
                    required property var modelData
                    width: stickerGrid.cellWidth
                    height: stickerGrid.cellHeight
                    readonly property bool favorite: root.isFavorite(modelData.id)

                    Superellipse {
                        anchors.fill: parent
                        anchors.margins: Theme.space1
                        fillColor: stickerHit.pressed ? Theme.primarySurface
                            : stickerHit.containsMouse ? Theme.surfaceHover : "transparent"
                        curveDepth: Theme.curveControl
                        borderWidth: 0
                    }
                    Image {
                        id: thumbnail
                        anchors.centerIn: parent
                        width: stickerTile.width - Theme.space3 * 2
                        height: width
                        source: Qt.platform.os !== "wasm" || (root.visible && root.page === "library")
                                ? root.stickerDisplayUrl(stickerTile.modelData.id) : ""
                        sourceSize: Qt.platform.os === "wasm" ? Qt.size(256, 256) : Qt.size(-1, -1)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        smooth: true
                        cache: Qt.platform.os !== "wasm"
                    }
                    MouseArea {
                        id: stickerHit
                        anchors.fill: parent
                        anchors.margins: Theme.space1
                        hoverEnabled: !root.compact
                        enabled: root.canAdd && root.libraryReady
                            && thumbnail.status === Image.Ready
                        cursorShape: Qt.PointingHandCursor
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTrId("genycaster.stickers.add")
                            + ": " + stickerTile.modelData.title
                        onClicked: root.chooseSticker(stickerTile.modelData.id)
                    }
                    ActionButton {
                        visible: stickerTile.favorite || root.compact || stickerHit.containsMouse
                        z: 2
                        anchors.right: parent.right
                        anchors.top: parent.top
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconBookmark
                        selected: stickerTile.favorite
                        text: stickerTile.favorite
                            ? qsTrId("genycaster.stickers.remove_favorite")
                            : qsTrId("genycaster.stickers.add_favorite")
                        onClicked: root.toggleFavorite(stickerTile.modelData.id)
                    }
                }
            }

            ColumnLayout {
                visible: root.libraryReady && root.installedIds.length === 0
                Layout.fillWidth: true
                Layout.fillHeight: visible
                spacing: Theme.space3
                Item { Layout.fillHeight: true }
                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    text: Theme.iconSticker
                    color: Theme.inkMuted
                    preferredSize: Theme.typeTitle1Size
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.stickers.empty_library")
                    color: Theme.inkMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                ActionButton {
                    Layout.alignment: Qt.AlignHCenter
                    prominent: true
                    text: qsTrId("genycaster.stickers.discover")
                    onClicked: root.page = "discover"
                }
                Item { Layout.fillHeight: true }
            }

            ColumnLayout {
                objectName: "stickerLibraryUnavailableState"
                visible: !root.libraryReady
                Layout.fillWidth: true
                Layout.fillHeight: visible
                spacing: Theme.space3
                Item { Layout.fillHeight: true }
                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.busy ? Theme.iconRefresh : Theme.iconWarning
                    color: root.busy ? Theme.inkMuted : Theme.warning
                    preferredSize: Theme.typeTitle1Size
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.errorLabel()
                    color: Theme.inkMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
                ActionButton {
                    visible: !root.busy
                    Layout.alignment: Qt.AlignHCenter
                    prominent: true
                    text: qsTrId("genycaster.status.retry")
                    onClicked: {
                        if (root.authorized)
                            openProofConnections.fetchStickerPacks()
                        else
                            openProofConnections.retryAccountSessionFromUser()
                    }
                }
                ActionButton {
                    Layout.alignment: Qt.AlignHCenter
                    quiet: true
                    text: qsTrId("genycaster.stickers.discover")
                    onClicked: root.page = "discover"
                }
                Item { Layout.fillHeight: true }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.installedIds.length > 0
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.canAdd ? qsTrId("genycaster.stickers.media_slot")
                        : qsTrId("genycaster.stickers.limit")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
                ActionButton {
                    compact: true
                    quiet: true
                    text: qsTrId("genycaster.stickers.manage")
                    onClicked: root.showPack(root.selectedPackId)
                }
            }
        }

        ColumnLayout {
            visible: root.page === "detail"
            Layout.fillWidth: true
            Layout.fillHeight: visible
            spacing: Theme.space2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.activePro ? qsTrId("genycaster.stickers.pro_description")
                    : qsTrId("genycaster.stickers.pack_description")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }

            GridView {
                id: packPreview
                objectName: "stickerPackPreview"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: width > 0 ? width / 4 : 110
                cellHeight: cellWidth + Theme.space1
                model: root.detailStickers
                boundsBehavior: Flickable.StopAtBounds

                delegate: Image {
                    required property var modelData
                    width: packPreview.cellWidth - Theme.space2
                    height: packPreview.cellHeight - Theme.space2
                    source: Qt.platform.os !== "wasm" || (root.visible && root.page === "detail")
                            ? root.stickerDisplayUrl(modelData.id) : ""
                    sourceSize: Qt.platform.os === "wasm" ? Qt.size(256, 256) : Qt.size(-1, -1)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                    cache: Qt.platform.os !== "wasm"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.activePack
                        ? qsTrId("genycaster.stickers.count")
                            .arg(LocaleFormat.formatInteger(root.activePack.count))
                        : ""
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                }
                ActionButton {
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconArrowUpRight
                    text: qsTrId("genycaster.stickers.attribution")
                    onClicked: {
                        if (root.activePack)
                            presentationBridge.openExternalUrl(root.activePack.credit)
                    }
                }
            }

            ActionButton {
                objectName: "installStickerPackAction"
                Layout.fillWidth: true
                prominent: !root.activeInstalled
                positive: !root.activeInstalled
                iconGlyph: root.activeInstalled ? Theme.iconCheck : Theme.iconPlus
                text: root.activeInstalled
                    ? qsTrId("genycaster.stickers.remove_pack")
                    : root.activePro && !root.verifiedPro
                        ? qsTrId("genycaster.stickers.unlock_pro")
                        : qsTrId("genycaster.stickers.install_count")
                            .arg(root.activePack ? LocaleFormat.formatInteger(root.activePack.count) : "")
                enabled: root.authorized && root.libraryReady && !root.busy
                busy: root.busy
                onClicked: root.togglePack()
            }
        }
    }
}
