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
    // Reuse casts already held by the feed pipeline. Home must not trigger an
    // extra request or invent personalized activity just to fill vertical space.
    readonly property var recentCasts: {
        const source = root.rows || []
        const result = []
        const seen = ({})
        for (let i = 0; i < source.length && result.length < 2; ++i) {
            const row = source[i] || ({})
            const castId = String(row.castId || "")
            if (!castId || seen[castId]) continue
            seen[castId] = true
            result.push({
                castId: castId,
                name: String(row.displayName || row.author || row.username || ""),
                avatarUrl: String(row.avatarUrl || ""),
                text: String(row.text || "").replace(/\s+/g, " ").trim()
            })
        }
        return result
    }
    property int localHour: new Date().getHours()
    property string draftText: ""
    // Curated, local-first writing starters. No extra feed request, tracking,
    // account requirement or automatic publishing.
    property int ideaOffset: 0
    readonly property var castIdeas: [
        { topic: qsTrId("genycaster.home.idea_dev_topic"), prompt: qsTrId("genycaster.home.idea_dev_prompt") },
        { topic: qsTrId("genycaster.home.idea_nft_topic"), prompt: qsTrId("genycaster.home.idea_nft_prompt") },
        { topic: qsTrId("genycaster.home.idea_opinion_topic"), prompt: qsTrId("genycaster.home.idea_opinion_prompt") },
        { topic: qsTrId("genycaster.home.idea_privacy_topic"), prompt: qsTrId("genycaster.home.idea_privacy_prompt") },
        { topic: qsTrId("genycaster.home.idea_farcaster_topic"), prompt: qsTrId("genycaster.home.idea_farcaster_prompt") }
    ]
    function ideaAt(offset) {
        return root.castIdeas[(root.ideaOffset + offset) % root.castIdeas.length]
    }
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

    // One reusable, keyboard-accessible idea surface for desktop and mobile.
    // Rotating examples is entirely local; selecting one opens a draft.
    component CastIdeaSection: ColumnLayout {
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            Icon {
                text: Theme.iconCompose
                color: Theme.accent
                preferredSize: Theme.iconSizeSmall
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.home.ideas_title")
                color: Theme.textPrimary
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }
            ActionButton {
                objectName: "homeIdeasRefresh"
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconRefresh
                text: qsTrId("genycaster.home.ideas_refresh")
                onClicked: root.ideaOffset = (root.ideaOffset + 1) % root.castIdeas.length
            }
        }

        Superellipse {
            id: featuredIdea
            objectName: "homeFeaturedCastIdea"
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 116 : 104
            fillColor: featuredMouse.pressed ? Theme.surfacePressed
                                             : featuredMouse.containsMouse ? Theme.surfaceHover : Theme.surface
            borderWidth: 1
            borderColor: featuredMouse.containsMouse ? Theme.borderStrong : Theme.borderMuted
            curveDepth: Theme.curveContainer
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: root.ideaAt(0).prompt
            Accessible.description: qsTrId("genycaster.home.ideas_hint")
            Accessible.onPressAction: root.composeRequested(root.ideaAt(0).prompt)
            Keys.onReturnPressed: root.composeRequested(root.ideaAt(0).prompt)
            Keys.onSpacePressed: root.composeRequested(root.ideaAt(0).prompt)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space3
                spacing: Theme.space1
                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: root.ideaAt(0).topic
                        color: Theme.accent
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                    Icon {
                        text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                        color: Theme.textTertiary
                        preferredSize: Theme.iconSizeSmall
                    }
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: root.ideaAt(0).prompt
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeBodySize
                    maximumLineCount: 3
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }
            MouseArea {
                id: featuredMouse
                anchors.fill: parent
                hoverEnabled: !root.compact
                cursorShape: Qt.PointingHandCursor
                onClicked: root.composeRequested(root.ideaAt(0).prompt)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2
            ActionButton {
                compact: true
                quiet: true
                iconGlyph: Theme.iconCompose
                text: root.ideaAt(1).topic
                onClicked: root.composeRequested(root.ideaAt(1).prompt)
            }
            ActionButton {
                compact: true
                quiet: true
                iconGlyph: Theme.iconCompose
                text: root.ideaAt(2).topic
                onClicked: root.composeRequested(root.ideaAt(2).prompt)
            }
            Item { Layout.fillWidth: true }
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

                CastIdeaSection {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: 760
                }

                Item {
                    Layout.fillHeight: true
                    Layout.minimumHeight: Theme.space2
                    Layout.maximumHeight: 48
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

            CastIdeaSection {
                visible: !root.composerFocused
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                Layout.topMargin: Theme.space1
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

            // The unused lower half of the original mobile dashboard is now
            // a small, actionable continuation of the *already-loaded* feed.
            // Never synthesize posts, identities, counts, or network requests.
            ColumnLayout {
                id: homeContinuation
                objectName: "homeContinuation"
                visible: !root.composerFocused
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space3
                Layout.rightMargin: Theme.space3
                Layout.topMargin: Theme.space2
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    layoutDirection: Theme.uiDirection

                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: qsTrId("genycaster.home.catch_up")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.typeMetaSize
                        font.weight: Theme.weightDemiBold
                        wrapMode: Text.Wrap
                    }
                    ActionButton {
                        compact: true
                        quiet: true
                        iconGlyph: Theme.iconArrowRight
                        text: qsTrId("genycaster.feed_header.explore")
                        onClicked: root.exploreRequested()
                    }
                }

                Repeater {
                    model: root.recentCasts
                    delegate: Superellipse {
                        required property var modelData
                        objectName: "homeRecentCastPreview"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 76
                        fillColor: recentHover.pressed ? Theme.surfacePressed : Theme.surface
                        borderWidth: 1
                        borderColor: Theme.borderMuted
                        curveDepth: Theme.curveContainer

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.space2
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2

                            Avatar {
                                avatarSize: 40
                                sourceUrl: String(modelData.avatarUrl || "")
                                displayName: String(modelData.name || "")
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(modelData.name || "")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(modelData.text || "")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                }
                            }
                            Icon {
                                text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                color: Theme.textTertiary
                                preferredSize: Theme.iconSizeSmall
                            }
                        }
                        MouseArea {
                            id: recentHover
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.castRequested(String(modelData.castId))
                        }
                    }
                }

                Superellipse {
                    visible: root.recentCasts.length === 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    fillColor: Theme.surface
                    borderWidth: 1
                    borderColor: Theme.borderMuted
                    curveDepth: Theme.curveContainer
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        Icon {
                            text: Theme.iconCompass
                            color: Theme.textSecondary
                            preferredSize: Theme.iconSizeLarge
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.feed_header.explore")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.typeBodySize
                        }
                        Icon {
                            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                            color: Theme.textTertiary
                            preferredSize: Theme.iconSizeSmall
                        }
                    }
                    TapHandler {
                        onTapped: root.exploreRequested()
                    }
                }
            }
        }
    }
}
