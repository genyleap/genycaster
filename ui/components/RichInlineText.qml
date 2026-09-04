import QtQuick
import QtQuick.Layouts

// Renders one rich-text block natively (RichInlineTextItem) with emoji as
// real inline layout objects and optional Noto Lottie animation overlays.
// Block data contract is identical to RichTextView's blockData so the two
// components are drop-in interchangeable per surface.
Item {
    id: root

    required property var blockData
    property bool compact: false
    signal externalRequested(string url)

    readonly property string kind: String(root.blockData.kind || "paragraph")
    readonly property bool textBlock: root.kind === "paragraph" || root.kind === "heading"
                                      || root.kind === "quote"
                                      || root.kind === "bullet-list" || root.kind === "ordered-list"
    readonly property bool rtlContent: localizationController.isRightToLeftText(String(root.blockData.text || ""))
    readonly property int textSize: root.kind === "heading" ? Theme.typeTitle3Size : Theme.typeBodySize
    readonly property bool reducedMotion: Theme.reduceMotion

    implicitHeight: textLayout.contentHeight

    // Flattens blockData into one inline-span list; list markers become plain
    // spans separated by newlines (the same layout RichTextView produced with
    // HTML <br>).
    function flattenedInlines() {
        const inlines = []
        const pushSpans = (spans) => {
            const values = spans || []
            for (let i = 0; i < values.length; ++i)
                inlines.push(values[i])
        }
        if (root.kind === "bullet-list" || root.kind === "ordered-list") {
            const items = root.blockData.items || []
            for (let i = 0; i < items.length; ++i) {
                if (i > 0)
                    inlines.push({ kind: "text", text: "\n" })
                inlines.push({ kind: "text", text: root.kind === "bullet-list" ? "• " : (i + 1) + ". " })
                pushSpans(items[i].inlines)
            }
            return inlines
        }
        pushSpans(root.blockData.inlines)
        return inlines
    }

    RichInlineTextItem {
        id: textLayout
        anchors.left: parent.left
        anchors.right: parent.right
        height: textLayout.contentHeight
        inlines: root.flattenedInlines()
        pixelSize: root.textSize
        lineHeight: Theme.typeBodyLineHeight
        fontFamily: Theme.fontFamilyForText(String(root.blockData.text || ""))
        textColor: Theme.ink
        linkColor: Theme.accent
        codeColor: Theme.ink
        codeBackground: Theme.codeInlineSurface
        codeFontFamily: Theme.codeFontFamily
        alignRight: root.rtlContent
        onLinkActivated: link => root.externalRequested(link)
    }

    Repeater {
        model: textLayout.emojiRects
        delegate: AnimatedEmoji {
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            unicodeSequence: modelData.sequence
            active: root.visible && root.width > 0 && root.height > 0
                    && Qt.application.state === Qt.ApplicationActive
            reducedMotion: root.reducedMotion
        }
    }
}