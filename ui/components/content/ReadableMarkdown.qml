import QtQuick
import QtQuick.Controls
import Farcaster.Client

// Shared reading surface for release notes, Terms and Privacy.
// Preserve translated Markdown semantics; provide consistent typography,
// paragraph/header separation, RTL alignment and HTTPS links.
DirectionAwareLabel {
    id: root
    property string markdownSource: ""
    property bool allowExternalLinks: true
    objectName: "genycasterReadableMarkdown"

    function formattedMarkdown(source) {
        const lines = String(source || "").replace(/\r\n?/g, "\n")
            .trim().split("\n")
        const result = []
        for (let i = 0; i < lines.length; ++i) {
            const current = lines[i].replace(/\s+$/, "")
            const previous = result.length > 0 ? result[result.length - 1] : ""
            // CommonMark block headings require a clear paragraph boundary in
            // long-form reading. Some translations omit this blank line.
            if (/^#{1,6}\s+\S/.test(current) && previous.trim().length > 0)
                result.push("")
            result.push(current)
            if (/^#{1,6}\s+\S/.test(current)
                && i + 1 < lines.length && lines[i + 1].trim().length > 0)
                result.push("")
        }
        return result.join("\n")
    }

    text: formattedMarkdown(markdownSource)
    textFormat: Text.MarkdownText
    font.pixelSize: Theme.typeLabelSize
    color: Theme.ink
    linkColor: Theme.accent
    wrapMode: Text.WordWrap
    lineHeight: 1.48
    alignToUi: true
    selectableText: false

    // Links are explicit user actions. Never follow script, file or custom
    // schemes from translated/legal or externally supplied release notes.
    onLinkActivated: function(link) {
        if (!root.allowExternalLinks) return
        const target = String(link || "").trim()
        const match = /^https:\/\/([a-z0-9](?:[a-z0-9.-]*[a-z0-9])?)(?::443)?(?:[\/?#][^\s]*)?$/i.exec(target)
        if (match && match[1].includes(".") && !match[1].includes(".."))
            Qt.openUrlExternally(target)
    }
}
