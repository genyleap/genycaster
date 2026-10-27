import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Server-indexed topic discovery. Never derive category membership from the
// currently loaded feed page; missing local previews are still discoverable.
Item {
    id: root
    property string scopeName: "Explorer"
    property string topicSlug: ""
    property var topics: []
    property var items: []
    property var cachedRows: []
    property bool busy: false
    property bool hasMore: false
    property string errorText: ""
    property bool compact: false
    property bool interactionsAvailable: false
    property double accountFid: 0
    signal backRequested()
    signal topicRequested(string slug)
    signal moreRequested()
    signal castRequested(string hash)
    signal profileRequested(double fid)
    signal mediaRequested(var mediaItems, int index)
    signal externalRequested(string url)

    function cachedCast(hash) {
        const key=String(hash || "").toLowerCase()
        const rows=root.cachedRows || []
        for (let i=0;i<rows.length;i++) {
            const item=rows[i] || ({})
            if (String(item.castId || "").toLowerCase()===key)
                return item
        }
        return null
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.mobileGutter
            Layout.rightMargin: Theme.mobileGutter
            Layout.topMargin: Theme.space2
            spacing: Theme.space2
            ActionButton {
                compact: true
                quiet: true
                iconGlyph: Theme.iconArrowLeft
                text: "Back"
                onClicked: root.backRequested()
            }
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.scopeName + " · Topics"
                color: Theme.textPrimary
                font.pixelSize: Theme.typeBodySize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }
            TopicFilterPicker {
                Layout.preferredWidth: root.compact ? 148 : 200
                Layout.maximumWidth: root.compact ? 148 : 200
                Layout.preferredHeight: Theme.controlHeight
                compact: root.compact
                selectedTopic: root.topicSlug
                catalog: root.topics
                onTopicRequested: slug => root.topicRequested(slug)
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.mobileGutter
            Layout.rightMargin: Theme.mobileGutter
            visible: root.errorText.length > 0
            text: root.errorText
            color: Theme.negative
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.mobileGutter
            Layout.rightMargin: Theme.mobileGutter
            visible: !root.busy && root.items.length===0 && root.errorText.length===0
            text: "No GenyCaster casts are tagged with this topic yet."
            color: Theme.inkMuted
            font.pixelSize: Theme.typeMetaSize
            wrapMode: Text.Wrap
        }
        ListView {
            id: results
            objectName: "serverIndexedTopicCastList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space2
            model: root.items || []
            boundsBehavior: Flickable.StopAtBounds
            delegate: Item {
                id: row
                required property var modelData
                width: results.width
                readonly property string hash: String(modelData.cast_hash || "")
                readonly property var cached: root.cachedCast(hash)
                readonly property bool hasPreview: cached!==null
                height: hasPreview ? richPreview.implicitHeight + Theme.space3*2
                                    : (root.compact ? 80 : 96)

                Superellipse {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.mobileGutter
                    anchors.rightMargin: Theme.mobileGutter
                    fillColor: Theme.surfaceRaised
                    borderColor: Theme.borderSubtle
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveContainer
                }
                CastItem {
                    id: richPreview
                    visible: row.hasPreview
                    x: Theme.mobileGutter + Theme.space2
                    width: Math.max(1,parent.width - (Theme.mobileGutter + Theme.space2)*2)
                    modelData: row.cached || ({})
                    compact: root.compact
                    interactionsAvailable: false
                    accountFid: root.accountFid
                    enrichmentActive: false
                    onOpenRequested: hash => root.castRequested(hash)
                    onProfileRequested: fid => root.profileRequested(fid)
                    onMediaRequested: (mediaItems,index) => root.mediaRequested(mediaItems,index)
                    onExternalRequested: url => root.externalRequested(url)
                }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.mobileGutter+Theme.space3
                    anchors.rightMargin: Theme.mobileGutter+Theme.space3
                    spacing: Theme.space1
                    visible: !row.hasPreview
                    RowLayout {
                        Layout.fillWidth: true
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: "Cast · FID " + String(row.modelData.author_fid || "")
                            font.pixelSize: Theme.typeMetaSize
                            color: Theme.textSecondary
                        }
                        ActionButton {
                            compact: true
                            quiet: true
                            iconGlyph: Theme.iconArrowUpRight
                            text: "View cast"
                            onClicked: root.castRequested(row.hash)
                        }
                    }
                    DirectionAwareLabel {
                        Layout.fillWidth: true
                        text: row.hash
                        forceLeftToRight: true
                        elide: Text.ElideMiddle
                        color: Theme.textTertiary
                        font.pixelSize: Theme.typeCaptionSize
                    }
                }
            }
            footer: ColumnLayout {
                width: results.width
                spacing: Theme.space2
                Item { Layout.preferredHeight: Theme.space2 }
                ActionButton {
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.hasMore
                    compact: true
                    busy: root.busy
                    enabled: !root.busy
                    text: "Load more casts"
                    onClicked: root.moreRequested()
                }
                GenySpinner {
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.busy && !root.hasMore
                    running: visible
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                }
                Item { Layout.preferredHeight: Theme.space4 }
            }
        }
    }
}
