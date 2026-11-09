import QtQuick
import QtQuick.Controls
import Farcaster.Client

// Numeric layout lives in the native model. Repeater receives only the
// viewport-window rows: offscreen casts are not QQuickItems or QML Loaders.
Flickable {
    id: view

    property var model: null
    property Component delegate
    property Component header
    property Component footer
    property var estimateHeight
    property real targetCardWidth: Theme.scaleUi(500)
    property int maximumColumns: 2
    property int layoutDirection: Theme.uiDirection
    property real gutter: Theme.space3
    property real outsideInset: gutter
    property real cacheBuffer: Math.min(height * 1.25, 1200)
    readonly property real retentionBuffer: cacheBuffer + Math.min(1200, Math.max(420, height * 1.15))

    // Exact painted heights survive viewport eviction and reverse scrolling.
    // The native window model holds the same width/revision-specific samples.
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
        return String(key) + "|" + String(revision) + "|" + Number(width).toFixed(2)
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
        if (layoutModel.retainHeight(key, revision, width, height, true))
            scheduleLayout()
    }

    readonly property int count: model ? model.count : 0
    readonly property Item headerItem: headerLoader.item
    readonly property Item footerItem: footerLoader.item
    readonly property real minimumCardWidth: Math.max(1, targetCardWidth * 0.76)
    readonly property real usableWidth: Math.max(1, width - outsideInset * 2)
    readonly property int columns: Math.max(1, Math.min(maximumColumns,
        Math.floor((usableWidth + gutter) / (minimumCardWidth + gutter))))
    readonly property real cardWidth: Math.max(1,
        (usableWidth - gutter * (columns - 1)) / columns)
    readonly property real leadingInset: outsideInset
    readonly property real rowsBottom: layoutModel.rowsBottom
    readonly property real shortestColumnBottom: layoutModel.shortestColumnBottom
    property bool arranging: false
    property bool positioned: false
    property bool relayoutPending: false

    signal layoutStarted()
    signal layoutFinished()

    clip: true
    contentWidth: width
    contentHeight: Math.max(0, rowsBottom + footerLoader.height)
    flickableDirection: Flickable.VerticalFlick
    boundsBehavior: Flickable.DragOverBounds
    boundsMovement: Flickable.FollowBoundsBehavior
    activeFocusOnTab: true

    MasonryWindowModel {
        id: layoutModel
        objectName: "masonryWindowModel"
        sourceModel: view.model
        onGeometryInvalidated: view.scheduleLayout()
    }

    function itemAtIndex(index) {
        const windowIndex = layoutModel.windowIndexForSource(index)
        return windowIndex >= 0 ? slots.itemAt(windowIndex) : null
    }
    function indexAt(x, y) { return layoutModel.indexAt(x, y) }

    function scheduleLayout() {
        if (arranging) {
            relayoutPending = true
            return
        }
        if (!layoutTimer.running) layoutTimer.start()
    }
    function scheduleAdmission() {
        if (!admissionTimer.running) admissionTimer.start()
    }
    function updateAdmissions() {
        layoutModel.setViewport(contentY, height, cacheBuffer, retentionBuffer,
                                view.visible && positioned && height > 0)
    }
    function forceLayout() {
        if (arranging) {
            relayoutPending = true
            return
        }
        layoutTimer.stop()
        arranging = true
        layoutStarted()
        const anchorIndex = positioned && !moving && contentY > 0
            ? indexAt(width / 2, contentY + Math.min(80, height / 4)) : -1
        const anchor = anchorIndex >= 0 ? layoutModel.geometryAt(anchorIndex) : null
        const offset = anchor && anchor.stableKey ? anchor.y - contentY : 0
        layoutModel.relayout(columns, cardWidth, gutter, headerLoader.height + gutter,
                             leadingInset, layoutDirection === Qt.RightToLeft)
        if (anchor && anchor.stableKey && !moving) {
            // A source-index is not identity when rows insert above the reader.
            const restored = layoutModel.geometryForKey(anchor.stableKey)
            if (restored && restored.stableKey)
                contentY = Math.max(0, Math.min(restored.y - offset,
                    Math.max(0, contentHeight - height)))
        }
        positioned = true
        arranging = false
        scheduleAdmission()
        layoutFinished()
        if (relayoutPending) {
            relayoutPending = false
            scheduleLayout()
        }
    }
    function positionViewAtBeginning() {
        cancelFlick()
        forceLayout()
        contentY = 0
    }
    function positionViewAtIndex(index, mode) {
        cancelFlick()
        forceLayout()
        const slot = layoutModel.geometryAt(index)
        if (!slot || !slot.stableKey) return
        const offset = mode === ListView.Center ? (height - slot.height) / 2 : 0
        contentY = Math.max(0, Math.min(slot.y - offset, Math.max(0, contentHeight - height)))
    }
    function scrollBy(distance) {
        cancelFlick()
        contentY = Math.max(0, Math.min(contentY + distance, Math.max(0, contentHeight - height)))
    }
    Keys.onPressed: function(event) {
        switch (event.key) {
        case Qt.Key_PageDown: view.scrollBy(view.height * 0.85); break
        case Qt.Key_PageUp: view.scrollBy(-view.height * 0.85); break
        case Qt.Key_Home: view.positionViewAtBeginning(); break
        case Qt.Key_End: view.scrollBy(view.contentHeight); break
        case Qt.Key_Down: view.scrollBy(48); break
        case Qt.Key_Up: view.scrollBy(-48); break
        default: return
        }
        event.accepted = true
    }
    onModelChanged: scheduleLayout()
    onWidthChanged: scheduleLayout()
    onHeightChanged: scheduleAdmission()
    onContentYChanged: scheduleAdmission()
    onVisibleChanged: scheduleAdmission()
    onCacheBufferChanged: scheduleAdmission()
    onColumnsChanged: scheduleLayout()
    onCardWidthChanged: scheduleLayout()
    onLayoutDirectionChanged: scheduleLayout()
    onGutterChanged: scheduleLayout()
    onOutsideInsetChanged: scheduleLayout()
    Component.onCompleted: scheduleLayout()

    // Collect text/media measurements into one geometry commit per frame.
    Timer { id: layoutTimer; interval: 16; onTriggered: view.forceLayout() }
    Timer { id: admissionTimer; interval: 0; onTriggered: view.updateAdmissions() }

    Loader {
        id: headerLoader
        width: view.width
        sourceComponent: view.header
        onHeightChanged: view.scheduleLayout()
    }

    Repeater {
        id: slots
        model: layoutModel
        delegate: Item {
            id: slot
            objectName: "masonrySlot"
            required property int sourceIndex
            required property string stableKey
            required property string revisionKey
            required property string presentationKind
            required property string mediaLayoutKind
            required property real cardX
            required property real cardY
            required property real cardHeight
            property real measuredHeight: view.recalledHeight(stableKey, revisionKey, width)
            property bool placed: true
            property bool admitted: true
            x: cardX
            y: cardY
            width: view.cardWidth
            height: Math.max(1, cardHeight)
            clip: cardLoader.item === null

            function offerEstimate() {
                if (!view.estimateHeight || view.recalledHeight(stableKey, revisionKey, width) > 0)
                    return
                const hint = Number(view.estimateHeight(stableKey, presentationKind,
                                                        mediaLayoutKind, width))
                if (hint > 0 && isFinite(hint)
                        && layoutModel.retainHeight(stableKey, revisionKey, width, hint, false))
                    view.scheduleLayout()
            }
            function recordHeight() {
                const content = cardLoader.item
                const implicit = content ? Number(content.implicitHeight || 0) : 0
                const next = implicit > 0
                    ? implicit : content ? Number(content.height || 0) : 0
                if (next > 0 && isFinite(next)) {
                    const resolved = Math.ceil(next)
                    if (Math.abs(resolved - measuredHeight) >= 1)
                        measuredHeight = resolved
                    view.retainHeight(stableKey, revisionKey, width, resolved)
                }
            }
            Component.onCompleted: offerEstimate()
            onStableKeyChanged: offerEstimate()
            onRevisionKeyChanged: {
                const previousPaintedHeight = measuredHeight
                measuredHeight = view.recalledHeight(stableKey, revisionKey, width)
                // A like/recast changes the row revision but not the cast's
                // text/media geometry. Retain its old painted height until
                // the updated content reports its own implicitHeight.
                if (!(previousPaintedHeight > 0)) offerEstimate()
            }
            onWidthChanged: offerEstimate()

            Loader {
                id: visiblePlaceholder
                anchors.fill: parent
                active: view.visible && slot.placed
                        && slot.y + slot.height > view.contentY
                        && slot.y < view.contentY + view.height
                        && (cardLoader.item === null
                            || Number(cardLoader.item.resolvedHeight
                                      || cardLoader.item.implicitHeight || 0) <= 0)
                asynchronous: false
                sourceComponent: Component {
                    Item {
                        Superellipse {
                            anchors.fill: parent
                            fillColor: Theme.surfaceRaised
                            borderWidth: 0
                            curveDepth: Theme.curveContainer
                        }
                        CastSkeleton {
                            width: parent.width
                            height: Math.min(parent.height, implicitHeight)
                            mediaPreview: slot.mediaLayoutKind !== "other"
                            animate: !view.moving
                        }
                    }
                }
            }
            Loader {
                id: cardLoader
                readonly property int rowIndex: slot.sourceIndex
                readonly property string stableKey: slot.stableKey
                readonly property string revisionKey: slot.revisionKey
                readonly property string presentationKind: slot.presentationKind
                readonly property string mediaLayoutKind: slot.mediaLayoutKind
                readonly property real slotY: slot.y
                width: slot.width
                active: slot.admitted
                // Incubate costly CastItem trees over multiple frames.
                asynchronous: true
                sourceComponent: view.delegate
                onLoaded: slot.recordHeight()
            }
            Connections {
                target: cardLoader.item
                function onHeightChanged() { slot.recordHeight() }
                function onImplicitHeightChanged() { slot.recordHeight() }
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
