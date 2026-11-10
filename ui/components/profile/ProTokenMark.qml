import QtQuick
import QtQuick.Window
import Farcaster.Client

// Keep payment marks vector-sharp on Retina/HiDPI screens. GENY's genuine
// artwork is a tall 228.73:465 SVG; square-rasterizing it makes the symbol
// look half-sized beside USDC. Respect each mark's natural geometry.
Item {
    id: root

    property string asset: "GENY"
    property real tokenSize: Theme.scaleUi(32)
    readonly property bool geny: asset === "GENY"

    implicitWidth: tokenSize
    implicitHeight: tokenSize
    Accessible.name: asset

    Image {
        id: mark
        anchors.centerIn: parent
        width: root.geny ? root.tokenSize * (228.73 / 465.02)
                         : root.tokenSize * 0.94
        height: root.geny ? root.tokenSize : root.tokenSize * 0.94
        source: root.geny
            ? (Theme.dark
                ? "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/geny-token-mark-dark.svg"
                : "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/geny-token-mark-light.svg")
            : "qrc:/qt/qml/Farcaster/Client/assets/icons/genyleap/icons/brands/usdc-token-mark.svg"
        // SVGs are rasterized only to a DPI-aware, oversampled target. Never
        // stretch a small decoded texture to the eventual layout size.
        sourceSize.width: Math.max(24, Math.ceil(width
            * Math.max(1, Screen.devicePixelRatio) * 2))
        sourceSize.height: Math.max(24, Math.ceil(height
            * Math.max(1, Screen.devicePixelRatio) * 2))
        fillMode: Image.PreserveAspectFit
        asynchronous: false
        cache: true
        smooth: true
        mipmap: false
    }
}
