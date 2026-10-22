import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

GenyDrawer {
    id: root
    objectName: "composerDrawer"

    property var parentCast: null
    property var quotedCast: null
    property string initialAction: ""
    // Canonical parent_url is sourced from the channel search response.
    property string selectedParentUrl: ""
    property string selectedContextLabel: ""
    property string selectedContextImage: ""
    property string selectedChannelId: ""
    property int initialFocusAttempts: 0
    property string suggestedText: ""
    property var suggestedEmbeds: []
    property bool transientDraft: false
    property string editorMode: "visual"
    property bool showAdvancedFormatting: false
    property real expandedViewportHeight: 0
    readonly property bool keyboardVisible: root.compact && root.opened
        && root.expandedViewportHeight > 0 && root.parent !== null
        && root.parent.height < root.expandedViewportHeight - 125
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
    readonly property int maximumThreadCasts: 12
    readonly property bool threadMode: threadTailModel.count > 0
    readonly property bool compact: parent ? parent.width < 720 : false
    readonly property string replyAuthor: root.parentCast
                                                   ? String(root.parentCast.author || "") : ""
    readonly property string replyName: root.parentCast
                                                 ? String(root.parentCast.displayName
                                                          || root.parentCast.author
                                                          || qsTrId("genycaster.shell.this_cast")) : ""
    readonly property string replyText: root.parentCast
                                                 ? String(root.parentCast.text || "") : ""
    readonly property bool replyContentRtl: localizationController.isRightToLeftText(
                                                root.replyText || root.replyName)

    signal publishRequested(string text, var mentionFids, var mentionPositions, var embeds, string parentUrl)
    signal replyRequested(string text, var mentionFids, var mentionPositions, var embeds)
    signal threadRequested(var casts)
    signal abandoned()

    ListModel { id: threadTailModel }

    // A native-feeling page on narrow screens; keep the rich editor and
    // publication/draft logic intact rather than replacing it with a form.
    bottomSheet: true
    fullScreen: compact
    showHandle: !compact
    modal: true
    focus: true
    dimBackground: true
    contentPadding: 0
    width: parent ? parent.width : 1040
    // During typing the Qt viewport already excludes the on-screen keyboard.
    // Consume its full available height instead of leaving a dead upper gap.
    // Parent is AppShell on mobile (not the scrolling reading column).
    height: parent ? (compact ? parent.height : Math.min(parent.height * 0.78, 720)) : 720
    // Composer content owns vertical scrolling; drawer-swipe gestures would otherwise
    // compete with Flickable/ScrollView input on touch devices.
    // Scrolling belongs to the content; swipe-to-dismiss was stealing
    // vertical gestures from multi-cast threads on mobile Safari.
    interactive: false
    property var focusedComposerEditor: null
    property bool pendingEditorReveal: false

    // Only reveal the *currently focused* editor. The former keyboard-height
    // listener scrolled to the first cast even while editing cast 2/3, while
    // Add Cast independently forced contentY to the bottom. On iOS this
    // produced the large jumps visible in thread composition recordings.
    function ensureEditorVisible(editor) {
        if (!root.opened || !editor || !editor.activeFocus)
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
        richComposer.newDocument()
        threadTailModel.clear()
        root.mediaMetadata = ({})
        root.mediaInsertFailed = false
        root.insertKind = ""
        root.selectedParentUrl = ""
        root.selectedContextLabel = ""
        root.selectedContextImage = ""
        root.selectedChannelId = ""
        root.quotedCast = null
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
        if (!files || files.length === 0)
            return false

        const additions = []
        for (let index = 0; index < files.length; ++index)
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
        // now, in the originating tap, so iOS WebKit may raise its keyboard.
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

    function hasLiveEditorText() {
        if (root.editorMode === "visual")
            return root.compact ? mobilePlainEditor.text.trim().length > 0
                                : visualEditor.hasLiveContent
        if (root.editorMode === "markdown")
            return markdownEditor.text.trim().length > 0
        return false
    }

    function threadTailWithinLimit() {
        for (let index = 0; index < threadTailModel.count; ++index) {
            const text = String(threadTailModel.get(index).text || "")
            if (text.trim().length === 0 || presentationBridge.utf8ByteCount(text) > 1024)
                return false
        }
        return true
    }

    function hasPublishableContent() {
        return (richComposer.safeText.trim().length > 0 || root.hasLiveEditorText()
                || richComposer.protocolEmbeds.length > 0)
               && root.threadTailWithinLimit()
    }

    function threadPayload() {
        const casts = [{
            "text": richComposer.safeText,
            "mentionFids": richComposer.mentionFids,
            "mentionPositions": richComposer.mentionPositions,
            "embeds": richComposer.protocolEmbeds
        }]
        for (let index = 0; index < threadTailModel.count; ++index) {
            casts.push({
                "text": String(threadTailModel.get(index).text || ""),
                "mentionFids": [],
                "mentionPositions": [],
                "embeds": []
            })
        }
        return casts
    }

    function publishCurrent() {
        root.synchronizeEditorState()
        if (!richComposer.withinProtocolLimit || !root.hasPublishableContent()
                || presentationBridge.writeBusy || mediaUpload.busy)
            return
        richComposer.saveDraft()
        if (root.threadMode)
            root.threadRequested(root.threadPayload())
        else if (root.parentCast)
            root.replyRequested(richComposer.safeText, richComposer.mentionFids, richComposer.mentionPositions, richComposer.protocolEmbeds)
        else
            root.publishRequested(richComposer.safeText, richComposer.mentionFids, richComposer.mentionPositions, richComposer.protocolEmbeds, root.selectedParentUrl)
    }

    // Repeater editors are incubated after the drawer begins opening; a
    // one-shot 120ms focus call often fires before their first delegate.
    function focusVisualEditorNow() {
        // Called synchronously from the originating tap: iOS WebKit normally
        // rejects keyboard activation if focus is deferred to a timer.
        if (root.compact)
            mobilePlainEditor.forceActiveFocus()
    }

    Timer {
        id: initialComposerFocus
        interval: 80
        repeat: false
        onTriggered: {
            if (!root.opened || !root.compact || root.editorMode !== "visual") return
            mobilePlainEditor.forceActiveFocus()
            root.ensureEditorVisible(mobilePlainEditor)
        }
    }

    onOpened: {
        root.focusedComposerEditor = null
        root.pendingEditorReveal = false
        root.initialFocusAttempts = 0
        root.selectedParentUrl = ""
        root.selectedContextLabel = ""
        root.selectedContextImage = ""
        root.selectedChannelId = ""
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
        if (!mediaUpload.busy) mediaUpload.clear()
        if (root.transientDraft || root.parentCast || root.quotedCast) {
            richComposer.newDocument()
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
        if (root.initialAction === "media") {
            root.pendingMediaBlockIndex = visualEditor.activeBlockIndex
            if (Qt.platform.os === "wasm") {
                // Safari requires a direct user tap to open the file picker.
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
        if (!root.transientDraft && richComposer.dirty && !presentationBridge.writeBusy)
            richComposer.saveDraft()
        presentationBridge.clearReplyTarget()
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
            if (!root.opened || !channelPicker.opened) return
            root.selectedParentUrl = String(selection.parentUrl || "")
            root.selectedContextLabel = selection.isHome ? "" : String(selection.name || "")
            root.selectedChannelId = selection.isHome ? "" : String(selection.id || "")
            root.selectedContextImage = selection.isHome ? "" : String(selection.imageUrl || "")
            channelPicker.close()
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

    GenyDrawer {
        id: channelPicker
        objectName: "composerChannelDrawer"
        hostItem: root.hostItem
        bottomSheet: true
        showHandle: true
        dimBackground: true
        interactive: false
        width: parent ? parent.width : root.width
        height: parent ? Math.min(parent.height * 0.76, 670) : 580
        contentPadding: Theme.space3
        onOpened: {
            channelSearch.text = ""
            channelResults.positionViewAtBeginning()
            channelDirectory.openForViewer(
                presentationBridge.isAuthenticated ? presentationBridge.accountFid : 0)
            channelDirectory.search("")
        }
        onClosed: channelDirectory.search("")
        contentItem: ColumnLayout {
            spacing: Theme.space3
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.composer.channel_picker_title")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
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
            DirectionAwareLabel {
                Layout.fillWidth: true
                color: Theme.textSecondary
                wrapMode: Text.Wrap
                text: qsTrId("genycaster.composer.channel_picker_hint")
                font.pixelSize: Theme.typeCaptionSize
            }
            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                fillColor: Theme.editorSurface
                borderWidth: 1
                borderColor: channelSearch.activeFocus ? Theme.accent : Theme.borderMuted
                curveDepth: Theme.curveControl
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space3
                    spacing: Theme.space2
                    Icon {
                        text: Theme.iconSearch
                        color: Theme.textSecondary
                        preferredSize: Theme.iconSizeSmall
                    }
                    TextField {
                        id: channelSearch
                        objectName: "composerChannelSearch"
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
                    height: 62
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
                            avatarSize: 39
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

    contentItem: ColumnLayout {
        spacing: 0
        ScrollView {
        id: composerScroll
        Layout.fillWidth: true
        Layout.fillHeight: true
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
                    quiet: true
                    compact: true
                    text: qsTrId("genycaster.composer.save_draft")
                    enabled: !presentationBridge.writeBusy
                    onClicked: {
                        if (richComposer.dirty)
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
                             && richComposer.withinProtocolLimit && root.hasPublishableContent()
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
                    spacing: 1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.threadMode ? qsTrId("genycaster.composer.thread_title")
                              : root.parentCast ? qsTrId("genycaster.shell.reply") : qsTrId("genycaster.shell.new_cast")
                        color: Theme.ink
                        font.pixelSize: Theme.typeTitle2Size
                        font.weight: Theme.weightBold
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.composer.rich_engine_subtitle")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }

                ActionButton {
                    objectName: "composerClearContentDesktop"
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
                    iconGlyph: Theme.iconSend
                    text: root.parentCast ? qsTrId("genycaster.shell.reply")
                                         : qsTrId("genycaster.shell.publish")
                    enabled: !presentationBridge.writeBusy && !mediaUpload.busy
                             && richComposer.withinProtocolLimit && root.hasPublishableContent()
                    onClicked: root.publishCurrent()
                }
            }

            ComposerToolbar {
                visible: !root.compact && !root.keyboardVisible
                Layout.fillWidth: true
                mode: root.editorMode
                onModeRequested: mode => root.setEditorMode(mode)
            }

            Superellipse {
                id: replyContextCard
                visible: (root.parentCast !== null || root.quotedCast !== null)
                         && !root.keyboardVisible
                Layout.fillWidth: true
                implicitHeight: replyContextContent.implicitHeight + Theme.space4 * 2
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
                    anchors.margins: Theme.space4
                    spacing: Theme.space3
                    layoutDirection: root.replyContentRtl ? Qt.RightToLeft : Qt.LeftToRight
                    LayoutMirroring.enabled: false
                    LayoutMirroring.childrenInherit: true

                    Avatar {
                        Layout.alignment: Qt.AlignTop
                        avatarSize: root.compact ? 44 : 48
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
                            maximumLineCount: root.compact ? 2 : 3
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

            FormattingToolbar {
                id: formattingToolbar
                visible: root.editorMode === "visual" && (!root.compact || root.showAdvancedFormatting)
                         && !root.keyboardVisible
                Layout.fillWidth: true
                activeBlockIndex: visualEditor.activeBlockIndex
                activeEditor: visualEditor.formattingEditor
                editorHost: visualEditor
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

            MediaUploadPanel {
                id: mediaPanel
                visible: root.editorMode === "visual"
                         && (root.insertKind === "media" || hasAttachedMedia || hasTransientUpload)
                Layout.fillWidth: true
                blockIndex: root.insertBlockIndex
                insertFailed: root.mediaInsertFailed
                queuedFiles: root.pendingMediaFiles
                attachmentMetadata: root.mediaMetadata
                onBrowseRequested: blockIndex => {
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

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.editorMode === "preview"
                                        ? farcasterPreview.implicitHeight
                                        : root.threadMode
                                          ? (root.compact
                                             ? Math.max(118, Math.min(188, composerScroll.height * 0.36))
                                             : 250)
                                          : (root.compact
                                             ? Math.max(135, composerScroll.height
                                                - (root.keyboardVisible ? 84 : 126))
                                             : 320)
                Layout.minimumHeight: root.editorMode === "preview"
                                      ? Math.min(farcasterPreview.implicitHeight, root.compact ? 210 : 230)
                                      : root.threadMode
                                        ? (root.compact ? 118 : 220)
                                        : (root.compact ? 135 : 280)
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
                        placeholderText: qsTrId("genycaster.shell.whats_happening")
                        background: null
                        selectByMouse: true
                        persistentSelection: true
                        wrapMode: TextEdit.Wrap
                        font.pixelSize: Theme.typeBodySize
                        onTextChanged: {
                            if (root.opened && root.compact && root.editorMode === "visual"
                                    && richComposer.safeText !== text)
                                richComposer.setBlockText(0, text)
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
                    visible: !root.compact
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
                        text: presentationBridge.writeErrorText.length > 0 ? presentationBridge.writeErrorText
                              : richComposer.simplifiedElementCount > 0
                                ? qsTrId("genycaster.composer.compatibility_simplified_count")
                                    .arg(LocaleFormat.formatInteger(richComposer.simplifiedElementCount))
                              : qsTrId("genycaster.composer.cross_client_safe")
                        visible: !root.compact || presentationBridge.writeErrorText.length > 0
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
                    text: root.compact
                          ? LocaleFormat.formatInteger(richComposer.publishedBytes)
                            + " / " + LocaleFormat.formatInteger(richComposer.maximumPublishedBytes)
                          : qsTrId("genycaster.composer.editor_count")
                              .arg(LocaleFormat.formatInteger(richComposer.editorCharacters))
                            + "  •  " + qsTrId("genycaster.composer.published_count")
                              .arg(LocaleFormat.formatInteger(richComposer.publishedBytes))
                              .arg(LocaleFormat.formatInteger(richComposer.maximumPublishedBytes))
                    color: richComposer.withinProtocolLimit ? Theme.inkSubtle : Theme.danger
                    font.family: Theme.fontFamilyForText(text)
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: richComposer.withinProtocolLimit ? Theme.weightMedium : Theme.weightBold
                }
            }

            Item { Layout.preferredHeight: root.compact ? 0 : Theme.space5 }
        }
    }

        ColumnLayout {
            id: mobileComposerFooter
            objectName: "mobileComposerStickyFooter"
            visible: root.compact && root.editorMode === "visual"
            Layout.fillWidth: true
            Layout.minimumHeight: 0
            spacing: Theme.space1
            Layout.leftMargin: Theme.mobileGutter
            Layout.rightMargin: Theme.mobileGutter
            // Keep buttons clear of the iOS home indicator and input accessory.
            // The parent Qt viewport has already shrunk above the keyboard;
            // never subtract keyboard height twice.
            Layout.bottomMargin: root.keyboardVisible ? Theme.space4 : Theme.space5
            Layout.topMargin: Theme.space2

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderSubtle
            }
                RowLayout {
                    objectName: "mobileCastContext"
                    visible: root.compact && root.editorMode === "visual"
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    spacing: Theme.space2

                    Item {
                        id: selectedChannelChip
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: Math.min(root.width * 0.57,
                            Math.max(110, selectedChannelContents.implicitWidth + Theme.space3 * 2))
                        Superellipse {
                            anchors.fill: parent
                            curveDepth: 18
                            fillColor: root.selectedParentUrl.length > 0
                                       ? Theme.primarySurface : "transparent"
                            borderWidth: 0
                        }
                        DashedCapsuleOutline {
                            anchors.fill: parent
                            visible: root.selectedParentUrl.length === 0
                            lineColor: Theme.textSecondary
                        }
                        RowLayout {
                            id: selectedChannelContents
                            anchors.centerIn: parent
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
                                Layout.maximumWidth: Math.max(58, root.width * 0.38)
                            }
                            Icon {
                                text: Theme.iconChevronDown
                                preferredSize: Theme.iconSizeSmall
                                color: Theme.textSecondary
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: channelPicker.open()
                        }
                    }
                    Item {
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: categoryText.implicitWidth + Theme.space3 * 2
                        opacity: 0.82
                        DashedCapsuleOutline {
                            anchors.fill: parent
                            lineColor: Theme.textSecondary
                        }
                        DirectionAwareLabel {
                            id: categoryText
                            anchors.centerIn: parent
                            text: qsTrId("genycaster.composer.category")
                            font.pixelSize: Theme.typeMetaSize
                            color: Theme.textSecondary
                        }
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTrId("genycaster.composer.category")
                        Accessible.description: qsTrId("genycaster.home.hub_coming_soon")
                    }
                    Item { Layout.fillWidth: true }
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

                RowLayout {
                    objectName: "mobileComposerMediaBar"
                    visible: root.compact && root.editorMode === "visual"
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection
                    ActionButton {
                        compact: true
                        quiet: true
                        iconOnly: true
                        iconGlyph: Theme.iconImage
                        text: qsTrId("genycaster.home.post_photo")
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
                        compact: true
                        quiet: true
                        text: "GIF"
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
                    color: richComposer.withinProtocolLimit ? Theme.textSecondary : Theme.danger
                    font.pixelSize: Theme.typeCaptionSize
                    text: LocaleFormat.formatInteger(richComposer.publishedBytes)
                          + " / " + LocaleFormat.formatInteger(richComposer.maximumPublishedBytes)
                }
            }
        }
    }
}
