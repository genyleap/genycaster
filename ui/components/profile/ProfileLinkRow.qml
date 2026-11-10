import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Presentation-only public web metadata. The shared C++ resolver owns bounded
// requests, domain checks, cache and favicon image-provider validation.
Button {
    id: root
    required property string targetUrl
    property string userLabel: ""
    // Generic numbered placeholders should not hide a verified page title.
    property bool genericLabel: false
    property string displayUrl: ""
    // Closed profile-link drawers must not start background favicon requests.
    property bool mediaActive: true
    signal openRequested(string url)

    Layout.fillWidth: true
    flat: true
    padding: Theme.space2
    hoverEnabled: true
    property var browserMetadata: ({})
    readonly property var metadata: {
        const revision = linkPreviewResolver ? linkPreviewResolver.revision : 0
        const native = linkPreviewResolver ? linkPreviewResolver.previewFor(root.targetUrl) : ({})
        return root.browserMetadata.resolved
               && root.browserMetadata.providerKind === "web"
               ? Object.assign({}, native, root.browserMetadata) : native
    }
    property bool artworkFallback: false
    property int browserFaviconVariant: 0
    onTargetUrlChanged: {
        browserMetadata = ({})
        artworkFallback = false
        browserFaviconVariant = 0
    }
    Component.onCompleted: {
        if (Qt.platform.os === "wasm"
                && typeof browserLinkMetadataController !== "undefined")
            browserMetadata = browserLinkMetadataController.previewFor(root.targetUrl, "link")
    }
    Connections {
        target: typeof browserLinkMetadataController !== "undefined"
                ? browserLinkMetadataController : null
        function onPreviewReady(url, kind, preview) {
            if (kind !== "link" || String(url) !== root.targetUrl.split("#")[0])
                return
            root.browserMetadata = preview && preview.resolved && preview.providerKind === "web"
                                   ? preview : ({})
        }
    }
    readonly property string websiteTitle: String(metadata.siteName || metadata.title || "").trim()
    readonly property string description: String(metadata.summary || metadata.description || "").trim()
    readonly property string previewArtworkSource: String(metadata.previewImage || "")
    // WebAssembly cannot reliably fetch arbitrary site icons through the
    // native remote image provider. The first-party browser image gateway
    // explicitly allows bounded /favicon.ico downloads with public DNS checks,
    // raster validation and CORS; native platforms retain their provider path.
    readonly property string browserFaviconSource: {
        const match = /^https:\/\/([^\/?#:]+)/i.exec(root.targetUrl)
        if (!match) return ""
        const path = ["/favicon.ico", "/favicon.png",
                      "/apple-touch-icon.png"][root.browserFaviconVariant]
        // Some publishers only expose an ICO that the public raster gateway
        // cannot decode. Use their verified OpenGraph artwork as the last
        // resort instead of a permanent empty monogram.
        if (!path) return root.previewArtworkSource
        const favicon = "https://" + match[1] + path
        return "https://storage-farcaster.genyleap.com/v1/browser/images?url="
            + encodeURIComponent(favicon)
    }
    readonly property string faviconUrl: {
        // Prefer the site's actual icon; the OpenGraph poster is artwork,
        // not necessarily a legible 27px favicon. Fall back on raster failure.
        if (root.artworkFallback) return root.previewArtworkSource
        const declared = String(metadata.favicon || "")
        if (declared.length > 0) return declared
        const matched = /^https:\/\/([^\/?#:]+)/i.exec(root.targetUrl)
        return matched && linkPreviewResolver ? linkPreviewResolver.imageSourceForUrl(
            "https://" + matched[1] + "/favicon.ico") : root.previewArtworkSource
    }
    readonly property string siteInitial: {
        const matched = /^https:\/\/([^\/?#:]+)/i.exec(root.targetUrl)
        return matched ? matched[1].replace(/^www\./i, "").charAt(0).toUpperCase() : "W"
    }
    readonly property string rowTitle: root.genericLabel && root.websiteTitle.length > 0
                                       ? root.websiteTitle
                                       : (root.userLabel.length > 0 ? root.userLabel
                                          : (websiteTitle || qsTrId("genycaster.profile.website")))
    Accessible.name: rowTitle + " " + root.displayUrl
    onClicked: root.openRequested(root.targetUrl)

    background: Rectangle {
        radius: Theme.curveCompact
        color: root.down ? Theme.surfacePressed
              : root.hovered ? Theme.surfaceHover : Theme.surface
        border.color: root.hovered ? Theme.borderMuted : "transparent"
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
    contentItem: RowLayout {
        spacing: Theme.space3
        layoutDirection: Theme.rtl ? Qt.RightToLeft : Qt.LeftToRight
        LayoutMirroring.enabled: false

        Rectangle {
            Layout.preferredWidth: Theme.scaleUi(44)
            Layout.preferredHeight: Theme.scaleUi(44)
            radius: Theme.curveCompact
            color: Theme.primarySurface
            border.color: Theme.borderMuted
            border.width: 1
            // Always paint a legible site monogram underneath the bitmap.
            // Some sites serve a transparent/empty ICO which Qt reports Ready.
            // A fallback conditioned solely on Image.status leaves blank tiles.
            Text {
                anchors.centerIn: parent
                text: root.siteInitial
                color: Theme.ink
                font.family: Theme.latinFontFamily
                font.pixelSize: Theme.typeTitle3Size
                font.weight: Theme.weightBold
                visible: true
            }
            Image {
                id: faviconImage
                anchors.centerIn: parent
                width: Theme.scaleUi(27)
                height: width
                // A favicon is essential UI artwork, not an optional feed
                // download. Force the bounded remote image provider to admit it.
                source: !root.mediaActive ? ""
                    : Qt.platform.os === "wasm"
                      ? root.browserFaviconSource
                      : (root.faviconUrl.length > 0
                          ? dataStorage.remoteImageSource(root.faviconUrl, "avatars", true) : "")
                sourceSize.width: Math.ceil(width * 3)
                sourceSize.height: Math.ceil(height * 3)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                visible: status === Image.Ready && implicitWidth > 1 && implicitHeight > 1
                function tryArtworkFallback() {
                    // RemoteImageProvider deliberately returns a transparent
                    // 1px placeholder for decode/policy failures (Image.Ready).
                    const unavailable = status === Image.Error
                        || (status === Image.Ready
                            && implicitWidth <= 1 && implicitHeight <= 1)
                    if (Qt.platform.os === "wasm") {
                        if (unavailable && root.browserFaviconVariant < 3)
                            root.browserFaviconVariant++
                        return
                    }
                    if (root.artworkFallback || root.previewArtworkSource.length === 0)
                        return
                    if (unavailable)
                        root.artworkFallback = true
                }
                onStatusChanged: tryArtworkFallback()
                onImplicitWidthChanged: tryArtworkFallback()
                onImplicitHeightChanged: tryArtworkFallback()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.rowTitle
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                color: Theme.ink
                elide: Text.ElideRight
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: !root.genericLabel && root.userLabel.length > 0
                         && root.websiteTitle.length > 0
                         && root.websiteTitle !== root.userLabel
                text: root.websiteTitle
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkMuted
                maximumLineCount: 1
                elide: Text.ElideRight
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.displayUrl
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkMuted
                forceLeftToRight: true
                elide: Text.ElideMiddle
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                visible: root.description.length > 0
                text: root.description
                maximumLineCount: 2
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.inkSubtle
            }
        }
        Icon {
            text: Theme.iconArrowUpRight
            color: Theme.inkMuted
            preferredSize: 16
        }
    }
}
