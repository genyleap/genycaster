import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string castId: ""
    property string plainText: ""
    property bool compact: false
    property int emojiSurface: 1
    // Protocol embeds are rendered by CastEmbedsPanel. Rich sidecar embed/media
    // blocks with the same URL are suppressed so a locally published cast does
    // not show the same link twice.
    property var suppressedEmbedUrls: []
    signal externalRequested(string url)
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
        const tokenPattern = /(https?:\/\/[^\s<]+)|(@[A-Za-z0-9_.-]+)|(\/[A-Za-z0-9_-]+)/g
        const spans = []
        let offset = 0
        let match
        while ((match = tokenPattern.exec(input)) !== null) {
            if (match.index > offset)
                spans.push({ kind: "text", text: input.substring(offset, match.index) })
            const token = match[0]
            if (token.startsWith("http://") || token.startsWith("https://")) {
                spans.push({ kind: "link", text: token, target: token })
            } else if (token.startsWith("@")) {
                spans.push({ kind: "mention", text: token, username: token.substring(1) })
            } else {
                spans.push({ kind: "text", text: token, color: Theme.accent })
            }
            offset = match.index + token.length
        }
        if (offset < input.length)
            spans.push({ kind: "text", text: input.substring(offset) })
        return spans
    }

    readonly property var fallbackBlock: {
        if (root.enhanced)
            return null
        const text = String(root.plainText || "")
        if (text.length === 0)
            return null
        return { kind: "paragraph", text: text, inlines: root.inlineSpansFor(text) }
    }

    function canonicalUrl(value) {
        let text = String(value || "").trim()
        if (text.length === 0)
            return ""
        const fragment = text.indexOf("#")
        if (fragment >= 0)
            text = text.substring(0, fragment)
        while (text.length > 1 && text.endsWith("/"))
            text = text.slice(0, -1)
        const scheme = text.indexOf("://")
        if (scheme < 0)
            return text
        const prefix = text.substring(0, scheme).toLowerCase()
        const remainder = text.substring(scheme + 3)
        const slash = remainder.indexOf("/")
        const authority = (slash >= 0 ? remainder.substring(0, slash) : remainder).toLowerCase()
        const path = slash >= 0 ? remainder.substring(slash) : ""
        return prefix + "://" + authority + path
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

    RichTextView {
        visible: root.fallbackBlock !== null
        Layout.fillWidth: true
        emojiSurface: root.emojiSurface
        blockData: root.fallbackBlock
        compact: root.compact
        onExternalRequested: url => root.externalRequested(url)
    }

    Repeater {
        model: root.enhanced ? root.richBlocks : []
        delegate: Loader {
            required property var modelData
            Layout.fillWidth: true
            asynchronous: true
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
