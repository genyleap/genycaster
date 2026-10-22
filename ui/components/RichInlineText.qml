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
    property bool selectionHandleDragging: false
    property bool preserveSelectionForGrip: false
    readonly property bool usePlatformSelection: textSelectionSettings.systemSelectionEnabled

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

    implicitHeight: root.usePlatformSelection ? platformText.implicitHeight
                                              : textLayout.contentHeight
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

    function escapedHtml(value) {
        return String(value || "").replace(/&/g, "&amp;").replace(/</g, "&lt;")
                                 .replace(/>/g, "&gt;").replace(/"/g, "&quot;")
    }

    function nativeMarkup() {
        // Qt TextEdit delegates selection to the platform's QInputMethod.
        // Keep safe bold/emphasis/code/link spans for this optional rendering
        // mode. Genycaster's richer animated emoji renderer stays the default.
        const html = []
        const spans = root.flattenedInlines()
        for (const span of spans) {
            if (span === null || span === undefined) continue
            if (typeof span === "string") {
                html.push(root.escapedHtml(span).replace(/\n/g, "<br/>"))
                continue
            }
            const kind = String(span.kind || "text")
            let value = root.escapedHtml(span.text || "")
            if (kind === "mention" && span.username)
                value = "@" + root.escapedHtml(span.username)
            value = value.replace(/\n/g, "<br/>")
            if (kind === "strong") value = "<b>" + value + "</b>"
            else if (kind === "emphasis") value = "<i>" + value + "</i>"
            else if (kind === "inline-code") value = "<code>" + value + "</code>"
            else if (kind === "link" && /^https?:\/\//i.test(String(span.target || "")))
                value = "<a href=\"" + root.escapedHtml(span.target) + "\">" + value + "</a>"
            html.push(value)
        }
        if (html.length === 0)
            html.push(root.escapedHtml(root.safeBlockData.text || "").replace(/\n/g, "<br/>"))
        return "<p style=\"margin:0;\">" + html.join("") + "</p>"
    }

    TextEdit {
        id: platformText
        objectName: "platformCastSelectableText"
        visible: root.usePlatformSelection && root.textBlock
        width: parent.width
        height: implicitHeight
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        persistentSelection: true
        activeFocusOnPress: true
        textFormat: TextEdit.RichText
        text: root.nativeMarkup()
        wrapMode: TextEdit.Wrap
        font.family: Theme.fontFamilyForText(String(root.safeBlockData.text || ""))
        font.pixelSize: root.textSize
        font.bold: root.fontBold
        color: Theme.ink
        selectionColor: Theme.primarySurfaceHover
        selectedTextColor: Theme.ink
        horizontalAlignment: root.rtlContent ? TextEdit.AlignRight : TextEdit.AlignLeft
        onLinkActivated: url => root.externalRequested(url)
    }

    RichInlineTextItem {
        id: textLayout
        visible: !root.usePlatformSelection
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
            if (!root.selectionHandleDragging)
                root.selectionReady = textLayout.selectedText.length > 0
        }
    }

    // Qt's read-only QTextDocument is a painted item and does not get iOS
    // UITextView handles. Overlay grips are drawn separately from the clipped
    // feed card and can move both endpoints across wrapped lines and bidi runs.
    Loader {
        id: castSelectionGripLoader
        active: !root.usePlatformSelection && textLayout.selectedText.length > 0
                && root.visible && textLayout.visible
        sourceComponent: Item {
        id: selectionOverlayGrips
        objectName: "castTextSelectionHandles"
        parent: Overlay.overlay
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        z: 200
        Repeater {
            model: 2
            delegate: Item {
                id: grip
                required property int index
                readonly property bool leading: index === 0
                readonly property rect selectionRect: leading
                    ? textLayout.selectionStartRect : textLayout.selectionEndRect
                readonly property point onscreen: textLayout.mapToItem(
                    selectionOverlayGrips, selectionRect.x, selectionRect.y)
                width: 44
                height: 44
                x: Math.max(0, Math.min(selectionOverlayGrips.width - width,
                    onscreen.x - width * 0.5))
                y: Math.max(0, Math.min(selectionOverlayGrips.height - height,
                    onscreen.y + (leading ? -height * 0.52
                                          : selectionRect.height - height * 0.48)))
                Rectangle {
                    x: parent.width / 2 - 1
                    y: grip.leading ? height / 2 - 2 : -height / 2 + 2
                    width: 2
                    height: Math.max(15, grip.selectionRect.height)
                    color: root.usePlatformSelection ? Theme.accent
                           : (Theme.dark ? "#0A84FF" : "#007AFF")
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 13
                    height: 13
                    radius: 6.5
                    color: root.usePlatformSelection ? Theme.accent
                           : (Theme.dark ? "#0A84FF" : "#007AFF")
                    border.width: 2
                    border.color: Theme.surfaceRaised
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    cursorShape: Qt.SizeHorCursor
                    onPressed: {
                        root.selectionHandleDragging = true
                        root.preserveSelectionForGrip = true
                        root.selectionReady = false
                    }
                    onPositionChanged: mouse => {
                        if (!pressed) return
                        const where = mapToItem(textLayout, mouse.x, mouse.y)
                        textLayout.dragSelectionBoundary(grip.leading, where.x, where.y)
                    }
                    onReleased: {
                        root.selectionHandleDragging = false
                        root.selectionReady = textLayout.selectedText.length > 0
                        Qt.callLater(function() { root.preserveSelectionForGrip = false })
                    }
                    onCanceled: {
                        root.selectionHandleDragging = false
                        root.selectionReady = textLayout.selectedText.length > 0
                        Qt.callLater(function() { root.preserveSelectionForGrip = false })
                    }
                }
            }
        }
        }
    }

    // Only instantiate a popup for a finished selection. The popup lives on
    // the application Overlay, not inside the clipped/recycled cast block.
    // It can therefore sit immediately above the selected text even when the
    // first selected word is at y=0 in its QTextDocument.
    Loader {
        id: selectionMenuLoader
        objectName: "richTextCopySelection"
        active: !root.usePlatformSelection && root.selectionReady
                && !root.selectionHandleDragging && textLayout.selectedText.length > 0
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
                    if (!root.selectionHandleDragging && !root.preserveSelectionForGrip)
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
