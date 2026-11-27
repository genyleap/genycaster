import QtQuick
import Farcaster.Client

// Non-visual admission contract shared by media consumers. Viewport visibility
// is distinct from Item.visible (ListView cache-buffer rows remain visible).
QtObject {
    id: root
    required property Item owner
    property bool observeViewport: true
    property real viewportMargin: 0
    // Some raster items hide themselves until Ready (favicon/fallback).
    // Admission must not depend on that item's own presentation state.
    property bool requireOwnerVisible: true
    readonly property bool foreground: Qt.application.state !== Qt.ApplicationSuspended
        && Qt.application.state !== Qt.ApplicationHidden
        && ((Qt.platform.os !== "android" && Qt.platform.os !== "ios")
             || Qt.application.state === Qt.ApplicationActive)
    readonly property bool eligible: owner !== null
        && (requireOwnerVisible ? owner.visible : (owner.parent !== null && owner.parent.visible))
        && foreground
        && (!observeViewport || intersectsViewport())

    // Reserve before asynchronous incubation, not after it. A subsequent Play
    // revokes this pending intent even if no QMediaPlayer exists yet.
    signal revoked()
    function coordinator() {
        return typeof mediaPlayback !== "undefined" && mediaPlayback !== null ? mediaPlayback : null
    }
    function reservePlayback() {
        if (!eligible) return false
        const session = coordinator()
        return !session || session.claimPrimary(root)
    }
    function handoffPlayback() {
        const session = coordinator()
        if (!eligible || (session && !session.isPrimary(root))) return false
        if (session) session.release(root)
        return true
    }
    function releaseReservation() {
        const session = coordinator()
        if (session) session.release(root)
    }
    function suspendFromPlaybackCoordinator() { root.revoked() }
    Component.onDestruction: releaseReservation()

    function intersectsViewport() {
        if (!owner || owner.width <= 0 || owner.height <= 0)
            return false
        // Walk ancestors so QML tracks position/size dependencies as well as
        // Flickable.contentX/Y. Nested scroll/clip containers all must admit.
        let item = owner
        let x = 0
        let y = 0
        while (item && item.parent) {
            x += item.x
            y += item.y
            const parent = item.parent
            if (parent.contentX !== undefined && parent.contentY !== undefined
                    && parent.contentItem !== undefined) {
                const cx = parent.contentX
                const cy = parent.contentY
                // contentItem's x/y already include the scroll translation.
                // Read the offsets explicitly to track changes on all backends.
                const mapped = owner.mapToItem(parent, 0, 0)
                if (!isFinite(x) || !isFinite(y) || !isFinite(cx) || !isFinite(cy)
                        || mapped.x + owner.width <= -viewportMargin || mapped.y + owner.height <= -viewportMargin
                        || mapped.x >= parent.width + viewportMargin || mapped.y >= parent.height + viewportMargin)
                    return false
            }
            item = parent
        }
        return true
    }
}
