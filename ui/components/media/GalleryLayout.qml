import QtQuick
import Farcaster.Client

Item {
    id: root

    property var embeds: []
    property bool compact: false
    // Timeline keeps staged incubation; viewport-bounded Magazine cards own
    // their nested tiles synchronously to avoid canceled incubator warnings.
    property bool asynchronousTiles: true
    property var sharedMediaGeometry: null
    // A link promoted to direct media keeps its already-visible feed slot.
    property real singleImageHeight: 0
    // Feed geometry must not change after an image finishes decoding. Provider
    // metadata can shape the grid immediately; decoded dimensions are only an
    // opt-in enhancement for non-scrolling/detail surfaces.
    property bool dynamicAspectDiscovery: false
    property var discoveredAspects: ({})
    property var pendingAspects: ({})
    property var reservedAspects: ({})
    // Stagger asynchronous tile construction, but keep it progressing during
    // continuous scrolling. Waiting for a full stop starves newly visible media.
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
        // A confirmed decoded aspect from a prior visit wins over the
        // first incarnation's fallback reservation (normally 4:3).
        // Otherwise a recycled card briefly letterboxes again.
        return Number((root.sharedMediaGeometry
                          ? root.sharedMediaGeometry.aspectForKey(root.mediaKey(index)) : 0)
                      || root.reservedAspects[root.mediaKey(index)]
                      || root.initialAspect(item))
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
            next[key] = (root.sharedMediaGeometry
                             ? root.sharedMediaGeometry.aspectForKey(key) : 0)
                || root.reservedAspects[key]
                || root.initialAspect(root.embedAt(index))
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
        if (root.sharedMediaGeometry)
            root.sharedMediaGeometry.rememberAspect(key, root.clampAspect(value))
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

            // Full-width, aspect-proportional photo cards: once the image's
            // actual ratio is known, reserve its correct height rather than
            // letterboxing a narrow portrait inside a wide fixed frame.
            // Restrict extreme panoramas and tall scans, not normal portraits.
            const maximumHeight = root.dynamicAspectDiscovery
                ? Math.max(900, contentWidth * 2.35)
                : root.compact ? Math.max(720, contentWidth * 1.6) : 860
            const minimumHeight = root.dynamicAspectDiscovery ? 64 : root.compact ? 120 : 140
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
                if (tileSlot.admitted || !root.scrollHostResolved || tileAdmission.running)
                    return
                tileAdmission.restart()
            }

            Component.onCompleted: tileSlot.scheduleAdmission()

            Connections {
                target: root
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
                asynchronous: root.asynchronousTiles

                sourceComponent: Component {
                    GalleryTile {
                        embed: root.embedAt(tileSlot.index)
                        // Single-media casts must show the complete image at every
                        // breakpoint. Multi-image galleries intentionally crop tiles.
                        preserveAspect: root.visibleCount === 1
                        // Keep native gallery artwork on the direct QSG image
                        // path, avoiding one offscreen image and mask layer per
                        // common single-cast photo. Browser/WebGL rendering still needs the
                        // mask because its vector corner cover can show seams at
                        // fractional DPR. Retain Metal's mask for single photos.
                        useLayeredMask: Qt.platform.os === "wasm"
                                        || (Qt.platform.os === "osx" && root.visibleCount === 1
                                            && !root.dynamicAspectDiscovery)
                        cornerCoverColor: root.dynamicAspectDiscovery ? Theme.surfaceRaised : Theme.background
                        // A full-width, aspect-correct Magazine photo does not
                        // need an additional blurred GPU render pass.
                        ambientEnabled: root.visibleCount === 1 && !root.dynamicAspectDiscovery
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
