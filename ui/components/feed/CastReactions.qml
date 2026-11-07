pragma Singleton
import QtQuick
import Farcaster.Client

// The browser reaction relay is a native Qt adapter, including on WebAssembly.
// QML owns no HTTP, JSON parsing, fetch, retry timer or global request queue.
// Cached positives and local writes remain separate: unknown is never false.
QtObject {
    id: root

    readonly property var adapter: typeof browserReactionController !== "undefined"
        ? browserReactionController : null
    property int revision: 0
    property Connections adapterConnection: Connections {
        target: root.adapter
        function onRevisionChanged() {
            root.revision = root.adapter ? root.adapter.revision : 0
        }
    }

    function keyFor(castId, viewerFid) {
        const hash = CastViews.normalizeHash(castId)
        const fid = Number(viewerFid)
        return hash && isFinite(fid) && Number.isSafeInteger(fid) && fid > 0
            ? String(fid) + ":" + hash : ""
    }

    function reacted(castId, viewerFid, kind) {
        const observedRevision = root.revision
        return root.adapter ? root.adapter.reacted(castId, viewerFid, kind) : false
    }

    function localValue(castId, viewerFid, kind) {
        const observedRevision = root.revision
        if (!root.adapter) return null
        const value = root.adapter.localValue(castId, viewerFid, kind)
        return value === null || value === undefined ? null : Boolean(value)
    }

    function setLocal(castId, viewerFid, kind, active) {
        if (root.adapter)
            root.adapter.setLocal(castId, viewerFid, kind, Boolean(active))
    }

    function retain(castId, viewerFid) {
        if (root.adapter)
            root.adapter.retain(castId, viewerFid)
    }

    function release(castId, viewerFid) {
        if (root.adapter)
            root.adapter.release(castId, viewerFid)
    }

    function ensure(castId, viewerFid) {
        if (root.adapter)
            root.adapter.ensure(castId, viewerFid)
    }
}
