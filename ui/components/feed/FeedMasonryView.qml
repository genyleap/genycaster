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
    property real cacheBuffer: Math.min(height * 0.8, 720)
    // Keep recently displayed cards mounted through ordinary reverse scrolls.
    // Churn from destroying/recreating image trees on every scroll reversal
    // costs substantially more than retaining a small adjacent runway.
    readonly property real retentionBuffer: cacheBuffer + Math.min(1200, Math.max(420, height * 1.15))
    // A recycled delegate must not forget its confirmed height. Store a
    // bounded, width-bucketed sample per cast revision; otherwise readmission
    // jumps from real height back to a text/image placeholder estimate.
    property var measuredHeights: ({})
    property var measuredOrder: []
    property var aspectMemory: ({})
    property var aspectOrder: []
    function aspectForKey(key) {
        return Number(aspectMemory[String(key)] || 0)
    }
    function rememberAspect(key, ratio) {
        const id = String(key || "")
        const value = Number(ratio || 0)
        if (!id || !(value > 0)) return
        if (!Object.prototype.hasOwnProperty.call(aspectMemory, id)) {
            aspectOrder.push(id)
            if (aspectOrder.length > 600)
                delete aspectMemory[aspectOrder.shift()]
        }
        aspectMemory[id] = value
    }
    function heightKey(key, revision, width) {
        return String(key) + "|" + String(revision) + "|" + Math.round(width / 24)
    }
    function recalledHeight(key, revision, width) {
        return Number(measuredHeights[heightKey(key, revision, width)] || 0)
    }
    function retainHeight(key, revision, width, height) {
        if (!(height > 0) || !key) return
        const id = heightKey(key, revision, width)
        if (!Object.prototype.hasOwnProperty.call(measuredHeights, id)) {
            measuredOrder.push(id)
            if (measuredOrder.length > 400)
                delete measuredHeights[measuredOrder.shift()]
        }
        measuredHeights[id] = height
    }
    readonly property int count: slots.count
    readonly property Item headerItem: headerLoader.item
    readonly property Item footerItem: footerLoader.item
    // targetCardWidth is a comfortable card size, not a hard minimum.
    // The former 85% cutoff collapsed a ~700dp reading pane to one narrow
    // centered card even though two compact cards fit comfortably.
    // 76% keeps a legible minimum while allowing 2 columns in a windowed
    // desktop and 3 columns in the wider full-screen workspace.
    readonly property real minimumCardWidth: Math.max(1, targetCardWidth * 0.76)
    readonly property int columns: Math.max(1, Math.min(maximumColumns,
        Math.floor((Math.max(0, width) - gutter) / (minimumCardWidth + gutter))))
    // Allocate the entire reading-pane width to the chosen columns. Do not
    // cap a lone card at 115% and leave unused outside slabs on either side.
    readonly property real cardWidth: Math.max(1,
        (width - gutter * (columns + 1)) / columns)
    readonly property real leadingInset: gutter
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

    // Admission must be a one-way update, NOT a binding on slot.height/y:
    // activating a Loader changes its measured height and the masonry
    // position, which formerly fed straight back into Loader.active.
    function scheduleAdmission() {
        if (!admissionTimer.running) admissionTimer.start()
    }

    function updateAdmissions() {
        const canDisplay = visible && positioned && height > 0
        const enteringTop = contentY - cacheBuffer
        const enteringBottom = contentY + height + cacheBuffer
        const retainedTop = contentY - retentionBuffer
        const retainedBottom = contentY + height + retentionBuffer
        for (let index = 0; index < slots.count; ++index) {
            const slot = slots.itemAt(index)
            if (!slot) continue
            const eligible = canDisplay && slot.placed
            const entering = eligible && slot.y + slot.height >= enteringTop
                && slot.y <= enteringBottom
            const retaining = eligible && slot.y + slot.height >= retainedTop
                && slot.y <= retainedBottom
            const nextAdmitted = slot.admitted ? retaining : entering
            if (slot.admitted !== nextAdmitted)
                slot.admitted = nextAdmitted
        }
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
        scheduleAdmission()
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

    // Qt Quick Keys exposes generic pressed/released handlers, not dedicated
    // PageUp/PageDown/Home/End signals. Handle all navigation keys here so a
    // missing attached signal cannot prevent the entire QML module loading.
    Keys.onPressed: function(event) {
        switch (event.key) {
        case Qt.Key_PageDown:
            view.scrollBy(view.height * 0.85)
            break
        case Qt.Key_PageUp:
            view.scrollBy(-view.height * 0.85)
            break
        case Qt.Key_Home:
            view.positionViewAtBeginning()
            break
        case Qt.Key_End:
            view.scrollBy(view.contentHeight)
            break
        case Qt.Key_Down:
            view.scrollBy(48)
            break
        case Qt.Key_Up:
            view.scrollBy(-48)
            break
        default:
            return
        }
        event.accepted = true
    }
    onWidthChanged: scheduleLayout()
    onHeightChanged: scheduleAdmission()
    onContentYChanged: scheduleAdmission()
    onVisibleChanged: scheduleAdmission()
    onCacheBufferChanged: scheduleAdmission()
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

    Timer {
        id: admissionTimer
        interval: 0
        onTriggered: view.updateAdmissions()
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
            property real previousWidth: 0
            property bool placed: false
            property bool admitted: false
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
            onWidthChanged: {
                const recalled = view.recalledHeight(stableKey, revisionKey, width)
                if (recalled > 0)
                    measuredHeight = recalled
                else if (previousWidth > 0 && measuredHeight > 0)
                    measuredHeight = Math.max(96, measuredHeight * width / previousWidth)
                previousWidth = width
            }
            onStableKeyChanged: {
                admitted = false
                measuredHeight = view.recalledHeight(stableKey, revisionKey, width)
                assignedColumn = -1
                view.scheduleLayout()
            }
            onRevisionKeyChanged: {
                // A changed cast payload can alter text/media height. Never
                // apply a sample from an earlier revision of that cast.
                measuredHeight = view.recalledHeight(stableKey, revisionKey, width)
            }

            function recordHeight() {
                const next = cardLoader.item ? cardLoader.item.height : 0
                if (next > 0) {
                    if (Math.abs(next - measuredHeight) >= 1)
                        measuredHeight = next
                    view.retainHeight(stableKey, revisionKey, width, next)
                }
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
                // A sampled boolean is independent of the loaded item's
                // geometry. Never bind active to slot.height or slot.y.
                active: slot.admitted
                asynchronous: false
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
