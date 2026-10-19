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
    property int emojiSurface: 1
    property bool fontBold: false
    signal externalRequested(string url)
    signal bodyActivated()

    // Required means the binding must exist; its value may still become null
    // briefly while a model delegate is being recycled.
    readonly property var safeBlockData: root.blockData || ({})
    readonly property string kind: String(root.safeBlockData.kind || "paragraph")
    readonly property bool textBlock: root.kind === "paragraph" || root.kind === "heading"
                                      || root.kind === "quote"
                                      || root.kind === "bullet-list" || root.kind === "ordered-list"
    readonly property bool rtlContent: localizationController.isRightToLeftText(
        String(root.safeBlockData.text || ""))
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
            for (let i = 0; i < values.length; ++i) {
                if (values[i])
                    inlines.push(values[i])
            }
        }
        if (root.kind === "bullet-list" || root.kind === "ordered-list") {
            const items = root.safeBlockData.items || []
            for (let i = 0; i < items.length; ++i) {
                const item = items[i] || ({})
                if (i > 0)
                    inlines.push({ kind: "text", text: "\n" })
                inlines.push({ kind: "text", text: root.kind === "bullet-list" ? "• " : (i + 1) + ". " })
                pushSpans(item.inlines)
            }
            return inlines
        }
        pushSpans(root.safeBlockData.inlines)
        return inlines
    }

    RichInlineTextItem {
        id: textLayout
        anchors.left: parent.left
        anchors.right: parent.right
        height: textLayout.contentHeight
        inlines: root.flattenedInlines()
        pixelSize: root.textSize
        bold: root.fontBold
        lineHeight: Theme.typeBodyLineHeight
        fontFamily: Theme.fontFamilyForText(String(root.safeBlockData.text || ""))
        textColor: Theme.ink
        linkColor: Theme.accent
        codeColor: Theme.ink
        codeBackground: Theme.codeInlineSurface
        codeFontFamily: Theme.codeFontFamily
        alignRight: root.rtlContent
        onLinkActivated: link => root.externalRequested(link)
        onPlainTextActivated: root.bodyActivated()
    }

    Repeater {
        model: textLayout.emojiRects
        delegate: Item {
            id: emojiOverlay

            required property var modelData
            readonly property var emojiRect: modelData || ({})
            readonly property int emojiPosition: emojiRect.position === undefined
                ? -1 : Number(emojiRect.position)
            property bool spriteFallbackArmed: false
            readonly property bool spriteReady: spriteEmojiLoader.item !== null
                                                && Boolean(spriteEmojiLoader.item.animationReady)
            // Exactly one overlay renderer may own the emoji box. Unicode
            // remains the fallback until either Lottie succeeds or an explicit
            // failure selects the sprite path.
            readonly property bool artworkReady: lottieEmoji.animationReady
                                                 || (!lottieEmoji.animationReady
                                                     && emojiOverlay.spriteReady)

            function syncAnimatedState() {
                if (emojiPosition >= 0)
                    textLayout.setEmojiAnimated(emojiPosition, emojiOverlay.artworkReady)
            }

            x: Number(emojiRect.x || 0)
            y: Number(emojiRect.y || 0)
            width: Number(emojiRect.width || 0)
            height: Number(emojiRect.height || 0)

            // Lottie remains the preferred Noto Animated Emoji renderer. If the
            // scene graph/backend cannot present it, or an individual asset does
            // not become ready promptly, the prepared PNG sprite path takes over
            // without hiding the Unicode glyph until real artwork is available.
            AnimatedEmoji {
                id: lottieEmoji
                anchors.fill: parent
                unicodeSequence: String(emojiOverlay.emojiRect.sequence || "")
                surface: root.emojiSurface
                preserveArtwork: true
                active: root.visible && root.width > 0 && root.height > 0
                        && Qt.application.state === Qt.ApplicationActive
                reducedMotion: root.reducedMotion
                onAnimationReadyChanged: emojiOverlay.syncAnimatedState()
                onRequestFailedChanged: {
                    emojiOverlay.spriteFallbackArmed = requestFailed || !rendererUsable
                    emojiOverlay.syncAnimatedState()
                }
                onRendererUsableChanged: {
                    emojiOverlay.spriteFallbackArmed = !rendererUsable || requestFailed
                    emojiOverlay.syncAnimatedState()
                }
            }

            Loader {
                id: spriteEmojiLoader
                anchors.fill: parent
                z: 1
                active: emojiOverlay.spriteFallbackArmed && !lottieEmoji.animationReady
                visible: active && !lottieEmoji.animationReady
                asynchronous: true

                sourceComponent: Component {
                    SpriteEmoji {
                        visible: !lottieEmoji.animationReady
                        unicodeSequence: String(emojiOverlay.emojiRect.sequence || "")
                        surface: root.emojiSurface
                        preserveArtwork: true
                        viewportVisible: emojiOverlay.visible
                        active: root.visible && root.width > 0 && root.height > 0
                                && Qt.application.state === Qt.ApplicationActive
                        reducedMotion: root.reducedMotion
                        onAnimationReadyChanged: emojiOverlay.syncAnimatedState()
                    }
                }

                onLoaded: emojiOverlay.syncAnimatedState()
            }

            Component.onCompleted: {
                if (!lottieEmoji.rendererUsable)
                    emojiOverlay.spriteFallbackArmed = true
                emojiOverlay.syncAnimatedState()
            }
            Component.onDestruction: {
                if (emojiPosition >= 0)
                    textLayout.setEmojiAnimated(emojiPosition, false)
            }
        }
    }
}
