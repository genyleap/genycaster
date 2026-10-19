pragma Singleton
import QtQuick

// One read-only, page-scoped cache for Farcaster client view counts. Cast
// delegates are recycled aggressively during flicking; never own an XHR or a
// "loaded" counter inside each row. Requests are coalesced across 16 casts,
// and values survive scrolling, tab switches and repeated delegate creation.
QtObject {
    id: root

    property int revision: 0
    property bool busy: false
    property var counts: ({})
    property var retryAfter: ({})
    property var pending: ({})

    property Timer dispatchTimer: Timer {
        interval: 430
        repeat: false
        onTriggered: root.dispatch()
    }

    function normalizeHash(value) {
        const hash = String(value || "").trim().toLowerCase()
        return /^0x[0-9a-f]{40}$/.test(hash) ? hash : ""
    }

    function countFor(castId) {
        // The explicit revision dependency invalidates bindings when an async
        // response arrives, without replacing the cast model or its media.
        const observedRevision = root.revision
        const key = root.normalizeHash(castId)
        if (key.length === 0)
            return null
        const value = root.counts[key]
        return value === undefined ? null : value
    }

    function ensure(castId, authorFid, text) {
        const key = root.normalizeHash(castId)
        const fid = Number(authorFid)
        if (!key || !isFinite(fid) || Math.floor(fid) !== fid
                || fid <= 0 || fid > 9007199254740991)
            return
        const now = Date.now()
        const source = String(text || "").replace(/\s+/g, " ").trim().slice(0, 160)
        if (root.pending[key] !== undefined) {
            root.pending[key] = {castId: key, authorFid: fid, text: source}
            return
        }
        if (Number(root.retryAfter[key] || 0) > now)
            return
        root.pending[key] = {castId: key, authorFid: fid, text: source}
        // Mark attempts immediately to prevent recycled QML delegates from
        // enqueueing the same cast multiple times during fast scrolling.
        root.retryAfter[key] = now + 90 * 1000
        if (!root.busy)
            dispatchTimer.restart()
    }

    function dispatch() {
        if (root.busy)
            return
        const keys = Object.keys(root.pending).slice(0, 16)
        if (keys.length === 0)
            return
        const batch = []
        for (let index = 0; index < keys.length; ++index) {
            batch.push(root.pending[keys[index]])
            delete root.pending[keys[index]]
        }

        root.busy = true
        const xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return
            const now = Date.now()
            if (xhr.status === 200) {
                try {
                    const body = JSON.parse(xhr.responseText)
                    const incoming = body.counts || ({})
                    const validated = ({})
                    for (let index = 0; index < batch.length; ++index) {
                        const hash = batch[index].castId
                        const number = incoming[hash]
                        if (typeof number === "number" && isFinite(number)
                                && Math.floor(number) === number
                                && number >= 0 && number <= 9007199254740991) {
                            validated[hash] = number
                            root.retryAfter[hash] = now + 3 * 60 * 1000
                        }
                    }
                    let changed = false
                    for (const hash in validated) {
                        if (root.counts[hash] !== validated[hash]) {
                            root.counts[hash] = validated[hash]
                            changed = true
                        }
                    }
                    if (changed)
                        root.revision += 1
                } catch (error) {
                    // Keep previously verified counts during transient errors.
                }
            }
            root.busy = false
            if (Object.keys(root.pending).length > 0)
                dispatchTimer.start()
        }
        xhr.onerror = function() {
            // The readyState callback performs the shared cleanup.
        }
        xhr.open("POST", "https://storage-farcaster.genyleap.com/v1/browser/cast-views", true)
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.send(JSON.stringify({casts: batch}))
    }
}
