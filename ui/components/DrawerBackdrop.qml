import QtQuick
import QtQuick.Effects

Item {
    id: root

    property Item sourceItem: null
    property bool active: false
    property bool addScrim: true
    property bool blockInput: false
    signal backgroundTapped()
    readonly property real captureScale: renderingController.profile === "fidelity"
                                              || renderingController.effectiveQuality === "fidelity" ? 1.0
                                            : renderingController.profile === "efficiency"
                                              || renderingController.effectiveQuality === "efficiency" ? 0.72
                                            : 0.90
    readonly property real blurStrength: renderingController.profile === "efficiency"
                                         || renderingController.effectiveQuality === "efficiency" ? 0.46 : 0.58

    anchors.fill: parent
    visible: root.active

    ShaderEffectSource {
        id: capture
        visible: false
        live: root.active && root.sourceItem !== null
        recursive: false
        hideSource: root.active && root.sourceItem !== null
        sourceItem: root.sourceItem
        textureSize: Qt.size(
            Math.max(1, Math.round(root.width * root.captureScale)),
            Math.max(1, Math.round(root.height * root.captureScale)))
        sourceRect: {
            if (!root.sourceItem || root.width <= 0 || root.height <= 0)
                return Qt.rect(0, 0, 1, 1)
            const origin = root.sourceItem.mapFromItem(root, 0, 0)
            return Qt.rect(origin.x, origin.y, root.width, root.height)
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.active && root.sourceItem !== null
        source: capture
        blurEnabled: true
        blur: root.blurStrength
        blurMax: 30
        saturation: 0.92
        autoPaddingEnabled: false
        antialiasing: true
    }

    Rectangle {
        anchors.fill: parent
        color: root.addScrim
               ? (Theme.dark ? Qt.rgba(0.02, 0.025, 0.035, 0.42)
                             : Qt.rgba(0.94, 0.95, 0.97, 0.38))
               : "transparent"
    }

    MouseArea {
        anchors.fill: parent
        z: 100
        enabled: root.blockInput
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        preventStealing: true
        propagateComposedEvents: false
        onPressed: mouse => mouse.accepted = true
        onReleased: mouse => mouse.accepted = true
        onDoubleClicked: mouse => mouse.accepted = true
        onClicked: mouse => {
            mouse.accepted = true
            root.backgroundTapped()
        }
        onWheel: wheel => wheel.accepted = true
    }
}
