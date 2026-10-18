import QtQuick
import QtQuick.Layouts

/**
 * @brief Normalized market-asset artwork with deterministic built-in glyphs and fallback initials.
 *
 * Known assets use bundled vector artwork so circular remote logos do not create a
 * second card shape inside the token tile. Unknown assets preserve their remote
 * artwork inside the same superellipse geometry. Missing or failed artwork falls
 * back to the first symbol/name character.
 */
Item {
    id: root

    property var asset: ({})
    property real iconInsetRatio: 0.13

    readonly property string symbol: String(root.asset && root.asset.symbol ? root.asset.symbol : "").trim().toUpperCase()
    readonly property string name: String(root.asset && root.asset.name ? root.asset.name : "").trim()
    readonly property string remoteSource: String(root.asset && root.asset.imageSource ? root.asset.imageSource : "")
    readonly property string semanticIcon: {
        if (root.symbol === "BTC") return "crypto/bitcoin"
        if (root.symbol === "ETH") return "crypto/ethereum"
        if (root.symbol === "GENY") return "brands/geny"
        return ""
    }
    // Verified market-source artwork has priority over decorative in-app icons.
    // In particular GENY's branding glyph is not its registered token logo.
    readonly property bool bundledArtwork: root.semanticIcon.length > 0 && root.remoteSource.length === 0
    readonly property string fallbackLetter: {
        const candidate = root.symbol.length > 0 ? root.symbol : root.name
        return candidate.length > 0 ? candidate.slice(0, 1).toUpperCase() : "?"
    }
    readonly property color tileColor: {
        if (root.symbol === "BTC") return Theme.cryptoBitcoinSurface
        if (root.symbol === "ETH") return Theme.cryptoEthereumSurface
        if (root.symbol === "GENY") return Theme.cryptoGenySurface
        return Theme.primarySurface
    }
    readonly property color glyphColor: {
        if (root.symbol === "BTC") return Theme.inkInverse
        if (root.symbol === "ETH") return Theme.cryptoEthereumGlyph
        if (root.symbol === "GENY") return Theme.accent
        return Theme.accent
    }
    readonly property bool showInitial: !root.bundledArtwork
                                                && (root.remoteSource.length === 0
                                                    || remoteArtwork.loadFailed
                                                    || remoteArtwork.status !== Image.Ready)

    implicitWidth: 40
    implicitHeight: 40

    Superellipse {
        anchors.fill: parent
        fillColor: root.tileColor
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderSubtle
        curveDepth: Math.max(8, Math.round(Math.min(width, height) * 0.29))
        exponent: Theme.superellipseExponent
    }

    Icon {
        anchors.centerIn: parent
        visible: root.bundledArtwork
        text: root.semanticIcon
        color: root.glyphColor
        preferredSize: Math.round(Math.min(root.width, root.height) * 0.62)
    }

    SuperellipseImage {
        id: remoteArtwork
        anchors.fill: parent
        anchors.margins: Math.max(3, Math.round(Math.min(root.width, root.height) * root.iconInsetRatio))
        visible: !root.bundledArtwork && root.remoteSource.length > 0
        source: root.remoteSource
        mediaCategory: "other"
        // Small token logos must never render the full-size 'Tap to load'
        // overlay: it escapes a 40px icon and obscures adjacent token text.
        forceAutomaticDownload: true
        showManualLoadIndicator: false
        showLoadingIndicator: false
        requestedSourceWidth: 96
        requestedSourceHeight: 96
        fillMode: Image.PreserveAspectFit
        fillColor: "transparent"
        borderWidth: 0
        curveDepth: Math.max(6, Math.round(Math.min(width, height) * 0.28))
        exponent: Theme.superellipseExponent
    }

    Text {
        anchors.centerIn: parent
        visible: root.showInitial
        text: root.fallbackLetter
        color: root.glyphColor
        font.family: Theme.latinFontFamily
        font.pixelSize: Math.max(13, Math.round(Math.min(root.width, root.height) * 0.42))
        font.weight: Theme.weightBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
