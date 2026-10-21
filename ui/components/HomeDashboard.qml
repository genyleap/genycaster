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
    implicitHeight: 630

    Timer {
        interval: 60000
        repeat: true
        running: true
        onTriggered: root.localHour = new Date().getHours()
    }

    // Hub is intentionally distinct from Explorer: no fake feed and no
    // network dependency. Locked capability tiles are informational only.
    component HubWorkspace: ColumnLayout {
        spacing: Theme.space2

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
            columns: 2
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: [
                    { title: qsTrId("genycaster.home.hub_agent"), description: qsTrId("genycaster.home.hub_agent_detail"), glyph: Theme.iconCpu },
                    { title: qsTrId("genycaster.home.hub_studio"), description: qsTrId("genycaster.home.hub_studio_detail"), glyph: Theme.iconImage }
                ]
                delegate: Superellipse {
                    required property int index
                    required property var modelData
                    objectName: index === 0 ? "homeAgentComingSoon" : "homeStudioComingSoon"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: root.compact ? 148 : 156
                    fillColor: Theme.surface
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveContainer

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
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
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightDemiBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: modelData.description
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.home.hub_coming_soon")
                            color: Theme.textTertiary
                            font.pixelSize: Theme.typeCaptionSize
                        }
                    }
                    // No TapHandler: capabilities are disabled until shipped.
                }
            }
        }

        Superellipse {
            id: discoverCard
            objectName: "homeDiscoverFarcaster"
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 86 : 92
            fillColor: discoverMouse.pressed ? Theme.surfacePressed
                       : discoverMouse.containsMouse ? Theme.surfaceHover : Theme.surface
            borderWidth: 1
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveContainer
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTrId("genycaster.home.hub_discover")
            Accessible.onPressAction: root.exploreRequested()
            Keys.onReturnPressed: root.exploreRequested()
            Keys.onSpacePressed: root.exploreRequested()

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space3
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                Icon {
                    text: Theme.iconCompass
                    preferredSize: Theme.iconSizeLarge
                    color: Theme.accent
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.home.hub_discover")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.typeBodySize
                        font.weight: Theme.weightDemiBold
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.home.hub_discover_detail")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeCaptionSize
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }
                }
                Icon {
                    text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                    preferredSize: Theme.iconSizeSmall
                    color: Theme.textTertiary
                }
            }
            MouseArea {
                id: discoverMouse
                anchors.fill: parent
                hoverEnabled: !root.compact
                cursorShape: Qt.PointingHandCursor
                onClicked: root.exploreRequested()
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
                        text: qsTrId("genycaster.home.hub_tagline")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
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
            DirectionAwareLabel { visible: !root.composerFocused; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: qsTrId("genycaster.home.hub_tagline"); color: Theme.textSecondary; font.pixelSize: Theme.typeMetaSize }

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
                    NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
                }
                ColumnLayout {
                    id: mobileComposer
                    anchors.fill: parent
                    anchors.margins: root.composerFocused ? Theme.space3 : Theme.space2
                    spacing: root.composerFocused ? Theme.space2 : 0
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