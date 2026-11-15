import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

NeonCard {
    id: root
    interactive: true
    ambientNeon: true
    neonStrength: Theme.neonPanelStrength

    property string kind: ""
    property int blockIndex: -1
    property int selectionStart: 0
    property int selectionLength: 0
    property string selectedText: ""

    signal finished()
    signal mediaFileRequested(int blockIndex)

    visible: kind.length > 0
    fillColor: Theme.surfaceElevated
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveContainer
    elevation: Theme.elevationControl
    shadowOpacityScale: 0.64
    implicitHeight: content.implicitHeight + Theme.space3 * 2

    function resetFields() {
        primary.text = root.kind === "mention" ? root.selectedText.replace(/^@/, "") : ""
        secondary.text = ""
    }

    function applyCurrent() {
        let applied = false
        if (root.kind === "link") {
            applied = richComposer.applyLink(
                root.blockIndex, root.selectionStart, root.selectionLength, primary.text)
        } else if (root.kind === "mention") {
            const fid = Number(secondary.text)
            if (fid > 0 && primary.text.trim().length > 0) {
                applied = richComposer.resolveMention(
                    root.blockIndex, root.selectionStart, root.selectionLength,
                    fid, primary.text.trim().replace(/^@/, ""), "")
            }
        } else if (root.kind === "embed") {
            applied = richComposer.insertEmbed(primary.text, secondary.text, root.blockIndex)
        }
        if (applied) root.finished()
    }

    onKindChanged: resetFields()
    onSelectedTextChanged: if (kind === "mention") resetFields()

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Icon {
                text: root.kind === "link" ? Theme.iconLink
                    : root.kind === "mention" ? Theme.iconMention
                    : root.kind === "media" ? Theme.iconImage
                    : Theme.iconLink
                color: Theme.accent
                preferredSize: Theme.typeBodySize
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.kind === "link" ? qsTrId("genycaster.composer.link")
                    : root.kind === "mention" ? qsTrId("genycaster.composer.mention")
                    : root.kind === "media" ? qsTrId("genycaster.composer.media")
                    : qsTrId("genycaster.composer.embed")
                color: Theme.ink
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
            }

            ActionButton {
                quiet: true
                compact: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                Accessible.name: qsTrId("genycaster.shell.close")
                onClicked: root.finished()
            }
        }

        ColumnLayout {
            visible: root.kind === "media"
            Layout.fillWidth: true
            spacing: Theme.space2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: mediaUpload.available
                    ? qsTrId("genycaster.media.choose_file_hint")
                    : qsTrId("genycaster.media.upload_unavailable")
                color: mediaUpload.available ? Theme.inkMuted : Theme.warning
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }

            ActionButton {
                prominent: true
                compact: true
                iconGlyph: Theme.iconImage
                text: qsTrId("genycaster.media.choose_file")
                enabled: mediaUpload.available && !mediaUpload.busy
                onClicked: root.mediaFileRequested(root.blockIndex)
            }
        }

        RowLayout {
            visible: root.kind !== "media"
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            DirectionAwareTextField {
                id: primary
                Layout.fillWidth: true
                placeholderText: root.kind === "mention"
                    ? qsTrId("genycaster.composer.username")
                    : qsTrId("genycaster.composer.url")
                forceLeftToRight: root.kind !== "mention"
                onAccepted: if (root.kind === "link") root.applyCurrent()
            }

            DirectionAwareTextField {
                id: secondary
                visible: root.kind !== "link"
                Layout.fillWidth: root.kind !== "mention"
                Layout.preferredWidth: root.kind === "mention" ? 160 : 320
                placeholderText: root.kind === "mention"
                    ? qsTrId("genycaster.composer.fid")
                    : qsTrId("genycaster.composer.title_optional")
                forceLeftToRight: root.kind === "mention"
                inputMethodHints: root.kind === "mention" ? Qt.ImhDigitsOnly : Qt.ImhNone
                onAccepted: root.applyCurrent()
            }

            ActionButton {
                prominent: true
                compact: true
                text: qsTrId("genycaster.composer.apply")
                enabled: primary.text.trim().length > 0
                         && (root.kind !== "mention" || Number(secondary.text) > 0)
                onClicked: root.applyCurrent()
            }
        }
    }
}
