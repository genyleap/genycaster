import QtQuick
import Farcaster.Client

Item {
    id: root
    property var modelData
    property bool compact: false
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

    implicitHeight: cast.implicitHeight + (root.compact ? Theme.space2 : Theme.space5)

    CastItem {
        id: cast
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: Theme.space2
        modelData: root.modelData
        compact: root.compact
        interactionsAvailable: root.interactionsAvailable
        enrichmentActive: root.enrichmentActive
        accountFid: root.accountFid
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
