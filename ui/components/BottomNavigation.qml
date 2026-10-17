import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: root
    property string activeView: "feed"
    property string activeFeed: "trending"
    property string shellRoute: ""
    property string primaryDestination: "home"
    property Item blurSourceItem: null
    property bool glassEnabled: true
    readonly property bool glassActive: root.visible && root.glassEnabled
                                        && root.blurSourceItem !== null
    readonly property real glassRenderScale: Math.max(0.72,
                                                       Math.min(1.0, renderingController.effectScale))

    signal trendingRequested()
    signal followingRequested()
    signal createRequested()
    signal settingsRequested()
    signal routeRequested(string route)

    implicitHeight: 60
    clip: false

    Item {
        id: glassLayer
        anchors.fill: parent
        anchors.margins: 1

        ShaderEffectSource {
            id: glassCapture
            anchors.fill: parent
            visible: false
            live: root.glassActive
            recursive: false
            sourceItem: root.blurSourceItem
            textureSize: Qt.size(Math.max(1, Math.round(width * root.glassRenderScale)),
                                 Math.max(1, Math.round(height * root.glassRenderScale)))
            smooth: true
            sourceRect: {
                if (!root.blurSourceItem || root.width <= 0 || root.height <= 0)
                    return Qt.rect(0, 0, 1, 1)
                const origin = root.blurSourceItem.mapFromItem(root, 0, 0)
                return Qt.rect(origin.x, origin.y, root.width, root.height)
            }
        }

        Superellipse {
            id: glassMask
            anchors.fill: parent
            fillColor: "white"
            curveDepth: height / 2
            visible: false
            layer.enabled: root.glassActive
            layer.samples: renderingController.layerSampleCount
        }

        MultiEffect {
            anchors.fill: parent
            visible: root.glassActive
            source: glassCapture
            blurEnabled: true
            // A navigation dock should read as frosted/matte, not transparent
            // glass. Strong blur + desaturation preserves context without
            // leaving legible text or vivid media behind the controls.
            blur: 1.0
            blurMax: 64
            saturation: 0.18
            brightness: Theme.dark ? -0.12 : 0.02
            maskEnabled: true
            maskSource: glassMask
            autoPaddingEnabled: false
        }
    }

    Superellipse {
        id: dockSurface
        anchors.fill: parent
        fillColor: Theme.dark ? Qt.rgba(0.035, 0.04, 0.055, 0.92)
                              : Qt.rgba(0.965, 0.97, 0.98, 0.93)
        borderWidth: Theme.strokeHairline
        borderColor: Theme.dark ? Qt.rgba(1, 1, 1, 0.14)
                                : Qt.rgba(0.08, 0.10, 0.15, 0.10)
        curveDepth: height / 2
        elevation: Theme.elevationNone
        shadowEnabled: false
    }


    RowLayout {
        anchors.fill: dockSurface
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        spacing: 0
        layoutDirection: Theme.uiDirection

        BottomNavButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.bottom_nav.home")
            iconGlyph: Theme.iconHome
            selected: root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "home"
            showLabel: false
            onClicked: root.followingRequested()
        }
        BottomNavButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.side_nav.explorer")
            iconGlyph: Theme.iconCompass
            selected: root.shellRoute === "" && root.activeView === "feed" && root.primaryDestination === "explorer"
            showLabel: false
            onClicked: root.trendingRequested()
        }
        BottomNavButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.bottom_nav.create")
            iconGlyph: Theme.iconCompose
            showLabel: false
            onClicked: root.createRequested()
        }
        BottomNavButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.apps.title")
            iconGlyph: Theme.iconApps
            selected: root.shellRoute === "apps"
            showLabel: false
            onClicked: root.routeRequested("apps")
        }
        BottomNavButton {
            Layout.fillWidth: true
            text: qsTrId("genycaster.settings.title")
            iconGlyph: Theme.iconSettings
            showLabel: false
            onClicked: root.settingsRequested()
        }
    }
}
