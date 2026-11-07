import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Farcaster.Client

Drawer {
    id: root

    property Item blurSourceItem: null
    property bool compact: false
    property var recentProfiles: []
    property var results: []
    property bool resultsBusy: false
    property string resultsErrorText: ""
    property var resultPayloads: ({})
    readonly property string searchScope: {
        const query = String(searchField.text || '').trim()
        if (/^(?:channel:|\/|#)/i.test(query)) return 'channel'
        if (/^@/.test(query)) return 'profile'
        if (/^cast:/i.test(query) || /^0x[0-9a-f]{18,}$/i.test(query)) return 'cast'
        return 'all'
    }
    readonly property int filteredCount: resultRows.count
    onSearchScopeChanged: Qt.callLater(root.syncResults)
    signal searchRequested(string query)
    signal suggestionsRequested(string query)
    signal suggestionsCleared()
    signal profileRequested(double fid)
    signal channelRequested(string channelId, var channelInfo)
    readonly property string shortcutLabel: Qt.platform.os === "osx" ? "⌘K" : "Ctrl+K"

    // Relationship replies patch existing rows instead of rebuilding every
    // avatar/delegate for each asynchronous field update.
    function syncResults() {
        const payloads = ({})
        let rowIndex = 0
        // A cleared query must not rehydrate stale search results from the
        // previous asynchronous bridge snapshot.
        const activeQuery = searchField.text.trim().length >= 2
        for (let index = 0; activeQuery && index < root.results.length; ++index) {
            const value = root.results[index] || ({})
            const kind = String(value.kind || '')
            if (root.searchScope !== 'all' && kind !== root.searchScope)
                continue
            const key = kind + ':' + String(value.fid || value.id || index)
            payloads[key] = value
            if (rowIndex >= resultRows.count)
                resultRows.append({ key: key })
            else if (resultRows.get(rowIndex).key !== key)
                resultRows.set(rowIndex, { key: key })
            rowIndex++
        }
        while (resultRows.count > rowIndex)
            resultRows.remove(resultRows.count - 1)
        root.resultPayloads = payloads
    }
    onResultsChanged: Qt.callLater(root.syncResults)
    Component.onCompleted: root.syncResults()
    ListModel { id: resultRows }

    function submit(query) {
        const value = String(query || searchField.text).trim()
        if (value.length === 0)
            return
        root.close()
        root.searchRequested(value)
    }

    function clearResults() {
        searchDebounce.stop()
        searchField.clear()
        resultRows.clear()
        root.resultPayloads = ({})
        root.suggestionsCleared()
        searchField.forceActiveFocus()
    }

    function useScope(prefix) {
        const value = String(searchField.text || "").trim()
        searchField.text = String(prefix || "") + value.replace(/^(@|#|channel:|cast:)\s*/i, "")
        searchField.cursorPosition = searchField.text.length
        searchField.forceActiveFocus()
    }

    parent: Overlay.overlay
    edge: Qt.TopEdge
    readonly property real outerInset: Theme.space3
    // Mobile search is a true edge-attached drawer. The drawer itself spans the
    // viewport; only its content receives the safe horizontal inset.
    width: parent ? (root.compact ? parent.width
                                  : Math.min(720, Math.max(1, parent.width - root.outerInset * 2))) : 720
    // The mobile panel follows useful content, not a fixed 620px sheet.
    // With a virtual keyboard open, empty space must not cover half the feed.
    height: parent ? (root.compact
        ? Math.min(Math.max(180, parent.height - Theme.space4),
                   searchField.text.trim().length >= 2
                   ? (root.filteredCount > 0 ? 470 : 225)
                   : (root.recentProfiles.length > 0 ? 420 : 300))
        // Keep an empty command palette compact, not a 560px empty panel.
        // Result lists get extra height only when there are actual rows.
        : Math.min(parent.height * 0.76,
            root.filteredCount > 0 ? Math.min(540, 224 + root.filteredCount * 86)
            : root.recentProfiles.length > 0 && searchField.text.trim().length < 2
              ? 400 : 250)) : 400
    x: parent ? (root.compact ? 0 : Math.round((parent.width - width) / 2)) : 0
    modal: true
    focus: true
    // Dragging the entire search panel steals selection/scroll gestures.
    interactive: !root.compact
    leftPadding: root.compact ? Theme.mobileGutter : Theme.space5
    rightPadding: root.compact ? Theme.mobileGutter : Theme.space5
    // Qt's popup overlay spans the window rather than ApplicationWindow's
    // safe content item. Respect the system status area on notched phones;
    // on Android some window managers report zero after implicit padding.
    readonly property real statusInset: root.compact
        ? Math.max(Qt.platform.os === "android" ? 24 : 0,
                   parent ? parent.SafeArea.margins.top : 0,
                   // Read the OS inset from the Window itself; Qt may already
                   // have padded its ApplicationWindow contentItem.
                   parent && parent.Window.window
                       ? parent.Window.window.SafeArea.margins.top : 0,
                   // The overlay/content item may also have custom safe insets.
                   parent && parent.Window.contentItem
                       ? parent.Window.contentItem.SafeArea.margins.top : 0) : 0
    topPadding: (root.compact ? Theme.space3 : Theme.space5) + root.statusInset
    bottomPadding: root.compact ? Theme.space2 : Theme.space5
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // Match the common drawer's interruptible edge-motion contract. Animate
    // Drawer.position (not x/y, which Qt owns), respecting reduced motion.
    enter: Transition {
        NumberAnimation {
            property: "position"
            from: 0.0
            to: 1.0
            duration: Theme.motionStandard
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "position"
            to: 0.0
            duration: Theme.motionStandard
            easing.type: Easing.InOutCubic
        }
    }

    Overlay.modal: DrawerBackdrop {
        sourceItem: root.blurSourceItem
        active: root.position > 0
        opacity: root.position
        snapshotBlurOnBrowser: true
        addScrim: true
        blockInput: true
        onBackgroundTapped: root.close()
    }
    background: Superellipse {
        // A top drawer is attached to the window edge: square top corners avoid
        // the floating-card look while the lower edge keeps the soft glass shape.
        fillColor: Theme.dark ? Qt.rgba(0.055, 0.065, 0.085, 0.96)
                              : Qt.rgba(0.98, 0.985, 1.0, 0.97)
        curveDepth: Theme.curveModal
        curveTopLeft: false
        curveTopRight: false
        borderWidth: Theme.strokeHairline
        borderColor: Theme.dark ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0.08, 0.10, 0.15, 0.12)
    }

    onOpened: {
        searchField.text = ""
        root.suggestionsCleared()
        searchField.forceActiveFocus()
    }
    onClosed: {
        searchDebounce.stop()
        searchField.focus = false
        root.suggestionsCleared()
    }

    Timer {
        id: searchDebounce
        interval: 220
        repeat: false
        onTriggered: {
            const value = searchField.text.trim()
            if (value.length >= 2)
                root.suggestionsRequested(value)
        }
    }

    contentItem: ColumnLayout {
        spacing: root.compact ? Theme.space2 : Theme.space3

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 44 : 48
            Layout.maximumHeight: root.compact ? 48 : 52
            Layout.alignment: Qt.AlignTop
            layoutDirection: Theme.uiDirection
            spacing: Theme.space3

            Superellipse {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                fillColor: Theme.primarySurface
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveControl
                Icon { anchors.centerIn: parent; text: Theme.iconSearch; color: Theme.accent; preferredSize: Theme.iconSizeLarge }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feed_header.universal_search")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feed_header.search_hint")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }

            // Mobile users must always have a visible exit target; outside
            // taps can be intercepted by the browser's virtual keyboard.
            ActionButton {
                objectName: "closeSearchPalette"
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconClose
                text: qsTrId("genycaster.shell.close")
                onClicked: root.close()
            }

            Superellipse {
                visible: root.width >= 520
                implicitWidth: shortcut.implicitWidth + Theme.space3
                implicitHeight: 28
                fillColor: Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderMuted
                curveDepth: 14
                exponent: 2.01
                Label {
                    id: shortcut
                    anchors.centerIn: parent
                    text: root.shortcutLabel
                    color: Theme.textTertiary
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                }
            }
        }

        Superellipse {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 54 : 62
            Layout.maximumHeight: root.compact ? 54 : 62
            Layout.alignment: Qt.AlignTop
            implicitHeight: root.compact ? 54 : 62
            fillColor: Theme.editorSurface
            borderWidth: searchField.activeFocus ? 2 : Theme.strokeHairline
            borderColor: searchField.activeFocus ? Theme.focus : Theme.borderMuted
            curveDepth: Theme.curveControl

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space2
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Icon {
                    text: Theme.iconSearch
                    color: searchField.activeFocus ? Theme.accent : Theme.textTertiary
                    preferredSize: Theme.iconSizeLarge
                }

                DirectionAwareTextField {
                    id: searchField
                    Layout.fillWidth: true
                    background: null
                    font.pixelSize: Theme.typeBodySize
                    placeholderText: qsTrId("genycaster.feed_header.search_people_channels_casts")
                    onAccepted: root.submit(text)
                    onTextChanged: {
                        searchDebounce.stop()
                        root.suggestionsCleared()
                        root.syncResults()
                        if (text.trim().length >= 2)
                            searchDebounce.restart()
                    }
                }

                GenySpinner {
                    visible: root.resultsBusy
                    running: visible
                    implicitWidth: 28
                    implicitHeight: 28
                }

                ActionButton {
                    objectName: "clearSearchResults"
                    visible: searchField.text.length > 0
                             || root.filteredCount > 0
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.search.clear_results")
                    onClicked: root.clearResults()
                }

                ActionButton {
                    compact: true
                    prominent: true
                    iconOnly: true
                    iconGlyph: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                    text: qsTrId("genycaster.feed_header.universal_search")
                    enabled: searchField.text.trim().length > 0
                    onClicked: root.submit(searchField.text)
                }
            }
        }

        ScrollView {
            visible: searchField.text.trim().length >= 2 && (root.filteredCount > 0 || root.resultsErrorText.length > 0)
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: visible ? 120 : 0
            Layout.alignment: Qt.AlignTop
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            id: resultViewport

            ColumnLayout {
                id: resultsColumn
                width: resultViewport.availableWidth
                spacing: Theme.space1

                Repeater {
                    model: resultRows
                    delegate: Item {
                        id: resultRow
                        required property string key
                        readonly property var modelData: root.resultPayloads[resultRow.key] || ({})
                        readonly property string kind: String(modelData.kind || "")
                        readonly property bool profileResult: kind === "profile"
                        Layout.fillWidth: true
                        implicitHeight: resultDetails.implicitHeight + Theme.space3 * 2

                        Superellipse {
                            anchors.fill: parent
                            fillColor: resultHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                            borderWidth: resultHover.hovered ? Theme.strokeHairline : 0
                            borderColor: Theme.borderSubtle
                            curveDepth: Theme.curveCompact
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space2
                            anchors.rightMargin: Theme.space2
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2

                            Avatar {
                                visible: resultRow.profileResult
                                avatarSize: 38
                                sourceUrl: String(modelData.avatarUrl || "")
                                displayName: String(modelData.displayName || modelData.username || "")
                            }

                            SuperellipseImage {
                                visible: !resultRow.profileResult && String(modelData.imageUrl || "").length > 0
                                Layout.preferredWidth: visible ? 38 : 0
                                Layout.preferredHeight: visible ? 38 : 0
                                source: String(modelData.imageUrl || "")
                                mediaCategory: "photos"
                                fillMode: Image.PreserveAspectCrop
                                fillColor: Theme.primarySurface
                                borderWidth: 0
                                curveDepth: 11
                                exponent: 3.8
                            }

                            Superellipse {
                                visible: !resultRow.profileResult && String(modelData.imageUrl || "").length === 0
                                Layout.preferredWidth: visible ? 38 : 0
                                Layout.preferredHeight: visible ? 38 : 0
                                fillColor: Theme.primarySurface
                                borderWidth: Theme.strokeHairline
                                borderColor: Theme.borderSubtle
                                curveDepth: 11
                                exponent: 3.8
                                Icon { anchors.centerIn: parent; text: Theme.iconGlobe; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
                            }

                            ColumnLayout {
                                id: resultDetails
                                Layout.fillWidth: true
                                spacing: 0
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: resultRow.profileResult
                                          ? String(modelData.displayName || modelData.username || "")
                                          : String(modelData.name || modelData.id || "")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeMetaSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                                Label {
                                    Layout.fillWidth: true
                                    LayoutMirroring.enabled: false
                                    text: resultRow.profileResult
                                          ? "@" + String(modelData.username || "")
                                          : "/" + String(modelData.id || "")
                                    color: Theme.textTertiary
                                    font.family: Theme.latinFontFamily
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Text.AlignLeft
                                    elide: Text.ElideRight
                                }

                                DirectionAwareLabel {
                                    visible: resultRow.profileResult || Number(modelData.followerCount || 0) > 0
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.profile.value_followers").arg(
                                              modelData.socialCountsKnown === false
                                              ? "—" : LocaleFormat.formatInteger(Number(modelData.followerCount || 0)))
                                          + (resultRow.profileResult ? " · "
                                             + qsTrId("genycaster.profile.value_following").arg(
                                                 modelData.socialCountsKnown === false
                                                 ? "—" : LocaleFormat.formatInteger(Number(modelData.followingCount || 0))) : "")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                }
                                DirectionAwareLabel {
                                    visible: resultRow.profileResult && presentationBridge.isAuthenticated
                                             && Number(modelData.fid || 0) !== presentationBridge.accountFid
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Theme.typeCaptionSize * 3
                                    Layout.maximumHeight: Theme.typeCaptionSize * 3
                                    text: (modelData.followingKnown
                                           ? (modelData.followingByMe ? qsTrId("genycaster.profile.followed")
                                                                     : qsTrId("genycaster.search.not_following"))
                                           : qsTrId("genycaster.search.relationship_unknown"))
                                          + " · " + (modelData.followsMeKnown
                                           ? (modelData.followsMe ? qsTrId("genycaster.profile.follows_you")
                                                                 : qsTrId("genycaster.search.not_followed_by"))
                                           : qsTrId("genycaster.search.relationship_unknown"))
                                    color: modelData.followingByMe || modelData.followsMe ? Theme.accent : Theme.textTertiary
                                    font.pixelSize: Theme.typeCaptionSize
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                                DirectionAwareLabel {
                                    visible: String(modelData.bio || modelData.description || "").length > 0
                                    Layout.fillWidth: true
                                    text: String(modelData.bio || modelData.description || "")
                                    color: Theme.textTertiary
                                    font.pixelSize: Theme.typeCaptionSize
                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                            }

                            Icon {
                                text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                color: resultHover.hovered ? Theme.accent : Theme.textTertiary
                                preferredSize: Theme.iconSizeSmall
                            }
                        }

                        HoverHandler { id: resultHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: {
                                if (resultRow.profileResult) {
                                    const fid = Number(modelData.fid || 0)
                                    if (fid > 0) {
                                        root.close()
                                        root.profileRequested(fid)
                                    }
                                } else {
                                    const channelId = String(modelData.id || "")
                                    if (channelId.length > 0) {
                                        root.close()
                                        root.channelRequested(channelId, modelData)
                                    }
                                }
                            }
                        }
                    }
                }

                DirectionAwareLabel {
                    visible: root.results.length === 0 && root.resultsErrorText.length > 0
                    Layout.fillWidth: true
                    text: root.resultsErrorText
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            visible: searchField.text.trim().length < 2
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space1

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.search.shortcuts")
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconProfile
                    text: qsTrId("genycaster.search.profile")
                    onClicked: root.useScope("@")
                }

                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconGlobe
                    text: qsTrId("genycaster.search.channel")
                    onClicked: root.useScope("channel:")
                }

                ActionButton {
                    Layout.fillWidth: true
                    compact: true
                    quiet: true
                    iconGlyph: Theme.iconMessage
                    text: qsTrId("genycaster.search.cast")
                    onClicked: root.useScope("cast:")
                }
            }
        }

        DirectionAwareLabel {
            visible: searchField.text.trim().length >= 2 && !root.resultsBusy
                     && root.results.length > 0 && root.filteredCount === 0
                     && root.searchScope !== "all"
            Layout.fillWidth: true
            text: root.searchScope === "channel"
                  ? qsTrId("genycaster.search.no_channel_results")
                  : qsTrId("genycaster.search.no_profile_results")
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.WordWrap
        }

        ScrollView {
            id: suggestionViewport
            visible: searchField.text.trim().length < 2 && root.recentProfiles.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: visible
            Layout.minimumHeight: visible ? 96 : 0
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: suggestionViewport.availableWidth
                spacing: Theme.space1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.context.suggested_for_you")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                }

                Repeater {
                    model: root.recentProfiles
                    delegate: Item {
                        id: suggestion
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 52

                        Superellipse {
                            anchors.fill: parent
                            fillColor: suggestionHover.hovered ? Theme.surfaceHover : "transparent"
                            borderWidth: suggestionHover.hovered ? Theme.strokeHairline : 0
                            borderColor: Theme.borderSubtle
                            curveDepth: Theme.curveCompact
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space2
                            anchors.rightMargin: Theme.space2
                            layoutDirection: Theme.uiDirection
                            spacing: Theme.space2

                            Avatar {
                                avatarSize: 36
                                sourceUrl: String(modelData.avatarUrl || "")
                                displayName: String(modelData.displayName || modelData.username || "")
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(modelData.displayName || modelData.username || "")
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.typeMetaSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    LayoutMirroring.enabled: false
                                    text: "@" + String(modelData.username || "")
                                    color: Theme.textTertiary
                                    font.family: Theme.latinFontFamily
                                    font.pixelSize: Theme.typeCaptionSize
                                    horizontalAlignment: Text.AlignLeft
                                    elide: Text.ElideRight
                                }
                            }

                            Icon {
                                text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
                                color: suggestionHover.hovered ? Theme.accent : Theme.textTertiary
                                preferredSize: Theme.iconSizeSmall
                            }
                        }

                        HoverHandler { id: suggestionHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: root.submit(String(modelData.username || modelData.displayName || ""))
                        }
                    }
                }
            }
        }

        // With no results the old ColumnLayout centered the search input in
        // the entire drawer. Keep heading and search input pinned to the top;
        // reserve flexible space only *below* them.
        Item {
            objectName: "searchEmptyBottomSpace"
            Layout.fillWidth: true
            Layout.fillHeight: visible
            Layout.minimumHeight: 0
            visible: !resultViewport.visible && !suggestionViewport.visible
        }
    }
}
