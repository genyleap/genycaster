import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Bounded, swipeable discovery shelf. All entries originate from the normalized
// catalogue; neither promotional artwork nor recommendation rank is invented.
Item {
    id: root

    property var apps: []
    property bool compact: false
    property string heading: qsTrId("genycaster.side_nav.explorer")
    readonly property int cardCount: Math.min(8, apps ? apps.length : 0)
    property int shownIndex: 0
    readonly property real cardWidth: compact
                                      ? Math.min(370, Math.max(236, width * 0.86))
                                      : Math.min(430, Math.max(285, width * 0.52))
    signal openRequested(var app)
    signal saveRequested(var app)

    function navigateTo(index) {
        if (cardCount < 2) return
        const chosen = Math.max(0, Math.min(cardCount - 1, index))
        const stride = root.cardWidth + deck.spacing
        const maxX = Math.max(0, deck.contentWidth - deck.width)
        const targetX = Math.min(maxX, chosen * stride)
        slideAnimation.stop()
        // The single authoritative index is shared by arrows and gestures.
        root.shownIndex = chosen
        if (Theme.reduceMotion) deck.contentX = targetX
        else {
            slideAnimation.from = deck.contentX
            slideAnimation.to = targetX
            slideAnimation.start()
        }
    }
    onAppsChanged: {
        if (root.cardCount === 0) root.shownIndex = 0
        else root.shownIndex = Math.min(root.shownIndex, root.cardCount - 1)
        Qt.callLater(function() {
            if (!deck.dragging && !deck.moving && root.cardCount > 0)
                deck.contentX = Math.min(Math.max(0, deck.contentWidth - deck.width),
                                         root.shownIndex * (root.cardWidth + deck.spacing))
        })
    }

    visible: cardCount > 0
    implicitHeight: visible ? (compact ? 272 : 303) : 0

    RowLayout {
        id: shelfHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        spacing: Theme.space2
        layoutDirection: Theme.uiDirection

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.heading
            color: Theme.ink
            font.pixelSize: Theme.typeTitle3Size
            font.weight: Theme.weightBold
        }
        DirectionAwareLabel {
            text: String(root.cardCount > 0 ? root.shownIndex + 1 : 0) + " / " + String(root.cardCount)
            color: Theme.inkMuted
            font.pixelSize: Theme.typeCaptionSize
            forceLeftToRight: true
        }
        ActionButton {
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconArrowLeft
            text: qsTr("Previous app")
            enabled: root.shownIndex > 0
            onClicked: root.navigateTo(root.shownIndex - 1)
        }
        ActionButton {
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconArrowRight
            text: qsTr("Next app")
            enabled: root.shownIndex < root.cardCount - 1
            onClicked: root.navigateTo(root.shownIndex + 1)
        }
    }

    ListView {
        id: deck
        anchors.top: shelfHeader.bottom
        anchors.topMargin: Theme.space2
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true
        orientation: ListView.Horizontal
        layoutDirection: Qt.LeftToRight
        spacing: Theme.space3
        model: root.cardCount
        snapMode: ListView.SnapOneItem
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3000
        // Retain both adjacent cards so a brief swipe backward does not
        // recreate its image provider and flash the square-icon fallback.
        cacheBuffer: root.cardWidth * 2 + spacing * 2
        interactive: root.cardCount > 1
        // Qt's ListView.currentIndex tracks selection, not the card visible
        // after a finger fling or positionViewAtIndex. Derive the counter from
        // the actual content offset, including kinetic motion and RTL swipes.
        onContentXChanged: {
            if (slideAnimation.running) return
            const stride = root.cardWidth + spacing
            if (stride > 0 && root.cardCount > 0)
                root.shownIndex = Math.max(0, Math.min(root.cardCount - 1,
                                                     Math.round(contentX / stride)))
        }
        onDragStarted: slideAnimation.stop()
        onMovementEnded: {
            if (!slideAnimation.running && root.cardCount > 0)
                root.navigateTo(Math.round(contentX / (root.cardWidth + spacing)))
        }

        NumberAnimation {
            id: slideAnimation
            target: deck
            property: "contentX"
            duration: Theme.reduceMotion ? 0 : Theme.motionStandard
            easing.type: Easing.OutCubic
        }

        delegate: Item {
            id: featuredCell
            required property int index
            readonly property var app: root.apps[index] || ({})
            width: root.cardWidth
            height: deck.height

            Superellipse {
                id: featureCard
                anchors.fill: parent
                clip: true
                fillColor: Theme.surfaceRaised
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderMuted
                curveDepth: Theme.curveContainer

                SuperellipseImage {
                    id: cover
                    anchors.fill: parent
                    source: String(featuredCell.app.coverSource || "")
                    mediaCategory: "miniapps"
                    forceAutomaticDownload: true
                    deferNewLoadsWhileScrolling: false
                    // Keep requests staggered rather than decoding several
                    // large banner textures on the same Safari render frame.
                    loadDelayMs: Math.min(160, featuredCell.index * 35)
                    showLoadingIndicator: false
                    showFailureIndicator: false
                    showManualLoadIndicator: false
                    fillMode: Image.PreserveAspectCrop
                    // Wide Qt/WASM textures can paint blank with MultiEffect.
                    // The card itself clips the guaranteed plain-image path.
                    clipToSuperellipse: false
                    requestedSourceWidth: Math.min(640, Math.ceil(width * 1.4))
                    requestedSourceHeight: Math.min(440, Math.ceil(height * 1.4))
                }

                Item {
                    anchors.centerIn: parent
                    width: 88
                    height: 88
                    visible: !cover.ready
                    SuperellipseImage {
                        id: fallbackArtwork
                        anchors.fill: parent
                        source: String(featuredCell.app.iconSource || "")
                        mediaCategory: "miniapps"
                        forceAutomaticDownload: true
                        showLoadingIndicator: false
                        showFailureIndicator: false
                        showManualLoadIndicator: false
                        fillMode: Image.PreserveAspectFit
                        requestedSourceWidth: 128
                        requestedSourceHeight: 128
                        curveDepth: Theme.curveControl
                        fillColor: Theme.surfaceRaised
                    }
                    Icon {
                        anchors.centerIn: parent
                        visible: !fallbackArtwork.ready
                        text: Theme.iconApps
                        preferredSize: 36
                        color: Theme.accent
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Math.min(98, parent.height * 0.45)
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#22000000" }
                        GradientStop { position: 0.48; color: "#bb101119" }
                        GradientStop { position: 1.0; color: "#e9101119" }
                    }
                }

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.space3
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    Item {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        SuperellipseImage {
                            id: badgeArtwork
                            anchors.fill: parent
                            source: String(featuredCell.app.iconSource || "")
                            mediaCategory: "miniapps"
                            forceAutomaticDownload: true
                            showLoadingIndicator: false
                            showFailureIndicator: false
                            showManualLoadIndicator: false
                            fillMode: Image.PreserveAspectFit
                            requestedSourceWidth: 64
                            requestedSourceHeight: 64
                            curveDepth: Theme.curveControl
                            fillColor: Theme.surfaceRaised
                        }
                        Icon {
                            anchors.centerIn: parent
                            visible: !badgeArtwork.ready
                            text: Theme.iconApps
                            preferredSize: 24
                            color: Theme.accent
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: String(featuredCell.app.name || featuredCell.app.domain || "")
                            color: "#ffffff"
                            font.pixelSize: Theme.typeBodySize
                            font.weight: Theme.weightBold
                            elide: Text.ElideRight
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: String(featuredCell.app.description || featuredCell.app.domain || "")
                            color: "#e4e5eb"
                            font.pixelSize: Theme.typeCaptionSize
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                        }
                    }
                    ActionButton {
                        compact: true
                        prominent: true
                        iconOnly: true
                        iconGlyph: Theme.iconArrowRight
                        text: qsTrId("genycaster.miniapp.open_internal")
                        onClicked: root.openRequested(featuredCell.app)
                    }
                }

                ActionButton {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space2
                    compact: true
                    iconOnly: true
                    quiet: !Boolean(featuredCell.app.saved)
                    positive: Boolean(featuredCell.app.saved)
                    iconGlyph: Boolean(featuredCell.app.saved) ? Theme.iconCheck : Theme.iconPlus
                    text: Boolean(featuredCell.app.saved)
                          ? qsTrId("genycaster.apps.saved") : qsTrId("genycaster.apps.save")
                    enabled: !Boolean(featuredCell.app.saved)
                    onClicked: root.saveRequested(featuredCell.app)
                }

                // The open action is explicit: a card-wide TapHandler would
                // also intercept the Save button and accidentally launch.
                Accessible.role: Accessible.Button
                Accessible.name: String(featuredCell.app.name || "")
            }
        }
    }
}
