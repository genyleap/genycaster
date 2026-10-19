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

    signal publishRequested(string text, var mentionFids, var mentionPositions, var embeds)
    signal replyRequested(string text, var mentionFids, var mentionPositions, var embeds)
    signal threadRequested(var casts)
    signal abandoned()

    ListModel { id: threadTailModel }

    bottomSheet: true
    modal: true
    focus: true
    dimBackground: true
    contentPadding: 0
    width: parent ? parent.width : 1040
    // During typing the Qt viewport already excludes the on-screen keyboard.
    // Consume its full available height instead of leaving a dead upper gap.
    // Parent is AppShell on mobile (not the scrolling reading column).
    height: parent ? (compact ? (keyboardVisible
                                   ? Math.max(1, parent.height - Theme.space2)
                                   : Math.min(parent.height * 0.92, 820))
                              : Math.min(parent.height * 0.78, 720)) : 720
    // Composer content owns vertical scrolling; drawer-swipe gestures would otherwise
    // compete with Flickable/ScrollView input on touch devices.
    // Scrolling belongs to the content; swipe-to-dismiss was stealing
    // vertical gestures from multi-cast threads on mobile Safari.
    interactive: false
    function ensureEditorVisible(editor) {
        if (!root.opened || !editor) return
        Qt.callLater(function() {
            if (!root.opened || !editor) return
            const flick = composerScroll.contentItem
            if (!flick || flick.height <= 0) return
            const mapped = editor.mapToItem(composerContent, 0, 0)
            const upper = Math.max(0, mapped.y - Theme.space3)
            const lower = mapped.y + Math.min(editor.height, 120) + Theme.space3
            if (upper < flick.contentY)
                flick.contentY = upper
            else if (lower > flick.contentY + flick.height)
                flick.contentY = Math.min(Math.max(0, flick.contentHeight - flick.height),
                                          lower - flick.height)
        })
    }
    onKeyboardVisibleChanged: {
        if (root.keyboardVisible) {
            root.ensureEditorVisible(visualEditor.activeEditor)
            root.ensureEditorVisible(markdownEditor.activeFocus ? markdownEditor : null)
        }
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
        // Qt finishes laying out the new delegate on the next event turn.
        // Scroll to the new card instead of leaving it beneath the fold.
        Qt.callLater(function() {
            const flick = composerScroll.contentItem
            if (flick)
                flick.contentY = Math.max(0, flick.contentHeight - flick.height)
        })
    }

    function removeThreadCast(index) {
        if (index >= 0 && index < threadTailModel.count)
            threadTailModel.remove(index)
    }

    function insertEmoji(emoji) {
        if (!emoji || emoji.length === 0)
            return
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
            return visualEditor.hasLiveContent
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
            root.publishRequested(richComposer.safeText, richComposer.mentionFids, richComposer.mentionPositions, richComposer.protocolEmbeds)
    }

    onOpened: {
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

    contentItem: ScrollView {
        id: composerScroll
        clip: true
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

            RowLayout {
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
                        font.pixelSize: root.keyboardVisible ? Theme.typeTitle3Size
                                                            : Theme.typeTitle2Size
                        font.weight: Theme.weightBold
                    }
                    DirectionAwareLabel {
                        visible: !root.compact
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.composer.rich_engine_subtitle")
                        color: Theme.inkMuted
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }


                ActionButton {
                    prominent: true
                    compact: true
                    iconGlyph: Theme.iconSend
                    text: presentationBridge.writeBusy
                          ? (root.threadMode ? qsTrId("genycaster.composer.publishing_thread")
                             : root.parentCast ? qsTrId("genycaster.shell.replying") : qsTrId("genycaster.shell.publishing"))
                          : (root.threadMode ? qsTrId("genycaster.composer.publish_thread")
                             : root.parentCast ? qsTrId("genycaster.shell.reply") : qsTrId("genycaster.shell.publish"))
                    enabled: !presentationBridge.writeBusy && !mediaUpload.busy
                             && richComposer.withinProtocolLimit && root.hasPublishableContent()
                    onClicked: root.publishCurrent()
                }
            }

            ComposerToolbar {
                visible: !root.keyboardVisible
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
                visible: root.compact && root.editorMode === "visual" && !root.keyboardVisible
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
                                          ? (root.compact ? 220 : 250)
                                          : (root.compact
                                             ? (root.keyboardVisible
                                                ? Math.max(260, root.height - Theme.space5 * 7)
                                                : 290)
                                             : 320)
                Layout.minimumHeight: root.editorMode === "preview"
                                      ? Math.min(farcasterPreview.implicitHeight, root.compact ? 210 : 230)
                                      : root.threadMode
                                        ? (root.compact ? 200 : 220)
                                        : (root.compact ? 250 : 280)
                spacing: Theme.space3
                layoutDirection: Qt.LeftToRight

                Superellipse {
                    visible: root.editorMode !== "preview"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    fillColor: Theme.editorSurface
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: Theme.curveLarge

                    RichTextEditor {
                        id: visualEditor
                        visible: root.editorMode === "visual"
                        onActiveEditorChangedExternally: (blockIndex, editor) =>
                            root.ensureEditorVisible(editor)
                        anchors.fill: parent
                        anchors.margins: Theme.space3
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

            ColumnLayout {
                visible: root.threadMode
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space2

                Repeater {
                    model: threadTailModel

                    // A single-column card avoids competing RTL anchors.
                    // Each continuation is a real scrollable document row,
                    // never a narrow textbox beside a vertical action strip.
                    delegate: ColumnLayout {
                        id: threadRow
                        required property int index
                        required property string text
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
                                objectName: "composerThreadEditor"
                                onActiveFocusChanged: if (activeFocus) root.ensureEditorVisible(threadEditor)
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

            Item { Layout.preferredHeight: root.compact ? Theme.space3 : Theme.space5 }
        }
    }
}
