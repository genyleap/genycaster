import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Superellipse {
    id: root

    property var embed: ({})
    property bool compact: false
    property bool scrollMoving: false

    readonly property string targetUrl: String(root.embed.url || root.embed.value || "")
    readonly property string rawArtworkSource: String(root.embed.imageSource || root.embed.artwork || root.embed.previewImage || "")
    readonly property string artworkSource: {
        const source = root.rawArtworkSource.trim()
        // Do not treat a marketplace HTML page as the artwork URL.
        if (source === root.targetUrl)
            return ""
        if (/^image:\/\/remote\//i.test(source) || /^qrc:\//i.test(source))
            return source
        if (/^https?:\/\//i.test(source))
            return linkPreviewResolver.imageSourceForUrl(source)
        return ""
    }
    readonly property string titleText: String(root.embed.title || root.embed.collection
                                               || qsTrId("genycaster.collectible.default_title"))
    readonly property string collectionText: String(root.embed.collection || "")
    readonly property string editionText: String(root.embed.edition || "")
    readonly property string chainText: String(root.embed.chain || "")
    readonly property string priceText: String(root.embed.price || root.embed.lastSale || "")
    readonly property string creatorText: String(root.embed.creator || root.embed.author || "")
    readonly property string providerText: root.marketplaceName()
    readonly property bool horizontalCard: !root.compact && width >= 560
    readonly property real captionLineHeight: Math.ceil(Theme.typeCaptionSize * Theme.typeCaptionLineHeight)
    readonly property real titleLineHeight: Math.ceil(root.compact
                                                     ? Theme.typeMetaSize * Theme.typeMetaLineHeight
                                                     : Theme.typeBodySize * Theme.typeBodyLineHeight)
    readonly property real cardPadding: root.compact ? Theme.space2 : Theme.space3
    // NFT artwork is the subject of the card, not a tiny decorative thumbnail.
    readonly property real artworkWidth: root.horizontalCard
                                         ? Math.min(360, Math.max(260, width * 0.39)) : width
    readonly property real artworkHeight: root.horizontalCard
                                          ? Math.min(340, Math.max(180, root.artworkWidth * 0.84))
                                          : Math.min(360, Math.max(152, width * 0.66))

    signal openRequested(string url)
    signal mediaRequested(var mediaItems, int index)


    function pointInsideArtwork(point) {
        if (!artworkFrame.visible)
            return false
        const local = artworkFrame.mapFromItem(root, point.x, point.y)
        return local.x >= 0 && local.y >= 0
                && local.x <= artworkFrame.width && local.y <= artworkFrame.height
    }

    function openArtwork() {
        if (!collectibleArtwork.ready) {
            if (root.targetUrl.length > 0)
                root.openRequested(root.targetUrl)
            return
        }
        root.mediaRequested([{
            kind: "image",
            url: root.targetUrl,
            imageSource: root.artworkSource,
            mediaCategory: "nfts"
        }], 0)
    }

    function marketplaceName() {
        const explicitName = String(root.embed.providerName || "").trim()
        if (explicitName.length > 0)
            return explicitName
        const provider = String(root.embed.providerKind || "").toLowerCase()
        if (provider === "superrare") return "SuperRare"
        if (provider === "objkt") return "objkt"
        if (provider === "transient") return "Transient"
        if (provider === "opensea") return "OpenSea"
        if (provider === "zora") return "Zora"
        if (provider === "foundation") return "Foundation"
        if (provider === "manifold") return "Manifold"
        if (provider === "rarible") return "Rarible"
        if (provider === "magiceden") return "Magic Eden"
        if (provider === "highlight") return "Highlight"
        if (provider === "mint") return "mint.fun"
        const domain = String(root.embed.domain || "")
        return domain.length > 0 ? domain : qsTrId("genycaster.collectible.marketplace")
    }

    Layout.fillWidth: true
    implicitHeight: cardLayout.implicitHeight + root.cardPadding * 2
    fillColor: !root.scrollMoving && collectibleHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
    borderWidth: Theme.strokeHairline
    borderColor: !root.scrollMoving && collectibleHover.hovered ? Theme.borderDefault : Theme.borderSubtle
    curveDepth: Theme.shapeMedia

    Behavior on fillColor {
        enabled: !root.scrollMoving
        ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
    }
    Behavior on borderColor {
        enabled: !root.scrollMoving
        ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
    }

    GridLayout {
        id: cardLayout
        anchors.fill: parent
        anchors.margins: root.cardPadding
        columns: root.horizontalCard ? 2 : 1
        columnSpacing: Theme.space3
        rowSpacing: Theme.space2
        layoutDirection: Theme.uiDirection

        Item {
            id: artworkFrame
            // Never hide artwork on a transient provider failure. Failure
            // state can clear on re-admission after kinetic scrolling.
            // Reserve the artwork region as soon as the collectible URL is
            // known, even before the marketplace resolves its image.
            visible: root.artworkSource.length > 0 && !Boolean(root.embed.pairedVisual)
            Layout.fillWidth: !root.horizontalCard
            Layout.preferredWidth: root.horizontalCard ? root.artworkWidth : -1
            Layout.preferredHeight: visible ? root.artworkHeight : 0
            Layout.alignment: Qt.AlignTop

            AmbientMediaBackdrop {
                anchors.fill: collectibleArtwork
                sourceItem: collectibleArtwork.renderItem
                active: collectibleArtwork.ready
                spread: 9
                strength: Theme.dark ? 0.62 : 0.50
                blurStrength: 1.0
                saturation: 0.0
                sampleSize: 80
            }

            SuperellipseImage {
                id: collectibleArtwork
                anchors.fill: parent
                source: root.artworkSource
                mediaCategory: "nfts"
                forceAutomaticDownload: true
                requestedSourceWidth: 520
                requestedSourceHeight: 360
                fillMode: Image.PreserveAspectFit
                fillColor: Theme.backgroundSecondary
                borderWidth: 0
                curveDepth: Theme.shapeMedia
                clipToSuperellipse: true
                showFailureIndicator: true
            }

            GenySpinner {
                anchors.centerIn: parent
                visible: root.artworkSource.length > 0 && collectibleArtwork.loading
                running: visible && !root.scrollMoving
            }

            Icon {
                anchors.centerIn: parent
                visible: root.artworkSource.length === 0 || collectibleArtwork.loadFailed
                text: Theme.iconImage
                color: Theme.textTertiary
                preferredSize: Theme.iconSizeLarge
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space2

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Superellipse {
                    implicitWidth: nftHeader.implicitWidth + Theme.space2 * 2
                    implicitHeight: 28
                    fillColor: Theme.primarySurface
                    borderWidth: 0
                    curveDepth: 14

                    RowLayout {
                        id: nftHeader
                        anchors.centerIn: parent
                        layoutDirection: Theme.uiDirection
                        spacing: 5

                        Label {
                            LayoutMirroring.enabled: false
                            text: qsTrId("genycaster.collectible.nft_badge")
                            color: Theme.accent
                            font.family: Theme.latinFontFamily
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightBold
                        }

                        DirectionAwareLabel {
                            text: root.providerText
                            color: Theme.textSecondary
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightMedium
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Icon {
                    visible: root.targetUrl.length > 0
                    text: Theme.iconArrowUpRight
                    color: Theme.textTertiary
                    preferredSize: Theme.iconSizeSmall
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                // The title may grow from one line to two once metadata lands.
                // Allocate both lines before the first presentation frame.
                Layout.preferredHeight: Math.min(root.titleLineHeight * 2, Math.max(root.titleLineHeight, implicitHeight))
                verticalAlignment: Text.AlignTop
                text: root.titleText
                color: Theme.textPrimary
                font.pixelSize: root.compact ? Theme.typeMetaSize : Theme.typeBodySize
                font.weight: Theme.weightDemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                visible: root.creatorText.length > 0
                Layout.preferredHeight: visible ? root.captionLineHeight : 0
                Layout.fillWidth: true
                text: root.creatorText
                color: Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                visible: root.collectionText.length > 0 && root.collectionText !== root.titleText
                Layout.preferredHeight: visible ? root.captionLineHeight : 0
                Layout.fillWidth: true
                text: root.collectionText !== root.titleText ? root.collectionText : ""
                color: Theme.textTertiary
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }

            Flow {
                id: metadataChips
                Layout.fillWidth: true
                visible: root.chainText.length > 0 || root.editionText.length > 0 || root.priceText.length > 0
                Layout.preferredHeight: visible ? 28 : 0
                Layout.minimumHeight: Layout.preferredHeight
                Layout.maximumHeight: Layout.preferredHeight
                layoutDirection: Theme.uiDirection
                spacing: Theme.space1

                Repeater {
                    model: [root.chainText, root.editionText, root.priceText]

                    delegate: Superellipse {
                        required property string modelData
                        visible: modelData.length > 0
                        implicitWidth: Math.min(Math.max(1, (metadataChips.width - Theme.space1 * 2) / 3),
                                                chipText.implicitWidth + Theme.space2 * 2)
                        implicitHeight: 28
                        fillColor: Theme.surfaceMuted
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.borderSubtle
                        curveDepth: 14

                        Label {
                            id: chipText
                            anchors.centerIn: parent
                            width: Math.max(1, parent.width - Theme.space2 * 2)
                            elide: Text.ElideRight
                            LayoutMirroring.enabled: false
                            text: modelData
                            color: Theme.textSecondary
                            font.family: Theme.fontFamilyForText(text)
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightMedium
                        }
                    }
                }
            }

            Item {
                visible: root.horizontalCard
                Layout.fillHeight: true
                Layout.minimumHeight: visible ? Theme.space1 : 0
            }

            ActionButton {
                visible: root.targetUrl.length > 0
                Layout.fillWidth: true
                compact: true
                iconGlyph: Theme.iconArrowUpRight
                text: qsTrId("genycaster.embed.open_link")
                onClicked: root.openRequested(root.targetUrl)
            }
        }
    }

    HoverHandler {
        id: collectibleHover
        enabled: !root.scrollMoving
        cursorShape: root.targetUrl.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        enabled: root.targetUrl.length > 0 || root.artworkSource.length > 0
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: eventPoint => {
            if (root.pointInsideArtwork(eventPoint.position)) {
                root.openArtwork()
                return
            }
            if (root.targetUrl.length > 0)
                root.openRequested(root.targetUrl)
        }
    }

    Accessible.role: Accessible.Link
    Accessible.name: qsTrId("genycaster.collectible.accessible_name").arg(root.titleText)
}
