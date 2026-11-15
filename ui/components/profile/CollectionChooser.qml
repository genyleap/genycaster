import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Popup {
    id: root
    objectName: "bookmarkCollectionChooser"
    parent: Overlay.overlay
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? Math.min(392,parent.width-Theme.space4*2) : 360
    x: parent ? (parent.width-width)/2 : 0
    y: parent ? Math.max(Theme.space3,(parent.height-height)/2) : 0
    padding: Theme.space3
    property var collections: []
    readonly property var entitlements: openProofConnections
        ? (openProofConnections.proEntitlements || ({})) : ({})
    readonly property bool proEligible: entitlements.status === "verified"
        && entitlements.eligible === true
    property bool busy: false
    property string errorText: ""
    property string castHash: ""
    property string newName: ""
    signal collectionPicked(string id)
    signal createRequested(string name)
    signal unfiledRequested()
    signal proDetailsRequested()

    background: PopupSurface { curveDepth: Theme.curveContainer }
    contentItem: ColumnLayout {
        width: Math.max(1,root.width-root.padding*2)
        spacing: Theme.space2
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.bookmarks.chooser_title")
            font.pixelSize: Theme.typeBodySize
            font.weight: Theme.weightDemiBold
            color: Theme.textPrimary
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            text: qsTrId("genycaster.bookmarks.chooser_description")
            wrapMode: Text.Wrap
            font.pixelSize: Theme.typeCaptionSize
            color: Theme.textSecondary
        }
        ActionButton {
            Layout.fillWidth: true
            compact: true
            quiet: true
            text: qsTrId("genycaster.bookmarks.uncategorized")
            enabled: !root.busy
            // Removing an old bookmark classification stays available
            // after a membership downgrade.
            onClicked: {
                root.unfiledRequested()
                root.close()
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(216,Math.max(0,root.collections.length*42))
            visible: root.collections.length>0
            clip: true
            ColumnLayout {
                width: Math.max(1,root.width-root.padding*2)
                spacing: 0
                Repeater {
                    model: root.collections
                    delegate: ActionButton {
                        required property var modelData
                        Layout.fillWidth: true
                        quiet: true
                        compact: true
                        iconGlyph: Theme.iconBookmark
                        text: String(modelData.name||"")
                        enabled: !root.busy
                        onClicked: {
                            if (!root.proEligible) {
                                collectionsProNotice.open()
                                return
                            }
                            root.collectionPicked(String(modelData.id||""))
                            root.close()
                        }
                    }
                }
            }
        }
        RowLayout {
            layoutDirection: Theme.uiDirection
            Layout.fillWidth: true
            spacing: Theme.space2
            DirectionAwareTextField {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                placeholderText: qsTrId("genycaster.bookmarks.new_collection")
                enabled: root.proEligible
                maximumLength: 80
                text: root.newName
                onTextChanged: root.newName=text
            }
            ActionButton {
                compact: true
                prominent: true
                iconGlyph: Theme.iconPlus
                text: qsTrId("genycaster.bookmarks.create")
                enabled: !root.busy
                         && (!root.proEligible || root.newName.trim().length > 0)
                onClicked: {
                    if (!root.proEligible) {
                        collectionsProNotice.open()
                        return
                    }
                    root.createRequested(root.newName.trim())
                }
            }
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: !root.proEligible
            text: qsTrId("genycaster.pro.unlock_hint").arg(
                Number(root.entitlements.monthly_usd_cents || 0) > 0
                    ? (Number(root.entitlements.monthly_usd_cents) / 100).toFixed(2) : "—")
            color: Theme.proAccent
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        DirectionAwareLabel {
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            text: root.errorText
            color: Theme.negative
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
        GenySpinner {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            running: root.busy
            visible: root.busy
        }
    }
    ProUpgradeNotice {
        id: collectionsProNotice
        featureLabel: qsTrId("genycaster.pro.bookmark_collections")
        onUpgradeRequested: root.proDetailsRequested()
    }
}
