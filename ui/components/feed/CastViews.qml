pragma Singleton
import QtQuick

// Compatibility/normalization facade for derived cast analytics. All HTTP,
// JSON parsing, request coalescing and freshness policy are owned by the
// native Qt presentation adapter (including when Qt runs in WebAssembly).
QtObject {
    function normalizeHash(value) {
        const raw = String(value || "").trim().toLowerCase()
        const hash = raw.startsWith("0x") ? raw : "0x" + raw
        return /^0x[0-9a-f]{40}$/.test(hash) ? hash : ""
    }

    function countFor(castId) {
        const entry = castViewController.entryFor(castId)
        return entry ? entry.count : null
    }

    function ensure(castId, authorFid, text) {
        // Existing shared callers use the same C++ admission controller.
        castViewController.ensure(castId, authorFid, text)
    }
}
