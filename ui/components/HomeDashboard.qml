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
    // Keep navigation shortcuts above future capabilities on short screens.
    function scrollToBeginning() {
        if (root.compact) {
            mobileFlick.cancelFlick()
            mobileFlick.contentY = 0
        }
    }
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
    signal trendsRequested()
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

    // This hub offers direct shortcuts, not a duplicate feed. Locked future
    // capabilities stay informational and never dispatch navigation or writes.
    function activateHubShortcut(destination) {
        if (destination === "search") root.searchRequested()
        else if (destination === "apps") root.appsRequested()
        else if (destination === "explorer") root.exploreRequested()
        else if (destination === "trends") root.trendsRequested()
    }

    // One quiet panel for destinations, with future features separated below.
    // Neither row generates feed data or performs a network request itself.
    component HubWorkspace: ColumnLayout {
        spacing: Theme.space3

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.home.hub_quick_access")
            color: Theme.textPrimary
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightDemiBold
        }

        Superellipse {
            objectName: "homeToolsPanel"
            Layout.fillWidth: true
            implicitHeight: toolsGrid.implicitHeight + Theme.space2 * 2
            fillColor: Theme.surface
            borderWidth: 1
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveSheet

            GridLayout {
                id: toolsGrid
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.space2
                anchors.rightMargin: Theme.space2
                columns: 2
                columnSpacing: Theme.space1
                rowSpacing: Theme.space1
                layoutDirection: Theme.uiDirection

                Repeater {
                    model: [
                        { destination: "explorer", name: qsTrId("genycaster.side_nav.explorer"), description: qsTrId("genycaster.home.hub_discover_detail"), glyph: Theme.iconCompass },
                        { destination: "search", name: qsTrId("genycaster.feed_header.universal_search"), description: qsTrId("genycaster.home.search_description"), glyph: Theme.iconSearch },
                        { destination: "apps", name: qsTrId("genycaster.apps.title"), description: qsTrId("genycaster.home.apps_description"), glyph: Theme.iconApps },
                        { destination: "trends", name: qsTrId("genycaster.home.hub_trends"), description: qsTrId("genycaster.home.hub_trends_detail"), glyph: Theme.iconChart }
                    ]
                    delegate: Item {
                        id: shortcut
                        required property var modelData
                        objectName: "homeShortcut_" + modelData.destination
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        Layout.preferredHeight: root.compact ? 79 : 88
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.name
                        Accessible.description: modelData.description
                        Accessible.onPressAction: root.activateHubShortcut(modelData.destination)
                        Keys.onReturnPressed: root.activateHubShortcut(modelData.destination)
                        Keys.onSpacePressed: root.activateHubShortcut(modelData.destination)

                        Superellipse {
                            anchors.fill: parent
                            fillColor: shortcutMouse.pressed ? Theme.surfacePressed
                                       : shortcutMouse.containsMouse ? Theme.surfaceHover : "transparent"
                            borderWidth: 0
                            curveDepth: Theme.curveControl
                            Behavior on fillColor {
                                ColorAnimation { duration: Theme.motionFast }
                            }
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space2
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2
                            Icon {
                                text: modelData.glyph
                                preferredSize: Theme.iconSizeLarge
                                color: Theme.accent
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                                DirectionAwareLabel {
                                    visible: !root.compact
                                    Layout.fillWidth: true
                                    text: modelData.description
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        MouseArea {
                            id: shortcutMouse
                            anchors.fill: parent
                            hoverEnabled: !root.compact
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activateHubShortcut(modelData.destination)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.home.hub_workspace")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightDemiBold
            }
            DirectionAwareLabel {
                text: qsTrId("genycaster.home.hub_coming_soon")
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: [
                    { key: "agent", title: qsTrId("genycaster.home.hub_agent"), glyph: Theme.iconCpu },
                    { key: "studio", title: qsTrId("genycaster.home.hub_studio"), glyph: Theme.iconImage },
                    { key: "wallet", title: qsTrId("genycaster.home.hub_wallet"), glyph: Theme.iconWallet }
                ]
                delegate: Superellipse {
                    required property var modelData
                    objectName: "homeLocked_" + modelData.key
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: root.compact ? 104 : 116
                    fillColor: Theme.surfaceMuted
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveContainer
                    Accessible.role: Accessible.StaticText
                    Accessible.name: modelData.title + " — " + qsTrId("genycaster.home.hub_coming_soon")

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space2
                        spacing: Theme.space1
                        RowLayout {
                            Layout.fillWidth: true
                            layoutDirection: Theme.uiDirection
                            Icon {
                                text: modelData.glyph
                                preferredSize: Theme.iconSizeLarge
                                color: Theme.textSecondary
                            }
                            Item { Layout.fillWidth: true }
                            Icon {
                                text: "system/lock"
                                preferredSize: Theme.iconSizeSmall
                                color: Theme.textTertiary
                            }
                        }
                        Item { Layout.fillHeight: true }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: Theme.textPrimary
                            font.pixelSize: root.compact ? Theme.typeMetaSize : Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.home.hub_coming_soon")
                            color: Theme.textTertiary
                            font.pixelSize: Theme.typeCaptionSize
                            elide: Text.ElideRight
                        }
                    }
                    // No MouseArea or TapHandler: a locked feature is not a button.
                }
            }
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

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: 760
                    spacing: Theme.space3
                    layoutDirection: Theme.uiDirection

                    Avatar {
                        avatarSize: 58
                        sourceUrl: root.avatarUrl
                        displayName: root.displayName || root.username
                        visible: root.authenticated && root.avatarUrl.length > 0
                    }
                    Icon {
                        text: Theme.iconLeaf
                        color: Theme.accent
                        preferredSize: 48
                        visible: !root.authenticated || root.avatarUrl.length === 0
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: root.greetingText
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeTitle1Size
                            font.weight: Theme.weightBold
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.home.hub_tagline")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeMetaSize
                            elide: Text.ElideRight
                        }
                    }
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

                HubWorkspace {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: 760
                }
                Item { Layout.fillHeight: true; Layout.minimumHeight: Theme.space2 }
            }
        }

    }

    Flickable {
        id: mobileFlick
        visible: root.compact
        anchors.fill: parent
        contentWidth: width
        // The former forced 108px tail made Home scroll into an empty area.
        // Real continuation rows now determine the content height instead.
        contentHeight: mobileColumn.implicitHeight + Theme.space3
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
            RowLayout {
                visible: !root.composerFocused
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                spacing: Theme.space3
                layoutDirection: Theme.uiDirection

                Avatar {
                    avatarSize: 52
                    sourceUrl: root.avatarUrl
                    displayName: root.displayName || root.username
                    visible: root.authenticated && root.avatarUrl.length > 0
                }
                Icon {
                    visible: !root.authenticated || root.avatarUrl.length === 0
                    text: Theme.iconLeaf
                    color: Theme.accent
                    preferredSize: 48
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.greetingText
                        color: Theme.textPrimary
                        font.pixelSize: root.densePhone ? Theme.typeTitle2Size : Theme.typeTitle1Size
                        font.weight: Theme.weightBold
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.home.hub_tagline")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        elide: Text.ElideRight
                    }
                }
            }

            Superellipse {
                id: mobileComposerSurface
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                fillColor: Theme.surface
                borderWidth: 1
                borderColor: root.composerFocused ? Theme.borderStrong : Theme.borderMuted
                curveDepth: root.composerFocused ? Theme.curveModal : height / 2
                implicitHeight: mobileComposer.implicitHeight
                                + (root.composerFocused ? Theme.space3 * 2 : Theme.space2 * 2)
                Behavior on implicitHeight {
                    NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionEmphasis; easing.type: Easing.OutCubic }
                }
                ColumnLayout {
                    id: mobileComposer
                    anchors.fill: parent
                    anchors.margins: root.composerFocused ? Theme.space3 : Theme.space2
                    spacing: root.composerFocused ? Theme.space2 : 0
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        TextArea {
                            id: mobileDraftEditor
                        objectName: "homeMobileDraftEditor"
                        ContextMenu.menu: EditorContextMenu { editor: mobileDraftEditor }
                        Layout.fillWidth: true
                        implicitHeight: root.composerFocused ? 98 : 38
                        text: root.draftText
                        placeholderText: qsTrId("genycaster.home.composer_placeholder")
                        wrapMode: TextEdit.Wrap
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeBodySize
                        background: Item {}
                        onTextChanged: root.draftText = text
                        }
                        Superellipse {
                            visible: !root.composerFocused
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            fillColor: Theme.accent
                            curveDepth: Theme.curveControl
                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconCompose
                                preferredSize: Theme.iconSize
                                color: Theme.accentInk
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.composeRequested(root.draftText)
                            }
                        }
                    }
                    RowLayout {
                        id: mobileComposeActions
                        visible: root.composerFocused
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

            HubWorkspace {
                visible: !root.composerFocused
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                Layout.topMargin: Theme.space2
            }
        }
    }
}