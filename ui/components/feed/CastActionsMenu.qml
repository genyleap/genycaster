import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Shared lazy Cast actions for feed cards and the conversation header.
Popup {
    id: root
    property var castData: ({})
    property bool showConversationAction: true
    property double accountFid: 0
    property bool interactionsAvailable: false
    readonly property bool canDelete: Number(root.castData.authorFid || 0) > 0
        && Number(root.castData.authorFid || 0) === Number(root.accountFid)
        && Boolean(root.castData.castId)
    readonly property bool canDeleteNow: root.canDelete && root.interactionsAvailable
    readonly property string castPermalink: presentationBridge.castShareUrl(
        CastViews.normalizeHash(root.castData.castId || ""))
    signal openRequested(string castId)
    signal profileRequested(double fid)
    signal removeRequested(var cast)
    function openAt(anchor, rtl) {
        if (!anchor || !Overlay.overlay) return
        const point = anchor.mapToItem(Overlay.overlay, 0, anchor.height + Theme.space1)
        const preferredX = rtl ? point.x : point.x + anchor.width - root.width
        root.x = Math.max(Theme.space2,
            Math.min(Overlay.overlay.width - root.width - Theme.space2, preferredX))
        root.y = Math.max(Theme.space2,
            Math.min(Overlay.overlay.height - root.height - Theme.space2, point.y))
        root.open()
    }
    objectName: "castMenu"
    parent: Overlay.overlay
    width: 224
    implicitHeight: menuContent.implicitHeight + Theme.space2
    padding: Theme.space1
    // A contextual menu is a modal input boundary: taps outside dismiss it
    // rather than accidentally opening the Cast below on touch browsers.
    modal: true
    dim: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: PopupSurface { curveDepth: Theme.curveContainer }
    onClosed: {
        castCopyCloseTimer.stop()
        castCopyTextAction.clearCopyFeedback()
        castCopyLinkAction.clearCopyFeedback()
        castCopyIdAction.clearCopyFeedback()
    }
    Timer {
        id: castCopyCloseTimer
        interval: 1100
        repeat: false
        onTriggered: root.close()
    }

    contentItem: ColumnLayout {
        id: menuContent
        spacing: 0

        ActionButton {
            Layout.fillWidth: true
            quiet: true
            alignStart: true
            iconGlyph: Theme.iconComment
            visible: root.showConversationAction && Boolean(root.castData.castId)
            text: qsTrId("genycaster.cast.view_conversation")
            onClicked: {
                root.close()
                root.openRequested(String(root.castData.castId || ""))
            }
        }

        ActionButton {
            visible: Number(root.castData.authorFid || 0) > 0
            Layout.fillWidth: true
            quiet: true
            alignStart: true
            iconGlyph: Theme.iconUser
            text: qsTrId("genycaster.cast.open_author_profile")
            onClicked: {
                root.close()
                root.profileRequested(Number(root.castData.authorFid))
            }
        }

        ActionButton {
            visible: String(root.castData.text || "").length > 0
            Layout.fillWidth: true
            quiet: true
            alignStart: true
            id: castCopyTextAction
            iconGlyph: Theme.iconCopy
            text: qsTrId("genycaster.cast.copy_text")
            onClicked: {
                const copied = presentationBridge.copyText(String(root.castData.text || ""))
                castCopyTextAction.acknowledgeCopy(copied)
                castCopyCloseTimer.restart()
            }
        }

        ActionButton {
            visible: root.castPermalink.length > 0
            Layout.fillWidth: true
            quiet: true
            alignStart: true
            id: castCopyLinkAction
            iconGlyph: Theme.iconLink
            text: qsTrId("genycaster.cast.copy_link")
            onClicked: {
                const copied = presentationBridge.copyText(root.castPermalink)
                castCopyLinkAction.acknowledgeCopy(copied)
                castCopyCloseTimer.restart()
            }
        }

        ActionButton {
            visible: String(root.castData.castId || "").length > 0
            Layout.fillWidth: true
            quiet: true
            alignStart: true
            id: castCopyIdAction
            iconGlyph: Theme.iconCopy
            text: qsTrId("genycaster.cast.copy_cast_id")
            onClicked: {
                const copied = presentationBridge.copyText(String(root.castData.castId || ""))
                castCopyIdAction.acknowledgeCopy(copied)
                castCopyCloseTimer.restart()
            }
        }

        Rectangle {
            visible: root.canDelete
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
        }

        ActionButton {
            visible: root.canDelete
            enabled: root.canDeleteNow
            Layout.fillWidth: true
            quiet: true
            alignStart: true
            destructive: true
            iconGlyph: Theme.iconClose
            text: qsTrId("genycaster.cast.delete")
            onClicked: {
                root.close()
                root.removeRequested(root.castData)
            }
        }
    }
}
