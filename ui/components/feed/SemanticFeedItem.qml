import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root

    property var modelData
    property int rowIndex: 0
    property string presentationKind: "conversation"
    property bool compact: false
    property bool magazineCard: false
    property var sharedMediaGeometry: null
    property bool sharedNeonPointerActive: false
    property real sharedNeonPointerX: 0
    property real sharedNeonPointerY: 0
    property bool interactionsAvailable: false
    property bool enrichmentActive: true
    property double accountFid: 0

    signal openRequested(string castId)
    signal profileRequested(double fid)
    signal replyRequested(var cast)
    signal quoteRequested(var cast)
    signal reactionRequested(string kind, var cast, bool active)
    signal removeRequested(var cast)
    signal externalRequested(string url)
    signal mediaRequested(var mediaItems, int index)
    signal authenticationRequested()

    readonly property string resolvedKind: root.presentationKind === "visual" ? "visual"
                                          : root.presentationKind === "developer" ? "developer"
                                          : "conversation"

    readonly property bool visualChrome: !root.magazineCard && root.resolvedKind === "visual"
    readonly property bool developerChrome: !root.magazineCard && root.resolvedKind === "developer"
    readonly property real topInset: root.developerChrome
        ? builderHeader.height + Theme.space1 : root.visualChrome ? Theme.space2 : 0
    readonly property real totalInset: root.developerChrome
        ? root.topInset + Theme.space3
        : root.visualChrome ? (root.compact ? Theme.space2 : Theme.space5) : 0

    // All presentation kinds have the same rich CastItem. A classification
    // change adjusts only its chrome and geometry, not the media renderer,
    // playback, subscriptions, decoded images, or menu ownership.
    implicitHeight: cast.implicitHeight + root.totalInset
    readonly property real magazinePaintHeight: root.magazineCard
        ? Number(cast.magazinePaintHeight || 0) : 0
    height: root.magazineCard ? Math.max(implicitHeight, magazinePaintHeight) : implicitHeight

    Loader {
        id: builderHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        anchors.top: parent.top
        active: root.developerChrome
        height: item ? item.implicitHeight : 0
        sourceComponent: Component {
            RowLayout {
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Icon { text: Theme.iconCode; color: Theme.accent; preferredSize: Theme.typeMetaSize }
                Label {
                    text: qsTrId("genycaster.home.builder_notes")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    CastItem {
        id: cast
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: root.topInset
        magazineCard: root.magazineCard
        // ConversationRow and Magazine used compact CastItem regardless of
        // the shell width. Visual and Developer used the shell compact flag.
        compact: root.magazineCard || root.resolvedKind === "conversation" || root.compact
        sharedMediaGeometry: root.magazineCard ? root.sharedMediaGeometry : null
        sharedNeonPointerActive: root.magazineCard && root.sharedNeonPointerActive
        sharedNeonPointerX: root.magazineCard ? root.sharedNeonPointerX : 0
        sharedNeonPointerY: root.magazineCard ? root.sharedNeonPointerY : 0
        modelData: root.modelData
        interactionsAvailable: root.interactionsAvailable
        accountFid: root.accountFid
        enrichmentActive: root.enrichmentActive
        onOpenRequested: castId => root.openRequested(castId)
        onProfileRequested: fid => root.profileRequested(fid)
        onReplyRequested: data => root.replyRequested(data)
        onQuoteRequested: data => root.quoteRequested(data)
        onReactionRequested: (kind, data, active) => root.reactionRequested(kind, data, active)
        onRemoveRequested: data => root.removeRequested(data)
        onExternalRequested: url => root.externalRequested(url)
        onMediaRequested: (mediaItems, index) => root.mediaRequested(mediaItems, index)
        onAuthenticationRequested: root.authenticationRequested()
    }
}
