import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Public discovery and signed composition have separate controller instances.
Item {
    id: root
    objectName: "channelsSurface"
    property var channelSource: null
    property var adminSource: null
    // An unconfigured caller must not silently select a concrete backend.
    property string readContract: ""
    property string readEndpoint: ""
    property string appliedWriteProvider: ""
    property double accountFid: 0
    property bool authenticated: false
    property bool compact: false
    property string scope: "discover"
    signal channelRequested(var channelInfo)
    signal signInRequested()

    readonly property bool busy: root.scope === "following"
        ? (adminSource ? adminSource.busy : false)
        : (channelSource ? channelSource.busy : false)
    readonly property string sourceError: channelSource ? channelSource.errorText : ""
    readonly property var shownChannels: {
        const values = scope === "following"
            ? (adminSource ? adminSource.following : [])
            : channelSource ? channelSource.rows : []
        const result = []
        for (let index = 0; index < Number(values.length || 0); ++index) {
            const channel = values[index] || ({})
            if (!String(channel.id || "").length) continue // Home belongs only in the composer.
            const filter = channelSearch.text.trim().toLocaleLowerCase()
            if (root.scope === "following" && filter.length > 0
                && !String(channel.name || "").toLocaleLowerCase().includes(filter)
                && !String(channel.id || "").toLocaleLowerCase().includes(filter)) continue
            if (root.scope === "joined" && !Boolean(channel.isMember)) continue
            // A signed-in member is NOT necessarily a moderator. Missing
            // role metadata is unknown, never permission to moderate.
            if (root.scope === "moderate"
                && !(Boolean(channel.roleKnown) && Boolean(channel.isModerator)))
                continue
            result.push(channel)
        }
        return result
    }

    function openChannel(channel) {
        const id = String(channel && channel.id || "")
        if (!/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(id)) return
        root.channelRequested(channel)
    }
    function configureProviders() {
        const fid = Math.max(0, Math.floor(accountFid))
        if (channelSource) {
            channelSource.configureReadSource(readContract, readEndpoint)
            channelSource.openForViewer(fid)
        }
        if (adminSource) adminSource.configureSession(
            readContract, readEndpoint, fid, appliedWriteProvider)
    }
    Component.onCompleted: root.configureProviders()
    onAccountFidChanged: root.configureProviders()
    onReadContractChanged: root.configureProviders()
    onReadEndpointChanged: root.configureProviders()
    onAppliedWriteProviderChanged: root.configureProviders()

    function showManagement(info) {
        if (!info || !info.id) return
        channelManager.channelInfo = info
        channelManager.present()
        channelManagerPopup.open()
    }
    onScopeChanged: {
        channelSearch.clear()
        if (scope === "following" && adminSource) adminSource.loadFollowing()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? Theme.mobileGutter : Theme.contentGutter
        anchors.rightMargin: anchors.leftMargin
        anchors.topMargin: Theme.space3
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            Superellipse {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveControl
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconGlobe
                    color: Theme.accent
                    preferredSize: Theme.iconSize
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.side_nav.channels")
                    color: Theme.ink
                    font.pixelSize: Theme.typeTitle2Size
                    font.weight: Theme.weightBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.channels.intro")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
            ActionButton {
                objectName: "channelsRefreshButton"
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.feed_header.refresh")
                busy: root.busy
                spinIconWhileBusy: true
                enabled: !root.busy
                         && (root.scope === "following" ? root.adminSource !== null
                                                         : root.channelSource !== null)
                onClicked: {
                    if (root.scope === "following" && root.adminSource)
                        root.adminSource.loadFollowing()
                    else if (root.channelSource)
                        root.channelSource.refreshDiscovery()
                }
            }
        }

        Superellipse {
            Layout.fillWidth: true
            implicitHeight: 48
            fillColor: channelSearch.activeFocus ? Theme.surfaceElevated : Theme.surfaceRaised
            borderWidth: Theme.strokeHairline
            borderColor: channelSearch.activeFocus ? Theme.focus : Theme.borderMuted
            curveDepth: Theme.curveControl
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space2
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection
                Icon { text: Theme.iconSearch; color: Theme.inkMuted; preferredSize: Theme.iconSize }
                DirectionAwareTextField {
                    id: channelSearch
                    objectName: "channelsSearchField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    implicitHeight: 44
                    background: Item {}
                    placeholderText: qsTrId("genycaster.composer.channel_search")
                    Accessible.name: placeholderText
                    onTextChanged: {
                        if (root.scope !== "following" && root.channelSource)
                            root.channelSource.search(text)
                    }
                }
                ActionButton {
                    visible: channelSearch.text.length > 0
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.search.clear_results")
                    onClicked: channelSearch.clear()
                }
            }
        }

        Flickable {
            id: categoryTabs
            objectName: "channelManagementCategories"
            Layout.fillWidth: true
            Layout.preferredHeight: 43
            contentWidth: categoryRow.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            RowLayout {
                id: categoryRow
                height: categoryTabs.height
                layoutDirection: Theme.uiDirection
                spacing: Theme.space1
                GenyTabButton {
                    objectName: "channelsDiscoverTab"
                    minimumWidth: 100
                    selected: root.scope === "discover"
                    text: qsTrId("genycaster.channels.discover")
                    iconGlyph: Theme.iconExplore
                    onClicked: root.scope = "discover"
                }
                GenyTabButton {
                    objectName: "channelsFollowingTab"
                    minimumWidth: 100
                    selected: root.scope === "following"
                    text: qsTrId("genycaster.channels.following")
                    iconGlyph: Theme.iconUser
                    onClicked: root.scope = "following"
                }
                GenyTabButton {
                    objectName: "channelsJoinedTab"
                    minimumWidth: 100
                    selected: root.scope === "joined"
                    text: qsTrId("genycaster.channels.member")
                    iconGlyph: Theme.iconUser
                    onClicked: root.scope = "joined"
                }
                GenyTabButton {
                    objectName: "channelsModerateTab"
                    minimumWidth: 100
                    selected: root.scope === "moderate"
                    text: qsTrId("genycaster.channels.moderate")
                    iconGlyph: Theme.iconUser
                    onClicked: root.scope = "moderate"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.scope === "joined" ? qsTrId("genycaster.channels.member")
                      : root.scope === "following" ? qsTrId("genycaster.channels.following")
                      : root.scope === "moderate" ? qsTrId("genycaster.channels.moderate")
                      : qsTrId("genycaster.channels.discover")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeMetaSize
            }
            DirectionAwareLabel {
                text: LocaleFormat.formatInteger(root.shownChannels.length)
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            clip: true
            ListView {
                id: channelList
                objectName: "channelsList"
                anchors.fill: parent
                model: root.scope === "following"
                    ? root.shownChannels
                    : root.channelSource ? root.channelSource.channelModel : null
                reuseItems: true
                cacheBuffer: Math.max(320, height)
                clip: true
                boundsBehavior: Flickable.DragOverBounds
                bottomMargin: root.compact ? Theme.scaleUi(96) : Theme.space4
                ScrollBar.vertical: GenyScrollBar {}
                delegate: ChannelRow {
                    required property var modelData
                    width: channelList.width
                    channelData: modelData
                    compact: root.compact
                    allowed: (root.scope !== "joined" || Boolean(modelData.isMember))
                             && (root.scope !== "moderate"
                                 || (Boolean(modelData.roleKnown)
                                     && Boolean(modelData.isModerator)))
                             && (root.scope !== "following"
                                 || channelSearch.text.length === 0
                                 || String(modelData.name || "").toLocaleLowerCase()
                                    .includes(channelSearch.text.toLocaleLowerCase())
                                 || String(modelData.id || "").toLocaleLowerCase()
                                    .includes(channelSearch.text.toLocaleLowerCase()))
                    scrolling: channelList.moving
                    onOpenRequested: info => root.showManagement(info)
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(parent.width - Theme.space4 * 2, 390))
                spacing: Theme.space3
                visible: !root.busy && !(root.adminSource && root.adminSource.busy)
                         && root.shownChannels.length === 0
                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.scope === "joined" ? Theme.iconUser : Theme.iconSearch
                    color: Theme.inkMuted
                    preferredSize: Theme.typeTitle1Size
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.scope === "following"
                          ? (root.adminSource && root.adminSource.message.length > 0
                             ? root.adminSource.message
                             : root.adminSource && root.adminSource.followsAvailable
                               ? qsTrId("genycaster.channels.following_empty")
                               : qsTrId("genycaster.channels.following_unavailable"))
                          : root.scope === "moderate"
                            ? qsTrId("genycaster.channels.moderate_hint")
                          : root.scope === "joined"
                            ? (root.authenticated ? qsTrId("genycaster.channels.joined_empty")
                               : qsTrId("genycaster.channels.sign_in_hint"))
                          : root.sourceError.length > 0 ? root.sourceError
                            : channelSearch.text.length > 0 ? qsTrId("genycaster.composer.channel_no_results")
                            : qsTrId("genycaster.channels.search_hint")
                    color: Theme.inkMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.typeMetaSize
                }
                ActionButton {
                    visible: (root.scope === "joined" || root.scope === "following"
                              || root.scope === "moderate") && !root.authenticated
                    Layout.alignment: Qt.AlignHCenter
                    prominent: true
                    text: qsTrId("genycaster.notifications.sign_in")
                    onClicked: root.signInRequested()
                }
                ActionButton {
                    visible: root.scope === "discover" && root.sourceError.length > 0
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTrId("genycaster.feed_header.refresh")
                    onClicked: root.channelSource.refreshDiscovery()
                }
            }
            GenySpinner {
                anchors.centerIn: parent
                visible: (root.busy || (root.scope === "following" && root.adminSource && root.adminSource.busy))
                          && root.shownChannels.length === 0
                running: visible
            }
        }
    }
    GenyModal {
        id: channelManagerPopup
        objectName: "channelManagementPopup"
        parent: root
        // Content-sized on desktop. A large fixed modal previously occupied
        // nearly the entire display even when Overview had one action only.
        width: Math.min(root.compact ? 590 : 620,
                        Math.max(280, root.width - Theme.space4 * 2))
        height: Math.min(Math.max(280, root.height - Theme.space4 * 2),
                         channelManager.preferredDialogHeight)
        padding: Theme.space4
        ChannelManagementPanel {
            id: channelManager
            anchors.fill: parent
            manager: root.adminSource
            compact: root.compact
            onCloseRequested: channelManagerPopup.close()
            onFeedRequested: info => {
                channelManagerPopup.close()
                root.openChannel(info)
            }
        }
    }

}
