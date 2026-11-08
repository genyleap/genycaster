import QtQuick
import Farcaster.Client

Item {
    id: root

    property var modelData
    property int rowIndex: 0
    property string presentationKind: "conversation"
    property bool compact: false
    property bool magazineCard: false
    property var sharedMediaGeometry: null
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

    implicitHeight: loader.item ? loader.item.implicitHeight : 0
    // Give the nested Loader a real vertical extent. Width-only anchoring
    // allows rich children to calculate their implicitHeight while painting
    // with a zero-height scene node, clipping markdown/media into the next row.
    height: implicitHeight

    Loader {
        id: loader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.height
        sourceComponent: root.magazineCard ? magazineComponent
                       : root.resolvedKind === "visual" ? visualComponent
                       : root.resolvedKind === "developer" ? developerComponent : conversationComponent
    }

    Component {
        id: magazineComponent
        CastItem {
            magazineCard: true
            sharedMediaGeometry: root.sharedMediaGeometry
            modelData: root.modelData
            compact: true
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

    Component {
        id: conversationComponent
        ConversationRow {
            modelData: root.modelData
            compact: root.compact
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

    Component {
        id: visualComponent
        VisualPost {
            modelData: root.modelData
            compact: root.compact
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

    Component {
        id: developerComponent
        DeveloperPost {
            modelData: root.modelData
            compact: root.compact
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
}
