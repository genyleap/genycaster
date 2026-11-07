import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Farcaster.Client

// Composer emoji palette. Classic Unicode browsing stays renderer-free;
// Animated browsing is bounded and retained independently so switching modes
// neither starts the whole catalog at once nor rebuilds warm renderers.
Popup {
    id: root
    objectName: "emojiPicker"

    signal emojiSelected(string emoji)

    property Item anchorItem: null
    property int categoryIndex: 0
    property int classicCategoryIndex: 0
    property int animatedCategoryIndex: 0
    property string searchQuery: ""
    property string committedSearchQuery: ""
    property var searchResults: []
    property bool animatedMode: false
    property string pendingPreviewSequence: ""
    property string previewSequence: ""
    property string animatedModelKey: ""

    readonly property var emojiGroups: root.animatedMode
        ? emojiController.specialEmojiGroups : emojiController.emojiGroups
    readonly property var activeGroup: root.emojiGroups.length > 0
        ? root.emojiGroups[Math.max(0, Math.min(root.categoryIndex, root.emojiGroups.length - 1))]
        : ({ id: "", icon: "", values: [] })
    readonly property var currentValues: root.committedSearchQuery.length > 0
        ? root.searchResults : (root.activeGroup.values || [])
    readonly property bool searching: root.committedSearchQuery.length > 0
    readonly property real safeMargin: Theme.space3
    readonly property real preferredPopupWidth: 420
    readonly property int cellUnits: 46
    readonly property int animatedBatchSize: 24
    readonly property var activeGrid: root.animatedMode ? animatedEmojiGrid : classicEmojiGrid
    readonly property string currentAnimatedModelKey: (root.searching
        ? "search:" + root.committedSearchQuery
        : "category:" + String(root.activeGroup.id || ""))
        + ":" + String(root.currentValues.length)
    readonly property bool animatedModelIsCurrent:
        root.animatedModelKey === root.currentAnimatedModelKey
    readonly property int animatedRemaining: root.animatedModelIsCurrent
        ? Math.max(0, root.currentValues.length - animatedGridModel.count) : 0

    ListModel {
        id: animatedGridModel
    }

    modal: true
    dim: true
    focus: true
    padding: Theme.space3
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(root.preferredPopupWidth,
                    parent ? Math.max(316, parent.width - root.safeMargin * 2) : root.preferredPopupWidth)
    height: Math.min(parent ? Math.max(360, parent.height - root.safeMargin * 2) : 520, 520)
    background: PopupSurface {}

    Overlay.modal: Rectangle {
        color: "transparent"
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            preventStealing: true
            onClicked: root.close()
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    function reposition() {
        if (!root.parent)
            return
        if (!root.anchorItem) {
            root.x = Math.round(Math.max(root.safeMargin, (root.parent.width - root.width) / 2))
            root.y = Math.round(Math.max(root.safeMargin, (root.parent.height - root.height) / 2))
            return
        }
        const below = root.anchorItem.mapToItem(root.parent, 0, root.anchorItem.height)
        const above = root.anchorItem.mapToItem(root.parent, 0, 0)
        const preferredX = Theme.rtl ? below.x + root.anchorItem.width - root.width : below.x
        root.x = Math.round(Math.max(root.safeMargin,
                                    Math.min(root.parent.width - root.width - root.safeMargin, preferredX)))
        const belowY = below.y + Theme.space2
        const aboveY = above.y - root.height - Theme.space2
        root.y = Math.round(belowY + root.height <= root.parent.height - root.safeMargin
                            ? belowY : Math.max(root.safeMargin, aboveY))
    }

    function categoryTitle(categoryId) {
        switch (String(categoryId || "")) {
        case "recent": return qsTrId("genycaster.emoji.category.recent")
        case "smileys": return qsTrId("genycaster.emoji.category.smileys")
        case "people": return qsTrId("genycaster.emoji.category.people")
        case "animals": return qsTrId("genycaster.emoji.category.animals")
        case "food": return qsTrId("genycaster.emoji.category.food")
        case "activities": return qsTrId("genycaster.emoji.category.activities")
        case "travel": return qsTrId("genycaster.emoji.category.travel")
        case "objects": return qsTrId("genycaster.emoji.category.objects")
        case "symbols": return qsTrId("genycaster.emoji.category.symbols")
        case "flags": return qsTrId("genycaster.emoji.category.flags")
        case "animated": return qsTrId("genycaster.emoji.animated")
        }
        return ""
    }

    function selectEmoji(value) {
        if (!value)
            return
        emojiController.recordUsage(value)
        root.emojiSelected(value)
        root.close()
    }

    function queuePreview(value) {
        if (!root.animatedMode || !value)
            return
        const sequence = String(value)
        if (sequence === root.previewSequence) {
            previewDwell.stop()
            root.pendingPreviewSequence = ""
            return
        }
        root.pendingPreviewSequence = sequence
        previewDwell.restart()
    }

    function resetPreview() {
        previewDwell.stop()
        root.pendingPreviewSequence = ""
        root.previewSequence = root.animatedMode && root.currentValues.length > 0
            ? String(root.currentValues[0]) : ""
    }

    function refreshAnimatedModel() {
        if (!root.animatedMode)
            return
        const key = root.currentAnimatedModelKey
        if (root.animatedModelKey === key && animatedGridModel.count > 0) {
            if (animatedEmojiGrid.currentIndex >= animatedGridModel.count)
                animatedEmojiGrid.currentIndex = animatedGridModel.count > 0 ? 0 : -1
            return
        }

        animatedGridModel.clear()
        const count = Math.min(root.animatedBatchSize, root.currentValues.length)
        for (let index = 0; index < count; ++index)
            animatedGridModel.append({ emojiValue: String(root.currentValues[index]) })
        root.animatedModelKey = key
        animatedEmojiGrid.currentIndex = count > 0 ? 0 : -1
        animatedEmojiGrid.positionViewAtBeginning()
    }

    function appendAnimatedBatch() {
        if (!root.animatedModelIsCurrent)
            root.refreshAnimatedModel()
        const begin = animatedGridModel.count
        const end = Math.min(begin + root.animatedBatchSize, root.currentValues.length)
        for (let index = begin; index < end; ++index)
            animatedGridModel.append({ emojiValue: String(root.currentValues[index]) })
    }

    function commitSearch() {
        root.committedSearchQuery = root.searchQuery.trim()
        if (root.committedSearchQuery.length === 0) {
            root.searchResults = []
            return
        }
        root.searchResults = root.animatedMode
            ? emojiController.searchSpecial(root.committedSearchQuery, 1024)
            : emojiController.search(root.committedSearchQuery, 1024)
    }

    onSearchQueryChanged: searchDebounce.restart()
    onCategoryIndexChanged: {
        root.resetPreview()
        animatedModelSync.restart()
    }
    onAnimatedModeChanged: {
        root.categoryIndex = root.animatedMode ? root.animatedCategoryIndex : root.classicCategoryIndex
        root.commitSearch()
        root.resetPreview()
        animatedModelSync.restart()
        Qt.callLater(() => root.activeGrid.forceActiveFocus())
    }
    onCurrentValuesChanged: {
        if (!root.animatedMode && classicEmojiGrid.currentIndex >= root.currentValues.length)
            classicEmojiGrid.currentIndex = root.currentValues.length > 0 ? 0 : -1
        animatedModelSync.restart()
        root.resetPreview()
    }
    onAboutToShow: {
        root.searchQuery = ""
        root.committedSearchQuery = ""
        root.searchResults = []
        root.categoryIndex = 0
        root.animatedMode = false
        root.reposition()
        classicEmojiGrid.currentIndex = 0
        classicEmojiGrid.positionViewAtBeginning()
        classicEmojiGrid.forceActiveFocus()
    }
    onWidthChanged: if (opened) root.reposition()
    onHeightChanged: if (opened) root.reposition()

    Timer {
        id: searchDebounce
        interval: 120
        repeat: false
        onTriggered: {
            root.commitSearch()
        }
    }

    // Coalesce the mode/category/search bindings, then populate only the first
    // animated page. "More" appends to this stable model without reloading the
    // already-created delegates.
    Timer {
        id: animatedModelSync
        interval: 0
        repeat: false
        onTriggered: root.refreshAnimatedModel()
    }

    // Throttle only the large preview swap. Cell animation begins independently
    // on viewport entry and never waits for this hover dwell.
    Timer {
        id: previewDwell
        interval: 90
        repeat: false
        onTriggered: root.previewSequence = root.pendingPreviewSequence
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.reduceMotion ? 0 : Theme.motionFast }
            NumberAnimation { property: "scale"; from: 0.985; to: 1; duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.reduceMotion ? 0 : Theme.motionFast }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space2
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTrId("genycaster.composer.emoji")
        Accessible.description: qsTrId("genycaster.composer.emoji_subtitle")

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.composer.emoji")
                color: Theme.ink
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightBold
            }

            ActionButton {
                quiet: true
                compact: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.shell.close")
                onClicked: root.close()
            }
        }

        // Both modes are catalog-complete but intentionally separate: Classic
        // never creates an animation renderer, while Special resolves Noto
        // assets lazily from the manifest-derived provider catalog.
        Superellipse {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            fillColor: Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderSubtle
            curveDepth: Theme.curveCompact

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space1
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection

                Repeater {
                    model: emojiController.spriteAvailable ? 2 : 1
                    delegate: Superellipse {
                        id: modeButton
                        required property int index
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        readonly property bool selected: root.animatedMode === (index === 1)
                        fillColor: selected ? Theme.primarySurface : "transparent"
                        borderWidth: selected ? Theme.strokeHairline : 0
                        borderColor: selected ? Theme.borderSubtle : "transparent"
                        curveDepth: Theme.curveCompact

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: Theme.space1
                            layoutDirection: Theme.uiDirection
                            Icon {
                                text: modeButton.index === 0 ? Theme.iconMoodPositive : Theme.iconFidelity
                                color: modeButton.selected ? Theme.accent : Theme.inkMuted
                                preferredSize: Theme.iconSizeSmall
                            }
                            DirectionAwareLabel {
                                text: modeButton.index === 0
                                    ? qsTrId("genycaster.emoji.classic")
                                    : qsTrId("genycaster.emoji.animated")
                                color: modeButton.selected ? Theme.ink : Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightDemiBold
                            }
                        }

                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: root.animatedMode = modeButton.index === 1
                        }
                    }
                }
            }
        }

        DirectionAwareTextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTrId("genycaster.emoji.search_placeholder")
            text: root.searchQuery
            onTextChanged: root.searchQuery = searchField.text
            Keys.onEscapePressed: {
                if (searchField.text.length > 0) {
                    searchField.text = ""
                    event.accepted = true
                }
            }
        }

        // The larger preview complements the already-animated visible cells;
        // its asset switch is dwell-throttled to avoid decode churn.
        Superellipse {
            Layout.fillWidth: true
            Layout.preferredHeight: root.animatedMode ? 72 : 0
            visible: root.animatedMode
            fillColor: Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderSubtle
            curveDepth: Theme.curveCompact
            clip: true

            Item {
                anchors.centerIn: parent
                width: 58
                height: 58

                Label {
                    anchors.centerIn: parent
                    text: root.previewSequence
                    visible: !animatedPreview.animationReady && !cachedPreview.animationReady
                    font.pixelSize: Theme.typeEmojiPreviewSize
                    renderType: Text.NativeRendering
                }

                SpriteEmoji {
                    id: animatedPreview
                    anchors.fill: parent
                    unicodeSequence: root.previewSequence
                    surface: 4
                    preserveArtwork: true
                    viewportVisible: root.opened && root.animatedMode
                    active: root.animatedMode && root.previewSequence.length > 0 && !Theme.reduceMotion
                    reducedMotion: Theme.reduceMotion
                }

                // Exactly one cache-only vector fallback; never created for
                // grid cells, never downloads, and yields to a prepared sprite.
                AnimatedEmoji {
                    id: cachedPreview
                    anchors.fill: parent
                    visible: root.opened && root.animatedMode && !animatedPreview.animationReady
                    unicodeSequence: visible ? root.previewSequence : ""
                    cachedOnly: true
                    surface: 4
                    preserveArtwork: true
                    active: visible && !Theme.reduceMotion
                    reducedMotion: Theme.reduceMotion
                }
            }

            GenySpinner {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                visible: animatedPreview.loading && !cachedPreview.animationReady
                running: visible
                Accessible.ignored: true
            }

            ActionButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                visible: animatedPreview.requestFailed
                quiet: true
                compact: true
                elevated: false
                text: qsTrId("genycaster.empty.retry")
                iconGlyph: Theme.iconRefresh
                onClicked: animatedPreview.retry()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            visible: !root.searching
            spacing: Theme.space1
            layoutDirection: Theme.uiDirection

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.categoryTitle(root.activeGroup.id)
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            GridView {
                id: classicEmojiGrid
                anchors.fill: parent
                visible: !root.animatedMode
                enabled: visible
                focus: visible
                activeFocusOnTab: visible
                cellWidth: root.cellUnits
                cellHeight: root.cellUnits
                layoutDirection: Theme.uiDirection
                model: root.currentValues
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                cacheBuffer: root.cellUnits * 2
                ScrollBar.vertical: GenyScrollBar {}

                Keys.onReturnPressed: {
                    if (currentIndex >= 0 && currentIndex < model.length)
                        root.selectEmoji(model[currentIndex])
                }
                Keys.onEnterPressed: {
                    if (currentIndex >= 0 && currentIndex < model.length)
                        root.selectEmoji(model[currentIndex])
                }

                delegate: Superellipse {
                    id: classicEmojiTile
                    required property int index
                    required property string modelData
                    width: root.cellUnits - Theme.space1
                    height: root.cellUnits - Theme.space1
                    readonly property bool current: classicEmojiGrid.currentIndex === index
                    fillColor: current ? Theme.primarySurface : "transparent"
                    borderWidth: current ? Theme.strokeHairline : 0
                    borderColor: current ? Theme.borderSubtle : "transparent"
                    curveDepth: Theme.curveCompact

                    Label {
                        anchors.centerIn: parent
                        text: classicEmojiTile.modelData
                        font.pixelSize: Theme.typeTitle1Size
                        renderType: Text.NativeRendering
                    }

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                        onHoveredChanged: if (hovered)
                            classicEmojiGrid.currentIndex = classicEmojiTile.index
                    }
                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: {
                            classicEmojiGrid.currentIndex = classicEmojiTile.index
                            root.selectEmoji(classicEmojiTile.modelData)
                        }
                    }
                }
            }

            // Keep the bounded view alive across mode switches. Hidden or
            // pooled cells pause; new cells use prepared, disk-cached frames.
            GridView {
                id: animatedEmojiGrid
                anchors.fill: parent
                visible: root.animatedMode
                enabled: visible
                focus: visible
                activeFocusOnTab: visible
                cellWidth: root.cellUnits
                cellHeight: root.cellUnits
                layoutDirection: Theme.uiDirection
                model: animatedGridModel
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                // Retain two rows of warm sheets for short reverse scrolls.
                cacheBuffer: root.cellUnits * 2
                ScrollBar.vertical: GenyScrollBar {}

                Keys.onReturnPressed: {
                    if (currentIndex >= 0 && currentIndex < model.count)
                        root.selectEmoji(model.get(currentIndex).emojiValue)
                }
                Keys.onEnterPressed: {
                    if (currentIndex >= 0 && currentIndex < model.count)
                        root.selectEmoji(model.get(currentIndex).emojiValue)
                }
                onCurrentIndexChanged: {
                    if (currentIndex >= 0 && currentIndex < model.count)
                        root.queuePreview(model.get(currentIndex).emojiValue)
                }

                delegate: Superellipse {
                    id: animatedEmojiTile
                    required property int index
                    required property string emojiValue
                    width: root.cellUnits - Theme.space1
                    height: root.cellUnits - Theme.space1
                    readonly property bool current: animatedEmojiGrid.currentIndex === index
                    property bool pooled: false
                    GridView.onPooled: pooled = true
                    GridView.onReused: pooled = false
                    fillColor: current ? Theme.primarySurface : "transparent"
                    borderWidth: current ? Theme.strokeHairline : 0
                    borderColor: current ? Theme.borderSubtle : "transparent"
                    curveDepth: Theme.curveCompact

                    Label {
                        anchors.centerIn: parent
                        text: animatedEmojiTile.emojiValue
                        visible: !animatedCell.animationReady
                        font.pixelSize: Theme.typeTitle1Size
                        renderType: Text.NativeRendering
                    }

                    SpriteEmoji {
                        id: animatedCell
                        anchors.fill: parent
                        anchors.margins: 3
                        visible: root.animatedMode
                        unicodeSequence: animatedEmojiTile.emojiValue
                        surface: 3
                        preserveArtwork: true
                        retainLoadedRenderer: root.opened
                        viewportVisible: root.opened && !animatedEmojiTile.pooled && !animatedEmojiGrid.moving
                            && animatedEmojiTile.y + animatedEmojiTile.height > animatedEmojiGrid.contentY
                            && animatedEmojiTile.y < animatedEmojiGrid.contentY + animatedEmojiGrid.height
                        active: root.animatedMode && !animatedEmojiGrid.moving && !Theme.reduceMotion
                        reducedMotion: Theme.reduceMotion
                    }

                    HoverHandler {
                        id: animatedEmojiHover
                        cursorShape: Qt.PointingHandCursor
                        onHoveredChanged: {
                            if (!animatedEmojiHover.hovered)
                                return
                            animatedEmojiGrid.currentIndex = animatedEmojiTile.index
                            root.queuePreview(animatedEmojiTile.emojiValue)
                        }
                    }
                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: {
                            animatedEmojiGrid.currentIndex = animatedEmojiTile.index
                            root.selectEmoji(animatedEmojiTile.emojiValue)
                        }
                    }
                }
            }
        }

        ActionButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: visible ? Theme.compactControlHeight : 0
            visible: root.animatedMode && root.animatedRemaining > 0
            quiet: true
            compact: true
            elevated: false
            text: qsTrId("genycaster.emoji.more") + " · " + String(root.animatedRemaining)
            onClicked: root.appendAnimatedBatch()
        }

        // Compact category rail shared by both complete catalogs. Special mode
        // keeps the same categories because it is derived from the full Noto
        // manifest rather than a hand-maintained shortlist.
        ListView {
            id: categoryBar
            Layout.fillWidth: true
            Layout.preferredHeight: root.searching ? 0 : 38
            visible: !root.searching
            orientation: Qt.Horizontal
            spacing: Theme.space1
            clip: true
            layoutDirection: Theme.uiDirection
            model: root.emojiGroups
            boundsBehavior: Flickable.StopAtBounds

            delegate: Superellipse {
                id: categoryButton
                required property int index
                required property var modelData
                width: 38
                height: 38
                readonly property bool selected: root.categoryIndex === index
                fillColor: selected ? Theme.primarySurface : "transparent"
                borderWidth: 0
                curveDepth: Theme.curveCompact

                Icon {
                    anchors.centerIn: parent
                    text: String(categoryButton.modelData.icon || "")
                    color: categoryButton.selected ? Theme.accent : Theme.inkMuted
                    preferredSize: Theme.iconSizeSmall
                }

                HoverHandler {
                    id: categoryHover
                    cursorShape: Qt.PointingHandCursor
                }
                Accessible.role: Accessible.Button
                Accessible.name: root.categoryTitle(categoryButton.modelData.id)

                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: {
                        if (root.animatedMode)
                            root.animatedCategoryIndex = categoryButton.index
                        else
                            root.classicCategoryIndex = categoryButton.index
                        root.categoryIndex = categoryButton.index
                        root.activeGrid.currentIndex = 0
                        root.activeGrid.positionViewAtBeginning()
                        root.activeGrid.forceActiveFocus()
                    }
                }
            }
        }
    }
}
