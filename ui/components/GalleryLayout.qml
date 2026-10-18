import QtQuick

Item {
    id: root

    property var embeds: []
    property bool compact: false
    // A link promoted to direct media keeps its already-visible feed slot.
    property real singleImageHeight: 0
    // Feed geometry must not change after an image finishes decoding. Provider
    // metadata can shape the grid immediately; decoded dimensions are only an
    // opt-in enhancement for non-scrolling/detail surfaces.
    property bool dynamicAspectDiscovery: false
    property var discoveredAspects: ({})
    property var pendingAspects: ({})
    property var reservedAspects: ({})
    // Building a GalleryTile allocates image state, vector corner geometry and
    // optional effect machinery. Do not construct that subtree while the owning
    // ListView is moving; keep the already-reserved slot as a cheap placeholder
    // and admit tiles in small batches after the gesture settles.
    property var scrollHost: null
    property bool scrollHostResolved: false
    readonly property bool scrollMoving: root.scrollHost !== null
                                         && Boolean(root.scrollHost.moving)
    readonly property int itemCount: root.embeds ? root.embeds.length : 0
    readonly property int visibleCount: Math.min(root.itemCount, 6)
    readonly property real gap: Theme.space2
    readonly property var layoutPlan: root.buildLayout(Math.max(1, root.width))
    readonly property real planHeight: root.layoutHeight(root.layoutPlan)
    readonly property real shadowTail: root.visibleCount > 0
                                       ? (root.visibleCount === 1
                                          ? (root.compact ? Theme.space2 : Theme.space5)
                                          : Theme.space2) : 0

    signal openRequested(var mediaItems, int index)

    function resolveScrollHost() {
        let candidate = root.parent
        while (candidate !== null) {
            if (candidate.moving !== undefined && candidate.contentY !== undefined) {
                root.scrollHost = candidate
                root.scrollHostResolved = true
                return
            }
            candidate = candidate.parent
        }
        root.scrollHost = null
        root.scrollHostResolved = true
    }

    Component.onCompleted: root.resolveScrollHost()
    onParentChanged: {
        if (root.scrollHostResolved)
            root.resolveScrollHost()
    }

    function embedAt(index) {
        if (index < 0 || index >= root.itemCount)
            return ({})
        return root.embeds[index] || ({})
    }

    function mediaKey(index) {
        const item = root.embedAt(index)
        return String(item.url || item.imageSource || item.value || index)
    }

    function clampAspect(value) {
        const ratio = Number(value || 0)
        return Math.max(0.45, Math.min(2.8, ratio > 0 ? ratio : 4 / 3))
    }

    function declaredAspect(index) {
        const item = root.embedAt(index)
        // Only detail surfaces that explicitly opt in may use decoded ratios.
        // Feed layouts retain their initial metadata/fallback reservation.
        const discovered = Number(root.discoveredAspects[root.mediaKey(index)] || 0)
        if (root.dynamicAspectDiscovery && discovered > 0)
            return root.clampAspect(discovered)
        return Number(root.reservedAspects[root.mediaKey(index)] || root.initialAspect(item))
    }

    function initialAspect(item) {
        const ratio = Number(item.aspectRatio || 0)
        if (ratio > 0)
            return root.clampAspect(ratio)
        const widthValue = Number(item.width || 0)
        const heightValue = Number(item.height || 0)
        if (widthValue > 0 && heightValue > 0)
            return root.clampAspect(widthValue / heightValue)
        return root.clampAspect(0)
    }

    onEmbedsChanged: {
        // Keep the reservation for unchanged media identities when a model
        // refresh adds dimensions later. Drop removed entries to bound state.
        const next = ({})
        for (let index = 0; index < root.visibleCount; ++index) {
            const key = root.mediaKey(index)
            next[key] = root.reservedAspects[key] || root.initialAspect(root.embedAt(index))
        }
        root.reservedAspects = next
    }

    function scrollingAncestor() {
        let candidate = root.parent
        while (candidate) {
            if (candidate.moving !== undefined && candidate.contentY !== undefined)
                return candidate
            candidate = candidate.parent
        }
        return null
    }

    function rememberAspect(index, ratio) {
        if (!root.dynamicAspectDiscovery) return
        const value = Number(ratio || 0)
        if (!(value > 0))
            return
        const key = root.mediaKey(index)
        const previous = Number(root.discoveredAspects[key] || 0)
        if (previous > 0 && Math.abs(previous - value) < 0.015)
            return
        const nextPending = Object.assign({}, root.pendingAspects)
        nextPending[key] = root.clampAspect(value)
        root.pendingAspects = nextPending
        aspectCommit.restart()
    }

    function commitPendingAspects() {
        const scroller = root.scrollingAncestor()
        if (scroller && scroller.moving) {
            aspectCommit.restart()
            return
        }
        const keys = Object.keys(root.pendingAspects)
        if (keys.length === 0)
            return
        const next = Object.assign({}, root.discoveredAspects)
        for (let index = 0; index < keys.length; ++index)
            next[keys[index]] = root.pendingAspects[keys[index]]
        root.pendingAspects = ({})
        root.discoveredAspects = next
    }

    Timer {
        id: aspectCommit
        interval: 180
        repeat: false
        onTriggered: root.commitPendingAspects()
    }

    function boundedHeight(rawHeight, minimumHeight, maximumHeight) {
        return Math.max(minimumHeight, Math.min(maximumHeight, rawHeight))
    }

    function buildLayout(availableWidth) {
        const plan = []
        if (root.visibleCount <= 0 || availableWidth <= 1)
            return plan

        const minimumTileHeight = root.compact ? 118 : 138
        const maximumTileHeight = root.compact ? 350 : 470

        if (root.visibleCount === 1) {
            const outerWidth = availableWidth
            const contentWidth = Math.min(outerWidth, root.compact ? outerWidth : 860)
            const contentOffset = Math.max(0, (outerWidth - contentWidth) / 2)
            const aspect = root.declaredAspect(0)

            // Fit the complete image inside the reserved frame. Decoding cannot
            // change this plan in the feed, even after a gesture has settled.
            const maximumHeight = root.compact ? Math.max(720, contentWidth * 1.6) : 860
            const minimumHeight = root.compact ? 120 : 140
            const heightValue = root.singleImageHeight > 0 ? root.singleImageHeight
                              : root.boundedHeight(contentWidth / aspect, minimumHeight, maximumHeight)
            plan.push({ x: Math.round(contentOffset), y: 0,
                        width: Math.round(contentWidth), height: Math.round(heightValue) })
            return plan
        }

        const oneColumn = availableWidth < (root.compact ? 360 : 430)
        if (oneColumn) {
            let yValue = 0
            for (let index = 0; index < root.visibleCount; ++index) {
                const aspect = root.declaredAspect(index)
                const heightValue = root.boundedHeight(availableWidth / aspect,
                                                       minimumTileHeight,
                                                       maximumTileHeight)
                plan.push({ x: 0, y: Math.round(yValue), width: Math.round(availableWidth),
                            height: Math.round(heightValue) })
                yValue += heightValue + root.gap
            }
            return plan
        }

        const columnWidth = (availableWidth - root.gap) / 2
        let columnY = [0, 0]
        for (let index = 0; index < root.visibleCount; ++index) {
            const aspect = root.declaredAspect(index)
            const isPanorama = aspect >= 1.72
            if (isPanorama) {
                const yValue = Math.max(columnY[0], columnY[1])
                const heightValue = root.boundedHeight(availableWidth / aspect,
                                                       minimumTileHeight,
                                                       root.compact ? 280 : 330)
                plan.push({ x: 0, y: Math.round(yValue), width: Math.round(availableWidth),
                            height: Math.round(heightValue) })
                const nextY = yValue + heightValue + root.gap
                columnY = [nextY, nextY]
                continue
            }

            const column = columnY[0] <= columnY[1] ? 0 : 1
            const heightValue = root.boundedHeight(columnWidth / aspect,
                                                   minimumTileHeight,
                                                   maximumTileHeight)
            plan.push({ x: Math.round(column * (columnWidth + root.gap)),
                        y: Math.round(columnY[column]),
                        width: Math.round(columnWidth),
                        height: Math.round(heightValue) })
            columnY[column] += heightValue + root.gap
        }
        return plan
    }

    function layoutHeight(plan) {
        let bottom = 0
        for (let index = 0; index < plan.length; ++index)
            bottom = Math.max(bottom, Number(plan[index].y || 0) + Number(plan[index].height || 0))
        return Math.round(bottom)
    }

    implicitHeight: root.planHeight + root.shadowTail

    Repeater {
        model: root.visibleCount

        delegate: Item {
            id: tileSlot
            required property int index
            readonly property var slot: root.layoutPlan[index] || ({ x: 0, y: 0, width: 1, height: 1 })
            property bool admitted: false

            function scheduleAdmission() {
                if (tileSlot.admitted || !root.scrollHostResolved || root.scrollMoving)
                    return
                tileAdmission.restart()
            }

            Component.onCompleted: tileSlot.scheduleAdmission()

            Connections {
                target: root
                function onScrollMovingChanged() {
                    if (!root.scrollMoving)
                        tileSlot.scheduleAdmission()
                    else
                        tileAdmission.stop()
                }
                function onScrollHostResolvedChanged() {
                    tileSlot.scheduleAdmission()
                }
            }

            Timer {
                id: tileAdmission
                // Stagger component creation independently from the image request
                // delay. This keeps Shape/QML object construction and later texture
                // work from landing in the same frame for adjacent gallery tiles.
                interval: 16 + tileSlot.index * 32
                repeat: false
                onTriggered: {
                    if (root.scrollMoving)
                        return
                    tileSlot.admitted = true
                }
            }

            x: Number(slot.x || 0)
            y: Number(slot.y || 0)
            width: Math.max(1, Number(slot.width || 1))
            height: Math.max(1, Number(slot.height || 1))

            // Repeater normally completes every nested GalleryTile in one GUI-thread
            // burst. Keep the already-computed slot in place while each heavy media
            // subtree is incubated across frames instead.
            Rectangle {
                anchors.fill: parent
                color: Theme.surface
                visible: tileLoader.status !== Loader.Ready
            }

            Loader {
                id: tileLoader
                objectName: "galleryTileLoader"
                anchors.fill: parent
                active: tileSlot.admitted
                asynchronous: true

                sourceComponent: Component {
                    GalleryTile {
                        embed: root.embedAt(tileSlot.index)
                        // Single-media casts must show the complete image at every
                        // breakpoint. Multi-image galleries intentionally crop tiles.
                        preserveAspect: root.visibleCount === 1
                        // Keep feed artwork on the direct scene-graph texture path.
                        // GalleryTile's vector corner covers provide the visual crop
                        // without allocating a full-size offscreen layer + MultiEffect,
                        // including the very common single-image post.
                        // The inverse-vector corner cover occasionally leaves
                        // one-pixel seams on Safari/WebGL at fractional DPR.
                        // Use the same image-layer mask across all WASM tiles.
                        // Use one alpha mask for the large single image on
                        // Metal and for all browser/WebGL gallery tiles. This
                        // removes inverse-vector antialias seams without adding
                        // extra offscreen layers to every native mosaic tile.
                        useLayeredMask: root.visibleCount === 1 || Qt.platform.os === "wasm"
                        ambientEnabled: root.visibleCount === 1
                        loadDelayMs: tileSlot.index * 48
                        continuationText: tileSlot.index === root.visibleCount - 1
                                          && root.itemCount > root.visibleCount
                                          ? "+" + String(root.itemCount - root.visibleCount) : ""
                        onOpenRequested: root.openRequested(root.embeds, tileSlot.index)
                        onAspectRatioResolved: ratio => root.rememberAspect(tileSlot.index, ratio)
                    }
                }
            }
        }
    }
}
