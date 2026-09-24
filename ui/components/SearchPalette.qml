import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

Drawer {
    id: root

    property Item blurSourceItem: null
    property bool compact: false
    property var recentProfiles: []
    property var results: []
    property bool resultsBusy: false
    property string resultsErrorText: ""
    signal searchRequested(string query)
    signal suggestionsRequested(string query)
    signal suggestionsCleared()
    signal profileRequested(double fid)
    signal channelRequested(string channelId)
    readonly property string shortcutLabel: Qt.platform.os === "osx" ? "⌘K" : "Ctrl+K"

    // Relationship replies patch existing rows instead of rebuilding every
    // avatar/delegate for each asynchronous field update.
    function syncResults() {
        for (let index = 0; index < root.results.length; ++index) {
            const value = root.results[index] || ({})
            const key = String(value.kind || "") + ":" + String(value.fid || value.id || index)
            if (index >= resultRows.count)
                resultRows.append({ key: key, payload: value })
            else if (resultRows.get(index).key === key)
                resultRows.setProperty(index, "payload", value)
            else
                resultRows.set(index, { key: key, payload: value })
        }
        while (resultRows.count > root.results.length)
            resultRows.remove(resultRows.count - 1)
    }
    onResultsChanged: Qt.callLater(root.syncResults)
    Component.onCompleted: root.syncResults()
    ListModel { id: resultRows; dynamicRoles: true }

    function submit(query) {
        const value = String(query || searchField.text).trim()
        if (value.length === 0)
            return
        root.close()
        root.searchRequested(value)
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
    height: parent ? Math.min(root.compact ? 620 : 560, Math.max(320, parent.height * 0.72)) : 560
    x: parent ? (root.compact ? 0 : Math.round((parent.width - width) / 2)) : 0
    modal: true
    focus: true
    interactive: true
    leftPadding: root.compact ? Theme.mobileGutter : Theme.space5
    rightPadding: root.compact ? Theme.mobileGutter : Theme.space5
    topPadding: root.compact ? Theme.space3 : Theme.space5
    bottomPadding: root.compact ? Theme.space3 : Theme.space5
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: DrawerBackdrop {
        sourceItem: root.blurSourceItem
        active: root.opened
        addScrim: true
        blockInput: true
        onBackgroundTapped: root.close()
    }
    background: Superellipse {
        // A top drawer is attached to the window edge: square top corners avoid
        // the floating-card look while the lower edge keeps the soft glass shape.
        fillColor: Theme.dark ? Qt.rgba(0.055, 0.065, 0.085, 0.82)
                              : Qt.rgba(0.98, 0.985, 1.0, 0.86)
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
    onClosed: root.suggestionsCleared()

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
            Layout.preferredHeight: 48
            Layout.maximumHeight: 52
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
            Layout.preferredHeight: 62
            Layout.maximumHeight: 62
            Layout.alignment: Qt.AlignTop
            implicitHeight: 62
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
                    visible: searchField.text.length > 0
                    compact: true
                    quiet: true
                    iconOnly: true
                    iconGlyph: Theme.iconClose
                    text: qsTrId("genycaster.shell.close")
                    onClicked: {
                        searchField.clear()
                        searchField.forceActiveFocus()
                    }
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
            visible: searchField.text.trim().length >= 2 && (root.results.length > 0 || root.resultsErrorText.length > 0)
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
                        required property var payload
                        readonly property var modelData: payload || ({})
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
                                borderWidth: Theme.strokeHairline
                                borderColor: Theme.borderSubtle
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
                                    Layout.fillWidth: true
                                    text: qsTrId("genycaster.profile.value_followers").arg(
                                              LocaleFormat.formatInteger(Number(modelData.followerCount || 0)))
                                          + (resultRow.profileResult ? " · "
                                             + qsTrId("genycaster.profile.value_following").arg(
                                                 LocaleFormat.formatInteger(Number(modelData.followingCount || 0))) : "")
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
                                        root.channelRequested(channelId)
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
    }
}
