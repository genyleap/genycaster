import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Presentation-only public web metadata. The shared C++ resolver owns bounded
// requests, domain checks, cache and favicon image-provider validation.
Button {
    id: root
    required property string targetUrl
    property string userLabel: ""
    property string displayUrl: ""
    signal openRequested(string url)

    Layout.fillWidth: true
    flat: true
    padding: Theme.space2
    hoverEnabled: true
    property var browserMetadata: ({})
    readonly property var metadata: {
        const revision = linkPreviewResolver.revision
        const native = linkPreviewResolver.previewFor(root.targetUrl)
        return root.browserMetadata.resolved
               && root.browserMetadata.providerKind === "web"
               ? root.browserMetadata : native
    }
    onTargetUrlChanged: browserMetadata = ({})
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
    readonly property string faviconUrl: String(metadata.favicon || "")
    readonly property string rowTitle: root.userLabel.length > 0
                                       ? root.userLabel
                                       : (websiteTitle || qsTrId("genycaster.profile.website"))
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
            color: Theme.surfaceRaised
            border.color: Theme.borderMuted
            border.width: 1
            Image {
                id: faviconImage
                anchors.centerIn: parent
                width: Theme.scaleUi(27)
                height: width
                source: root.faviconUrl
                sourceSize.width: Math.ceil(width * 2)
                sourceSize.height: Math.ceil(height * 2)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
                visible: status === Image.Ready
            }
            Icon {
                anchors.centerIn: parent
                text: Theme.iconGlobe
                color: Theme.inkMuted
                preferredSize: 24
                visible: faviconImage.status !== Image.Ready
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
                visible: root.userLabel.length > 0
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
