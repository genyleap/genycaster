import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client
import "StickerCatalog.js" as StickerCatalog

Item {
    id: root
    property int activeBlockIndex: -1
    property var activeEditor: null
    property string activeEditorKind: "formatted"
    readonly property var formattingEditor: root.activeEditorKind === "formatted" ? root.activeEditor : null
    // TextArea.text contains HTML markup for RichText editors, even when the
    // user-visible paragraph is empty. Read the plain QTextDocument range.
    readonly property string activeText: {
        if (root.activeEditor === null || root.activeEditor.text === undefined)
            return ""
        // Reading text keeps this binding reactive even for same-length edits.
        return String(root.activeEditor.getText(0, root.activeEditor.length) || "")
    }
    readonly property bool hasLiveContent: root.activeText.replace(/[\s\u200B-\u200D\u2060\uFEFF]/g, "").length > 0
    property var blockModel: []
    // The visual Composer may contain a single long, wrapped report. Expose
    // its measured QTextDocument height so the parent can grow instead of
    // clipping the last line in a fixed-height editor.
    property real singleParagraphContentHeight: 0
    readonly property real naturalSingleParagraphHeight: singleParagraphContentHeight
                                                     + Theme.space5 * 3
    property bool blockRefreshPending: false
    signal activeEditorChangedExternally(int blockIndex, var editor)

    function escapeHtml(value) {
        return String(value || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\"/g, "&quot;")
    }

    // An inline's literal line breaks are content, not HTML whitespace.
    // Render them as soft breaks within ONE editable paragraph; otherwise
    // Qt RichText collapses them and the report becomes unreadable.
    function escapeInlineHtml(value) {
        return root.escapeHtml(value).replace(/\r\n?/g, "\n").replace(/\n/g, "<br/>")
    }

    function formattedMarkup(data) {
        const values = data && data.inlines ? data.inlines : []
        if (!values.length) return root.escapeInlineHtml(String(data && data.text ? data.text : ""))
        let html = ""
        for (let i = 0; i < values.length; ++i) {
            const span = values[i] || ({})
            const raw = span.kind === "mention" && span.username ? "@" + span.username : String(span.text || "")
            const body = root.escapeInlineHtml(raw)
            if (span.kind === "strong") html += "<b>" + body + "</b>"
            else if (span.kind === "emphasis") html += "<i>" + body + "</i>"
            else if (span.kind === "inline-code") html += "<span style='font-family:" + Theme.codeFontFamily + ";background-color:" + Theme.codeInlineSurface + "'>" + body + "</span>"
            else if (span.kind === "link" && span.target) html += "<a href='" + root.escapeHtml(span.target) + "' style='color:" + Theme.accent + ";text-decoration:none'>" + body + "</a>"
            else if (span.kind === "mention" && Number(span.fid || 0) > 0) {
                const username = encodeURIComponent(String(span.username || ""))
                html += "<a href='genycaster-mention://" + Number(span.fid) + "/" + username + "' style='color:" + Theme.accent + ";text-decoration:none'>" + body + "</a>"
            } else if (span.kind === "mention") html += "<span style='color:" + Theme.accent + "'>" + body + "</span>"
            else html += body
        }
        return html
    }

    function insertTextAtCursor(value) {
        if (root.activeEditor === null || root.activeBlockIndex < 0 || !value || value.length === 0)
            return false
        const position = root.activeEditor.cursorPosition
        root.activeEditor.insert(position, value)
        root.activeEditor.cursorPosition = position + value.length
        root.commitActiveEditor()
        root.activeEditor.focusEditing()
        return true
    }

    function commitActiveEditor() {
        if (root.activeEditor === null || root.activeBlockIndex < 0)
            return false
        if (root.activeEditorKind === "plain")
            return richComposer.setBlockText(root.activeBlockIndex, String(root.activeEditor.text || ""))
        return richComposer.setBlockFormattedText(
            root.activeBlockIndex,
            root.activeEditor.getFormattedText(0, root.activeEditor.length))
    }

    function hasLiveText() {
        return root.hasLiveContent
    }

    function applyInlineStyle(style) {
        const editor = root.formattingEditor
        const blockIndex = root.activeBlockIndex
        if (editor === null || blockIndex < 0 || editor.selectionStart === editor.selectionEnd)
            return false
        const start = Math.min(editor.selectionStart, editor.selectionEnd)
        const end = Math.max(editor.selectionStart, editor.selectionEnd)
        const selectedLength = end - start
        const formattedText = editor.getFormattedText(0, editor.length)
        if (!richComposer.setBlockFormattedText(blockIndex, formattedText))
            return false
        if (!richComposer.applyInlineStyle(blockIndex, start, selectedLength, style))
            return false
        const updatedBlocks = richComposer.blocks
        if (blockIndex >= 0 && blockIndex < updatedBlocks.length) {
            editor.text = root.formattedMarkup(updatedBlocks[blockIndex])
            editor.select(start, end)
            editor.focusEditing()
        }
        root.blockModel = updatedBlocks
        return true
    }

    function clearBlock(blockIndex) {
        if (blockIndex < 0)
            return false
        if (root.activeBlockIndex === blockIndex && root.activeEditor !== null) {
            root.activeEditor.text = ""
            root.activeEditor.cursorPosition = 0
        }
        const cleared = richComposer.setBlockText(blockIndex, "")
        if (cleared)
            root.scheduleBlockRefresh()
        return cleared
    }

    function scheduleBlockRefresh() {
        if (root.blockRefreshPending)
            return
        root.blockRefreshPending = true
        Qt.callLater(root.refreshBlockModel)
    }

    function refreshBlockModel() {
        root.blockRefreshPending = false
        root.blockModel = richComposer.blocks
    }

    Component.onCompleted: root.refreshBlockModel()

    Connections {
        target: richComposer
        function onDocumentChanged() { root.scheduleBlockRefresh() }
    }

    function enhancementModel(data) {
        const kind = String(data.kind || "")
        if (kind === "cast-reference") {
            return {
                kind: "cast-reference",
                castId: String(data.castId || ""),
                displayName: qsTrId("genycaster.embed.quoted_cast"),
                text: ""
            }
        }
        if (kind === "media") {
            const mediaType = String(data.mediaType || "").toLowerCase()
            const resolvedKind = mediaType.indexOf("video") >= 0 ? "video"
                              : mediaType.indexOf("image") >= 0 ? "image" : "external"
            return {
                kind: resolvedKind,
                url: String(data.url || ""),
                host: String(data.altText || data.mediaType || "")
            }
        }
        return {
            kind: "external",
            url: String(data.url || ""),
            host: String(data.title || "")
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        model: root.blockModel
        spacing: Theme.space3
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: GenyScrollBar {}

        delegate: Loader {
            id: loader
            required property int index
            required property var modelData
            width: ListView.view.width
            sourceComponent: {
                const kind = String(modelData.kind || "")
                if (kind === "code") return codeEditorComponent
                if (kind === "separator") return separatorComponent
                if (kind === "bullet-list" || kind === "ordered-list") return listEditorComponent
                // Ordinary uploaded images/videos are rendered once by the
                // shared MediaUploadPanel below the editor. Do not add a
                // duplicate "Image / Media" card inside the text document.
                // Sticker media stays inline at its authored block position.
                if (kind === "media" && !StickerCatalog.isStickerUrl(String(modelData.url || "")))
                    return null
                if (kind === "embed" || kind === "media" || kind === "cast-reference") return enhancementEditorComponent
                return textEditorComponent
            }
            property int blockIndex: index
            property var blockData: modelData
        }
    }

    Component {
        id: separatorComponent
        Superellipse {
            implicitHeight: 46
            fillColor: "transparent"
            curveDepth: Theme.curveCompact

            Rectangle {
                anchors.left: parent.left
                anchors.right: removeButton.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: Theme.space2
                height: 1
                color: Theme.divider
            }

            ActionButton {
                id: removeButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                quiet: true
                compact: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                Accessible.name: qsTrId("genycaster.composer.remove_block")
                onClicked: richComposer.removeBlock(parent.parent.blockIndex)
            }
        }
    }

    Component {
        id: codeEditorComponent
        CodeBlockEditor {
            blockIndex: parent.blockIndex
            blockData: parent.blockData
            compact: root.width < 700
            onEditorFocusChanged: (blockIndex, editor, active) => {
                if (active) {
                    root.activeBlockIndex = blockIndex
                    root.activeEditor = editor
                    root.activeEditorKind = "plain"
                    root.activeEditorChangedExternally(blockIndex, editor)
                } else if (root.activeBlockIndex === blockIndex) {
                    root.commitActiveEditor()
                }
            }
        }
    }

    Component {
        id: listEditorComponent
        Superellipse {
            id: listSurface
            readonly property bool ordered: String(parent.blockData.kind || "") === "ordered-list"
            readonly property bool rtlContent: localizationController.isRightToLeftText(String(parent.blockData.text || ""))
            implicitHeight: Math.max(104, listEditor.contentHeight + Theme.space6)
            fillColor: listEditor.editingFocus ? Theme.editorFocusedSurface : Theme.surfaceMuted
            borderWidth: listEditor.editingFocus ? 1 : 0
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveControl

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space2
                spacing: Theme.space2
                layoutDirection: listSurface.rtlContent ? Qt.RightToLeft : Qt.LeftToRight

                DirectionAwareLabel {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: Theme.space2
                    text: listSurface.ordered ? LocaleFormat.formatInteger(1) + "." : "•"
                    color: Theme.accent
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightDemiBold
                }

                DirectionAwareTextArea {

                    id: listEditor
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: String(listSurface.parent.blockData.text || "")
                    placeholderText: listSurface.ordered
                                     ? qsTrId("genycaster.composer.ordered_list_placeholder")
                                     : qsTrId("genycaster.composer.list_placeholder")
                    background: null
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    onEditingFocusChanged: {
                        if (editingFocus) {
                            root.activeBlockIndex = listSurface.parent.blockIndex
                            root.activeEditor = listEditor
                            root.activeEditorKind = "plain"
                            root.activeEditorChangedExternally(root.activeBlockIndex, listEditor)
                        } else if (root.activeBlockIndex === listSurface.parent.blockIndex) {
                            root.commitActiveEditor()
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.space1
                    ActionButton {
                        quiet: true
                        compact: true
                        iconOnly: true
                        iconGlyph: Theme.iconTrash
                        Accessible.name: qsTrId("genycaster.composer.clear_block")
                        onClicked: root.clearBlock(listSurface.parent.blockIndex)
                    }
                    ActionButton {
                        quiet: true
                        compact: true
                        iconOnly: true
                        iconGlyph: Theme.iconClose
                        Accessible.name: qsTrId("genycaster.composer.remove_block")
                        onClicked: richComposer.removeBlock(listSurface.parent.blockIndex)
                    }
                }
            }
        }
    }

    Component {
        id: enhancementEditorComponent
        Superellipse {
            id: enhancementSurface
            readonly property bool mediaBlock: String(enhancementSurface.parent.blockData.kind || "") === "media"
            readonly property bool videoBlock: String(enhancementSurface.parent.blockData.mediaType || "").toLowerCase().indexOf("video") >= 0
            readonly property bool stickerBlock: mediaBlock && StickerCatalog.isStickerUrl(
                String(enhancementSurface.parent.blockData.url || ""))
            implicitHeight: stickerBlock ? (root.width < 700 ? 156 : 174)
                           : enhancementLayout.implicitHeight + Theme.space2 * 2
            fillColor: stickerBlock ? "transparent" : Theme.surfaceMuted
            borderWidth: stickerBlock ? 0 : 1
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveContainer

            StickerDraftTile {
                objectName: "visualEditorStickerPreview"
                visible: enhancementSurface.stickerBlock
                tileSize: root.width < 700 ? 148 : 164
                x: Theme.rtl ? Math.max(0, parent.width - width) : 0
                y: 0
                imageUrl: String(enhancementSurface.parent.blockData.url || "")
                altText: String(enhancementSurface.parent.blockData.altText || "")
                onRemoveRequested: richComposer.removeBlock(enhancementSurface.parent.blockIndex)
            }

            RowLayout {
                id: enhancementLayout
                visible: !enhancementSurface.stickerBlock
                anchors.fill: parent
                anchors.margins: Theme.space2
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                EmbedCard {
                    visible: !enhancementSurface.mediaBlock
                    Layout.fillWidth: true
                    embed: root.enhancementModel(enhancementSurface.parent.blockData)
                    onOpenRequested: url => presentationBridge.openExternalUrl(url)
                    onCastRequested: castId => presentationBridge.openCast(castId)
                }

                Superellipse {
                    visible: enhancementSurface.mediaBlock
                    Layout.fillWidth: true
                    Layout.preferredHeight: 54
                    fillColor: Theme.surface
                    borderWidth: 0
                    curveDepth: Theme.curveControl

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection

                        Superellipse {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            fillColor: Theme.primarySurface
                            borderWidth: 0
                            curveDepth: Theme.curveCompact

                            Icon {
                                anchors.centerIn: parent
                                text: enhancementSurface.videoBlock ? Theme.iconPlay : Theme.iconImage
                                color: Theme.accent
                                preferredSize: Theme.typeCaptionSize
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: enhancementSurface.videoBlock
                                      ? qsTrId("genycaster.embed.video") : qsTrId("genycaster.embed.image")
                                color: Theme.ink
                                font.pixelSize: Theme.typeLabelSize
                                font.weight: Theme.weightDemiBold
                            }

                            // Media belongs to the attachment, not the cast's
                            // text. Never paint the raw object-storage URL in
                            // the visual editor or its eventual rich document.
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: qsTrId("genycaster.composer.media")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                            }
                        }

                        ActionButton {
                            quiet: true
                            compact: true
                            iconOnly: true
                            iconGlyph: Theme.iconArrowUpRight
                            Accessible.name: qsTrId("genycaster.embed.open_link")
                            onClicked: presentationBridge.openExternalUrl(String(enhancementSurface.parent.blockData.url || ""))
                        }
                    }
                }

                ActionButton {
                    Layout.alignment: Qt.AlignTop
                    quiet: true
                    compact: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    Accessible.name: qsTrId("genycaster.composer.remove_block")
                    onClicked: richComposer.removeBlock(enhancementSurface.parent.blockIndex)
                }
            }
        }
    }

    Component {
        id: textEditorComponent
        Superellipse {
            id: blockSurface
            // One editable text block fills the available composer work
            // area. Previously it was 86px high inside a 320px outer card,
            // leaving a nested tiny input and a large inaccessible blank area.
            // The composer owns the only editor boundary/focus ring.
            implicitHeight: root.blockModel.length === 1
                ? Math.max(list.height - Theme.space2, editor.contentHeight + Theme.space5)
                : Math.max(86, editor.contentHeight + Theme.space5)
            fillColor: "transparent"
            borderWidth: 0
            curveDepth: Theme.curveControl

            function editorMarkup() {
                return root.formattedMarkup(blockSurface.parent.blockData)
            }

            DirectionAwareTextArea {

                id: editor
                anchors.left: parent.left
                anchors.right: blockActions.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space2
                anchors.rightMargin: Theme.space1
                textFormat: TextEdit.RichText
                // The rendered HTML begins with Latin markup tags. Detect
                // direction from the *actual paragraph*, not that markup.
                // Otherwise Persian/Arabic are mistakenly left-aligned.
                forceRightToLeft: localizationController.isRightToLeftText(
                                      String(blockSurface.parent.blockData.text || ""))
                                  || (Theme.rtl
                                      && String(blockSurface.parent.blockData.text || "").length === 0)
                text: blockSurface.editorMarkup()
                placeholderText: String(blockSurface.parent.blockData.kind || "") === "heading"
                                 ? qsTrId("genycaster.composer.heading_placeholder")
                                 : String(blockSurface.parent.blockData.kind || "") === "quote"
                                   ? qsTrId("genycaster.composer.quote_placeholder")
                                   : qsTrId("genycaster.shell.whats_happening")
                background: null
                selectByMouse: true
                persistentSelection: true
                wrapMode: TextEdit.Wrap
                // Independent of the editor viewport height; contentHeight
                // includes all wrapped lines in RTL and LTR paragraphs.
                onContentHeightChanged: {
                    if (root.blockModel.length === 1)
                        root.singleParagraphContentHeight = Math.max(0, contentHeight)
                }
                Component.onCompleted: {
                    if (root.blockModel.length === 1)
                        root.singleParagraphContentHeight = Math.max(0, contentHeight)
                }
                font.pixelSize: String(blockSurface.parent.blockData.kind || "") === "heading" ? Theme.typeTitle3Size : Theme.typeBodySize
                font.weight: String(blockSurface.parent.blockData.kind || "") === "heading" ? Theme.weightBold : Theme.weightRegular
                onEditingFocusChanged: {
                    if (editingFocus) {
                        root.activeBlockIndex = blockSurface.parent.blockIndex
                        root.activeEditor = editor
                        root.activeEditorKind = "formatted"
                        root.activeEditorChangedExternally(root.activeBlockIndex, editor)
                    } else if (root.activeBlockIndex === blockSurface.parent.blockIndex) {
                        root.commitActiveEditor()
                    }
                }
                Keys.onPressed: event => {
                    const modifier = (event.modifiers & Qt.ControlModifier) || (event.modifiers & Qt.MetaModifier)
                    if (!modifier) return
                    if (event.key === Qt.Key_B && selectionStart !== selectionEnd) {
                        root.applyInlineStyle("strong")
                        event.accepted = true
                    } else if (event.key === Qt.Key_I && selectionStart !== selectionEnd) {
                        root.applyInlineStyle("emphasis")
                        event.accepted = true
                    }
                }
            }

            Row {
                id: blockActions
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.space2
                spacing: Theme.space1

                ActionButton {
                    quiet: true
                    compact: true
                    iconOnly: true
                    iconGlyph: Theme.iconTrash
                    Accessible.name: qsTrId("genycaster.composer.clear_block")
                    onClicked: root.clearBlock(blockSurface.parent.blockIndex)
                }
                ActionButton {
                    quiet: true
                    compact: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    visible: root.blockModel.length > 1
                    Accessible.name: qsTrId("genycaster.composer.remove_block")
                    onClicked: richComposer.removeBlock(blockSurface.parent.blockIndex)
                }
            }
        }
    }
}
