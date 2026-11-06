import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    objectName: "mediaGallery"

    property var items: []
    property int requestedIndex: 0
    readonly property int itemCount: root.items ? root.items.length : 0
    readonly property int currentIndex: gallery.currentIndex

    function sourceFor(item) {
        if (!item)
            return ""
        const direct = String(item.imageSource || item.source || "")
        if (direct.length > 0)
            return direct
        const target = String(item.url || item.value || "")
        if (typeof linkPreviewResolver !== "undefined" && linkPreviewResolver !== null)
            return linkPreviewResolver.imageSourceForUrl(target)
        return target
    }

    function openAt(mediaItems, index) {
        root.items = mediaItems || []
        root.requestedIndex = Math.max(0, Math.min(root.itemCount - 1, Number(index || 0)))
        gallery.currentIndex = root.requestedIndex
        root.open()
    }

    function previous() {
        if (gallery.currentIndex > 0)
            gallery.decrementCurrentIndex()
    }

    function next() {
        if (gallery.currentIndex + 1 < root.itemCount)
            gallery.incrementCurrentIndex()
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 1000
    height: parent ? parent.height : 720
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle { color: Theme.mediaViewerModal }
    background: Rectangle { color: Theme.mediaViewerBackground }

    enter: Transition {
        ParallelAnimation {
            OpacityAnimator {
                from: 0
                to: 1
                duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                easing.type: Easing.OutCubic
            }
            ScaleAnimator {
                from: 0.985
                to: 1
                duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                easing.type: Easing.OutCubic
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            OpacityAnimator {
                from: 1
                to: 0
                duration: Theme.reduceMotion ? 0 : Theme.motionFast
            }
            ScaleAnimator {
                from: 1
                to: 0.992
                duration: Theme.reduceMotion ? 0 : Theme.motionFast
            }
        }
    }

    onOpened: {
        gallery.currentIndex = root.requestedIndex
        gallery.forceActiveFocus()
    }

    contentItem: Item {
        focus: true

        // The media stage owns the complete popup viewport. Viewer chrome is
        // composited above the image instead of reserving top/bottom layout
        // bands, so opening a cast image always produces a true full-screen
        // gallery composition.
        SwipeView {
            id: gallery
            anchors.fill: parent
            interactive: root.itemCount > 1
            clip: true

            Repeater {
                id: mediaRepeater
                model: root.items

                delegate: Item {
                    id: mediaPage
                    required property int index
                    required property var modelData
                    readonly property rect paintedRect: mediaPageLoader.item !== null
                                                        ? mediaPageLoader.item.paintedRect
                                                        : Qt.rect(0, 0, width, height)

                    // Full-screen decodes are expensive. Keep the current page
                    // and one neighbor on either side warm for swipe continuity,
                    // but do not allocate full-resolution textures for every
                    // gallery item at once.
                    Loader {
                        id: mediaPageLoader
                        anchors.fill: parent
                        active: Math.abs(mediaPage.index - gallery.currentIndex) <= 1

                        sourceComponent: Component {
                            Item {
                                readonly property rect paintedRect: fullImage.status === Image.Ready
                                                                    ? Qt.rect((width - fullImage.paintedWidth) / 2,
                                                                              (height - fullImage.paintedHeight) / 2,
                                                                              fullImage.paintedWidth,
                                                                              fullImage.paintedHeight)
                                                                    : Qt.rect(0, 0, width, height)

                                Image {
                                    id: fullImage
                                    anchors.fill: parent
                                    source: dataStorage.remoteImageSource(root.sourceFor(mediaPage.modelData),
                                                                          String(mediaPage.modelData.mediaCategory || "photos"),
                                                                          true)
                                    readonly property int decodeEdge: Math.max(1, Math.round(
                                        Math.max(width, height) * Screen.devicePixelRatio
                                        * renderingController.mediaDecodeScale))
                                    // Bound the decoded texture to the physical viewport while
                                    // preserving source aspect in the provider. Qt documents images
                                    // as one of the largest QML memory consumers, so full-resolution
                                    // source pixels are not retained when the window cannot display them.
                                    sourceSize.width: decodeEdge
                                    sourceSize.height: decodeEdge
                                    // A media viewer is a content surface, not a decorative cover.
                                    // PreserveAspectFit guarantees the complete source is visible and
                                    // never crops user media. Letterbox space belongs to the viewer
                                    // background and the floating chrome stays over the painted image.
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    // RemoteImageProvider/DataStorage own the bounded media
                                    // cache. Avoid retaining a second full-screen QML image
                                    // cache entry for every gallery visit.
                                    cache: false
                                    smooth: true
                                    mipmap: false
                                    opacity: status === Image.Ready ? 1 : 0
                                }

                                GenySpinner {
                                    anchors.centerIn: parent
                                    visible: fullImage.status === Image.Loading
                                    running: visible
                                }

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    width: Math.min(360, parent.width - Theme.space8)
                                    spacing: Theme.space2
                                    visible: fullImage.status === Image.Error || fullImage.source.toString().length === 0

                                    Icon {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: Theme.iconImage
                                        color: Theme.mediaViewerInkMuted
                                        preferredSize: Theme.typeTitle1Size
                                    }

                                    DirectionAwareLabel {
                                        Layout.fillWidth: true
                                        text: qsTrId("genycaster.embed.image_embed")
                                        color: Theme.mediaViewerInkMuted
                                        font.pixelSize: Theme.typeMetaSize
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        readonly property rect currentMediaRect: {
            const page = mediaRepeater.itemAt(gallery.currentIndex)
            if (page === null || page === undefined)
                return Qt.rect(0, 0, width, height)
            return page.paintedRect
        }
        readonly property rect chromeRect: currentMediaRect.width >= 140 && currentMediaRect.height >= 72
                                                   ? currentMediaRect
                                                   : Qt.rect(0, 0, width, height)

        // Viewer controls belong to fixed safe viewport edges, never the
        // moving image's painted bounds. Portrait and landscape are identical.
        RowLayout {
            z: 20
            x: Theme.space3
            // Keep Close above the displayed media, within the top safe-area
            // region rather than aligned to the painted image.
            y: root.width < 700 ? Math.max(Theme.space3, 48) : Theme.space3
            width: Math.max(0, parent.width - Theme.space3 * 2)
            layoutDirection: Theme.uiDirection
            spacing: Theme.space2

            // Only Close occupies the upper safe-area edge.
            Item { Layout.fillWidth: true }

            ToolButton {
                id: closeButton
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                hoverEnabled: true
                text: qsTrId("genycaster.shell.close")
                Accessible.name: text
                onClicked: root.close()

                contentItem: Icon {
                    text: Theme.iconClose
                    color: Theme.mediaViewerInk
                    preferredSize: Theme.typeBodySize
                }

                background: Superellipse {
                    fillColor: closeButton.hovered || closeButton.down
                               ? Theme.mediaViewerControlHover : Theme.mediaViewerControl
                    borderWidth: 1
                    borderColor: Theme.keyboardFocusVisible(closeButton) ? Theme.accent : Theme.mediaViewerBorder
                    curveDepth: closeButton.height / 2
                }
            }
        }

        // Image count remains at the lower viewport edge, not over the
        // middle of portrait media. On multi-image galleries it sits above
        // the thumbnail dock; single images use the very bottom safe area.
        Superellipse {
            id: galleryCounter
            z: 20
            anchors.left: parent.left
            anchors.leftMargin: Theme.space3
            anchors.bottom: parent.bottom
            anchors.bottomMargin: (root.itemCount > 1 ? 82 : 0)
                                  + (root.width < 700 ? Math.max(Theme.space3, 28) : Theme.space3)
            width: counterLabel.implicitWidth + Theme.space3 * 2
            height: 38
            fillColor: Theme.mediaViewerControl
            borderWidth: 1
            borderColor: Theme.mediaViewerBorder
            curveDepth: height / 2

            DirectionAwareLabel {
                id: counterLabel
                anchors.centerIn: parent
                text: root.itemCount > 0
                      ? LocaleFormat.formatInteger(root.currentIndex + 1)
                        + " / " + LocaleFormat.formatInteger(root.itemCount)
                      : ""
                color: Theme.mediaViewerInk
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightDemiBold
                forceLeftToRight: true
            }
        }

        ActionButton {
            z: 20
            visible: root.itemCount > 1 && root.width >= 700
                     && (Theme.rtl ? gallery.currentIndex + 1 < root.itemCount
                                   : gallery.currentIndex > 0)
            anchors.left: parent.left
            anchors.leftMargin: Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            compact: false
            quiet: false
            iconOnly: true
            iconGlyph: Theme.iconArrowLeft
            text: Theme.rtl ? qsTrId("genycaster.bridge.loading_the_next_page")
                            : qsTrId("genycaster.profile.back_back_to_feed")
            onClicked: Theme.rtl ? root.next() : root.previous()
        }

        ActionButton {
            z: 20
            visible: root.itemCount > 1 && root.width >= 700
                     && (Theme.rtl ? gallery.currentIndex > 0
                                   : gallery.currentIndex + 1 < root.itemCount)
            anchors.right: parent.right
            anchors.rightMargin: Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            compact: false
            quiet: false
            iconOnly: true
            iconGlyph: Theme.iconArrowRight
            text: Theme.rtl ? qsTrId("genycaster.profile.back_back_to_feed")
                            : qsTrId("genycaster.bridge.loading_the_next_page")
            onClicked: Theme.rtl ? root.previous() : root.next()
        }

        Item {
            id: thumbnailDock
            z: 20
            visible: root.itemCount > 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            anchors.bottomMargin: root.width < 700 ? Math.max(Theme.space3, 28) : Theme.space3
            height: 68

            Superellipse {
                anchors.fill: parent
                fillColor: Theme.mediaViewerControl
                borderWidth: 1
                borderColor: Theme.mediaViewerBorder
                curveDepth: Theme.curveContainer
            }

            ListView {
                anchors.fill: parent
                anchors.margins: Theme.space1
                orientation: ListView.Horizontal
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2
                clip: true
                model: root.items
                currentIndex: gallery.currentIndex

                delegate: Item {
                    required property int index
                    required property var modelData
                    width: 58
                    height: 58

                    SuperellipseImage {
                        anchors.fill: parent
                        anchors.margins: index === gallery.currentIndex ? 0 : 3
                        source: root.sourceFor(modelData)
                        mediaCategory: String(modelData.mediaCategory || "photos")
                        manualLoadRequested: true
                        requestedSourceWidth: 160
                        requestedSourceHeight: 160
                        fillMode: Image.PreserveAspectCrop
                        curveDepth: Theme.curveCompact
                        borderWidth: index === gallery.currentIndex ? 2 : 1
                        borderColor: index === gallery.currentIndex ? Theme.accent : Theme.mediaViewerBorder
                        fillColor: Theme.mediaViewerThumbnail
                    }

                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: gallery.currentIndex = index
                    }
                }
            }
        }

        Shortcut {
            enabled: root.opened
            sequence: "Left"
            onActivated: Theme.rtl ? root.next() : root.previous()
        }

        Shortcut {
            enabled: root.opened
            sequence: "Right"
            onActivated: Theme.rtl ? root.previous() : root.next()
        }
    }
}
