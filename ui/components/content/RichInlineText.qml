import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Farcaster.Client

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
    // Reserve real room above/below the line box for color-emoji ascenders
    // and animated artwork; clipping a perfectly valid glyph is not allowed.
    readonly property real emojiVerticalInset: Math.max(2, Math.ceil(root.textSize * 0.15))

    implicitHeight: (root.usePlatformSelection ? platformText.implicitHeight
                                              : textLayout.contentHeight) + emojiVerticalInset * 2
    clip: false

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
            if (kind === "mention" && /^(?:fid:[1-9][0-9]*|[A-Za-z0-9_]+(?:[.-][A-Za-z0-9_]+)*)$/.test(String(span.username || ""))) {
                const handle = "@" + root.escapedHtml(span.username)
                value = "<a href=\"" + handle + "\" style=\"color:" + Theme.accent + ";text-decoration:none\">" + value + "</a>"
            } else if (span.color) {
                value = "<span style=\"color:" + root.escapedHtml(span.color) + "\">" + value + "</span>"
            }
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
        y: root.emojiVerticalInset
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

        // Read-only TextEdit supports selection, but Qt does not always
        // expose context actions automatically with non-native UI styles.
        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: platformSelectionMenu.popup()
        }
        TapHandler {
            acceptedDevices: PointerDevice.TouchScreen
            onLongPressed: {
                // A first long press might precede Qt's selection update.
                if (platformText.selectedText.length === 0)
                    platformText.selectWord()
                platformSelectionMenu.popup()
            }
        }

        Menu {
            id: platformSelectionMenu
            objectName: "systemCastSelectionMenu"
            // Native Qt platforms receive a real OS menu. WASM falls back
            // to a Qt menu because Canvas text is not browser DOM text.
            popupType: Qt.platform.os === "wasm" ? Popup.Item : Popup.Native
            MenuItem {
                text: qsTrId("genycaster.editor.copy")
                enabled: platformText.selectedText.length > 0
                onTriggered: presentationBridge.copyText(platformText.selectedText)
            }
            MenuItem {
                text: qsTrId("genycaster.editor.select_all")
                onTriggered: platformText.selectAll()
            }
            MenuItem {
                text: qsTrId("genycaster.editor.clear_selection")
                enabled: platformText.selectedText.length > 0
                onTriggered: platformText.deselect()
            }
        }
    }

    RichInlineTextItem {
        id: textLayout
        visible: !root.usePlatformSelection
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.emojiVerticalInset
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

    // Qt's read-only QTextDocument is a painted item without platform-native
    // selection handles. Overlay grips are drawn separately from the clipped
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
        // The native TextEdit handles its own glyphs when OS selection is
        // enabled; it must not also spawn the animated QML overlay.
        model: root.usePlatformSelection ? [] : textLayout.emojiRects
        delegate: Item {
            id: emojiOverlay

            required property var modelData
            readonly property var emojiRect: modelData || ({})
            readonly property int emojiPosition: emojiRect.position === undefined
                ? -1 : Number(emojiRect.position)
            readonly property int emojiGeneration: Number(emojiRect.generation || -1)
            readonly property bool spriteFallbackArmed: lottieEmoji.requestFailed
                || !lottieEmoji.rendererUsable || lottieEmoji.rendererFailed
            readonly property bool spriteReady: spriteEmojiLoader.item !== null
                                                && Boolean(spriteEmojiLoader.item.animationReady)
            // The QML delegate owns a reliable native-rendered fallback glyph
            // even while Qt VectorImage/Sprite changes readiness. The native
            // QTextDocument only paints its emergency glyph when no matching
            // overlay exists at all (e.g. before incubation or on teardown).
            // This avoids depending on two different scene-graph layers
            // receiving readiness callbacks in exactly the same frame.
            readonly property bool artworkReady: lottieEmoji.animationReady
                                                 || (!lottieEmoji.animationReady
                                                     && emojiOverlay.spriteReady)

            function claimFallbackOwnership() {
                if (emojiPosition >= 0 && emojiGeneration >= 0)
                    textLayout.setEmojiAnimated(emojiPosition, emojiGeneration, true)
            }
            // Only delegate lifetime and document generation control
            // QTextDocument glyph suppression. Lottie status transitions now
            // affect only the two locally co-located QML renderers.
            onEmojiGenerationChanged: emojiOverlay.claimFallbackOwnership()

            x: Number(emojiRect.x || 0)
            y: root.emojiVerticalInset + Number(emojiRect.y || 0)
            width: Number(emojiRect.width || 0)
            height: Number(emojiRect.height || 0)

            // Always keep a native color-emoji fallback within this same
            // scene-graph delegate. Qt's QTextObjectInterface glyph is not
            // reliably repainted during a VectorImage's last transparent
            // frame or recycled delegate transitions in Qt Quick.
            Label {
                id: inlineStaticEmoji
                objectName: "richInlineEmojiStaticFallback"
                anchors.fill: parent
                visible: !emojiOverlay.artworkReady
                text: String(emojiOverlay.emojiRect.sequence || "")
                textFormat: Text.PlainText
                renderType: Text.NativeRendering
                font.pixelSize: Math.max(1, Math.round(emojiOverlay.height * 0.86))
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.NoWrap
                clip: false
            }

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
                // Reusing the same cast delegate during a short scroll must
                // pause the Lottie, not destroy and re-incubate its scene.
                // The owning feed delegate still releases it on destruction.
                retainLoadedRenderer: true
                active: root.visible && root.width > 0 && root.height > 0
                // Keep the renderer loading while its pixels are transparent.
                // Only one of Unicode, Lottie or Sprite may paint this box.
                opacity: animationReady ? 1 : 0
                reducedMotion: root.reducedMotion
            }

            Loader {
                id: spriteEmojiLoader
                anchors.fill: parent
                z: 1
                active: emojiOverlay.spriteFallbackArmed && !lottieEmoji.animationReady
                visible: active && !lottieEmoji.animationReady
                opacity: emojiOverlay.spriteReady && !lottieEmoji.animationReady ? 1 : 0
                // Sprite fallbacks are tiny viewport-bound delegates.
                // Do not leave an incubator outstanding when a scrolled-off
                // cast destroys its RichInlineText parent mid-frame.
                asynchronous: false

                sourceComponent: Component {
                    SpriteEmoji {
                        visible: !lottieEmoji.animationReady
                        unicodeSequence: String(emojiOverlay.emojiRect.sequence || "")
                        surface: root.emojiSurface
                        preserveArtwork: true
                        retainLoadedRenderer: true
                        viewportVisible: lottieEmoji.insideViewport
                        active: lottieEmoji.motionActive
                        reducedMotion: root.reducedMotion
                    }
                }
            }

            Component.onCompleted: {
                emojiOverlay.claimFallbackOwnership()
            }
            Component.onDestruction: {
                if (emojiPosition >= 0)
                    textLayout.setEmojiAnimated(emojiPosition, emojiGeneration, false)
            }
        }
    }
}
