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
    property real cacheBuffer: Qt.platform.os === "wasm" ? Math.min(height * 0.35, 280) : Math.min(height * 1.25, 1200)
    readonly property real retentionBuffer: cacheBuffer + Math.min(Qt.platform.os === "wasm" ? 240 : 1200,
        Math.max(Qt.platform.os === "wasm" ? 120 : 420, height * 1.15))

    // Exact painted heights survive viewport eviction and reverse scrolling.
    // The native window model holds the same width/revision-specific samples.
    property var measuredHeights: ({})
    property var measuredOrder: []
    property var aspectMemory: ({})
    property var aspectOrder: []
    // A reservation belongs to a specific cast/media identity, not a texture.
    // Keep it across delegate destruction and reverse scrolling; a separate
    // cache below remembers intrinsic metadata for new presentations.
    property var mediaReservations: Object.create(null)
    property var mediaReservationOrder: []
    property var exposedMediaRows: Object.create(null)
    property var exposedMediaOrder: []
    function markMediaRowExposed(key) {
        const id = String(key || "")
        if (!id || exposedMediaRows[id]) return
        exposedMediaRows[id] = true
        exposedMediaOrder.push(id)
        if (exposedMediaOrder.length > 8192)
            delete exposedMediaRows[exposedMediaOrder.shift()]
    }
    function wasMediaRowExposed(key) {
        return Boolean(exposedMediaRows[String(key || "")])
    }
    function reserveMediaAspect(key, proposed) {
        const id = String(key || "")
        const value = Number(proposed)
        if (!id || !(value > 0) || !isFinite(value)) return 4 / 3
        if (Object.prototype.hasOwnProperty.call(mediaReservations, id))
            return mediaReservations[id]
        mediaReservationOrder.push(id)
        if (mediaReservationOrder.length > 8192)
            delete mediaReservations[mediaReservationOrder.shift()]
        mediaReservations[id] = value
        return value
    }
    function refineMediaAspect(key, ratio) {
        const id = String(key || "")
        const value = Number(ratio)
        if (!id || !(value > 0) || !isFinite(value)) return
        reserveMediaAspect(id, value)
        mediaReservations[id] = value
    }
    function aspectForKey(key) {
        return Number(aspectMemory[String(key)] || 0)
    }
    function rememberAspect(key, ratio) {
        const id = String(key || "")
        const value = Number(ratio || 0)
        if (!id || !(value > 0) || !isFinite(value)) return
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
        if (resizeInProgress) {
            if (layoutModel.retainHeight(key, revision, width, height, true, true))
                scheduleLayout()
            return
        }
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
    property bool resizeInProgress: false
    property real committedCardWidth: 1
    function viewportResized() {
        if (!positioned || !visible) {
            scheduleLayout()
            return
        }
        resizeInProgress = true
        resizeSettleTimer.restart()
        scheduleLayout()
    }
    Timer {
        id: resizeSettleTimer
        interval: 160
        repeat: false
        onTriggered: {
            view.resizeInProgress = false
            view.forceLayout()
        }
    }

    signal layoutStarted()
    signal layoutFinished()

    clip: true
    // A single passive hover observer covers the masonry gutters, including
    // four-way intersections. Each admitted card receives its own local
    // coordinates; no offscreen item or extra hit target is instantiated.
    HoverHandler {
        id: masonryPointer
        enabled: view.visible && Theme.neonEnabled && !view.moving
        blocking: false
    }
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
        if (!visible) return
        if (!layoutTimer.running) layoutTimer.start()
    }
    function scheduleAdmission() {
        if (!admissionTimer.running) admissionTimer.start()
    }
    function updateAdmissions() {
        // A zero-delay viewport timer can run during layout invalidation;
        // only commit admissions against the completed numeric geometry.
        if (arranging || layoutTimer.running)
            return
        layoutModel.setViewport(contentY, height, cacheBuffer, retentionBuffer,
                                view.visible && positioned && height > 0,
                                view.resizeInProgress)
    }
    function forceLayout() {
        if (arranging) {
            relayoutPending = true
            return
        }
        layoutTimer.stop()
        arranging = true
        layoutStarted()
        // A tall-media column can leave the centre of the viewport empty.
        // Probe further down for a real cast before relayout so source
        // geometry changes preserve the reader's stable-key scroll anchor.
        let anchorIndex = -1
        if (positioned && !moving && contentY > 0) {
            const probes = [Math.min(80, height / 4), height * 0.35, height * 0.7]
            for (let i = 0; i < probes.length && anchorIndex < 0; ++i)
                anchorIndex = indexAt(width / 2, contentY + probes[i])
        }
        const anchor = anchorIndex >= 0 ? layoutModel.geometryAt(anchorIndex) : null
        const offset = anchor && anchor.stableKey ? anchor.y - contentY : 0
        // Apply the card width in the same geometry transaction, then
        // restore the reading anchor before admitting/evicting anything.
        committedCardWidth = cardWidth
        layoutModel.relayout(columns, committedCardWidth, gutter, headerLoader.height + gutter,
                             leadingInset, layoutDirection === Qt.RightToLeft, true)
        if (anchor && anchor.stableKey && !moving) {
            // A source-index is not identity when rows insert above the reader.
            const restored = layoutModel.geometryForKey(anchor.stableKey)
            if (restored && restored.stableKey)
                contentY = Math.max(0, Math.min(restored.y - offset,
                    Math.max(0, contentHeight - height)))
        }
        positioned = true
        arranging = false
        updateAdmissions()
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
    onWidthChanged: viewportResized()
    onHeightChanged: {
        viewportResized()
        scheduleAdmission()
    }
    onContentYChanged: scheduleAdmission()
    onVisibleChanged: {
        if (!visible) {
            resizeSettleTimer.stop()
            resizeInProgress = false
            layoutModel.setViewport(contentY, height, cacheBuffer, retentionBuffer, false, false)
        } else {
            scheduleLayout()
        }
    }
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
            // A revision update (especially Like/Recast) can briefly shrink
            // the nested Qt Layout before it completes rearrangement.
            // Grow immediately, but only commit a smaller height after the
            // row has settled over another event-loop/frame boundary.
            property bool revisionSettling: false
            property bool placed: true
            property bool admitted: true
            readonly property var loadedCard: cardLoader.item
            readonly property real paintedHeight: !loadedCard ? 0
                : loadedCard.resolvedHeight !== undefined
                  ? Number(loadedCard.resolvedHeight || 0)
                  : Math.max(Number(loadedCard.magazinePaintHeight || 0),
                      Number(loadedCard.implicitHeight || loadedCard.height || 0))
            readonly property bool contentReady: cardLoader.status === Loader.Ready
                && loadedCard !== null && loadedCard.incubating !== true
                && paintedHeight > 0
            // Do not reveal an actual card inside the previous estimated slot.
            // Once revealed, transient inner Layout notifications cannot
            // toggle its opacity or bring the skeleton back.
            property bool cardPresented: false
            readonly property bool cardReady: cardLoader.status === Loader.Ready
                && loadedCard !== null && (cardPresented
                    || (contentReady && Math.abs(Math.ceil(paintedHeight) - cardHeight) <= 1))
            onCardReadyChanged: if (cardReady) cardPresented = true
            readonly property bool intersectsViewport: view.visible && view.positioned
                && y + height > view.contentY && y < view.contentY + view.height
            property bool everExposed: false
            function markExposed() {
                everExposed = true
                view.markMediaRowExposed(stableKey)
            }
            onIntersectsViewportChanged: if (intersectsViewport) markExposed()
            // Refining an unseen slot below the reader cannot push visible
            // cards away. A card already seen (including reverse scroll) is
            // not allowed to change its media reservation.
            function mayRefineMediaGeometry() {
                return !everExposed && !view.wasMediaRowExposed(stableKey)
                    && !intersectsViewport && view.positioned && !view.moving
                    && y >= view.contentY + view.height + view.gutter
            }
            x: cardX
            y: cardY
            width: view.committedCardWidth
            height: Math.max(1, cardHeight)
            // The native slot boundary is authoritative. During a pending
            // Like or delayed rich preview, prevent a temporarily oversized
            // child from painting over the next independently placed card.
            // Once the actual painted extent is measured the native lane grows.
            clip: true

            function offerEstimate() {
                if (view.resizeInProgress || !view.estimateHeight || view.recalledHeight(stableKey, revisionKey, width) > 0)
                    return
                const hint = Number(view.estimateHeight(stableKey, presentationKind,
                                                        mediaLayoutKind, width))
                if (hint > 0 && isFinite(hint)
                        && layoutModel.retainHeight(stableKey, revisionKey, width, hint, false))
                    view.scheduleLayout()
            }
            function recordHeight() {
                const content = cardLoader.item
                if (!content || content.incubating === true)
                    return

                // A nested FeedRow Loader paints a provisional, potentially
                // much taller reservedHeight before SemanticFeedItem is ready.
                // Loader.height is NOT evidence of painted content: caching it
                // as an exact sample leaves a permanently empty 200-400px slot.
                // Prefer the inner item's resolvedHeight, not the outer Loader
                // reservation, even if the outer implicitHeight is nonzero.
                const hasResolvedHeight = content.resolvedHeight !== undefined
                const next = hasResolvedHeight
                    ? Number(content.resolvedHeight || 0)
                    : Math.max(Number(content.magazinePaintHeight || 0),
                        Number(content.implicitHeight || content.height || 0))
                if (next > 0 && isFinite(next)) {
                    const resolved = Math.ceil(next)
                    if (revisionSettling
                            && resolved < Math.max(cardHeight, measuredHeight) - 1)
                        return
                    if (Math.abs(resolved - measuredHeight) >= 1)
                        measuredHeight = resolved
                    view.retainHeight(stableKey, revisionKey, width, resolved)
                }
            }
            Component.onCompleted: {
                if (intersectsViewport) markExposed()
                if (cardReady) cardPresented = true
                offerEstimate()
            }
            onStableKeyChanged: {
                cardPresented = false
                everExposed = view.wasMediaRowExposed(stableKey)
                if (intersectsViewport) markExposed()
                offerEstimate()
            }
            onRevisionKeyChanged: {
                revisionSettling = true
                revisionSettleTimer.restart()
                const previousPaintedHeight = measuredHeight
                measuredHeight = view.recalledHeight(stableKey, revisionKey, width)
                // A like/recast changes the row revision but not the cast's
                // text/media geometry. Retain its old painted height until
                // the updated content reports its own implicitHeight.
                if (!(previousPaintedHeight > 0)) offerEstimate()
            }
            onWidthChanged: {
                // Measure the actual rewrapped card after Qt Layout has
                // processed its new width; avoid a guessed height overwrite.
                widthMeasureTimer.restart()
            }
            Connections {
                target: view
                function onResizeInProgressChanged() {
                    if (!view.resizeInProgress)
                        widthMeasureTimer.restart()
                }
            }
            Timer {
                id: widthMeasureTimer
                interval: 16
                repeat: false
                onTriggered: {
                    if (slot.contentReady) slot.recordHeight()
                    else slot.offerEstimate()
                }
            }

            Timer {
                id: revisionSettleTimer
                interval: 48
                repeat: false
                onTriggered: {
                    slot.revisionSettling = false
                    slot.recordHeight()
                }
            }

            Loader {
                id: visiblePlaceholder
                objectName: "masonrySkeleton"
                anchors.fill: parent
                z: 2
                enabled: false
                active: view.visible && slot.placed
                        && slot.y + slot.height > view.contentY
                        && slot.y < view.contentY + view.height
                        && (!slot.cardReady || opacity > 0.01)
                opacity: slot.cardReady ? 0 : 1
                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.reduceMotion || view.moving ? 0 : Theme.motionStandard
                        easing.type: Easing.OutCubic
                    }
                }
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
                objectName: "masonryCast"
                readonly property int rowIndex: slot.sourceIndex
                readonly property string stableKey: slot.stableKey
                readonly property string revisionKey: slot.revisionKey
                readonly property string presentationKind: slot.presentationKind
                readonly property string mediaLayoutKind: slot.mediaLayoutKind
                readonly property real slotY: slot.y
                readonly property bool sharedNeonPointerActive: masonryPointer.hovered && !view.moving
                readonly property real sharedNeonPointerX: masonryPointer.point.position.x - slot.x
                readonly property real sharedNeonPointerY: masonryPointer.point.position.y
                    + view.contentY - slot.y
                width: slot.width
                active: slot.admitted
                // Crossfade the hydrated card under the existing skeleton.
                // Opacity is a scene graph operation; no height animation or
                // extra image layer is needed for large image-heavy feeds.
                opacity: slot.cardReady ? 1 : 0
                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.reduceMotion || view.moving ? 0 : Theme.motionStandard
                        easing.type: Easing.OutCubic
                    }
                }
                // Incubate costly CastItem trees over multiple frames.
                asynchronous: true
                sourceComponent: view.delegate
                onLoaded: slot.scheduleHeightMeasurement()
            }
            // Coalesce Layout's intermediate implicit-height notifications.
            // A new measurement is committed at most once per event turn;
            // the old measured slot stays in place during incubation.
            function scheduleHeightMeasurement() {
                if (!heightMeasurementTimer.running) heightMeasurementTimer.start()
            }
            Timer {
                id: heightMeasurementTimer
                interval: 0
                repeat: false
                onTriggered: slot.recordHeight()
            }
            Connections {
                target: cardLoader.item
                function onResolvedHeightChanged() { slot.scheduleHeightMeasurement() }
                function onIncubatingChanged() { slot.scheduleHeightMeasurement() }
                function onHeightChanged() { slot.scheduleHeightMeasurement() }
                function onImplicitHeightChanged() { slot.scheduleHeightMeasurement() }
                ignoreUnknownSignals: true
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
