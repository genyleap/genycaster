import QtQuick
import QtQuick.Controls
import Farcaster.Client

// Variable-height Explorer columns. The model owns identity and order; this
// view owns only geometry and bounded admission of expensive cast subtrees.
Flickable {
    id: view

    property alias model: slots.model
    property Component delegate
    property Component header
    property Component footer
    property var estimateHeight
    property real targetCardWidth: Theme.scaleUi(500)
    property int maximumColumns: 2
    property int layoutDirection: Theme.uiDirection
    property real gutter: Theme.space3
    property real cacheBuffer: Math.min(height * 0.5, 480)
    readonly property int count: slots.count
    readonly property Item headerItem: headerLoader.item
    readonly property Item footerItem: footerLoader.item
    readonly property int columns: Math.max(1, Math.min(maximumColumns,
        Math.floor((width - gutter) / (targetCardWidth * 0.85 + gutter))))
    readonly property real cardWidth: Math.max(1, Math.min(targetCardWidth * 1.15,
        (width - gutter * (columns + 1)) / columns))
    readonly property real leadingInset: Math.max(gutter,
        (width - columns * cardWidth - (columns - 1) * gutter) / 2)
    property real rowsBottom: 0
    property bool arranging: false
    property bool positioned: false

    signal layoutStarted()
    signal layoutFinished()

    clip: true
    contentWidth: width
    contentHeight: Math.max(0, rowsBottom + footerLoader.height)
    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.DragOverBounds
    boundsMovement: Flickable.FollowBoundsBehavior
    activeFocusOnTab: true

    function itemAtIndex(index) { return slots.itemAt(index) }

    function indexAt(x, y) {
        let nearest = -1
        let distance = Number.POSITIVE_INFINITY
        for (let index = 0; index < slots.count; ++index) {
            const slot = slots.itemAt(index)
            if (!slot) continue
            if (y >= slot.y && y < slot.y + slot.height) {
                // A probe in a column gutter still needs a real cast anchor.
                const delta = Math.abs(x - slot.x - slot.width / 2)
                if (delta < distance) { nearest = index; distance = delta }
            }
        }
        return nearest
    }

    function scheduleLayout() {
        if (!arranging && !layoutTimer.running) layoutTimer.start()
    }

    function forceLayout() {
        if (arranging) return
        layoutTimer.stop()
        arranging = true
        layoutStarted()
        const anchorIndex = positioned && !moving && contentY > 0
            ? indexAt(width / 2, contentY + Math.min(80, height / 4)) : -1
        const anchor = anchorIndex >= 0 ? slots.itemAt(anchorIndex) : null
        const offset = anchor ? anchor.y - contentY : 0
        const start = headerLoader.height + gutter
        const bottoms = Array(columns).fill(start)
        for (let index = 0; index < slots.count; ++index) {
            const slot = slots.itemAt(index)
            if (!slot) continue
            // Place new rows in source order into the shortest lane. Keep that
            // assignment while metadata settles; only reordering or a column
            // count change can move an existing card horizontally.
            if (slot.assignedColumn < 0 || slot.assignmentColumns !== columns) {
                let shortest = 0
                for (let candidate = 1; candidate < columns; ++candidate) {
                    if (bottoms[candidate] < bottoms[shortest]) shortest = candidate
                }
                slot.assignedColumn = shortest
                slot.assignmentColumns = columns
            }
            const column = slot.assignedColumn
            const visualColumn = layoutDirection === Qt.RightToLeft ? columns - column - 1 : column
            slot.x = leadingInset + visualColumn * (cardWidth + gutter)
            slot.y = bottoms[column]
            slot.placed = true
            bottoms[column] += slot.height + gutter
        }
        rowsBottom = slots.count > 0 ? Math.max(...bottoms) : headerLoader.height
        if (anchor) contentY = Math.max(0, Math.min(anchor.y - offset,
            Math.max(0, contentHeight - height)))
        positioned = true
        arranging = false
        layoutFinished()
    }

    function positionViewAtBeginning() {
        cancelFlick()
        forceLayout()
        contentY = 0
    }

    function positionViewAtIndex(index, mode) {
        cancelFlick()
        forceLayout()
        const slot = slots.itemAt(index)
        if (!slot) return
        const offset = mode === ListView.Center ? (height - slot.height) / 2 : 0
        contentY = Math.max(0, Math.min(slot.y - offset, Math.max(0, contentHeight - height)))
    }

    function scrollBy(distance) {
        cancelFlick()
        contentY = Math.max(0, Math.min(contentY + distance, Math.max(0, contentHeight - height)))
    }

    Keys.onPageDownPressed: scrollBy(height * 0.85)
    Keys.onPageUpPressed: scrollBy(-height * 0.85)
    Keys.onHomePressed: positionViewAtBeginning()
    Keys.onEndPressed: scrollBy(contentHeight)
    Keys.onDownPressed: scrollBy(48)
    Keys.onUpPressed: scrollBy(-48)
    onWidthChanged: scheduleLayout()
    onColumnsChanged: scheduleLayout()
    onCardWidthChanged: scheduleLayout()
    onLayoutDirectionChanged: scheduleLayout()
    onGutterChanged: scheduleLayout()
    Component.onCompleted: scheduleLayout()

    Timer {
        id: layoutTimer
        interval: 0
        onTriggered: view.forceLayout()
    }

    Loader {
        id: headerLoader
        width: view.width
        sourceComponent: view.header
        onHeightChanged: view.scheduleLayout()
    }

    Repeater {
        id: slots
        onItemAdded: view.scheduleLayout()
        onItemRemoved: view.scheduleLayout()

        delegate: Item {
            id: slot
            objectName: "masonrySlot"
            required property int index
            required property string stableKey
            required property string revisionKey
            required property string presentationKind
            required property string mediaLayoutKind
            property real measuredHeight: 0
            property bool placed: false
            property int assignedColumn: -1
            property int assignmentColumns: 0
            readonly property real estimatedHeight: view.estimateHeight
                ? view.estimateHeight(stableKey, presentationKind, mediaLayoutKind, width) : 320
            width: view.cardWidth
            height: Math.max(1, measuredHeight > 0 ? measuredHeight : estimatedHeight)
            onHeightChanged: view.scheduleLayout()
            onIndexChanged: {
                assignedColumn = -1
                view.scheduleLayout()
            }
            onWidthChanged: measuredHeight = 0
            onStableKeyChanged: {
                measuredHeight = 0
                assignedColumn = -1
                view.scheduleLayout()
            }

            function recordHeight() {
                const next = cardLoader.item ? cardLoader.item.height : 0
                if (next > 0 && Math.abs(next - measuredHeight) >= 1)
                    measuredHeight = next
            }

            Loader {
                id: cardLoader
                // Explicitly forward every model role to the component created
                // in FeedSurface's scope; Loader does not inject model roles.
                readonly property int rowIndex: slot.index
                readonly property string stableKey: slot.stableKey
                readonly property string revisionKey: slot.revisionKey
                readonly property string presentationKind: slot.presentationKind
                readonly property string mediaLayoutKind: slot.mediaLayoutKind
                readonly property real slotY: slot.y
                width: slot.width
                active: view.visible && view.positioned && slot.placed
                    && slot.y + slot.height >= view.contentY - view.cacheBuffer
                    && slot.y <= view.contentY + view.height + view.cacheBuffer
                sourceComponent: view.delegate
                onLoaded: slot.recordHeight()
            }

            Connections {
                target: cardLoader.item
                function onHeightChanged() { slot.recordHeight() }
            }
        }
    }

    Loader {
        id: footerLoader
        y: view.rowsBottom
        width: view.width
        sourceComponent: view.footer
    }

    ScrollBar.vertical: GenyScrollBar {}
}
