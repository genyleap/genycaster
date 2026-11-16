import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// The native controller owns provider credentials, signer verification and role checks.
Item {
    id: root
    property var manager: null
    property var channelInfo: ({})
    property bool compact: false
    property string activeTab: "overview"
    readonly property real preferredDialogHeight: root.compact ? 580
        : root.activeTab === "members" ? 470
        : root.activeTab === "moderate" ? 380
        : root.activeTab === "invites" ? 355 : 335
    property string pendingAction: ""
    property string pendingChannelId: ""
    property double pendingFid: 0
    signal closeRequested()
    signal feedRequested(var info)

    readonly property var channel: manager && String(manager.channel.id || "") === String(channelInfo.id || "")
                                   ? manager.channel : channelInfo
    readonly property bool moderator: manager ? manager.canModerate : false
    readonly property bool writeReady: manager ? manager.canWrite : false
    readonly property bool following: {
        const rows = manager ? manager.following : []
        for (let i = 0; i < rows.length; ++i)
            if (String(rows[i].id || "") === String(channelInfo.id || "")) return true
        return false
    }
    function present() {
        activeTab = "overview"
        if (!manager || !channelInfo.id) return
        manager.openChannel(String(channelInfo.id))
        manager.loadMembers()
        if (manager.followsAvailable) manager.loadFollowing()
    }
    function confirm(action, fid, id) {
        if (!writeReady) return
        pendingAction = action
        pendingFid = fid || 0
        pendingChannelId = id || String(channelInfo.id || "")
        confirmPopup.open()
    }
    function commit() {
        confirmPopup.close()
        if (!manager) return
        switch (pendingAction) {
        case "follow": manager.followChannel(true); break
        case "unfollow": manager.followChannel(false); break
        case "invite": manager.inviteMember(pendingFid); break
        case "remove": manager.removeMember(pendingFid); break
        case "accept": manager.respondToInvite(pendingChannelId, true); break
        case "reject": manager.respondToInvite(pendingChannelId, false); break
        }
        pendingAction = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space3
        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Avatar {
                avatarSize: 48
                sourceUrl: String(root.channel.imageUrl || "")
                displayName: String(root.channel.name || root.channelInfo.id || "")
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: String(root.channel.name || root.channelInfo.id || "")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: "/" + String(root.channelInfo.id || "")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideRight
                }
            }
            ActionButton {
                quiet: true
                compact: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.settings.done")
                onClicked: root.closeRequested()
            }
        }
        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            contentWidth: tabRow.implicitWidth
            contentHeight: height
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick
            clip: true
            RowLayout {
                id: tabRow
                height: 44
                layoutDirection: Theme.uiDirection
                spacing: Theme.space1
                Repeater {
                    model: [
                        {key: "overview", title: qsTrId("genycaster.channels.overview")},
                        {key: "members", title: qsTrId("genycaster.channels.members")},
                        {key: "moderate", title: qsTrId("genycaster.channels.moderate")},
                        {key: "invites", title: qsTrId("genycaster.channels.invites")}
                    ]
                    GenyTabButton {
                        required property var modelData
                        Layout.preferredWidth: Math.max(90, implicitWidth)
                        text: modelData.title
                        selected: root.activeTab === modelData.key
                        dense: true
                        onClicked: {
                            root.activeTab = modelData.key
                            if (modelData.key === "members" && root.manager)
                                root.manager.loadMembers()
                            if (modelData.key === "invites" && root.manager)
                                root.manager.loadInvites()
                        }
                    }
                }
            }
        }
        ScrollView {
            id: channelDetailsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical: GenyScrollBar {}
            ColumnLayout {
                width: Math.max(1, channelDetailsScroll.availableWidth - Theme.space2)
                spacing: Theme.space3
                ColumnLayout {
                    visible: root.activeTab === "overview"
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: String(root.channel.description || "")
                        wrapMode: Text.Wrap
                        color: Theme.inkMuted
                    }
                    ActionButton {
                        Layout.alignment: Theme.rtl ? Qt.AlignRight : Qt.AlignLeft
                        Layout.preferredWidth: Math.min(260, channelDetailsScroll.availableWidth - Theme.space4)
                        compact: true
                        prominent: true
                        iconGlyph: Theme.iconArrowRight
                        text: qsTrId("genycaster.channels.open_feed")
                        onClicked: root.feedRequested(root.channelInfo)
                    }
                    ActionButton {
                        Layout.fillWidth: true
                        visible: root.manager && root.manager.activeReadContract === "neynar-v2"
                                         && root.manager.followsAvailable
                        enabled: root.writeReady
                        iconGlyph: Theme.iconUser
                        text: root.following ? qsTrId("genycaster.channels.unfollow")
                                             : qsTrId("genycaster.channels.follow")
                        onClicked: root.confirm(root.following ? "unfollow" : "follow", 0, "")
                    }
                }
                ColumnLayout {
                    visible: root.activeTab === "members"
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Repeater {
                        model: root.manager ? root.manager.members : []
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            layoutDirection: Theme.uiDirection
                            Avatar {
                                avatarSize: 40
                                sourceUrl: String(modelData.avatarUrl || "")
                                displayName: String(modelData.displayName || "")
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(modelData.displayName || modelData.fid)
                                    color: Theme.ink
                                    elide: Text.ElideRight
                                }
                                DirectionAwareLabel {
                                    readonly property string actualHandle: String(modelData.username || "").trim().replace(/^@/, "")
                                    visible: actualHandle.length > 0 && !/^fid:[0-9]+$/i.test(actualHandle)
                                    text: visible ? "@" + actualHandle : ""
                                    color: Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                }
                            }
                            ActionButton {
                                visible: root.moderator
                                enabled: root.writeReady && Number(modelData.fid || 0) > 0
                                         && Number(modelData.fid) !== Number(root.channel.leadFid || 0)
                                compact: true
                                quiet: true
                                iconGlyph: Theme.iconClose
                                text: qsTrId("genycaster.channels.remove_member")
                                onClicked: root.confirm("remove", Number(modelData.fid), "")
                            }
                        }
                    }
                }
                ColumnLayout {
                    visible: root.activeTab === "moderate"
                    Layout.fillWidth: true
                    spacing: Theme.space3
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.moderator ? qsTrId("genycaster.channels.moderator_verified")
                                             : qsTrId("genycaster.channels.moderator_required")
                        color: Theme.inkMuted
                        wrapMode: Text.Wrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.moderator
                        layoutDirection: Theme.uiDirection
                        spacing: Theme.space2
                        DirectionAwareTextField {
                            id: invitationFid
                            Layout.fillWidth: true
                            placeholderText: qsTrId("genycaster.channels.invite_fid")
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: RegularExpressionValidator {
                                regularExpression: /^[0-9]{1,16}$/
                            }
                        }
                        ActionButton {
                            text: qsTrId("genycaster.channels.invite")
                            enabled: root.writeReady && /^[0-9]{1,16}$/.test(invitationFid.text)
                            onClicked: {
                                const fid = Number(invitationFid.text)
                                if (Number.isSafeInteger(fid) && fid > 0)
                                    root.confirm("invite", fid, "")
                            }
                        }
                    }
                }
                ColumnLayout {
                    visible: root.activeTab === "invites"
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        visible: !root.manager || !root.manager.invitesAvailable
                        text: qsTrId("genycaster.channels.invites_unavailable")
                        color: Theme.inkMuted
                        wrapMode: Text.Wrap
                    }
                    Repeater {
                        model: root.manager ? root.manager.invites : []
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: "/" + String(modelData.channelId || "")
                                color: Theme.ink
                                font.pixelSize: Theme.typeMetaSize
                            }
                            ActionButton {
                                compact: true
                                enabled: root.writeReady
                                text: qsTrId("genycaster.channels.accept")
                                onClicked: root.confirm("accept", 0, String(modelData.channelId))
                            }
                            ActionButton {
                                compact: true
                                quiet: true
                                enabled: root.writeReady
                                text: qsTrId("genycaster.channels.reject")
                                onClicked: root.confirm("reject", 0, String(modelData.channelId))
                            }
                        }
                    }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: root.activeTab !== "overview" && text.length > 0
                    text: root.manager ? String(root.manager.message || "") : ""
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeMetaSize
                    wrapMode: Text.Wrap
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    visible: (root.activeTab === "moderate" || root.activeTab === "invites")
                             && text.length > 0
                    text: root.manager ? String(root.manager.providerNotice || "") : ""
                    color: Theme.inkSubtle
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
        }
    }
    GenyModal {
        id: confirmPopup
        parent: root
        width: Math.min(390, Math.max(0, root.width - Theme.space4 * 2))
        height: Math.min(235, Math.max(0, root.height - Theme.space4 * 2))
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.space3
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.channels.confirm_title")
                color: Theme.ink
                font.pixelSize: Theme.typeTitle2Size
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.channels.confirm_detail")
                color: Theme.inkMuted
                wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                ActionButton {
                    Layout.fillWidth: true
                    quiet: true
                    text: qsTrId("genycaster.settings.cancel")
                    onClicked: confirmPopup.close()
                }
                ActionButton {
                    Layout.fillWidth: true
                    prominent: true
                    text: qsTrId("genycaster.channels.confirm")
                    enabled: root.writeReady
                    onClicked: root.commit()
                }
            }
        }
    }
}
