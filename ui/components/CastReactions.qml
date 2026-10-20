pragma Singleton
import QtQuick

// Public positive reaction evidence for WebAssembly, keyed by signed-in FID.
// Never infer a negative status from a partial public reaction list. Like
// CastViews, this singleton survives recycled delegates and never updates the
// feed/media model while the user scrolls.
QtObject {
    id: root

    property int revision: 0
    property bool busy: false
    property var positive: ({})
    property var local: ({})
    property var pending: ({})
    property var retryAfter: ({})

    property Timer dispatchTimer: Timer {
        interval: 700
        repeat: false
        onTriggered: root.dispatch()
    }

    function validFid(value) {
        const number = Number(value)
        return isFinite(number) && Math.floor(number) === number
               && number > 0 && number <= 9007199254740991 ? number : 0
    }

    function keyFor(castId, viewerFid) {
        const hash = CastViews.normalizeHash(castId)
        const fid = root.validFid(viewerFid)
        return hash && fid ? String(fid) + ":" + hash : ""
    }

    function reacted(castId, viewerFid, kind) {
        const observedRevision = root.revision
        const key = root.keyFor(castId, viewerFid) + ":" + kind
        if (root.local[key] !== undefined)
            return root.local[key] === true
        return root.positive[key] === true
    }

    function localValue(castId, viewerFid, kind) {
        const observedRevision = root.revision
        const key = root.keyFor(castId, viewerFid) + ":" + kind
        return root.local[key] === undefined ? null : root.local[key] === true
    }

    function setLocal(castId, viewerFid, kind, active) {
        const key = root.keyFor(castId, viewerFid)
        if (!key || (kind !== "like" && kind !== "recast"))
            return
        root.local[key + ":" + kind] = Boolean(active)
        root.revision += 1
    }

    function ensure(castId, viewerFid) {
        if (Qt.platform.os !== "wasm")
            return
        const key = root.keyFor(castId, viewerFid)
        if (!key || Number(root.retryAfter[key] || 0) > Date.now()
                || root.pending[key] !== undefined)
            return
        // An unbounded QML singleton queue would retain every cast a long
        // scrolling session has ever crossed, even after delegates recycle.
        // Prefer the newest visible interests; evicted work can be retried
        // when that cast returns to the viewport.
        const waiting = Object.keys(root.pending)
        if (waiting.length >= 32) {
            const obsolete = waiting[0]
            delete root.pending[obsolete]
            delete root.retryAfter[obsolete]
            delete root.positive[obsolete + ":like"]
            delete root.positive[obsolete + ":recast"]
        }
        // Positive evidence is opportunistic; retain only a bounded recent
        // window. Never evict the separate optimistic local-write state.
        const retained = Object.keys(root.retryAfter)
        if (retained.length >= 256) {
            const obsolete = retained[0]
            delete root.retryAfter[obsolete]
            delete root.positive[obsolete + ":like"]
            delete root.positive[obsolete + ":recast"]
        }
        const split = key.indexOf(":")
        root.pending[key] = {castId: key.slice(split + 1), viewerFid: Number(key.slice(0, split))}
        // A recycled row should not immediately repeat a network lookup.
        root.retryAfter[key] = Date.now() + 5 * 60 * 1000
        if (!root.busy)
            dispatchTimer.restart()
    }

    function dispatch() {
        if (root.busy)
            return
        const keys = Object.keys(root.pending).slice(0, 8)
        if (!keys.length)
            return
        const first = root.pending[keys[0]]
        const batch = []
        for (let index = 0; index < keys.length; ++index) {
            const key = keys[index]
            if (root.pending[key].viewerFid !== first.viewerFid)
                continue
            batch.push(root.pending[key].castId)
            delete root.pending[key]
        }
        root.busy = true
        const xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return
            if (xhr.status === 200) {
                try {
                    const incoming = JSON.parse(xhr.responseText).reactions || ({})
                    let changed = false
                    for (const hash of batch) {
                        const item = incoming[hash] || ({})
                        const key = String(first.viewerFid) + ":" + hash
                        for (const kind of ["like", "recast"]) {
                            const present = kind === "like" ? item.liked === true : item.recasted === true
                            if (present && root.positive[key + ":" + kind] !== true) {
                                root.positive[key + ":" + kind] = true
                                changed = true
                            }
                        }
                    }
                    if (changed)
                        root.revision += 1
                } catch (ignored) { /* Do not invalidate previously verified states. */ }
            }
            root.busy = false
            if (Object.keys(root.pending).length > 0)
                dispatchTimer.restart()
        }
        xhr.open("POST", "https://storage-farcaster.genyleap.com/v1/browser/cast-reactions", true)
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.send(JSON.stringify({viewerFid: first.viewerFid, casts: batch}))
    }
}
