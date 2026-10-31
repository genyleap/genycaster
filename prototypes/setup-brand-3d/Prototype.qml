import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import QtQuick3D.AssetUtils

ApplicationWindow {
    id: root
    width: 1320
    height: 780
    minimumWidth: 860
    minimumHeight: 540
    visible: true
    title: "GenyCaster — 3D Brand Prototype (not production Setup)"
    color: dark ? "#0D111A" : "#FFFFFF"
    property bool dark: true
    property alias brandSidebar: sidebar
    property bool reducedMotion: false
    property real pointerX: 0
    property real pointerY: 0
    property color accent: dark ? "#58D9DB" : "#159EB2"
    readonly property color surface: dark ? "#111B29" : "#FFFFFF"
    readonly property color ink: dark ? "#F5F8FC" : "#15253C"
    readonly property color muted: dark ? "#9AAABB" : "#697F94"

    Rectangle {
        id: sidebar
        width: Math.min(root.width * 0.29, 418)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        clip: true
        gradient: Gradient {
            GradientStop { position: 0; color: dark ? "#071521" : "#F4FAFC" }
            GradientStop { position: 0.55; color: dark ? "#103044" : "#DFF3F8" }
            GradientStop { position: 1; color: dark ? "#091927" : "#F1F7FB" }
        }
        View3D {
            id: scene
            anchors.fill: parent
            anchors.topMargin: 92
            anchors.bottomMargin: 64
            environment: SceneEnvironment {
                backgroundMode: SceneEnvironment.Transparent
                antialiasingMode: SceneEnvironment.MSAA
                antialiasingQuality: SceneEnvironment.High
                lightProbe: Texture {
                    source: root.dark ? "assets/studio-dark.png" : "assets/studio-light.png"
                }
                probeExposure: root.dark ? 2.0 : 1.4
            }
            PerspectiveCamera {
                id: camera
                position: Qt.vector3d(0, 0, 950)
                clipFar: 2200
                fieldOfView: 43
            }
            camera: camera
            DirectionalLight {
                eulerRotation: Qt.vector3d(-35, -30, 0)
                brightness: root.dark ? 2.8 : 1.8
                color: root.dark ? "#A7FAF1" : "#FFFFFF"
            }
            DirectionalLight {
                eulerRotation: Qt.vector3d(42, 25, 0)
                brightness: root.dark ? 2.4 : 1.5
                color: root.dark ? "#FACB8B" : "#F7D6A2"
            }
            RuntimeLoader {
                id: art
                source: root.dark ? "assets/brand-dark.glb" : "assets/brand-light.glb"
                eulerRotation: Qt.vector3d(root.pointerY * 3.5, root.pointerX * 7.5, 0)
                x: -15
                y: 0
                onStatusChanged: {
                    if (status === RuntimeLoader.Error)
                        console.error("Brand3D GLB load:", errorString)
                    if (status === RuntimeLoader.Success)
                        console.log("Brand3D GLB loaded:", source)
                }
                Behavior on eulerRotation {
                    enabled: !root.reducedMotion
                    Vector3dAnimation { duration: 380; easing.type: Easing.OutCubic }
                }
            }
        }

        // 2D is deliberately separate from the 3D renderer: text stays crisp.
        Row {
            z: 5
            x: 28; y: 32
            spacing: 11
            Image {
                width: 38; height: 38
                source: "../../ui/wasm/genycaster-icon.svg"
                fillMode: Image.PreserveAspectFit
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "GENYCASTER"
                color: dark ? "#F0FCFF" : "#0B2941"
                font.family: "Inter"
                font.pixelSize: 21
                font.weight: Font.Bold
                font.letterSpacing: 1.3
            }
        }
        Text {
            x: 29; y: 106
            z: 4
            text: "A NEW PERSPECTIVE"
            color: dark ? "#A9E2E2" : "#547D93"
            font.pixelSize: 10
            font.letterSpacing: 2.8
            font.weight: Font.DemiBold
        }
        Rectangle {
            z: 5
            width: 58; height: 2; radius: 1
            x: 29; y: sidebar.height - 102
            color: root.accent
            opacity: 0.85
        }
        Column {
            x: 29; y: sidebar.height - 86
            spacing: 5
            z: 5
            Text {
                text: "© 2026 Genyleap"
                font.pixelSize: 12
                color: dark ? "#D4EBF1" : "#526C7F"
            }
            Text {
                text: "Beta 1.0 · v1.0.0"
                font.pixelSize: 11
                color: dark ? "#A3BDCD" : "#728C9F"
            }
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            z: 3
            onPositionChanged: function(mouse) {
                if (root.reducedMotion) return
                root.pointerX = Math.max(-1, Math.min(1, 2*mouse.x/width-1))
                root.pointerY = Math.max(-1, Math.min(1, 2*mouse.y/height-1))
            }
            onExited: {
                root.pointerX = 0
                root.pointerY = 0
            }
            onClicked: {
                if (!root.reducedMotion) {
                    pulse.restart()
                }
            }
        }
        SequentialAnimation {
            id: pulse
            NumberAnimation { target: art; property: "scale"; to: Qt.vector3d(1.026,1.026,1.026); duration: 150 }
            NumberAnimation { target: art; property: "scale"; to: Qt.vector3d(1,1,1); duration: 460 }
        }
    }

    Rectangle {
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: root.surface
        Column {
            id: content
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 620)
            spacing: 22
            Row {
                spacing: 11
                Text { text: "01"; color: root.accent; font.bold: true; font.pixelSize: 14 }
                Text { text: "/ 08"; color: root.muted; font.pixelSize: 14 }
                Text { text: "  •  SETUP PREVIEW"; color: root.muted; font.pixelSize: 12; font.letterSpacing: 1.3 }
            }
            Item { width: 1; height: 2 }
            Text {
                text: "Welcome to GenyCaster"
                font.pixelSize: 34
                font.weight: Font.Bold
                color: root.ink
                width: content.width
                wrapMode: Text.WordWrap
            }
            Text {
                text: "Explore a dedicated GPU-rendered brand scene before bringing it into the onboarding flow."
                font.pixelSize: 16
                lineHeight: 1.5
                color: root.muted
                wrapMode: Text.WordWrap
                width: content.width
            }
            Item { width: 1; height: 8 }
            Repeater {
                model: [
                    {title:"3D brand composition", detail:"Procedural glass architecture · two light rigs"},
                    {title:"Reactive camera", detail:"Subtle pointer-driven rotation · no forced looping"},
                    {title:"Accessible presentation", detail:"2D logo and text · reduced-motion support"}
                ]
                Rectangle {
                    required property var modelData
                    width: content.width
                    height: 94
                    radius: 17
                    color: root.dark ? "#182635" : "#F8FBFE"
                    border.color: root.dark ? "#2E4556" : "#DFEBF2"
                    border.width: 1
                    Rectangle {
                        x: 19; y: 25; width: 42; height: 42; radius: 13
                        color: root.dark ? "#224955" : "#DDF5F8"
                        Text { anchors.centerIn: parent; text: "✦"; color: root.accent; font.pixelSize: 23 }
                    }
                    Column {
                        x: 79; y: 23
                        spacing: 7
                        Text { text: modelData.title; color: root.ink; font.pixelSize: 16; font.bold: true }
                        Text { text: modelData.detail; color: root.muted; font.pixelSize: 12 }
                    }
                }
            }
            Item { width: 1; height: 10 }
            Row {
                spacing: 12
                Button {
                    text: root.dark ? "Switch to light" : "Switch to dark"
                    onClicked: root.dark = !root.dark
                }
                Button {
                    text: root.reducedMotion ? "Enable motion" : "Reduce motion"
                    onClicked: root.reducedMotion = !root.reducedMotion
                }
            }
        }
    }
}
