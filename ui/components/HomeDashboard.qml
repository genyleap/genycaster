import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string displayName: ""
    property string username: ""
    property string avatarUrl: ""
    property bool authenticated: false
    property bool compact: false
    property bool busy: false
    property int notificationUnreadCount: 0
    property var rows: []
    property int localHour: new Date().getHours()
    property string draftText: ""

    signal exploreRequested()
    signal composeRequested(string text)
    signal photoRequested()
    signal searchRequested()
    signal notificationsRequested()
    signal channelsRequested()
    signal appsRequested()
    signal settingsRequested()
    signal profileRequested(double fid)
    signal castRequested(string castId)

    readonly property string welcomeName: {
        const source = String(root.displayName || root.username || qsTrId("genycaster.side_nav.genycaster"))
        const trimmed = source.trim()
        if (trimmed.length === 0)
            return qsTrId("genycaster.side_nav.genycaster")
        const parts = trimmed.split(/\s+/)
        return parts.length > 0 ? parts[0] : trimmed
    }
    readonly property string greetingText: (root.localHour < 5
                                             ? qsTrId("genycaster.home.greeting_night")
                                             : root.localHour < 12
                                               ? qsTrId("genycaster.home.greeting_morning")
                                               : root.localHour < 17
                                                 ? qsTrId("genycaster.home.greeting_afternoon")
                                                 : root.localHour < 21
                                                   ? qsTrId("genycaster.home.greeting_evening")
                                                   : qsTrId("genycaster.home.greeting_night")).arg(root.welcomeName)
    implicitWidth: 980
    implicitHeight: 720

    Timer {
        interval: 60000
        repeat: true
        running: true
        onTriggered: root.localHour = new Date().getHours()
    }

    component ActionTile: Superellipse {
        id: tile
        property string titleText: ""
        property string subtitleText: ""
        property string iconGlyph: Theme.iconLeaf
        signal clicked()

        implicitHeight: root.compact ? 108 : 116
        fillColor: tileMouse.pressed ? Theme.surfacePressed
                                     : tileMouse.containsMouse ? Theme.surfaceHover : Theme.surface
        borderWidth: 1
        borderColor: tileMouse.containsMouse ? Theme.borderStrong : Theme.borderMuted
        curveDepth: Theme.curveControl
        scale: tileMouse.pressed ? 0.988 : 1.0

        Behavior on fillColor { ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
        Behavior on borderColor { ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
        Behavior on scale { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.OutCubic } }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space2

            Superellipse {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                fillColor: Theme.primarySurface
                curveDepth: Theme.curveCompact
                Icon {
                    anchors.centerIn: parent
                    text: tile.iconGlyph
                    color: Theme.accent
                    preferredSize: Theme.iconSize
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: tile.titleText
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: tile.subtitleText
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: tileMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tile.clicked()
        }
    }

    RowLayout {
        id: desktopLayout
        visible: !root.compact
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 560

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space4
                anchors.rightMargin: Theme.space4
                anchors.topMargin: Theme.space3
                anchors.bottomMargin: Theme.space3
                spacing: Theme.space2

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(24, Math.min(54, root.height * 0.055))
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Superellipse {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 86
                        Layout.preferredHeight: 86
                        fillColor: Theme.primarySurface
                        curveDepth: 43
                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconLeaf
                            color: Theme.accent
                            preferredSize: 38
                        }
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: root.greetingText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeDisplaySize
                        font.weight: Theme.weightBold
                        wrapMode: Text.Wrap
                    }

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTrId("genycaster.home.prompt")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
                    }
                }

                GridLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    Layout.maximumWidth: 720
                    columns: 3
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space2

                    ActionTile {
                        Layout.fillWidth: true
                        titleText: qsTrId("genycaster.feed_header.explore")
                        subtitleText: qsTrId("genycaster.feed_header.discover_people_channels_and_casts")
                        iconGlyph: Theme.iconExplore
                        onClicked: root.exploreRequested()
                    }
                    ActionTile {
                        Layout.fillWidth: true
                        titleText: qsTrId("genycaster.home.share_cast")
                        subtitleText: qsTrId("genycaster.home.compose_description")
                        iconGlyph: Theme.iconCompose
                        onClicked: root.composeRequested("")
                    }
                    ActionTile {
                        Layout.fillWidth: true
                        titleText: qsTrId("genycaster.apps.title")
                        subtitleText: qsTrId("genycaster.home.apps_description")
                        iconGlyph: Theme.iconApps
                        onClicked: root.appsRequested()
                    }
                }

                Item {
                    Layout.fillHeight: true
                    Layout.minimumHeight: Theme.space3
                    Layout.maximumHeight: 112
                }

                Superellipse {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: 760
                    fillColor: Theme.surface
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveModal
                    implicitHeight: composerColumn.implicitHeight + Theme.space3 * 2

                    ColumnLayout {
                        id: composerColumn
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2

                        TextArea {
                            Layout.fillWidth: true
                            implicitHeight: 80
                            text: root.draftText
                            placeholderText: qsTrId("genycaster.home.composer_placeholder")
                            wrapMode: TextEdit.Wrap
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                            selectByMouse: true
                            background: Item {}
                            onTextChanged: root.draftText = text
                        }

                        RowLayout {
                            id: composerActionRow
                            readonly property bool compactActions: width < 620
                            Layout.fillWidth: true
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            ActionButton { compact: true; quiet: true; iconOnly: composerActionRow.compactActions; iconGlyph: Theme.iconImage; text: qsTrId("genycaster.home.post_photo"); onClicked: root.photoRequested() }
                            ActionButton { compact: true; quiet: true; iconOnly: composerActionRow.compactActions; iconGlyph: Theme.iconGlobe; text: qsTrId("genycaster.side_nav.channels"); onClicked: root.channelsRequested() }
                            ActionButton { compact: true; quiet: true; iconOnly: composerActionRow.compactActions; iconGlyph: Theme.iconSearch; text: qsTrId("genycaster.feed_header.universal_search"); onClicked: root.searchRequested() }
                            Item { Layout.fillWidth: true }
                            ActionButton {
                                compact: true
                                prominent: true
                                iconGlyph: Theme.iconCompose
                                text: qsTrId("genycaster.header.new_cast")
                                onClicked: root.composeRequested(root.draftText)
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: Theme.space2 }
            }
        }

    }

    Flickable {
        visible: root.compact
        anchors.fill: parent
        contentWidth: width
        contentHeight: mobileColumn.implicitHeight + 108
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AlwaysOff }

        ColumnLayout {
            id: mobileColumn
            width: parent.width
            spacing: Theme.space3

            Item { Layout.preferredHeight: Theme.space3 }
            Superellipse {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                fillColor: Theme.primarySurface
                curveDepth: 36
                Icon { anchors.centerIn: parent; text: Theme.iconLeaf; color: Theme.accent; preferredSize: 32 }
            }
            DirectionAwareLabel { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: root.greetingText; color: Theme.textPrimary; font.pixelSize: Theme.typeTitle1Size; font.weight: Theme.weightBold; wrapMode: Text.Wrap }
            DirectionAwareLabel { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTrId("genycaster.home.prompt"); color: Theme.textSecondary; font.pixelSize: Theme.typeMetaSize }

            ActionTile { Layout.fillWidth: true; Layout.leftMargin: Theme.space3; Layout.rightMargin: Theme.space3; titleText: qsTrId("genycaster.feed_header.explore"); subtitleText: qsTrId("genycaster.feed_header.discover_people_channels_and_casts"); iconGlyph: Theme.iconExplore; onClicked: root.exploreRequested() }
            ActionTile { Layout.fillWidth: true; Layout.leftMargin: Theme.space3; Layout.rightMargin: Theme.space3; titleText: qsTrId("genycaster.home.share_cast"); subtitleText: qsTrId("genycaster.home.compose_description"); iconGlyph: Theme.iconCompose; onClicked: root.composeRequested("") }
            ActionTile { Layout.fillWidth: true; Layout.leftMargin: Theme.space3; Layout.rightMargin: Theme.space3; titleText: qsTrId("genycaster.apps.title"); subtitleText: qsTrId("genycaster.home.apps_description"); iconGlyph: Theme.iconApps; onClicked: root.appsRequested() }

            Superellipse {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                fillColor: Theme.surface
                borderWidth: 1
                borderColor: Theme.borderMuted
                curveDepth: Theme.curveModal
                implicitHeight: mobileComposer.implicitHeight + Theme.space3 * 2
                ColumnLayout {
                    id: mobileComposer
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2
                    TextArea {
                        Layout.fillWidth: true
                        implicitHeight: 92
                        text: root.draftText
                        placeholderText: qsTrId("genycaster.home.composer_placeholder")
                        wrapMode: TextEdit.Wrap
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeBodySize
                        background: Item {}
                        onTextChanged: root.draftText = text
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        ActionButton { compact: true; quiet: true; iconOnly: true; iconGlyph: Theme.iconImage; text: qsTrId("genycaster.home.post_photo"); onClicked: root.photoRequested() }
                        ActionButton { compact: true; quiet: true; iconOnly: true; iconGlyph: Theme.iconSearch; text: qsTrId("genycaster.feed_header.universal_search"); onClicked: root.searchRequested() }
                        Item { Layout.fillWidth: true }
                        ActionButton { compact: true; prominent: true; iconGlyph: Theme.iconCompose; text: qsTrId("genycaster.header.new_cast"); onClicked: root.composeRequested(root.draftText) }
                    }
                }
            }

        }
    }
}
