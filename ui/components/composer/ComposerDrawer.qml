import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import "ProCastPolicy.js" as ProCastPolicy
import Farcaster.Client

GenyDrawer {
    id: root
    objectName: "composerDrawer"

    ProUpgradeNotice {
        id: composerProNotice
        featureLabel: qsTrId("genycaster.pro.advanced_editor")
        onUpgradeRequested: root.proRequested()
    }

    property var parentCast: null
    property var quotedCast: null
    // One canonical intent for each composition. A quote is a new top-level
    // cast containing a reference; a reply has a canonical parent cast.
    readonly property string composeIntent: root.quotedCast !== null ? "quote"
                                             : root.parentCast !== null ? "reply" : "new"
    readonly property bool replyMode: composeIntent === "reply"
    readonly property bool quoteMode: composeIntent === "quote"
    readonly property bool showTaxonomyControls: composeIntent !== "reply"
    property string initialAction: ""
    // Canonical parent_url is sourced from the channel search response.
    property string selectedParentUrl: ""
    property string selectedContextLabel: ""
    property string selectedContextImage: ""
    property string selectedChannelId: ""
    // Cast topics are metadata owned by GenyCaster, not a Farcaster parent URL.
    property var selectedTopics: []
    readonly property int maxCastTopics: 3
    // Account quotas are fetched from the authenticated Geny Pro service.
    // Farcaster ordinary messages still have a 1024-byte/2-embed limit;
    // larger drafts require an explicitly confirmed multi-cast thread.
    readonly property var proEntitlements: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property bool verifiedGenyPro:
        proEntitlements.status === "verified"
        && proEntitlements.eligible === true
    readonly property int maxComposerCharacters: verifiedGenyPro
        ? Math.min(4096, Math.max(1024, Number(
            root.proEntitlements.limits?.cast_characters || 4096)))
        : 1024
    readonly property int authoredCharacters: ProCastPolicy.characterCount(richComposer.safeText)
    readonly property bool withinAccountCharacterLimit: authoredCharacters <= maxComposerCharacters
    readonly property int maxMediaUploads: verifiedGenyPro
        ? Math.min(4, Math.max(1, Number(
            root.proEntitlements.limits?.image_embeds || 4)))
        : 1
    property var pendingPublicationCasts: []
    property string publicationPlanError: ""
    readonly property int attachedMediaCount: mediaPanel.attachedMedia.length
    readonly property bool canUploadMedia:
        attachedMediaCount + root.pendingMediaFiles.length < maxMediaUploads
    property int initialFocusAttempts: 0
    property string suggestedText: ""
    // Stats reports are generated from localized, app-owned text. Only these
    // drafts use the paragraph-aware Markdown importer; ordinary quick
    // composer handoffs still retain their exact literal characters.
    property bool structuredReport: false
    property var suggestedEmbeds: []
    property bool transientDraft: false
    property string editorMode: "visual"
    property bool showAdvancedFormatting: false
    property real expandedViewportHeight: 0
    readonly property bool keyboardVisible: root.compact && root.opened
        && (Qt.inputMethod.visible
            || (root.expandedViewportHeight > 0 && root.parent !== null
                && root.parent.height < root.expandedViewportHeight - 125))
    // Popups own no SafeArea; read the host Item/Window as in Settings.
    readonly property real bottomSafeInset: root.compact && root.parent
        ? Math.max(root.parent.SafeArea.margins.bottom,
                   root.parent.Window.window
                       ? root.parent.Window.window.SafeArea.margins.bottom : 0,
                   root.parent.Window.contentItem
                       ? root.parent.Window.contentItem.SafeArea.margins.bottom : 0) : 0
    property bool restoredDraft: false
    property string insertKind: ""
    property int insertBlockIndex: -1
    property int insertSelectionStart: 0
    property int insertSelectionLength: 0
    property string insertSelectedText: ""
    property int pendingMediaBlockIndex: -1
    property var pendingMediaFiles: []
    property int pendingMediaAfterIndex: -1
    property var mediaMetadata: ({})
    property bool mediaInsertFailed: false
    property int emojiThreadIndex: -1
    property int emojiThreadCursorPosition: -1
    readonly property int maximumThreadCasts: 24
    readonly property bool threadMode: threadTailModel.count > 0
    // Dropdowns in the phone composer occupy real layout space, never cover
    // the avatar/editor or the fixed media/actions footer.
    readonly property bool mobileSelectorOpen: root.compact
        && (channelPicker.visible || topicPicker.visible)
    // Presentation mode is captured when the composer opens. Resizing an
    // already-open editor must never replace its desktop chrome, editor or
    // selection with a different mobile component tree.
    property bool requestedCompact: false
    property bool compact: false
    onAboutToShow: root.compact = root.requestedCompact
    readonly property string replyAuthor: root.parentCast
                                                   ? String(root.parentCast.author || "") : ""
    readonly property string replyName: root.parentCast
                                                 ? String(root.parentCast.displayName
                                                          || root.parentCast.author
                                                          || qsTrId("genycaster.shell.this_cast")) : ""
    readonly property string replyText: root.parentCast
                                                 ? String(root.parentCast.text || "") : ""
    readonly property bool replyContentRtl: localizationController.isRightToLeftText(
        root.quotedCast
        ? String(root.quotedCast.text || root.quotedCast.displayName || "")
        : (root.replyText || root.replyName))

    signal publishRequested(string text, var mentionFids, var mentionPositions, var embeds, string parentUrl)
    signal replyRequested(string text, var mentionFids, var mentionPositions, var embeds)
    signal threadRequested(var casts)
    signal proRequested()
    signal abandoned()

    function topicName(slug) {
        switch (String(slug || "")) {
        case "art": return qsTrId("genycaster.topic.art")
        case "development": return qsTrId("genycaster.topic.development")
        case "photography": return qsTrId("genycaster.topic.photography")
        case "design": return qsTrId("genycaster.topic.design")
        case "technology": return qsTrId("genycaster.topic.technology")
        case "music": return qsTrId("genycaster.topic.music")
        case "writing": return qsTrId("genycaster.topic.writing")
        case "science": return qsTrId("genycaster.topic.science")
        case "business": return qsTrId("genycaster.topic.business")
        case "gaming": return qsTrId("genycaster.topic.gaming")
        case "trading": return qsTrId("genycaster.topic.trading")
        default: {
            const tokens=String(slug).split("-")
            return tokens.map(t=>t.length ? t.charAt(0).toUpperCase()+t.slice(1) : "").join(" ")
        }
        }
    }

    function toggleTopic(slug) {
        const key=String(slug || "")
        const old=root.selectedTopics || []
        if(old.indexOf(key)>=0) {
            root.selectedTopics=old.filter(t=>t!==key)
        } else if(old.length<root.maxCastTopics) {
            root.selectedTopics=old.concat([key])
        }
    }

    ListModel { id: threadTailModel }

    // A consistent bottom-edge sheet on desktop and a full-height editor on
    // phones. The mode stays fixed for this editing session even if the app
    // crosses a responsive breakpoint or a virtual keyboard resizes the view.
    bottomSheet: true
    fullScreen: root.compact
    showHandle: !root.fullScreen
    modal: true
    focus: true
    dimBackground: true
    contentPadding: 0
    // A full-page mobile editor belongs to the app canvas; elevated card
    // colors belong to floating controls, not to the entire writing surface.
    background: PopupSurface {
        fillColor: root.compact ? Theme.background : Theme.surfaceElevated
        borderWidth: root.compact ? 0 : 1
        curveDepth: root.compact ? 0 : Theme.curveSheet
        elevation: root.compact ? Theme.elevationNone : Theme.elevationModal
    }
    // The modal is hosted by the application viewport, not the changing
    // reading column. Only its dimensions and center position respond to
    // resize; its layout mode and component identities stay unchanged.
    width: parent ? (root.compact ? parent.width
                    : Math.min(720, Math.max(0, parent.width - Theme.space4 * 2))) : 720
    x: parent ? (root.compact ? 0 : Math.max(0, (parent.width - width) / 2)) : 0
    // Keep both ends inside the Qt viewport, including short desktop windows.
    // On mobile Qt already subtracts the on-screen keyboard from this height.
    height: parent ? (root.compact ? parent.height
                      : Math.max(0, Math.min(parent.height - Theme.space4 * 2,
                          replyMode ? Math.min(parent.height * 0.68, 560)
                                    : Math.min(parent.height * 0.78, 660)))) : 660
    // Composer content owns vertical scrolling; drawer-swipe gestures would otherwise
    // compete with Flickable/ScrollView input on touch devices.
    // Scrolling belongs to the content; swipe-to-dismiss was stealing
    // vertical gestures from multi-cast threads on touch browsers.
    interactive: false
    property var focusedComposerEditor: null
    property bool pendingEditorReveal: false

    // Only reveal the *currently focused* editor. The former keyboard-height
    // listener scrolled to the first cast even while editing cast 2/3, while
    // Add Cast independently forced contentY to the bottom. This
    // produced the large jumps visible in thread composition recordings.
    function ensureEditorVisible(editor) {
        if (!root.opened || !editor || !editor.activeFocus
                || root.mobileSelectorOpen)
            return
        root.focusedComposerEditor = editor
        if (root.pendingEditorReveal)
            return
        root.pendingEditorReveal = true
        Qt.callLater(function() {
            root.pendingEditorReveal = false
            const activeEditor = root.focusedComposerEditor
            if (!root.opened || !activeEditor || !activeEditor.activeFocus)
                return
            const flick = composerScroll.contentItem
            if (!flick || flick.height <= 0)
                return
            const mapped = activeEditor.mapToItem(composerContent, 0, 0)
            if (!mapped)
                return
            const caret = activeEditor.cursorRectangle
            const caretY = caret && isFinite(caret.y)
                        ? Math.max(0, Math.min(activeEditor.height - 28, caret.y)) : 0
            const margin = Theme.space3
            // Empty/short replies should always start below their parent
            // context. Scrolling to the oversized editor previously hid the
            // parent beneath the status bar when the virtual keyboard opened.
            if (root.compact && root.replyMode && !root.threadMode
                    && Number(activeEditor.cursorPosition || 0) <= 1
                    && mapped.y + Math.min(activeEditor.height, 46) < flick.height - margin) {
                flick.contentY = 0
                return
            }
            const maxY = Math.max(0, flick.contentHeight - flick.height)
            const panelTop = mapped.y - Theme.space3
            const panelBottom = mapped.y + activeEditor.height + Theme.space3
            const fitsFully = panelBottom - panelTop <= flick.height - margin * 2
            const targetTop = fitsFully ? panelTop : mapped.y + caretY
            const targetBottom = fitsFully
                ? panelBottom : targetTop + Math.max(28, caret && caret.height ? caret.height : 28)
            if (targetTop < flick.contentY + margin)
                flick.contentY = Math.max(0, Math.min(maxY, targetTop - margin))
            else if (targetBottom > flick.contentY + flick.height - margin)
                flick.contentY = Math.max(0, Math.min(maxY,
                    targetBottom + margin - flick.height))
        })
    }
    onKeyboardVisibleChanged: {
        if (root.keyboardVisible && root.focusedComposerEditor)
            root.ensureEditorVisible(root.focusedComposerEditor)
    }
    function openLinkFromSelection() {
        const editor = visualEditor.formattingEditor
        if (root.editorMode !== "visual" || visualEditor.activeBlockIndex < 0 || editor === null
                || editor.selectionStart === editor.selectionEnd) return
        root.insertKind = "link"
        root.insertBlockIndex = visualEditor.activeBlockIndex
        root.insertSelectionStart = editor.selectionStart
        root.insertSelectionLength = editor.selectionEnd - editor.selectionStart
        root.insertSelectedText = editor.selectedText
    }

    function insertCodeBlockFromKeyboard() {
        if (root.editorMode !== "visual") return
        richComposer.insertBlock("code", visualEditor.activeBlockIndex)
    }


    function openMediaPicker() {
        if (Qt.platform.os === "wasm")
            mediaUpload.browseBrowserImage(root.pendingMediaBlockIndex)
        else
            mediaFileDialog.open()
    }

    function clearPendingMediaQueue() {
        root.pendingMediaFiles = []
        root.pendingMediaAfterIndex = -1
    }

    function clearAllComposerContent() {
        // Clear the complete unpublished document, attachments and thread,
        // never the reply target; losing that would silently change where the
        // new cast is published. Managed media cleanup is queued by newDocument.
        if (presentationBridge.writeBusy || presentationBridge.writePhase === "queued")
            return
        root.closeComposerPopups()
        root.clearPendingMediaQueue()
        if (mediaUpload.busy)
            mediaUpload.cancel()
        else
            mediaUpload.clear()
        if (root.replyMode || root.quoteMode || root.transientDraft)
            richComposer.beginContextDocument()
        else
            richComposer.newDocument()
        threadTailModel.clear()
        root.mediaMetadata = ({})
        root.mediaInsertFailed = false
        root.insertKind = ""
        root.selectedParentUrl = ""
        root.selectedContextLabel = ""
        root.selectedContextImage = ""
        root.selectedChannelId = ""
        // Clearing a quote draft must preserve the referenced Cast, just as
        // clearing a reply must never silently lose its reply parent.
        if (root.quoteMode && root.quotedCast)
            richComposer.insertCastReference(String(root.quotedCast.castId || ""),
                                             Number(root.quotedCast.authorFid || 0))
        root.emojiThreadIndex = -1
        root.emojiThreadCursorPosition = -1
        if (root.compact) {
            mobilePlainEditor.resetSelection()
            mobilePlainEditor.clear()
            mobilePlainEditor.forceActiveFocus()
        }
    }


    function startNextMediaUpload() {
        if (mediaUpload.busy || root.pendingMediaFiles.length === 0)
            return false

        const queue = root.pendingMediaFiles.slice()
        const nextFile = queue.shift()
        root.pendingMediaFiles = queue
        if (mediaUpload.beginUpload(nextFile, root.pendingMediaAfterIndex))
            return true

        return root.startNextMediaUpload()
    }

    function enqueueMediaFiles(files, afterIndex) {
        if (!files || files.length === 0 || !root.canUploadMedia)
            return false

        const additions = []
        const remaining = Math.max(0, root.maxMediaUploads
            - root.attachedMediaCount - root.pendingMediaFiles.length)
        for (let index = 0; index < Math.min(files.length, remaining); ++index)
            additions.push(files[index])
        if (additions.length === 0)
            return false

        if (!mediaUpload.busy && root.pendingMediaFiles.length === 0)
            root.pendingMediaAfterIndex = afterIndex
        root.pendingMediaFiles = root.pendingMediaFiles.concat(additions)
        return root.startNextMediaUpload() || root.pendingMediaFiles.length > 0
    }

    function addThreadCast() {
        if (threadTailModel.count + 1 >= root.maximumThreadCasts)
            return
        root.synchronizeEditorState()
        threadTailModel.append({ "text": "" })
        const index = threadTailModel.count - 1
        // A Repeater delegate is usually materialized synchronously. Focus
        // now, in the originating tap, so the browser may raise its keyboard.
        const row = threadRepeater.itemAt(index)
        if (row) {
            row.focusThreadEditor()
        } else {
            Qt.callLater(function() {
                if (!root.opened) return
                const pendingRow = threadRepeater.itemAt(index)
                if (pendingRow) pendingRow.focusThreadEditor()
            })
        }
    }

    function removeThreadCast(index) {
        if (index >= 0 && index < threadTailModel.count)
            threadTailModel.remove(index)
    }

    function insertEmoji(emoji) {
        if (!emoji || emoji.length === 0)
            return
        if (root.compact && root.editorMode === "visual") {
            mobilePlainEditor.insert(mobilePlainEditor.cursorPosition, emoji)
            mobilePlainEditor.forceActiveFocus()
            return
        }
        if (root.emojiThreadIndex >= 0 && root.emojiThreadIndex < threadTailModel.count) {
            const current = String(threadTailModel.get(root.emojiThreadIndex).text || "")
            const position = Math.max(0, Math.min(current.length, root.emojiThreadCursorPosition >= 0
                                                   ? root.emojiThreadCursorPosition : current.length))
            threadTailModel.setProperty(root.emojiThreadIndex, "text",
                                        current.slice(0, position) + emoji + current.slice(position))
            root.emojiThreadCursorPosition = position + emoji.length
            return
        }
        if (root.editorMode === "visual" && visualEditor.insertTextAtCursor(emoji))
            return
        if (root.editorMode === "markdown") {
            const position = markdownEditor.cursorPosition
            markdownEditor.insert(position, emoji)
            markdownEditor.cursorPosition = position + emoji.length
            return
        }
        richComposer.markdownSource = richComposer.markdownSource + emoji
    }

    function closeComposerPopups() {
        if (emojiPicker.opened)
            emojiPicker.close()
        if (channelPicker.opened)
            channelPicker.close()
        if (topicPicker.opened)
            topicPicker.close()
    }

    function synchronizeEditorState() {
        if (root.editorMode === "visual") {
            if (root.compact) {
                richComposer.setBlockText(0, mobilePlainEditor.text)
                return
            }
            visualEditor.commitActiveEditor()
            return
        }
        if (root.editorMode === "markdown" && markdownEditor.text !== richComposer.markdownSource)
            richComposer.markdownSource = markdownEditor.text
    }

    function setEditorMode(mode) {
        const nextMode = String(mode || "")
        if (nextMode !== "visual" && nextMode !== "markdown" && nextMode !== "preview")
            return
        if (nextMode === root.editorMode)
            return
        root.synchronizeEditorState()
        root.closeComposerPopups()
        root.editorMode = nextMode
    }

    function hasMeaningfulText(value) {
        // Space, line breaks and invisible format-only characters are not a Cast.
        return String(value || "").replace(/[\s\u200B-\u200D\u2060\uFEFF]/g, "").length > 0
    }

    function hasLiveEditorText() {
        if (root.editorMode === "visual")
            return root.compact ? root.hasMeaningfulText(mobilePlainEditor.text)
                                : visualEditor.hasLiveContent
        if (root.editorMode === "markdown")
            return root.hasMeaningfulText(markdownEditor.text)
        return false
    }

    // An empty new-cast draft is not a useful saved document. Read the live
    // editor first (the persisted document may lag while the user erases it),
    // then include actual media, links and populated thread parts. Selecting
    // a topic/channel alone does not make a draft meaningful.
    function hasDraftContent() {
        let hasText = root.hasMeaningfulText(richComposer.safeText)
        if (root.editorMode === "markdown")
            hasText = root.hasMeaningfulText(markdownEditor.text)
        else if (root.editorMode === "visual" && root.compact)
            hasText = root.hasMeaningfulText(mobilePlainEditor.text)
        else if (root.editorMode === "visual" && visualEditor.activeEditor !== null
                 && richComposer.blocks.length <= 1)
            hasText = visualEditor.hasLiveContent
        else if (root.editorMode === "visual")
            hasText = hasText || visualEditor.hasLiveContent
        const embeds = richComposer.protocolEmbeds || []
        const hasEmbeds = embeds.some(function(embed) {
            return String(embed && embed.kind || "") !== "cast-reference"
        })
        let hasThreadText = false
        for (let index = 0; index < threadTailModel.count; ++index) {
            if (root.hasMeaningfulText(threadTailModel.get(index).text)) {
                hasThreadText = true
                break
            }
        }
        return hasText || hasEmbeds || hasThreadText
            || root.attachedMediaCount > 0 || root.pendingMediaFiles.length > 0
    }

    function threadTailWithinLimit() {
        for (let index = 0; index < threadTailModel.count; ++index) {
            const text = String(threadTailModel.get(index).text || "")
            if (!root.hasMeaningfulText(text) || presentationBridge.utf8ByteCount(text) > 1024)
                return false
        }
        return true
    }

    function hasPublishableContent() {
        const hasEmbeds = richComposer.protocolEmbeds.length > 0
        // A draft's stored value can lag behind the active text editor.
        // For single-block drafts, never re-enable Publish from stale text
        // after the user has erased the editor or typed only whitespace.
        let hasText = root.hasMeaningfulText(richComposer.safeText)
        if (root.editorMode === "markdown")
            hasText = root.hasMeaningfulText(markdownEditor.text)
        else if (root.editorMode === "visual" && root.compact)
            hasText = root.hasMeaningfulText(mobilePlainEditor.text)
        else if (root.editorMode === "visual" && visualEditor.activeEditor !== null
                 && richComposer.blocks.length <= 1)
            hasText = visualEditor.hasLiveContent
        else if (root.editorMode === "visual")
            hasText = hasText || visualEditor.hasLiveContent
        return (hasText || hasEmbeds) && root.threadTailWithinLimit()
    }

    function publicationEmbeds() {
        // Context associations are intentional only after tapping Quote.
        // Old persisted drafts might still hold CastReference nodes; those
        // must never attach to a new Cast or ordinary reply.
        const embeds = richComposer.protocolEmbeds || []
        return root.quoteMode ? embeds : embeds.filter(function(embed) {
            return String(embed && embed.kind || "") !== "cast-reference"
        })
    }

    function threadPayload() {
        const tails = []
        for (let index = 0; index < threadTailModel.count; ++index)
            tails.push(String(threadTailModel.get(index).text || ""))
        return ProCastPolicy.publicationPlan(richComposer.safeText,
            richComposer.mentionFids, richComposer.mentionPositions,
            root.publicationEmbeds(), tails, root.verifiedGenyPro,
            root.attachedMediaCount)
    }

    function publishCurrent() {
        root.synchronizeEditorState()
        if (!root.withinAccountCharacterLimit || !root.hasPublishableContent()
                || root.attachedMediaCount > root.maxMediaUploads
                || presentationBridge.writeBusy || mediaUpload.busy)
            return
        const planned = root.threadPayload()
        if (planned.error) {
            root.publicationPlanError = planned.error
            return
        }
        root.publicationPlanError = ""
        if (planned.requiresThread) {
            // NEVER send extra posts without a visible confirmation.
            root.pendingPublicationCasts = planned.parts
            threadPublicationConfirmation.open()
            return
        }
        richComposer.saveDraft()
        if (root.replyMode)
            root.replyRequested(richComposer.safeText, richComposer.mentionFids,
                richComposer.mentionPositions, planned.parts[0].embeds)
        else
            root.publishRequested(richComposer.safeText, richComposer.mentionFids,
                richComposer.mentionPositions, planned.parts[0].embeds,
                root.showTaxonomyControls ? root.selectedParentUrl : "")
    }

    // Repeater editors are incubated after the drawer begins opening; a
    // one-shot 120ms focus call often fires before their first delegate.
    function focusVisualEditorNow() {
        // Called synchronously from the originating tap: browser input activation
        // may be rejected if focus is deferred to a timer.
        if (root.compact) {
            mobilePlainEditor.forceActiveFocus()
            if (Qt.platform.os === "ios" || Qt.platform.os === "android"
                    || Qt.platform.os === "wasm")
                Qt.inputMethod.show()
        }
    }

    Timer {
        id: initialComposerFocus
        interval: 80
        repeat: false
        onTriggered: {
            if (!root.opened || !root.compact || root.editorMode !== "visual") return
            if (!mobilePlainEditor.activeFocus)
                mobilePlainEditor.forceActiveFocus()
            root.ensureEditorVisible(mobilePlainEditor)
            if (!mobilePlainEditor.activeFocus && ++root.initialFocusAttempts < 3)
                initialComposerFocus.restart()
        }
    }

    Timer {
        id: mobileInputCommit
        interval: 110
        repeat: false
        onTriggered: {
            if (root.opened && root.compact && root.editorMode === "visual"
                    && mobilePlainEditor.preeditText.length === 0
                    && richComposer.safeText !== mobilePlainEditor.text)
                richComposer.setBlockText(0, mobilePlainEditor.text)
        }
    }

    Connections {
        target: richComposer
        function onAccountDraftApplied() {
            // The native adapter applies remote content only to a pristine
            // editor; update the manually-backed mobile text field as well.
            if (root.opened && root.compact && root.editorMode === "visual")
                mobilePlainEditor.text = richComposer.safeText
        }
    }

    onOpened: {
        richComposer.setComposerActive(true)
        // Reused Drawer scroll position must not hide the next reply target.
        if (composerScroll.contentItem)
            composerScroll.contentItem.contentY = 0
        root.focusedComposerEditor = null
        root.pendingEditorReveal = false
        root.initialFocusAttempts = 0
        root.selectedParentUrl = ""
        root.selectedContextLabel = ""
        root.selectedContextImage = ""
        root.selectedChannelId = ""
        root.selectedTopics = []
        openProofConnections.fetchTopics()
        root.expandedViewportHeight = root.parent ? root.parent.height : root.height
        root.closeComposerPopups()
        editorMode = "visual"
        showAdvancedFormatting = !root.compact
        insertKind = ""
        mediaInsertFailed = false
        root.emojiThreadIndex = -1
        threadTailModel.clear()
        root.mediaMetadata = ({})
        root.clearPendingMediaQueue()
        root.pendingPublicationCasts = []
        root.publicationPlanError = ""
        if (!mediaUpload.busy) mediaUpload.clear()
        if (root.transientDraft || root.parentCast || root.quotedCast) {
            richComposer.beginContextDocument()
            restoredDraft = false
        } else {
            restoredDraft = richComposer.restoreDraft()
            if (!restoredDraft) richComposer.newDocument()
        }
        if (root.transientDraft) {
            richComposer.markdownSource = String(root.suggestedText || "")
            const embeds = root.suggestedEmbeds || []
            for (let index = 0; index < Math.min(2, embeds.length); ++index)
                richComposer.insertEmbed(String(embeds[index] || ""), "", -1)
        } else if (!root.parentCast && root.suggestedText.length > 0) {
            // Explicit text handed off by the Home quick composer takes
            // precedence over a previously restored rich draft. Import it
            // as literal paragraph text, not Markdown (e.g. "*" and "#"
            // must remain exactly what the user typed).
            richComposer.newDocument()
            restoredDraft = false
            if (root.structuredReport)
                richComposer.markdownSource = root.suggestedText
            else
                richComposer.setBlockText(0, root.suggestedText)
        }
        if (root.initialAction === "quote" && root.quotedCast) {
            richComposer.insertCastReference(String(root.quotedCast.castId || ""),
                                             Number(root.quotedCast.authorFid || 0))
        }
        if (root.compact) {
            mobilePlainEditor.text = richComposer.safeText
            if (root.initialAction !== "media")
                initialComposerFocus.restart()
        }
        // Opening an ordinary Cast pulls the latest account draft immediately.
        // A reply, quote or handoff stays isolated; the native adapter refuses
        // to replace text once the user begins editing this opening.
        if (!root.transientDraft && !root.parentCast && !root.quotedCast)
            openProofConnections.refreshAccountDraft()
        if (root.initialAction === "media") {
            root.pendingMediaBlockIndex = visualEditor.activeBlockIndex
            if (Qt.platform.os === "wasm") {
                // Browser file pickers may require a direct user gesture to open.
                // Reveal the upload panel; its Choose button owns that tap.
                root.insertKind = "media"
                root.initialAction = ""
            } else {
                Qt.callLater(function() {
                    root.openMediaPicker()
                    root.initialAction = ""
                })
            }
        }
    }

    onClosed: {
        // Overlay-hosted selection popups otherwise survive a closed composer
        // and can appear on the Home screen with no selected text.
        mobilePlainEditor.dismissSelectionActions()
        markdownEditor.dismissSelectionActions()
        root.focusedComposerEditor = null
        channelPicker.close()
        initialComposerFocus.stop()
        root.closeComposerPopups()
        root.initialAction = ""
        root.clearPendingMediaQueue()
        if (mediaUpload.busy) mediaUpload.cancel()
        // Cancel/dismiss must not leave unpublished media in QStorage.
        // Keep text as a draft, but release managed media when no Cast is
        // in flight. A queued write may still publish, so it retains media.
        if (!presentationBridge.writeBusy && presentationBridge.writePhase !== "queued")
            richComposer.discardUnpublishedManagedMedia()
        if (!root.transientDraft && !root.parentCast && !root.quotedCast
                && richComposer.dirty && !presentationBridge.writeBusy
                && root.hasDraftContent())
            richComposer.saveDraft()
        presentationBridge.clearReplyTarget()
        richComposer.setComposerActive(false)
        root.abandoned()
    }

    EmojiPicker {
        id: emojiPicker
        parent: Overlay.overlay
        anchorItem: formattingToolbar
        onEmojiSelected: emoji => root.insertEmoji(emoji)
    }

    Connections {
        target: presentationBridge
        function onAuthenticationStateChanged() {
            if (!channelPicker.opened) return
            // A membership from the previous FID must never remain selectable.
            root.selectedParentUrl = ""
            root.selectedContextLabel = ""
            root.selectedContextImage = ""
            root.selectedChannelId = ""
            channelDirectory.openForViewer(
                presentationBridge.isAuthenticated ? presentationBridge.accountFid : 0)
        }
    }

    // The controller confirms the choice after the delegate's tap returns.
    // Never mutate/destroy a ListView row inside its own onClicked handler.
    Connections {
        target: channelDirectory
        function onChannelChosen(selection) {
            if (!root.opened || !channelPicker.opened || !root.showTaxonomyControls) return
            root.selectedParentUrl = String(selection.parentUrl || "")
            root.selectedContextLabel = selection.isHome ? "" : String(selection.name || "")
            root.selectedChannelId = selection.isHome ? "" : String(selection.id || "")
            root.selectedContextImage = selection.isHome ? "" : String(selection.imageUrl || "")
            channelPicker.close()
        }
    }

    // Publishing a draft which cannot fit a canonical Farcaster CAST_ADD
    // requires a deliberate second tap: each independent Cast becomes visible
    // on the network and cannot be made atomic by the editor.
    Popup {
        id: threadPublicationConfirmation
        objectName: "genyProExplicitThreadConfirmation"
        parent: Overlay.overlay
        modal: true
        dim: true
        focus: true
        width: parent ? Math.min(Theme.scaleUi(390),parent.width - Theme.space4 * 2)
                      : Theme.scaleUi(360)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        padding: Theme.space4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: PopupSurface {
            curveDepth: Theme.curveContainer
        }
        contentItem: ColumnLayout {
            spacing: Theme.space3
            RowLayout {
                spacing: Theme.space2
                Icon {
                    text: Theme.iconList
                    preferredSize: Theme.iconSizeSmall
                    color: Theme.proAccent
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.pro.thread_title")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.pro.thread_explanation")
                    .arg(LocaleFormat.formatInteger(root.pendingPublicationCasts.length))
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeBodySize
                color: Theme.textSecondary
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: LocaleFormat.formatInteger(root.authoredCharacters) + " / "
                      + LocaleFormat.formatInteger(root.maxComposerCharacters)
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.proAccent
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    Layout.fillWidth: true
                    quiet: true
                    compact: true
                    text: qsTrId("genycaster.shell.cancel")
                    onClicked: {
                        threadPublicationConfirmation.close()
                        root.pendingPublicationCasts = []
                    }
                }
                ActionButton {
                    Layout.fillWidth: true
                    prominent: true
                    compact: true
                    text: qsTrId("genycaster.pro.publish_thread")
                    onClicked: {
                        if (!root.pendingPublicationCasts.length
                                || presentationBridge.writeBusy
                                || !root.withinAccountCharacterLimit
                                || root.attachedMediaCount > root.maxMediaUploads)
                            return
                        const casts = root.pendingPublicationCasts
                        root.pendingPublicationCasts = []
                        threadPublicationConfirmation.close()
                        richComposer.saveDraft()
                        root.threadRequested(casts)
                    }
                }
            }
        }
    }

    Popup {
        id: clearContentConfirmation
        objectName: "composerClearContentConfirmation"
        parent: Overlay.overlay
        modal: true
        dim: true
        focus: true
        width: parent ? Math.min(400, parent.width - Theme.space5 * 2) : 360
        height: clearContentColumns.implicitHeight + Theme.space4 * 2
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        padding: Theme.space4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Superellipse {
            fillColor: Theme.surfaceElevated
            borderWidth: 1
            borderColor: Theme.borderStrong
            curveDepth: Theme.curveContainer
        }
        contentItem: ColumnLayout {
            id: clearContentColumns
            spacing: Theme.space3
            DirectionAwareLabel {
                Layout.fillWidth: true
                color: Theme.ink
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightBold
                text: qsTrId("genycaster.composer.clear_all_title")
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                text: qsTrId("genycaster.composer.clear_all_confirmation")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                Item { Layout.fillWidth: true }
                ActionButton {
                    compact: true
                    quiet: true
                    text: qsTrId("genycaster.shell.cancel")
                    onClicked: clearContentConfirmation.close()
                }
                ActionButton {
                    compact: true
                    prominent: true
                    iconGlyph: Theme.iconTrash
                    text: qsTrId("genycaster.composer.clear_content")
                    onClicked: {
                        clearContentConfirmation.close()
                        root.clearAllComposerContent()
                    }
                }
            }
        }
    }

    // Desktop uses an anchored combobox; compact uses a modal bottom sheet.
    // Parent URL remains owned by the C++ bridge.
    Popup {
        id: channelPicker
        objectName: "composerChannelDropdown"
        // Popups in Qt use coordinates relative to their parent Item.
        // Anchor to the *actual* selector instead of the window overlay:
        // the latter makes placement drift as the composer's Drawer moves.
        readonly property Item triggerItem: root.compact ? selectedChannelChip : desktopCastChannelTrigger
        parent: root.compact ? root.contentItem : triggerItem
        popupType: Popup.Item
        z: 100
        modal: root.compact
        dim: root.compact
        focus: true
        padding: Theme.space3
        topPadding: root.compact ? Theme.space6 : Theme.space3
        bottomPadding: root.compact ? Math.max(Theme.space3, root.bottomSafeInset) : Theme.space3
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        readonly property real boundaryInset: Theme.space6
        // Make the position binding responsive to the trigger's own geometry,
        // along with the enclosing composer's size.
        readonly property point triggerOrigin: {
            const repaint = triggerItem ? triggerItem.x + triggerItem.y : 0
            const viewportSize = root.width + root.height
            return triggerItem && root.contentItem
                ? triggerItem.mapToItem(root.contentItem, 0, 0) : Qt.point(0, 0)
        }
        width: root.compact ? (root.contentItem ? root.contentItem.width : root.width)
            : Math.max(0, Math.min(410, root.contentItem
                ? root.contentItem.width - boundaryInset * 2 : root.width - boundaryInset * 2))
        // Constrain horizontally to the *composer*, never to the window.
        // In RTL, prefer the trigger's right edge instead of its left edge.
        x: {
            if (!root.contentItem || !triggerItem) return 0
            if (root.compact) return 0
            const leftLimit = boundaryInset - triggerOrigin.x
            const rightLimit = root.contentItem.width - boundaryInset
                               - triggerOrigin.x - width
            const preferred = Theme.rtl ? triggerItem.width - width : 0
            return Math.max(leftLimit, Math.min(Math.max(leftLimit, rightLimit), preferred))
        }
        // The mobile sheet is attached to the viewport's bottom edge, not the
        // selector. Resizing for the keyboard preserves a bounded scroll area.
        y: root.compact
            ? Math.max(0, (root.contentItem ? root.contentItem.height : root.height) - height)
            : (triggerItem ? triggerItem.height + Theme.space2 : 0)
        height: root.compact
            ? Math.max(0, Math.min(520,
                (root.contentItem ? root.contentItem.height : root.height) * 0.72))
            : Math.max(0, Math.min(390, root.contentItem
                ? root.contentItem.height - boundaryInset - triggerOrigin.y - y : 0))
        background: PopupSurface {
            fillColor: root.compact ? Theme.surfaceRaised : Theme.surfaceElevated
            borderWidth: 1
            borderColor: Theme.borderMuted
            curveDepth: root.compact ? Theme.curveSheet : Theme.curveContainer
            elevation: root.compact ? Theme.elevationNone : Theme.elevationModal
            Superellipse {
                visible: root.compact
                anchors.top: parent.top
                anchors.topMargin: Theme.space2
                anchors.horizontalCenter: parent.horizontalCenter
                width: 36
                height: 4
                fillColor: Theme.textTertiary
                borderWidth: 0
            }
        }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "y"
                    from: root.compact ? root.height : channelPicker.y
                    duration: root.compact && !Theme.reduceMotion ? Theme.motionStandard : 0
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "opacity"
                    from: 0; to: 1
                    duration: Theme.reduceMotion ? 0 : Theme.motionFast
                }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "y"
                    to: root.compact ? root.height : channelPicker.y
                    duration: root.compact && !Theme.reduceMotion ? Theme.motionFast : 0
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    property: "opacity"
                    to: 0
                    duration: Theme.reduceMotion ? 0 : Theme.motionFast
                }
            }
        }
        onOpened: {
            if (root.compact) Qt.inputMethod.hide()
            topicPicker.close()
            if (!root.showTaxonomyControls) {
                close()
                return
            }
            channelSearch.text = ""
            channelResults.positionViewAtBeginning()
            channelDirectory.openForViewer(
                presentationBridge.isAuthenticated ? presentationBridge.accountFid : 0)
            channelDirectory.search("")
        }
        onClosed: channelDirectory.search("")
        contentItem: ColumnLayout {
            spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.composer.channel_picker_title")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                ActionButton {
                    iconOnly: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.shell.cancel")
                    onClicked: channelPicker.close()
                }
            }
            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                fillColor: Theme.editorSurface
                borderWidth: 1
                borderColor: channelSearch.activeFocus ? Theme.accent : Theme.borderMuted
                curveDepth: Theme.curveControl
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space3
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection
                    Icon {
                        text: Theme.iconSearch
                        color: Theme.textSecondary
                        preferredSize: Theme.iconSizeSmall
                    }
                    TextField {
                        id: channelSearch
                        objectName: "composerChannelSearch"
                        LayoutMirroring.enabled: false
                        horizontalAlignment: Theme.textStartAlignment
                        Layout.fillWidth: true
                        placeholderText: qsTrId("genycaster.composer.channel_search")
                        color: Theme.textPrimary
                        background: null
                        selectByMouse: true
                        onTextChanged: {
                            if (channelPicker.opened)
                                channelDirectory.search(text)
                        }
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: channelSearch.text.trim().length > 0
                      ? qsTrId("genycaster.composer.channel_suggestions")
                      : qsTrId("genycaster.composer.your_channels")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }
            ListView {
                id: channelResults
                objectName: "composerChannelResults"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.space1
                boundsBehavior: Flickable.StopAtBounds
                // Stable C++ model: metadata updates repaint only the affected
                // row, preserving ListView delegates and already-loaded icons.
                model: channelDirectory.channelModel
                ScrollBar.vertical: GenyScrollBar {}
                delegate: Item {
                    id: channelRow
                    required property var modelData
                    width: channelResults.width
                    height: 52
                    readonly property string channelParentUrl: String(modelData.parentUrl || "")
                    readonly property bool isHome: Boolean(modelData.isHome)
                    readonly property bool selectable: Boolean(modelData.selectable)
                    readonly property bool isMember: Boolean(modelData.isMember)
                    Superellipse {
                        anchors.fill: parent
                        fillColor: channelRow.selectable && rowTap.pressed
                                   ? Theme.surfacePressed
                                   : channelRow.selectable && rowTap.containsMouse
                                     ? Theme.surfaceHover : Theme.surfaceRaised
                        borderWidth: 1
                        borderColor: Theme.borderSubtle
                        curveDepth: Theme.curveControl
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        spacing: Theme.space3
                        layoutDirection: Theme.uiDirection
                        Avatar {
                            Layout.alignment: Qt.AlignVCenter
                            avatarSize: 32
                            forceAutomaticDownload: true
                            sourceUrl: String(channelRow.modelData.imageUrl || "")
                            displayName: String(channelRow.modelData.name || "")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                color: channelRow.selectable ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.typeBodySize
                                font.weight: Theme.weightDemiBold
                                text: channelRow.isHome
                                      ? qsTrId("genycaster.composer.home_channel")
                                      : String(channelRow.modelData.name || channelRow.modelData.id || "")
                                alignToUi: true
                                elide: Text.ElideRight
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                color: Theme.textSecondary
                                font.pixelSize: Theme.typeCaptionSize
                                text: channelRow.isHome
                                    ? qsTrId("genycaster.composer.no_channel")
                                    : channelRow.modelData.followerCount >= 0
                                      ? qsTrId("genycaster.composer.channel_follower_count")
                                          .arg(LocaleFormat.formatInteger(channelRow.modelData.followerCount))
                                      : channelRow.modelData.memberCount >= 0
                                        ? qsTrId("genycaster.composer.channel_member_count")
                                            .arg(LocaleFormat.formatInteger(channelRow.modelData.memberCount))
                                        : (channelRow.isMember
                                            ? qsTrId("genycaster.composer.joined_channel")
                                            : qsTrId("genycaster.composer.not_joined_channel"))
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }
                        DirectionAwareLabel {
                            visible: !channelRow.isHome && !channelRow.selectable
                            text: qsTrId("genycaster.composer.not_joined_channel")
                            color: Theme.textTertiary
                            font.pixelSize: Theme.typeCaptionSize
                        }
                        Icon {
                            text: (channelRow.isHome && root.selectedParentUrl.length === 0)
                                  || (channelRow.channelParentUrl.length > 0
                                      && channelRow.channelParentUrl === root.selectedParentUrl)
                                  ? Theme.iconCheck : channelRow.selectable ? Theme.iconArrowRight : "system/lock"
                            color: channelRow.selectable ? Theme.accent : Theme.textTertiary
                            preferredSize: Theme.iconSizeSmall
                        }
                    }
                    Accessible.role: channelRow.selectable ? Accessible.Button : Accessible.StaticText
                    Accessible.name: String(channelRow.modelData.name || "")
                    Accessible.description: channelRow.isHome
                        ? qsTrId("genycaster.composer.no_channel")
                        : channelRow.isMember
                          ? qsTrId("genycaster.composer.joined_channel")
                          : qsTrId("genycaster.composer.not_joined_channel")
                    MouseArea {
                        id: rowTap
                        anchors.fill: parent
                        enabled: channelRow.selectable
                        hoverEnabled: !root.compact && channelRow.selectable
                        onClicked: {
                            // A synchronous model refresh inside chooseChannel
                            // previously invalidated this delegate before the
                            // chip was updated. A controller signal applies it.
                            channelDirectory.chooseChannel(String(channelRow.modelData.id || ""))
                        }
                    }
                }
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: !channelDirectory.busy && channelDirectory.rows.length <= 1
                color: Theme.textSecondary
                wrapMode: Text.Wrap
                text: channelDirectory.errorText.length > 0
                      ? channelDirectory.errorText
                      : channelSearch.text.trim().length > 0
                        ? qsTrId("genycaster.composer.channel_no_results")
                        : qsTrId("genycaster.composer.channel_search_prompt")
                font.pixelSize: Theme.typeCaptionSize
            }
            GenySpinner {
                Layout.alignment: Qt.AlignHCenter
                visible: channelDirectory.busy
                running: visible
            }
        }
    }

    // Topic selection shares the mobile bottom-sheet presentation.
    // Topic metadata remains owned by the C++ bridge.
    Popup {
        id: topicPicker
        objectName: "composerTopicDropdown"
        // Popups in Qt use coordinates relative to their parent Item.
        // Anchor to the *actual* selector instead of the window overlay:
        // the latter makes placement drift as the composer's Drawer moves.
        readonly property Item triggerItem: root.compact ? selectedTopicChip : desktopCastTopicTrigger
        parent: root.compact ? root.contentItem : triggerItem
        popupType: Popup.Item
        z: 100
        modal: root.compact
        dim: root.compact
        focus: true
        padding: Theme.space3
        topPadding: root.compact ? Theme.space6 : Theme.space3
        bottomPadding: root.compact ? Math.max(Theme.space3, root.bottomSafeInset) : Theme.space3
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        readonly property real boundaryInset: Theme.space6
        // Make the position binding responsive to the trigger's own geometry,
        // along with the enclosing composer's size.
        readonly property point triggerOrigin: {
            const repaint = triggerItem ? triggerItem.x + triggerItem.y : 0
            const viewportSize = root.width + root.height
            return triggerItem && root.contentItem
                ? triggerItem.mapToItem(root.contentItem, 0, 0) : Qt.point(0, 0)
        }
        width: root.compact ? (root.contentItem ? root.contentItem.width : root.width)
            : Math.max(0, Math.min(410, root.contentItem
                ? root.contentItem.width - boundaryInset * 2 : root.width - boundaryInset * 2))
        // Constrain horizontally to the *composer*, never to the window.
        // In RTL, prefer the trigger's right edge instead of its left edge.
        x: {
            if (!root.contentItem || !triggerItem) return 0
            if (root.compact) return 0
            const leftLimit = boundaryInset - triggerOrigin.x
            const rightLimit = root.contentItem.width - boundaryInset
                               - triggerOrigin.x - width
            const preferred = Theme.rtl ? triggerItem.width - width : 0
            return Math.max(leftLimit, Math.min(Math.max(leftLimit, rightLimit), preferred))
        }
        // The mobile sheet is attached to the viewport's bottom edge, not the
        // selector. Resizing for the keyboard preserves a bounded scroll area.
        y: root.compact
            ? Math.max(0, (root.contentItem ? root.contentItem.height : root.height) - height)
            : (triggerItem ? triggerItem.height + Theme.space2 : 0)
        height: root.compact
            ? Math.max(0, Math.min(460,
                (root.contentItem ? root.contentItem.height : root.height) * 0.62))
            : Math.max(0, Math.min(330, root.contentItem
                ? root.contentItem.height - boundaryInset - triggerOrigin.y - y : 0))
        background: PopupSurface {
            fillColor: root.compact ? Theme.surfaceRaised : Theme.surfaceElevated
            borderWidth: 1
            borderColor: Theme.borderMuted
            curveDepth: root.compact ? Theme.curveSheet : Theme.curveContainer
            elevation: root.compact ? Theme.elevationNone : Theme.elevationModal
            Superellipse {
                visible: root.compact
                anchors.top: parent.top
                anchors.topMargin: Theme.space2
                anchors.horizontalCenter: parent.horizontalCenter
                width: 36
                height: 4
                fillColor: Theme.textTertiary
                borderWidth: 0
            }
        }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "y"
                    from: root.compact ? root.height : topicPicker.y
                    duration: root.compact && !Theme.reduceMotion ? Theme.motionStandard : 0
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "opacity"
                    from: 0; to: 1
                    duration: Theme.reduceMotion ? 0 : Theme.motionFast
                }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "y"
                    to: root.compact ? root.height : topicPicker.y
                    duration: root.compact && !Theme.reduceMotion ? Theme.motionFast : 0
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    property: "opacity"
                    to: 0
                    duration: Theme.reduceMotion ? 0 : Theme.motionFast
                }
            }
        }
        onOpened: {
            if (root.compact) Qt.inputMethod.hide()
            channelPicker.close()
            topicSearch.text = ""
            openProofConnections.fetchTopics()
        }
        contentItem: ColumnLayout {
            spacing: Theme.space2
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.topics.choose")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                }
                ActionButton {
                    compact: true
                    iconOnly: true
                    quiet: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.shell.cancel")
                    onClicked: topicPicker.close()
                }
            }
            DirectionAwareTextField {
                id: topicSearch
                objectName: "composerTopicSearch"
                visible: !root.compact
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 40 : 0
                placeholderText: qsTrId("genycaster.topics.search")
            }
            Flickable {
                id: topicScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: topicFlow.height
                Flow {
                    id: topicFlow
                    width: topicScroll.width
                    height: childrenRect.height
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    Repeater {
                        model: openProofConnections ? openProofConnections.topicCatalog : []
                        delegate: Item {
                            id: topicChip
                            required property var modelData
                            readonly property string slug: String(modelData.slug || "")
                            readonly property string title: root.topicName(slug)
                            readonly property bool chosen: root.selectedTopics.indexOf(slug)>=0
                            visible: topicSearch.text.trim().length === 0
                                     || title.toLocaleLowerCase().includes(topicSearch.text.trim().toLocaleLowerCase())
                                     || slug.includes(topicSearch.text.trim().toLocaleLowerCase())
                            width: Math.min(topicFlow.width, chipText.implicitWidth
                                            + (chosen ? Theme.scaleUi(36) : Theme.scaleUi(24)))
                            // Keep the 44dp hit target, but render compact
                            // topic chips in every client form factor.
                            height: Math.max(44, Theme.touchTarget)

                            Superellipse {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: Theme.scaleUi(34)
                                fillColor: topicChip.chosen ? Theme.primarySurface : Theme.surfaceRaised
                                borderWidth: topicChip.chosen ? 2 : 1
                                borderColor: topicChip.chosen ? Theme.accent : Theme.borderMuted
                                curveDepth: Theme.curveControl
                            }
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: Theme.space1
                                layoutDirection: Theme.uiDirection
                                Icon {
                                    visible: topicChip.chosen
                                    text: Theme.iconCheck
                                    color: Theme.accent
                                    preferredSize: Theme.iconSizeSmall
                                }
                                DirectionAwareLabel {
                                    id: chipText
                                    text: topicChip.title
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeMetaSize
                                    font.weight: topicChip.chosen
                                                 ? Theme.weightDemiBold : Theme.weightMedium
                                    alignToUi: true
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: topicChip.chosen
                                         || root.selectedTopics.length < root.maxCastTopics
                                onClicked: root.toggleTopic(topicChip.slug)
                            }
                            Accessible.role: Accessible.Button
                            Accessible.name: topicChip.title
                            Accessible.description: topicChip.chosen
                                ? qsTrId("genycaster.topics.remove_selection")
                                : qsTrId("genycaster.topics.add_selection")
                        }
                    }
                }
            }
            GenySpinner {
                Layout.alignment: Qt.AlignHCenter
                visible: openProofConnections ? openProofConnections.topicsBusy : false
                running: visible
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: !openProofConnections
                         || (!openProofConnections.topicsBusy
                             && openProofConnections.topicCatalog.length === 0)
                text: qsTrId("genycaster.topics.unavailable")
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.topics.limit").arg(
                        LocaleFormat.formatInteger(root.selectedTopics.length)).arg(
                        LocaleFormat.formatInteger(root.maxCastTopics))
                    color: Theme.textSecondary
                }
                ActionButton {
                    compact: true
                    quiet: true
                    enabled: root.selectedTopics.length > 0
                    text: qsTrId("genycaster.topics.clear")
                    onClicked: root.selectedTopics=[]
                }
                ActionButton {
                    compact: true
                    prominent: true
                    text: qsTrId("genycaster.topics.done")
                    onClicked: topicPicker.close()
                }
            }
        }
    }

    FileDialog {
        id: mediaFileDialog
        title: qsTrId("genycaster.media.choose_file")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTrId("genycaster.media.images_and_videos") + " (*.jpg *.jpeg *.png *.webp *.gif *.avif *.mp4 *.webm *.mov)",
            qsTrId("genycaster.media.images") + " (*.jpg *.jpeg *.png *.webp *.gif *.avif)",
            qsTrId("genycaster.media.videos") + " (*.mp4 *.webm *.mov)"
        ]
        onAccepted: {
            root.mediaInsertFailed = false
            root.enqueueMediaFiles(selectedFiles, root.pendingMediaBlockIndex)
        }
    }

    Connections {
        target: mediaUpload
        function onAssetReady(url, mediaType, managementToken, afterIndex) {
            // Profile avatar/cover uploads are handled elsewhere and must
            // never be constrained by cast media admission.
            if (afterIndex === -1001 || afterIndex === -1002) return
            if (root.attachedMediaCount >= root.maxMediaUploads) {
                root.mediaInsertFailed = true
                root.pendingMediaFiles = []
                mediaUpload.clear()
                return
            }
            const canonicalUrl = String(url || "")
            const nextMetadata = Object.assign({}, root.mediaMetadata)
            nextMetadata[canonicalUrl] = {
                fileName: mediaUpload.fileName,
                localPreviewUrl: mediaUpload.localPreviewUrl.toString(),
                mediaType: String(mediaType || mediaUpload.selectedContentType || ""),
                sizeBytes: Number(mediaUpload.totalBytes || 0)
            }
            root.mediaMetadata = nextMetadata

            if (!richComposer.insertManagedMedia(url, mediaType, managementToken, "", afterIndex)) {
                root.mediaInsertFailed = true
                return
            }
            root.mediaInsertFailed = false
            if (afterIndex >= 0)
                root.pendingMediaAfterIndex = afterIndex + 1
            mediaUpload.clear()
            if (!root.startNextMediaUpload())
                root.insertKind = ""
        }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: root.opened && !emojiPicker.opened
                 && !channelPicker.opened && !topicPicker.opened
                 && !clearContentConfirmation.opened
        onActivated: root.close()
    }
    Shortcut { sequence: "Ctrl+Return"; enabled: root.opened && !emojiPicker.opened; onActivated: root.publishCurrent() }
    Shortcut { sequence: "Meta+Return"; enabled: root.opened && !emojiPicker.opened; onActivated: root.publishCurrent() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: root.opened && !emojiPicker.opened && richComposer.canUndo; onActivated: richComposer.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: root.opened && !emojiPicker.opened && richComposer.canRedo; onActivated: richComposer.redo() }
    Shortcut { sequence: "Ctrl+K"; enabled: root.opened && !emojiPicker.opened; onActivated: root.openLinkFromSelection() }
    Shortcut { sequence: "Meta+K"; enabled: root.opened && !emojiPicker.opened; onActivated: root.openLinkFromSelection() }
    Shortcut { sequence: "Ctrl+Shift+C"; enabled: root.opened && !emojiPicker.opened; onActivated: root.insertCodeBlockFromKeyboard() }
    Shortcut { sequence: "Meta+Shift+C"; enabled: root.opened && !emojiPicker.opened; onActivated: root.insertCodeBlockFromKeyboard() }

    // Fixed chrome stays outside the scrollable document. Resizing the window
    // must never move publish/cancel or mode switches behind the editor.
    contentItem: ColumnLayout {
        spacing: 0
        ColumnLayout {
            id: composerFixedChrome
            Layout.fillWidth: true
            Layout.leftMargin: root.compact ? Theme.mobileGutter : Theme.space6
            Layout.rightMargin: Layout.leftMargin
            spacing: root.compact ? Theme.space2 : Theme.space2
            Item { Layout.preferredHeight: root.keyboardVisible ? 0
                                      : root.compact ? Theme.space1 : Theme.space2 }

            // Full-page mobile top bar: Cancel / Drafts / Cast. Desktop
            // retains its existing rich-composer header.
            RowLayout {
                visible: root.compact
                objectName: "mobileComposerNavigation"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touchTarget
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                ActionButton {
                    quiet: true
                    compact: true
                    text: qsTrId("genycaster.shell.cancel")
                    onClicked: root.close()
                }
                Item { Layout.fillWidth: true }
                ActionButton {
                    // The only persistent draft slot belongs to a new Cast.
                    // Contextual quote/reply drafts use isolated documents.
                    visible: !root.replyMode && !root.quoteMode && !root.transientDraft
                    quiet: true
                    compact: true
                    text: qsTrId("genycaster.composer.save_draft")
                    enabled: !presentationBridge.writeBusy && root.hasDraftContent()
                    onClicked: {
                        if (!root.hasDraftContent()) return
                        root.synchronizeEditorState()
                        if (root.hasDraftContent())
                            richComposer.saveDraft()
                        root.close()
                    }
                }
                ActionButton {
                    prominent: true
                    compact: true
                    text: root.parentCast ? qsTrId("genycaster.shell.reply")
                                         : qsTrId("genycaster.shell.publish")
                    enabled: !presentationBridge.writeBusy && !mediaUpload.busy
                             && root.withinAccountCharacterLimit && root.hasPublishableContent()
                    onClicked: root.publishCurrent()
                }
            }

            RowLayout {
                visible: !root.compact
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space3

                ActionButton {
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    iconSize: Theme.iconSize
                    text: qsTrId("genycaster.shell.close")
                    pill: true
                    elevated: false
                    onClicked: root.close()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.threadMode ? qsTrId("genycaster.composer.thread_title")
                              : root.parentCast ? qsTrId("genycaster.shell.reply") : qsTrId("genycaster.shell.new_cast")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle3Size
                        font.weight: Theme.weightBold
                    }
                }

                ActionButton {
                    objectName: "composerClearContentDesktop"
                    visible: root.hasPublishableContent()
                    iconOnly: root.width < 480
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconTrash
                    text: qsTrId("genycaster.composer.clear_content")
                    enabled: !presentationBridge.writeBusy && root.hasPublishableContent()
                    onClicked: clearContentConfirmation.open()
                }
                ActionButton {
                    prominent: true
                    compact: true
                    iconOnly: root.width < 480
                    iconGlyph: Theme.iconSend
                    text: root.parentCast ? qsTrId("genycaster.shell.reply")
                                         : qsTrId("genycaster.shell.publish")
                    enabled: !presentationBridge.writeBusy && !mediaUpload.busy
                             && root.withinAccountCharacterLimit && root.hasPublishableContent()
                    onClicked: root.publishCurrent()
                }
            }

            ComposerToolbar {
                visible: !root.compact && !root.keyboardVisible
                Layout.fillWidth: true
                mode: root.editorMode
                onModeRequested: mode => root.setEditorMode(mode)
            }

            RowLayout {
                objectName: "desktopCastTopics"
                visible: !root.compact && root.showTaxonomyControls
                Layout.fillWidth: true
                // The visual editor already provides its own breathing room.
                // Markdown and Preview use the same taxonomy spacing, without
                // touching the media/footer geometry or the mobile sheet.
                Layout.topMargin: root.editorMode === "visual" ? 0 : Theme.space2
                Layout.bottomMargin: root.editorMode === "visual" ? 0 : Theme.space3
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                ActionButton {
                    id: desktopCastChannelTrigger
                    objectName: "desktopCastChannelTrigger"
                    compact: true
                    selected: root.selectedParentUrl.length > 0
                    iconGlyph: Theme.iconGlobe
                    text: root.selectedContextLabel.length > 0
                        ? root.selectedContextLabel : qsTrId("genycaster.composer.channel")
                    onClicked: channelPicker.open()
                }
                ActionButton {
                    visible: root.selectedParentUrl.length > 0
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.composer.context_clear")
                    onClicked: {
                        root.selectedParentUrl = ""
                        root.selectedContextLabel = ""
                        root.selectedContextImage = ""
                        root.selectedChannelId = ""
                    }
                }
                ActionButton {
                    id: desktopCastTopicTrigger
                    compact: true
                    selected: root.selectedTopics.length > 0
                    iconGlyph: Theme.iconPalette
                    text: root.selectedTopics.length > 0
                          ? qsTrId("genycaster.topics.selected_count").arg(
                                LocaleFormat.formatInteger(root.selectedTopics.length))
                          : qsTrId("genycaster.topics.label")
                    onClicked: topicPicker.open()
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    text: root.selectedTopics.map(s=>root.topicName(s)).join(" · ")
                    elide: Text.ElideRight
                    alignToUi: true
                }
            }

            // The tool row is stable chrome above the scrollable document,
            // never overlaid on Channel/Topics or clipped during resizing.
            FormattingToolbar {
                id: formattingToolbar
                Layout.bottomMargin: !root.compact ? Theme.space2 : 0
                visible: root.editorMode === "visual" && (!root.compact || root.showAdvancedFormatting)
                         && !root.keyboardVisible
                Layout.fillWidth: true
                activeBlockIndex: visualEditor.activeBlockIndex
                activeEditor: visualEditor.formattingEditor
                editorHost: visualEditor
                onProFeatureRequested: composerProNotice.open()
                onLinkRequested: (blockIndex, selectionStart, selectionLength, selectedText) => {
                    root.insertKind = "link"
                    root.insertBlockIndex = blockIndex
                    root.insertSelectionStart = selectionStart
                    root.insertSelectionLength = selectionLength
                    root.insertSelectedText = selectedText
                }
                onMentionRequested: (blockIndex, selectionStart, selectionLength, selectedText) => {
                    root.insertKind = "mention"
                    root.insertBlockIndex = blockIndex
                    root.insertSelectionStart = selectionStart
                    root.insertSelectionLength = selectionLength
                    root.insertSelectedText = selectedText
                }
                onEmbedRequested: blockIndex => {
                    root.insertKind = "embed"
                    root.insertBlockIndex = blockIndex
                    root.insertSelectedText = ""
                }
                onMediaRequested: blockIndex => {
                    root.insertKind = "media"
                    root.insertBlockIndex = blockIndex
                    root.insertSelectedText = ""
                }
                onEmojiRequested: {
                    root.emojiThreadIndex = -1
                    root.emojiThreadCursorPosition = -1
                    emojiPicker.anchorItem = formattingToolbar
                    emojiPicker.open()
                }
            }

        }
        ScrollView {
        id: composerScroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 0
        clip: true
        onHeightChanged: {
            if (root.keyboardVisible && root.focusedComposerEditor)
                root.ensureEditorVisible(root.focusedComposerEditor)
        }
        contentWidth: availableWidth
        contentHeight: composerContent.implicitHeight
        ScrollBar.horizontal: GenyScrollBar { policy: ScrollBar.AlwaysOff }
        ScrollBar.vertical: GenyScrollBar {}

        ColumnLayout {
            id: composerContent
            readonly property int sideMargin: root.compact ? Theme.mobileGutter : Theme.space6
            width: Math.max(0, composerScroll.availableWidth - sideMargin * 2)
            x: sideMargin
            spacing: root.keyboardVisible ? Theme.space2 : Theme.space3

            Superellipse {
                id: replyContextCard
                visible: root.replyMode || root.quoteMode
                Layout.fillWidth: true
                // Keep the parent/quoted Cast readable even with the keyboard
                // open, while saving vertical room for the reply editor.
                implicitHeight: replyContextContent.implicitHeight
                                + (root.compact && root.keyboardVisible
                                   ? Theme.space2 * 2 : Theme.space4 * 2)
                curveDepth: Theme.curveContainer
                fillColor: replyContextHover.hovered ? Theme.primarySurfaceHover : Theme.primarySurface
                borderWidth: Theme.strokeHairline
                borderColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                     Theme.dark ? 0.46 : 0.28)
                Accessible.role: Accessible.Button
                Accessible.name: root.quotedCast
                                 ? qsTrId("genycaster.composer.quote")
                                 : qsTrId("genycaster.cast.replying_to_value").arg(
                                     root.replyAuthor.length > 0 ? "@" + root.replyAuthor : root.replyName)
                Accessible.description: root.replyText

                Behavior on fillColor {
                    ColorAnimation { duration: Theme.motionFast }
                }

                RowLayout {
                    id: replyContextContent
                    anchors.fill: parent
                    anchors.margins: root.compact && root.keyboardVisible
                                     ? Theme.space2 : Theme.space4
                    spacing: root.compact && root.keyboardVisible ? Theme.space2 : Theme.space3
                    layoutDirection: root.replyContentRtl ? Qt.RightToLeft : Qt.LeftToRight
                    LayoutMirroring.enabled: false
                    LayoutMirroring.childrenInherit: true

                    Avatar {
                        Layout.alignment: Qt.AlignTop
                        avatarSize: root.compact ? (root.keyboardVisible ? 32 : 40) : 48
                        sourceUrl: root.quotedCast ? String(root.quotedCast.avatarUrl || "")
                                                  : root.parentCast ? String(root.parentCast.avatarUrl || "") : ""
                        displayName: root.quotedCast ? String(root.quotedCast.displayName || root.quotedCast.author || "") : root.replyName
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: root.replyContentRtl ? Qt.RightToLeft : Qt.LeftToRight

                            Superellipse {
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 26
                                curveDepth: 13
                                fillColor: Theme.surfaceRaised

                                Icon {
                                    anchors.centerIn: parent
                                    text: root.quotedCast ? Theme.iconQuote : Theme.iconReply
                                    color: Theme.accent
                                    preferredSize: Theme.iconSizeSmall
                                }
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: root.quotedCast ? qsTrId("genycaster.composer.quote")
                                      : qsTrId("genycaster.cast.replying_to_value").arg(
                                          root.replyAuthor.length > 0 ? "@" + root.replyAuthor : root.replyName)
                                color: Theme.accent
                                font.pixelSize: Theme.typeMetaSize
                                font.weight: Theme.weightDemiBold
                                elide: Text.ElideRight
                            }
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            visible: !root.compact || !root.keyboardVisible
                            text: root.quotedCast
                                  ? String(root.quotedCast.displayName || root.quotedCast.author || "")
                                  : root.replyName
                            color: Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightBold
                            elide: Text.ElideRight
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.quotedCast
                                  ? String(root.quotedCast.text || qsTrId("genycaster.embed.open_quoted_cast"))
                                  : root.replyText.length > 0
                                    ? root.replyText : qsTrId("genycaster.embed.open_quoted_cast")
                            color: Theme.ink
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightRegular
                            lineHeight: Theme.typeBodyLineHeight
                            maximumLineCount: root.compact && root.keyboardVisible ? 1
                                              : root.compact ? 2 : 3
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                        }
                    }

                    Superellipse {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: Theme.compactControlHeight
                        Layout.preferredHeight: Theme.compactControlHeight
                        curveDepth: Theme.compactControlHeight / 2
                        fillColor: Theme.surfaceRaised
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderSubtle

                        Icon {
                            anchors.centerIn: parent
                            text: root.replyContentRtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                            color: Theme.inkMuted
                            preferredSize: Theme.iconSizeSmall
                        }
                    }
                }

                HoverHandler { id: replyContextHover }
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: {
                        const castId = root.quotedCast ? String(root.quotedCast.castId || "")
                                       : root.parentCast ? String(root.parentCast.castId || "") : ""
                        if (castId.length > 0) presentationBridge.openCast(castId)
                    }
                }
            }

            ActionButton {
                objectName: "composerMobileFormattingToggle"
                visible: false
                compact: true
                quiet: true
                Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                iconGlyph: Theme.iconSettings
                text: qsTrId("genycaster.composer.formatting_tools")
                onClicked: root.showAdvancedFormatting = !root.showAdvancedFormatting
            }

            ComposerInsertPanel {
                visible: root.editorMode === "visual" && root.insertKind.length > 0
                         && root.insertKind !== "media"
                Layout.fillWidth: true
                kind: root.insertKind
                blockIndex: root.insertBlockIndex
                selectionStart: root.insertSelectionStart
                selectionLength: root.insertSelectionLength
                selectedText: root.insertSelectedText
                onFinished: root.insertKind = ""
                onMediaFileRequested: blockIndex => {
                    root.pendingMediaBlockIndex = blockIndex
                    root.openMediaPicker()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.editorMode === "preview"
                                        ? farcasterPreview.implicitHeight
                                        : root.threadMode
                                          ? (root.compact
                                             ? Math.max(118, Math.min(188, composerScroll.height * 0.36))
                                             : 250)
                                          : root.replyMode
                                            ? (root.compact
                                               ? Math.max(118, Math.min(192, composerScroll.height * 0.36))
                                               : 225)
                                            : root.quoteMode
                                              ? (root.compact
                                                 ? Math.max(138, Math.min(232, composerScroll.height * 0.43))
                                                 : 280)
                                              : (root.compact
                                                 ? (mediaPanel.visible
                                                    ? Math.max(135, Math.min(192, composerScroll.height * 0.30))
                                                    : Math.max(150, Math.min(
                                                        Math.max(260, mobilePlainEditor.contentHeight
                                                            + Theme.space4 * 2),
                                                        Math.max(300, composerScroll.height * 1.5))))
                                                 : root.structuredReport && root.editorMode === "visual"
                                                   // Use the wrapped text's actual height. Content
                                                   // exceeding the viewport scrolls in composerScroll.
                                                   ? Math.max(255, visualEditor.naturalSingleParagraphHeight)
                                                   : Math.max(128, Math.min(255, root.height * 0.4)))
                Layout.minimumHeight: root.editorMode === "preview"
                                      ? Math.min(farcasterPreview.implicitHeight, root.compact ? 210 : 230)
                                      : root.threadMode
                                        ? (root.compact ? 118 : 96)
                                        : root.replyMode
                                          ? (root.compact ? 118 : 96)
                                          : root.quoteMode
                                            ? (root.compact ? 138 : 96)
                                            : (root.compact ? 135 : 108)
                spacing: Theme.space3
                layoutDirection: Qt.LeftToRight

                Avatar {
                    visible: root.compact && root.editorMode === "visual"
                    Layout.alignment: Qt.AlignTop
                    avatarSize: 42
                    sourceUrl: String(presentationBridge.accountAvatarUrl || "")
                    displayName: String(presentationBridge.accountName || "")
                }
                Superellipse {
                    visible: root.editorMode !== "preview"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    fillColor: root.compact ? "transparent" : Theme.editorSurface
                    borderWidth: root.compact ? 0 : Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: root.compact ? 0 : Theme.curveLarge

                    DirectionAwareTextArea {
                        id: mobilePlainEditor
                        selectAllOnDoubleTap: true
                        objectName: "composerPrimaryMobileEditor"
                        visible: root.compact && root.editorMode === "visual"
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        // Content-sized editor avoids cutting off multi-line
                        // statistics reports in the iOS/WASM keyboard viewport.
                        clip: true
                        placeholderText: qsTrId("genycaster.shell.whats_happening")
                        background: null
                        selectByMouse: true
                        persistentSelection: true
                        wrapMode: TextEdit.Wrap
                        font.pixelSize: Theme.typeBodySize
                        // Do not rebuild the rich document during a native
                        // keyboard's marked-text/emoji composition. Qt may
                        // otherwise replace the active preedit selection.
                        onTextChanged: {
                            if (root.opened && root.compact && root.editorMode === "visual"
                                    && preeditText.length === 0)
                                mobileInputCommit.restart()
                        }
                        onPreeditTextChanged: {
                            if (preeditText.length === 0 && root.opened)
                                mobileInputCommit.restart()
                        }
                        onActiveFocusChanged: {
                            if (activeFocus)
                                root.ensureEditorVisible(mobilePlainEditor)
                        }
                        onCursorRectangleChanged: {
                            if (activeFocus)
                                root.ensureEditorVisible(mobilePlainEditor)
                        }
                    }

                    RichTextEditor {
                        id: visualEditor
                        visible: !root.compact && root.editorMode === "visual"
                        onActiveEditorChangedExternally: (blockIndex, editor) =>
                            root.ensureEditorVisible(editor)
                        anchors.fill: parent
                        anchors.margins: root.compact ? 0 : Theme.space3
                    }

                    Item {
                        visible: root.editorMode === "markdown"
                        anchors.fill: parent
                        anchors.margins: Theme.space3

                        Timer {
                            id: markdownSync
                            interval: 90
                            repeat: false
                            onTriggered: if (markdownEditor.text !== richComposer.markdownSource) richComposer.markdownSource = markdownEditor.text
                        }
                        DirectionAwareTextArea {
                            id: markdownEditor
                        selectAllOnDoubleTap: true
                            anchors.fill: parent
                            text: richComposer.markdownSource
                            placeholderText: qsTrId("genycaster.composer.markdown_placeholder")
                            background: null
                            selectByMouse: true
                            persistentSelection: true
                            wrapMode: TextEdit.Wrap
                            font.pixelSize: Theme.typeBodySize
                            onTextChanged: if (activeFocus) markdownSync.restart()
                            onActiveFocusChanged: {
                                if (activeFocus)
                                    root.ensureEditorVisible(markdownEditor)
                                else if (text !== richComposer.markdownSource)
                                    richComposer.markdownSource = text
                            }
                        }
                    }
                }

                FarcasterPreview {
                    id: farcasterPreview
                    visible: root.editorMode === "preview"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    compact: root.compact
                    attachmentMetadata: root.mediaMetadata
                }
            }

            // Attachments belong after the cast text, as the Farcaster composer
            // publishes text followed by embeds. Keep the same upload lifecycle.
            MediaUploadPanel {
                id: mediaPanel
                visible: root.editorMode === "visual"
                         && (root.insertKind === "media" || hasAttachedMedia || hasTransientUpload)
                Layout.fillWidth: true
                blockIndex: root.insertBlockIndex
                maxAttachments: root.maxMediaUploads
                insertFailed: root.mediaInsertFailed
                queuedFiles: root.pendingMediaFiles
                attachmentMetadata: root.mediaMetadata
                onBrowseRequested: blockIndex => {
                    if (!root.canUploadMedia) return
                    root.pendingMediaBlockIndex = blockIndex
                    root.mediaInsertFailed = false
                    root.openMediaPicker()
                }
                onFilesDropped: (urls, blockIndex) => {
                    root.mediaInsertFailed = false
                    root.enqueueMediaFiles(urls, blockIndex)
                }
                onResetFailureRequested: root.mediaInsertFailed = false
                onFinished: {
                    root.insertKind = ""
                    root.clearPendingMediaQueue()
                    if (!mediaUpload.busy) {
                        root.mediaInsertFailed = false
                        if (mediaUpload.canRetry || mediaUpload.errorText.length > 0)
                            mediaUpload.clear()
                    }
                }
            }

            // Lightweight, always-available mobile media actions. These are
            // real actions on the existing rich editor, not another composer.
            ColumnLayout {
                visible: root.threadMode
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space2

                Repeater {
                    id: threadRepeater
                    model: threadTailModel

                    // A single-column card avoids competing RTL anchors.
                    // Each continuation is a real scrollable document row,
                    // never a narrow textbox beside a vertical action strip.
                    delegate: ColumnLayout {
                        id: threadRow
                        required property int index
                        required property string text
                        function focusThreadEditor() {
                            threadEditor.forceActiveFocus()
                            root.ensureEditorVisible(threadEditor)
                        }
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: Theme.space1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 14
                                color: Theme.primarySurface
                                border.width: Theme.strokeHairline
                                border.color: Theme.accent
                                Label {
                                    anchors.centerIn: parent
                                    text: LocaleFormat.formatInteger(threadRow.index + 2)
                                    color: Theme.accent
                                    font.pixelSize: Theme.typeCaptionSize
                                    font.weight: Theme.weightBold
                                }
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: qsTrId("genycaster.composer.thread_placeholder")
                                      .arg(LocaleFormat.formatInteger(threadRow.index + 2))
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                            ActionButton {
                                quiet: true
                                compact: true
                                iconOnly: true
                                iconGlyph: Theme.iconEmoji
                                iconSize: Theme.iconSizeSmall
                                text: qsTrId("genycaster.composer.emoji")
                                onClicked: {
                                    root.emojiThreadIndex = threadRow.index
                                    root.emojiThreadCursorPosition = threadEditor.cursorPosition
                                    emojiPicker.anchorItem = threadEditor
                                    emojiPicker.open()
                                }
                            }
                            ActionButton {
                                quiet: true
                                compact: true
                                iconOnly: true
                                iconGlyph: Theme.iconTrash
                                text: qsTrId("genycaster.composer.remove_thread_cast")
                                onClicked: root.removeThreadCast(threadRow.index)
                            }
                        }

                        Superellipse {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: Math.max(118, Math.min(210, threadEditor.contentHeight + Theme.space5))
                            fillColor: threadEditor.activeFocus ? Theme.editorFocusedSurface : Theme.editorSurface
                            borderWidth: threadEditor.activeFocus ? Theme.strokeEmphasis : Theme.strokeHairline
                            borderColor: threadEditor.activeFocus ? Theme.accent : Theme.borderSubtle
                            curveDepth: Theme.curveLarge

                            DirectionAwareTextArea {
                                id: threadEditor
                        selectAllOnDoubleTap: true
                                objectName: "composerThreadEditor"
                                onActiveFocusChanged: if (activeFocus) root.ensureEditorVisible(threadEditor)
                                onCursorRectangleChanged: if (activeFocus) root.ensureEditorVisible(threadEditor)
                                anchors.fill: parent
                                anchors.margins: Theme.space1
                                text: threadRow.text
                                placeholderText: qsTrId("genycaster.composer.thread_placeholder")
                                                 .arg(LocaleFormat.formatInteger(threadRow.index + 2))
                                background: null
                                wrapMode: TextEdit.Wrap
                                onTextChanged: if (activeFocus
                                                  && text !== threadTailModel.get(threadRow.index).text)
                                                   threadTailModel.setProperty(threadRow.index, "text", text)
                            }
                        }

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: LocaleFormat.formatInteger(presentationBridge.utf8ByteCount(threadRow.text))
                                  + " / " + LocaleFormat.formatInteger(1024)
                            color: presentationBridge.utf8ByteCount(threadRow.text) <= 1024
                                   ? Theme.inkSubtle : Theme.danger
                            font.pixelSize: Theme.typeCaptionSize
                            forceLeftToRight: true
                            horizontalAlignment: Theme.rtl ? Text.AlignLeft : Text.AlignRight
                        }
                    }
                }
            }

            RowLayout {
                visible: !root.compact
                layoutDirection: Theme.uiDirection
                Layout.fillWidth: true
                spacing: Theme.space2

                ActionButton {
                    quiet: true
                    iconGlyph: Theme.iconPlus
                    text: qsTrId("genycaster.composer.add_cast")
                    enabled: threadTailModel.count + 1 < root.maximumThreadCasts
                    onClicked: root.addThreadCast()
                }
                DirectionAwareLabel {
                    visible: !root.compact && root.threadMode
                    Layout.fillWidth: true
                    text: root.threadMode ? qsTrId("genycaster.composer.thread_protocol_hint")
                                          : qsTrId("genycaster.composer.thread_hint")
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }

            GridLayout {
                visible: !root.compact || presentationBridge.writeErrorText.length > 0
                Layout.fillWidth: true
                columns: root.compact ? 1 : 2
                rowSpacing: Theme.space1
                columnSpacing: Theme.space3
                layoutDirection: Theme.uiDirection

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: root.publicationPlanError.length > 0
                        text: qsTrId("genycaster.pro.thread_plan_failed")
                        color: Theme.danger
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: presentationBridge.writeErrorText.length > 0 ? presentationBridge.writeErrorText
                              : richComposer.simplifiedElementCount > 0
                                ? qsTrId("genycaster.composer.compatibility_simplified_count")
                                    .arg(LocaleFormat.formatInteger(richComposer.simplifiedElementCount))
                              : qsTrId("genycaster.composer.cross_client_safe")
                        visible: presentationBridge.writeErrorText.length > 0
                                 || richComposer.simplifiedElementCount > 0
                        color: presentationBridge.writeErrorText.length > 0 ? Theme.warning : Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        wrapMode: Text.Wrap
                    }
                }

                Label {
                    LayoutMirroring.enabled: false
                    Layout.fillWidth: root.compact
                    Layout.minimumWidth: 0
                    horizontalAlignment: Theme.textEndAlignment
                    wrapMode: Text.Wrap
                    text: LocaleFormat.formatInteger(root.authoredCharacters)
                          + " / " + LocaleFormat.formatInteger(root.maxComposerCharacters)
                          + " · " + LocaleFormat.formatInteger(richComposer.publishedBytes) + " B"
                    color: root.withinAccountCharacterLimit ? Theme.inkSubtle : Theme.danger
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: root.withinAccountCharacterLimit ? Theme.weightMedium : Theme.weightBold
                }
            }

            Item { Layout.preferredHeight: root.compact ? 0 : Theme.space2 }
        }
    }

        // Compact selector bar follows the document; its sheets float above it.
        RowLayout {
            objectName: "mobileCastContext"
            visible: root.compact && root.editorMode === "visual"
                     && root.showTaxonomyControls && !root.keyboardVisible
            Layout.fillWidth: true
            Layout.leftMargin: Theme.mobileGutter
            Layout.rightMargin: Theme.mobileGutter
            Layout.topMargin: Theme.space2
            Layout.bottomMargin: Theme.space1
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Item {
                id: selectedChannelChip
                Layout.preferredHeight: Theme.touchTarget
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                Superellipse {
                    anchors.fill: parent
                    curveDepth: Theme.curveControl
                    fillColor: root.selectedParentUrl.length > 0
                               ? Theme.primarySurface : Theme.surfaceMuted
                    borderWidth: 1
                    borderColor: root.selectedParentUrl.length > 0
                                 ? Theme.accent : Theme.borderMuted
                }
                RowLayout {
                    id: selectedChannelContents
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space2
                    anchors.rightMargin: Theme.space2
                    spacing: Theme.space1
                    Avatar {
                        avatarSize: 24
                        sourceUrl: root.selectedContextImage
                        displayName: root.selectedContextLabel.length > 0
                                     ? root.selectedContextLabel : "#"
                        visible: root.selectedParentUrl.length > 0
                    }
                    Icon {
                        visible: root.selectedParentUrl.length === 0
                        text: Theme.iconGlobe
                        preferredSize: Theme.iconSizeSmall
                        color: Theme.textSecondary
                    }
                    DirectionAwareLabel {
                        text: root.selectedContextLabel.length > 0
                              ? root.selectedContextLabel : qsTrId("genycaster.composer.channel")
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                    }
                    Icon {
                        text: Theme.iconChevronDown
                        preferredSize: Theme.iconSizeSmall
                        color: Theme.textSecondary
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: if (root.showTaxonomyControls) channelPicker.open()
                }
            }
            Item {
                id: selectedTopicChip
                objectName: "composerTopicSelection"
                Layout.preferredHeight: Theme.touchTarget
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                Superellipse {
                    anchors.fill: parent
                    curveDepth: Theme.curveControl
                    fillColor: root.selectedTopics.length > 0
                               ? Theme.primarySurface : Theme.surfaceMuted
                    borderWidth: 1
                    borderColor: root.selectedTopics.length > 0
                                 ? Theme.accent : Theme.borderMuted
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space2
                    anchors.rightMargin: Theme.space2
                    spacing: Theme.space1
                    Icon {
                        text: Theme.iconPalette
                        preferredSize: Theme.iconSizeSmall
                        color: root.selectedTopics.length > 0
                               ? Theme.accent : Theme.textSecondary
                    }
                    DirectionAwareLabel {
                        id: categoryText
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        elide: Text.ElideRight
                        text: root.selectedTopics.length > 0
                            ? qsTrId("genycaster.topics.selected_count").arg(
                                LocaleFormat.formatInteger(root.selectedTopics.length))
                            : qsTrId("genycaster.topics.label")
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: root.selectedTopics.length > 0
                                     ? Theme.weightDemiBold : Theme.weightMedium
                        color: Theme.textPrimary
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: topicPicker.open()
                }
                Accessible.role: Accessible.Button
                Accessible.name: qsTrId("genycaster.topics.label")
            }
            ActionButton {
                visible: root.selectedParentUrl.length > 0
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.composer.context_clear")
                onClicked: {
                    root.selectedParentUrl = ""
                    root.selectedContextLabel = ""
                    root.selectedContextImage = ""
                    root.selectedChannelId = ""
                }
            }
        }


        ColumnLayout {
            id: mobileComposerFooter
            objectName: "mobileComposerStickyFooter"
            // Keep the footer geometry stable as sheets overlay the composer.
            visible: root.compact && root.editorMode === "visual"
            Layout.fillWidth: true
            Layout.minimumHeight: 0
            spacing: Theme.space1
            Layout.leftMargin: Theme.mobileGutter
            Layout.rightMargin: Theme.mobileGutter
            // Keep buttons clear of system gesture areas and keyboard accessories.
            // The parent Qt viewport has already shrunk above the keyboard;
            // never subtract keyboard height twice.
            Layout.bottomMargin: root.keyboardVisible ? Theme.space2
                                 : Math.max(Theme.space3, root.bottomSafeInset)
            Layout.topMargin: Theme.space2

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderSubtle
            }
                RowLayout {
                    objectName: "mobileComposerMediaBar"
                    visible: root.compact && root.editorMode === "visual"
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection
                    ActionButton {
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconImage
                        text: qsTrId("genycaster.home.post_photo")
                        enabled: root.canUploadMedia
                        onClicked: {
                            root.pendingMediaBlockIndex = root.compact ? 0 : visualEditor.activeBlockIndex
                            root.openMediaPicker()
                        }
                    }
                    ActionButton {
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconEmoji
                        text: qsTrId("genycaster.composer.emoji")
                        onClicked: {
                            root.emojiThreadIndex = -1
                            root.emojiThreadCursorPosition = -1
                            emojiPicker.anchorItem = root.compact ? mobilePlainEditor : visualEditor
                            emojiPicker.open()
                        }
                    }
                    ActionButton {
                        objectName: "mobileComposerGifAction"
                        compact: true
                        quiet: true
                        // A full-width textual ActionButton crowded the emoji
                        // glyph in narrow RTL layouts. This is a compact media tool.
                        Layout.minimumWidth: Theme.scaleUi(50)
                        Layout.preferredWidth: Theme.scaleUi(50)
                        Layout.maximumWidth: Theme.scaleUi(50)
                        leftPadding: Theme.space1
                        rightPadding: Theme.space1
                        text: "GIF"
                        enabled: root.canUploadMedia
                        onClicked: {
                            // The same MIME-safe file picker accepts animated GIF.
                            root.pendingMediaBlockIndex = root.compact ? 0 : visualEditor.activeBlockIndex
                            root.openMediaPicker()
                        }
                    }
                    Item { Layout.fillWidth: true }
                }


            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection
                ActionButton {
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconPlus
                    text: qsTrId("genycaster.composer.add_cast")
                    enabled: threadTailModel.count + 1 < root.maximumThreadCasts
                    onClicked: root.addThreadCast()
                }
                Item { Layout.fillWidth: true }
                ActionButton {
                    objectName: "composerClearContentMobile"
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconTrash
                    text: qsTrId("genycaster.composer.clear_content")
                    enabled: !presentationBridge.writeBusy &&
                             (richComposer.safeText.length > 0
                              || richComposer.protocolEmbeds.length > 0
                              || threadTailModel.count > 0
                              || root.selectedParentUrl.length > 0)
                    onClicked: clearContentConfirmation.open()
                }
                DirectionAwareLabel {
                    Layout.minimumWidth: implicitWidth
                    Layout.alignment: Qt.AlignVCenter
                    forceLeftToRight: true
                    color: root.withinAccountCharacterLimit ? Theme.textSecondary : Theme.danger
                    font.pixelSize: Theme.typeCaptionSize
                    text: LocaleFormat.formatInteger(root.authoredCharacters)
                          + " / " + LocaleFormat.formatInteger(root.maxComposerCharacters)
                }
            }
        }
    }
}
