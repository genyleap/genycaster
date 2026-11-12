import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Public discovery and signed composition have separate controller instances.
Item {
    id: root
    objectName: "channelsSurface"
    property var channelSource: null
    property double accountFid: 0
    property bool authenticated: false
    property bool compact: false
    property string scope: "discover"
    signal channelRequested(var channelInfo)
    signal signInRequested()

    readonly property bool busy: channelSource ? channelSource.busy : false
    readonly property string sourceError: channelSource ? channelSource.errorText : ""
    readonly property var shownChannels: {
        const values = channelSource ? channelSource.rows : []
        const result = []
        for (let index = 0; index < Number(values.length || 0); ++index) {
            const channel = values[index] || ({})
            if (!String(channel.id || "").length) continue // Home belongs only in the composer.
            if (root.scope === "joined" && !Boolean(channel.isMember)) continue
            result.push(channel)
        }
        return result
    }

    function openChannel(channel) {
        const id = String(channel && channel.id || "")
        if (!/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(id)) return
        root.channelRequested(channel)
    }
    Component.onCompleted: {
        if (channelSource) channelSource.openForViewer(Math.max(0, Math.floor(accountFid)))
    }
    onAccountFidChanged: {
        if (channelSource) channelSource.openForViewer(Math.max(0, Math.floor(accountFid)))
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
                enabled: !root.busy && root.channelSource !== null
                onClicked: root.channelSource.refreshDiscovery()
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
                        if (root.channelSource) root.channelSource.search(text)
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

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection
            GenyTabButton {
                objectName: "channelsDiscoverTab"
                Layout.fillWidth: true
                selected: root.scope === "discover"
                text: qsTrId("genycaster.channels.discover")
                iconGlyph: Theme.iconExplore
                onClicked: root.scope = "discover"
            }
            GenyTabButton {
                objectName: "channelsJoinedTab"
                Layout.fillWidth: true
                selected: root.scope === "joined"
                text: qsTrId("genycaster.composer.joined_channel")
                iconGlyph: Theme.iconUser
                onClicked: root.scope = "joined"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.scope === "joined" ? qsTrId("genycaster.composer.joined_channel")
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
                model: root.channelSource ? root.channelSource.channelModel : null
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
                    allowed: root.scope !== "joined" || Boolean(modelData.isMember)
                    scrolling: channelList.moving
                    onOpenRequested: info => root.openChannel(info)
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(parent.width - Theme.space4 * 2, 390))
                spacing: Theme.space3
                visible: !root.busy && root.shownChannels.length === 0
                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.scope === "joined" ? Theme.iconUser : Theme.iconSearch
                    color: Theme.inkMuted
                    preferredSize: Theme.typeTitle1Size
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.scope === "joined"
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
                    visible: root.scope === "joined" && !root.authenticated
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
                visible: root.busy && root.shownChannels.length === 0
                running: visible
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.channels.read_note")
            color: Theme.inkSubtle
            font.pixelSize: Theme.typeCaptionSize
            maximumLineCount: 2
            elide: Text.ElideRight
            wrapMode: Text.Wrap
        }
    }
}
