import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

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
    // Set only after releasing the selection gesture, avoiding a floating
    // action button intercepting the very drag that extends the highlight.
    property bool selectionReady: false

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
    // Painted inline emoji and text must never spill outside a recycled Cast
    // delegate before the native QTextDocument recomputes after a resize.
    clip: true

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
        onSelectionChanged: {
            if (textLayout.selectedText.length === 0)
                root.selectionReady = false
        }
        onSelectionCompleted: {
            root.selectionReady = textLayout.selectedText.length > 0
        }
    }

    // Only instantiate a popup for a finished selection. The popup lives on
    // the application Overlay, not inside the clipped/recycled cast block.
    // It can therefore sit immediately above the selected text even when the
    // first selected word is at y=0 in its QTextDocument.
    Loader {
        id: selectionMenuLoader
        objectName: "richTextCopySelection"
        active: root.selectionReady && textLayout.selectedText.length > 0
        sourceComponent: Item {
            Component.onCompleted: copyPopup.open()
            Popup {
                id: copyPopup
                parent: Overlay.overlay
                modal: false
                dim: false
                focus: false
                padding: Theme.space1
                closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
                x: {
                    if (!parent) return 0
                    const anchor = root.mapToItem(parent,
                        textLayout.selectionMenuPosition.x,
                        textLayout.selectionMenuPosition.y)
                    return Math.max(Theme.space2,
                        Math.min(Math.max(Theme.space2, parent.width - width - Theme.space2),
                                 anchor.x - width / 2))
                }
                y: {
                    if (!parent) return 0
                    const anchor = root.mapToItem(parent,
                        textLayout.selectionMenuPosition.x,
                        textLayout.selectionMenuPosition.y)
                    const above = anchor.y - height - Theme.space2
                    const below = anchor.y + root.textSize * Theme.typeBodyLineHeight + Theme.space2
                    const ideal = above >= Theme.space2 ? above : below
                    return Math.max(Theme.space2,
                        Math.min(Math.max(Theme.space2, parent.height - height - Theme.space2),
                                 ideal))
                }
                background: Superellipse {
                    fillColor: Theme.surfaceElevated
                    borderColor: Theme.borderStrong
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveControl
                    elevation: Theme.elevationControl
                }
                contentItem: RowLayout {
                    spacing: Theme.space1
                    ActionButton {
                        objectName: "castSelectionCopy"
                        compact: true
                        quiet: true
                        focusPolicy: Qt.NoFocus
                        text: qsTrId("genycaster.feedback.copy")
                        onClicked: {
                            if (textLayout.selectedText.length > 0)
                                presentationBridge.copyText(textLayout.selectedText)
                            copyPopup.close()
                        }
                    }
                    ActionButton {
                        objectName: "castSelectionAll"
                        compact: true
                        quiet: true
                        focusPolicy: Qt.NoFocus
                        text: qsTrId("genycaster.editor.select_all")
                        onClicked: {
                            textLayout.selectAllText()
                            // Keep the menu open so Copy and Clear remain
                            // available for the enlarged selection.
                        }
                    }
                    ActionButton {
                        objectName: "castSelectionReset"
                        compact: true
                        quiet: true
                        focusPolicy: Qt.NoFocus
                        text: qsTrId("genycaster.editor.clear_selection")
                        onClicked: {
                            // onClosed clears the highlight and releases the
                            // lazy loader. Close first to avoid accessing a
                            // destroyed popup after selectionChanged.
                            copyPopup.close()
                        }
                    }
                }
                onClosed: {
                    root.selectionReady = false
                    textLayout.clearSelection()
                }
            }
        }
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
