import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "homeDashboard"

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
    // The on-screen keyboard shrinks iOS Safari's visual viewport. Keep the
    // draft and its actions visible rather than squeezing the full dashboard
    // and floating dock into the remaining height.
    readonly property bool composerFocused: mobileDraftEditor.activeFocus
    // A phone's usable height excludes the shell header and floating dock.
    // Fit the hero, four shortcuts and composer above the dock on short screens.
    readonly property bool densePhone: root.compact && root.height < 840
    // Dismiss the draft editor without discarding its contents.
    TapHandler {
        enabled: root.compact && root.composerFocused
        onTapped: point => {
            const local = mobileComposerSurface.mapFromItem(
                root, point.position.x, point.position.y)
            if (local.x < 0 || local.y < 0
                    || local.x >= mobileComposerSurface.width
                    || local.y >= mobileComposerSurface.height)
                root.finishComposerEditing()
        }
    }
    // iOS can dismiss its software keyboard while Qt's text editor keeps
    // activeFocus. The shell explicitly ends editing on viewport restoration.
    function finishComposerEditing() {
        if (!root.composerFocused)
            return
        mobileDraftEditor.focus = false
        root.forceActiveFocus()
    }

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

        implicitHeight: root.compact ? (root.densePhone ? 148 : 170) : 156
        fillColor: tileMouse.pressed ? Theme.surfacePressed
                                     : tileMouse.containsMouse ? Theme.surfaceHover : Theme.surface
        borderWidth: 1
        borderColor: tileMouse.containsMouse ? Theme.borderStrong : Theme.borderMuted
        curveDepth: Theme.curveModal
        scale: tileMouse.pressed ? 0.988 : 1.0

        Behavior on fillColor { ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
        Behavior on borderColor { ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast } }
        Behavior on scale { NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.OutCubic } }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.densePhone ? Theme.space3 : Theme.space4
            spacing: root.densePhone ? Theme.space2 : Theme.space3

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2
                layoutDirection: Theme.uiDirection

                Superellipse {
                    Layout.preferredWidth: root.compact ? 48 : 52
                    Layout.preferredHeight: Layout.preferredWidth
                    fillColor: Theme.surfaceSelected
                    borderWidth: 0
                    curveDepth: Theme.curveContainer

                    Icon {
                        anchors.centerIn: parent
                        text: tile.iconGlyph
                        color: Theme.accent
                        preferredSize: root.compact ? 27 : 29
                    }
                }

                Item { Layout.fillWidth: true }

                Icon {
                    text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                    color: Theme.textTertiary
                    preferredSize: Theme.iconSizeSmall
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: tile.titleText
                color: Theme.textPrimary
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.fillHeight: true
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
                        Layout.preferredWidth: 66
                        Layout.preferredHeight: 66
                        fillColor: "transparent"
                        borderWidth: 0
                        curveDepth: 33
                        Avatar {
                            anchors.centerIn: parent
                            avatarSize: 62
                            sourceUrl: root.avatarUrl
                            displayName: root.displayName || root.username
                            visible: root.authenticated && root.avatarUrl.length > 0
                        }
                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconLeaf
                            color: Theme.accent
                            preferredSize: 42
                            visible: !root.authenticated || root.avatarUrl.length === 0
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
                    columns: 4
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
                        onClicked: root.composeRequested(root.draftText)
                    }
                    ActionTile {
                        Layout.fillWidth: true
                        titleText: qsTrId("genycaster.apps.title")
                        subtitleText: qsTrId("genycaster.home.apps_description")
                        iconGlyph: Theme.iconApps
                        onClicked: root.appsRequested()
                    }
                    ActionTile {
                        Layout.fillWidth: true
                        titleText: qsTrId("genycaster.feed_header.universal_search")
                        subtitleText: qsTrId("genycaster.home.search_description")
                        iconGlyph: Theme.iconSearch
                        onClicked: root.searchRequested()
                    }
                }

                Item {
                    Layout.fillHeight: true
                    Layout.minimumHeight: Theme.space3
                    Layout.maximumHeight: 112
                }

                Superellipse {
                    id: desktopComposerSurface
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: 760
                    fillColor: Theme.surface
                    borderWidth: desktopDraftEditor.activeFocus ? Theme.strokeEmphasis : 1
                    borderColor: desktopDraftEditor.activeFocus ? Theme.accent : Theme.borderMuted
                    curveDepth: Theme.curveModal
                    implicitHeight: composerColumn.implicitHeight + Theme.space3 * 2

                    ColumnLayout {
                        id: composerColumn
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2

                        TextArea {
                            id: desktopDraftEditor
                            ContextMenu.menu: EditorContextMenu { editor: desktopDraftEditor }
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
        id: mobileFlick
        visible: root.compact
        anchors.fill: parent
        contentWidth: width
        contentHeight: mobileColumn.implicitHeight
                       + (root.composerFocused ? Theme.space4
                          : Math.max(108, Theme.touchTarget + Theme.space6 * 2))
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AlwaysOff }

        // When a textarea takes focus, the composer becomes the first
        // meaningful content and the top of the flickable stays reachable.
        Connections {
            target: root
            function onComposerFocusedChanged() {
                if (root.composerFocused)
                    mobileFlick.contentY = 0
            }
        }

        ColumnLayout {
            id: mobileColumn
            width: parent.width
            spacing: root.densePhone ? Theme.space2 : Theme.space3
            // Keep the focused composer clear of the fixed application header
            // even after the navigation tiles are hidden by the keyboard.
            Item { Layout.preferredHeight: root.composerFocused ? Theme.space4
                                                  : root.densePhone ? Theme.space1 : Theme.space3 }
            Superellipse {
                visible: !root.composerFocused
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: root.densePhone ? 60 : 80
                Layout.preferredHeight: Layout.preferredWidth
                fillColor: "transparent"
                borderWidth: 0
                curveDepth: width / 2
                Avatar {
                    anchors.centerIn: parent
                    avatarSize: root.densePhone ? 56 : 76
                    sourceUrl: root.avatarUrl
                    displayName: root.displayName || root.username
                    visible: root.authenticated && root.avatarUrl.length > 0
                }
                Icon {
                    anchors.centerIn: parent
                    text: Theme.iconLeaf
                    color: Theme.accent
                    preferredSize: root.densePhone ? 38 : 50
                    visible: !root.authenticated || root.avatarUrl.length === 0
                }
            }
            DirectionAwareLabel { visible: !root.composerFocused; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: root.greetingText; color: Theme.textPrimary; font.pixelSize: root.densePhone ? Theme.typeTitle1Size : Theme.typeDisplaySize; font.weight: Theme.weightBold; wrapMode: Text.Wrap }
            DirectionAwareLabel { visible: !root.composerFocused; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTrId("genycaster.home.prompt"); color: Theme.textSecondary; font.pixelSize: Theme.typeMetaSize }

            // Phone: a true two-column navigation grid. All four targets
            // remain visible at once instead of three wasteful full-width rows.
            GridLayout {
                visible: !root.composerFocused
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                columns: 2
                columnSpacing: root.densePhone ? Theme.space2 : Theme.space3
                rowSpacing: root.densePhone ? Theme.space2 : Theme.space3
                layoutDirection: Theme.uiDirection

                ActionTile {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    titleText: qsTrId("genycaster.feed_header.explore")
                    subtitleText: qsTrId("genycaster.feed_header.discover_people_channels_and_casts")
                    iconGlyph: Theme.iconExplore
                    onClicked: root.exploreRequested()
                }
                ActionTile {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    titleText: qsTrId("genycaster.home.share_cast")
                    subtitleText: qsTrId("genycaster.home.compose_description")
                    iconGlyph: Theme.iconCompose
                    onClicked: root.composeRequested(root.draftText)
                }
                ActionTile {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    titleText: qsTrId("genycaster.apps.title")
                    subtitleText: qsTrId("genycaster.home.apps_description")
                    iconGlyph: Theme.iconApps
                    onClicked: root.appsRequested()
                }
                ActionTile {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    titleText: qsTrId("genycaster.feed_header.universal_search")
                    subtitleText: qsTrId("genycaster.home.search_description")
                    iconGlyph: Theme.iconSearch
                    onClicked: root.searchRequested()
                }
            }

            Superellipse {
                id: mobileComposerSurface
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                fillColor: Theme.surface
                borderWidth: mobileDraftEditor.activeFocus ? Theme.strokeEmphasis : 1
                borderColor: mobileDraftEditor.activeFocus ? Theme.accent : Theme.borderMuted
                curveDepth: Theme.curveModal
                implicitHeight: mobileComposer.implicitHeight
                                + (root.densePhone ? Theme.space3 : Theme.space4) * 2
                ColumnLayout {
                    id: mobileComposer
                    anchors.fill: parent
                    anchors.margins: root.densePhone ? Theme.space3 : Theme.space4
                    spacing: Theme.space3
                    TextArea {
                        id: mobileDraftEditor
                        objectName: "homeMobileDraftEditor"
                        ContextMenu.menu: EditorContextMenu { editor: mobileDraftEditor }
                        Layout.fillWidth: true
                        implicitHeight: root.densePhone ? 62 : 92
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
                        // Accessible explicit exit, even if the browser's
                        // virtual-keyboard dismissal does not clear Qt focus.
                        ActionButton {
                            objectName: "homeMobileFinishEditingButton"
                            visible: root.composerFocused
                            compact: true
                            quiet: true
                            iconOnly: true
                            iconGlyph: Theme.iconCheck
                            text: qsTrId("genycaster.shell.close")
                            onClicked: root.finishComposerEditing()
                        }
                        Item { Layout.fillWidth: true }
                        ActionButton { compact: true; prominent: true; iconGlyph: Theme.iconCompose; text: qsTrId("genycaster.header.new_cast"); onClicked: root.composeRequested(root.draftText) }
                    }
                }
            }

        }
    }
}
