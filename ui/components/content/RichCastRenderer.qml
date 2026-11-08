import QtQuick
import QtQuick.Layouts
import Farcaster.Client

ColumnLayout {
    id: root
    // A Cast can be narrower than an unbroken URL or handle. Allow the
    // containing layout to shrink so native rich text wraps to its real width.
    Layout.minimumWidth: 0
    property string castId: ""
    property string plainText: ""
    property bool compact: false
    property bool magazineCard: false
    property var sharedMediaGeometry: null
    property int emojiSurface: 1
    // Protocol embeds are rendered by CastEmbedsPanel. Rich sidecar embed/media
    // blocks with the same URL are suppressed so a locally published cast does
    // not show the same link twice.
    property var suppressedEmbedUrls: []
    signal externalRequested(string url)
    signal bodyActivated()
    signal mediaRequested(var mediaItems, int index)
    signal castRequested(string castId)

    readonly property var richBlocks: {
        const revision = richComposer.revision
        return root.castId.length > 0 ? richComposer.richBlocksForCast(root.castId) : []
    }
    readonly property bool enhanced: richBlocks && richBlocks.length > 0
    spacing: Theme.space2

    // Feed casts without a rich-composer sidecar still render through the
    // native inline pipeline (RichInlineTextItem) so emoji boxes are laid out
    // and the animated Lottie overlays can play. Token styling mirrors the old
    // HTML fallback: URLs become clickable links, mentions/channels are
    // accent-colored, everything else is plain text.
    function inlineSpansFor(value) {
        const input = String(value || "")
        const tokenPattern = /(`+)([^\n]+?)\1|(https?:\/\/[^\s<]+)|(@fid:[1-9][0-9]*)|(@[A-Za-z0-9_]+(?:[.-][A-Za-z0-9_]+)*)|([#$][A-Za-z][A-Za-z0-9_]*)|(\/[A-Za-z0-9_-]+)/g
        const spans = []
        let offset = 0
        let match
        while ((match = tokenPattern.exec(input)) !== null) {
            // Do not style a currency sign in a number, an email domain,
            // or a token embedded inside another ordinary word.
            if (!match[1] && match.index > 0 && /[\w@./$#]/.test(input[match.index - 1])) continue
            if (match.index > offset)
                spans.push({ kind: "text", text: input.substring(offset, match.index) })
            const token = match[0]
            if (match[1]) {
                spans.push({ kind: "inline-code", text: match[2] })
            } else if (token.startsWith("http://") || token.startsWith("https://")) {
                spans.push({ kind: "link", text: token, target: token })
            } else if (token.startsWith("@")) {
                spans.push({ kind: "mention", text: token, username: token.substring(1) })
            } else if (token.startsWith("$") || token.startsWith("#")) {
                spans.push({ kind: "text", text: token, color: Theme.accent })
            } else {
                spans.push({ kind: "text", text: token, color: Theme.accent })
            }
            offset = match.index + token.length
        }
        if (offset < input.length)
            spans.push({ kind: "text", text: input.substring(offset) })
        return spans
    }

    // Protocol Markdown must render without a local Rich Composer sidecar.
    // Unclosed fences are kept literally instead of silently dropping content.
    readonly property var fallbackBlocks: {
        if (root.enhanced) return []
        const text = String(root.plainText || "").replace(/\r\n?/g, "\n")
        if (text.length === 0) return []
        const lines = text.split("\n")
        const blocks = []
        let prose = []
        const appendProse = () => {
            if (prose.length === 0) return
            const value = prose.join("\n").replace(/^\n+|\n+$/g, "")
            if (value.length > 0)
                blocks.push({ kind: "paragraph", text: value,
                              inlines: root.inlineSpansFor(value) })
            prose = []
        }
        for (let i = 0; i < lines.length; ++i) {
            const start = lines[i].match(/^ {0,3}(`{3,}|~{3,})([A-Za-z0-9_+#.-]*)[ \t]*$/)
            if (start) {
                let end = i + 1
                while (end < lines.length) {
                    const closing = lines[end].match(/^ {0,3}(`{3,}|~{3,})[ \t]*$/)
                    if (closing && closing[1][0] === start[1][0]
                            && closing[1].length >= start[1].length)
                        break
                    ++end
                }
                if (end < lines.length) {
                    appendProse()
                    blocks.push({ kind: "code",
                                  text: lines.slice(i + 1, end).join("\n"),
                                  language: start[2] || "auto",
                                  lineNumbers: false, wrap: false })
                    i = end
                    continue
                }
            }
            // GenyCaster's earlier FarcasterSafeSerializer emitted
            // "javascript:\n\nsource" instead of portable code fences.
            // Restore only clear language-tagged code, without re-signing the
            // immutable published Cast or treating generic prose as code.
            const legacy = lines[i].match(/^(javascript|typescript|python|cpp|c\+\+|rust|go|java|json|bash|shell|qml|sql|css|html):[ \t]*$/i)
            if (legacy && lines[i + 1] === "" && i + 2 < lines.length) {
                const candidate = lines[i + 2]
                const codeLike = /[{};=()[\]<>]|^\s*(?:console\.|const\s|let\s|var\s|def\s|class\s|import\s|from\s|fn\s|SELECT\s|print\()/i.test(candidate)
                if (codeLike) {
                    appendProse()
                    let end = i + 2
                    while (end < lines.length && lines[end].trim().length > 0) ++end
                    blocks.push({ kind: "code", text: lines.slice(i + 2, end).join("\n"),
                                  language: legacy[1].toLowerCase().replace("c++", "cpp"),
                                  lineNumbers: false, wrap: false })
                    i = end - 1
                    continue
                }
            }
            prose.push(lines[i])
        }
        appendProse()
        return blocks
    }

    function canonicalUrl(value) {
        let text = String(value || "").trim()
        if (!text) return ""
        text = text.split("#")[0]
        const scheme = text.indexOf("://")
        if (scheme < 0) return text
        const rest = text.substring(scheme + 3)
        let boundary = rest.length
        const slash = rest.indexOf("/")
        const question = rest.indexOf("?")
        if (slash >= 0) boundary = Math.min(boundary, slash)
        if (question >= 0) boundary = Math.min(boundary, question)
        const host = rest.substring(0, boundary).toLowerCase()
        const tail = rest.substring(boundary)
        const queryStart = tail.indexOf("?")
        let path = queryStart >= 0 ? tail.substring(0, queryStart) : tail
        while (path.endsWith("/")) path = path.slice(0, -1)
        // Only discard attribution/tracking parameters; keep meaningful IDs.
        const rawQuery = queryStart >= 0 ? tail.substring(queryStart + 1) : ""
        const query = rawQuery.split("&").filter(part => {
            if (!part) return false
            const key = part.split("=")[0].toLowerCase()
            return !/^(?:utm_[a-z0-9_]+|fbclid|gclid|igshid|ref|referrer|sharedby|si|mc_cid|mc_eid)$/.test(key)
        }).sort().join("&")
        return "https://" + host + path + (query ? "?" + query : "")
    }

    function blockSuppressed(block) {
        const value = block || ({})
        const kind = String(value.kind || "")
        if (kind !== "embed" && kind !== "media")
            return false
        const target = root.canonicalUrl(value.url || "")
        if (target.length === 0)
            return false
        const values = root.suppressedEmbedUrls || []
        for (let index = 0; index < values.length; ++index) {
            if (root.canonicalUrl(values[index]) === target)
                return true
        }
        return false
    }

    function componentFor(kind) {
        const value = String(kind || "")
        if (value === "code") return codeView
        if (value === "embed" || value === "media" || value === "cast-reference") return enhancementView
        if (value === "separator") return separatorView
        return textView
    }

    Repeater {
        model: root.enhanced ? root.richBlocks : root.fallbackBlocks
        delegate: Loader {
            required property var modelData
            Layout.fillWidth: true
            // These blocks are cheap compared with their images; synchronous
            // creation avoids orphaned incubators when a feed row is recycled.
            asynchronous: false
            active: !root.blockSuppressed(modelData)
            sourceComponent: active ? root.componentFor(modelData.kind) : null
            property var blockData: modelData
        }
    }

    Component {
        id: textView
        RichTextView {
            blockData: parent.blockData
            emojiSurface: root.emojiSurface
            compact: root.compact
            onExternalRequested: url => root.externalRequested(url)
            onBodyActivated: root.bodyActivated()
        }
    }

    Component {
        id: codeView
        CodeBlockView {
            source: String(parent.blockData.text || "")
            language: String(parent.blockData.language || "auto")
            lineNumbers: Boolean(parent.blockData.lineNumbers)
            wrap: Boolean(parent.blockData.wrap)
            compact: root.compact
        }
    }

    Component {
        id: enhancementView
        EmbedPreview {
            compact: root.compact
            magazineCard: root.magazineCard
            sharedMediaGeometry: root.sharedMediaGeometry
            readonly property string richKind: String(parent.blockData.kind || "")
            readonly property string mediaType: String(parent.blockData.mediaType || "").toLowerCase()
            embed: {
                if (richKind === "cast-reference") {
                    return {
                        kind: "cast-reference",
                        castId: String(parent.blockData.castId || ""),
                        author: "",
                        displayName: qsTrId("genycaster.embed.quoted_cast"),
                        text: ""
                    }
                }
                if (richKind === "media") {
                    const resolvedKind = mediaType.indexOf("video") >= 0 ? "video"
                                      : mediaType.indexOf("image") >= 0 ? "image" : "external"
                    return {
                        kind: resolvedKind,
                        url: String(parent.blockData.url || ""),
                        host: String(parent.blockData.altText || parent.blockData.mediaType || ""),
                        mediaType: String(parent.blockData.mediaType || ""),
                        adaptiveUrl: String(parent.blockData.adaptiveUrl || parent.blockData.hlsUrl
                                            || parent.blockData.manifestUrl || ""),
                        renditions: parent.blockData.renditions || parent.blockData.videoRenditions
                                    || parent.blockData.variants || []
                    }
                }
                return {
                    kind: "external",
                    url: String(parent.blockData.url || ""),
                    host: String(parent.blockData.title || "")
                }
            }
            onOpenRequested: url => root.externalRequested(url)
            onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
            onCastRequested: castId => root.castRequested(castId)
        }
    }

    Component {
        id: separatorView
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space2
            Layout.bottomMargin: Theme.space2
            implicitHeight: 1
            color: Theme.divider
        }
    }
}
