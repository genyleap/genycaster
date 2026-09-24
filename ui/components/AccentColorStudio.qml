import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

GenyAdaptiveDialog {
    id: root
    objectName: "accentColorStudio"
    popupZ: 10000

    property string draftColor: String(Theme.customAccentQuickMint)
    property real hue: 0.47
    property real saturation: 0.80
    property real value: 0.78
    property bool inputError: false

    signal colorAccepted(string color)

    contentPadding: Theme.space3
    mobileHeightRatio: 0.92
    width: root.compactPresentation
           ? (parent ? parent.width : 420)
           : (parent ? Math.min(560, parent.width - Theme.space4 * 2) : 560)
    height: root.compactPresentation
            ? Math.min(parent ? parent.height * 0.92 : 700, 760)
            : Math.min((root.contentItem ? root.contentItem.implicitHeight : 620) + Theme.space3 * 2,
                       parent ? parent.height - Theme.space4 * 2 : 680)

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value))
    }

    function componentHex(value) {
        const text = Math.round(root.clamp(value, 0, 255)).toString(16).toUpperCase()
        return text.length < 2 ? "0" + text : text
    }

    function hsvToHex(h, s, v) {
        const normalizedHue = ((h % 1) + 1) % 1
        const sector = normalizedHue * 6
        const index = Math.floor(sector)
        const fraction = sector - index
        const p = v * (1 - s)
        const q = v * (1 - fraction * s)
        const t = v * (1 - (1 - fraction) * s)
        let red = 0
        let green = 0
        let blue = 0
        switch (index % 6) {
        case 0: red = v; green = t; blue = p; break
        case 1: red = q; green = v; blue = p; break
        case 2: red = p; green = v; blue = t; break
        case 3: red = p; green = q; blue = v; break
        case 4: red = t; green = p; blue = v; break
        default: red = v; green = p; blue = q; break
        }
        return "#" + root.componentHex(red * 255)
                   + root.componentHex(green * 255)
                   + root.componentHex(blue * 255)
    }

    function hexToHsv(value) {
        let text = String(value || "").trim()
        if (text.charAt(0) === "#")
            text = text.slice(1)
        if (!/^[0-9a-fA-F]{6}$/.test(text))
            return null
        const red = parseInt(text.slice(0, 2), 16) / 255
        const green = parseInt(text.slice(2, 4), 16) / 255
        const blue = parseInt(text.slice(4, 6), 16) / 255
        const maximum = Math.max(red, green, blue)
        const minimum = Math.min(red, green, blue)
        const delta = maximum - minimum
        let nextHue = 0
        if (delta > 0) {
            if (maximum === red)
                nextHue = ((green - blue) / delta) % 6
            else if (maximum === green)
                nextHue = (blue - red) / delta + 2
            else
                nextHue = (red - green) / delta + 4
            nextHue /= 6
            if (nextHue < 0)
                nextHue += 1
        }
        return {
            hue: nextHue,
            saturation: maximum === 0 ? 0 : delta / maximum,
            value: maximum
        }
    }

    function syncFromColor(value) {
        const parsed = root.hexToHsv(value)
        if (!parsed)
            return false
        root.hue = parsed.hue
        root.saturation = parsed.saturation
        root.value = parsed.value
        root.draftColor = root.hsvToHex(root.hue, root.saturation, root.value)
        root.inputError = false
        if (hexField.text !== root.draftColor)
            hexField.text = root.draftColor
        return true
    }

    function syncFromPicker() {
        root.draftColor = root.hsvToHex(root.hue, root.saturation, root.value)
        root.inputError = false
        if (hexField.text !== root.draftColor)
            hexField.text = root.draftColor
    }

    function updateSaturationValue(x, y, width, height) {
        if (width <= 0 || height <= 0)
            return
        root.saturation = root.clamp(x / width, 0, 1)
        root.value = 1 - root.clamp(y / height, 0, 1)
        root.syncFromPicker()
    }

    function updateHue(x, width) {
        if (width <= 0)
            return
        root.hue = root.clamp(x / width, 0, 0.999999)
        root.syncFromPicker()
    }

    function openWith(value) {
        root.syncFromColor(value) || root.syncFromColor(String(Theme.customAccentQuickMint))
        Qt.callLater(function() { root.open() })
    }

    contentItem: ColumnLayout {
        spacing: Theme.space3

        ModalHeader {
            Layout.fillWidth: true
            title: qsTrId("genycaster.settings.custom_color")
            subtitle: qsTrId("genycaster.settings.custom_color_description")
            onCloseRequested: root.close()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            layoutDirection: Theme.uiDirection

            Superellipse {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                fillColor: root.draftColor
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderStrong
                curveDepth: 17
                exponent: 3.8
                elevation: Theme.elevationControl
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.draftColor.toUpperCase()
                    color: Theme.ink
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeTitle3Size
                    font.weight: Theme.weightDemiBold
                }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.settings.custom_color_description")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
        }

        Item {
            id: colorPlane
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(250, Math.max(170, root.width * 0.40))

            Item {
                id: colorPlaneSource
                anchors.fill: parent
                visible: false
                layer.enabled: true
                layer.smooth: true

                Rectangle {
                    anchors.fill: parent
                    color: root.hsvToHex(root.hue, 1, 1)
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.colorPickerWhite }
                        GradientStop { position: 1.0; color: Theme.colorPickerTransparentWhite }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: Theme.colorPickerTransparentBlack }
                        GradientStop { position: 1.0; color: Theme.colorPickerBlack }
                    }
                }
            }

            Superellipse {
                id: colorPlaneMask
                anchors.fill: parent
                fillColor: Theme.colorPickerWhite
                curveDepth: Theme.curveContainer
                exponent: 3.8
                visible: false
                layer.enabled: true
                layer.smooth: true
            }

            MultiEffect {
                anchors.fill: parent
                source: colorPlaneSource
                maskEnabled: true
                maskSource: colorPlaneMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
                autoPaddingEnabled: false
                antialiasing: true
            }

            Superellipse {
                width: 24
                height: 24
                curveDepth: width / 2
                exponent: 2.01
                x: root.clamp(root.saturation * colorPlane.width - width / 2, -width / 2, colorPlane.width - width / 2)
                y: root.clamp((1 - root.value) * colorPlane.height - height / 2, -height / 2, colorPlane.height - height / 2)
                fillColor: Theme.colorPickerTransparentBlack
                borderWidth: 3
                borderColor: Theme.colorPickerWhite
                Superellipse {
                    anchors.fill: parent
                    anchors.margins: 3
                    curveDepth: width / 2
                    exponent: 2.01
                    fillColor: root.draftColor
                    borderWidth: 1
                    borderColor: Theme.colorPickerShadow
                }
            }

            MouseArea {
                anchors.fill: parent
                preventStealing: true
                cursorShape: Qt.CrossCursor
                onPressed: function(mouse) { root.updateSaturationValue(mouse.x, mouse.y, width, height) }
                onPositionChanged: function(mouse) {
                    if (pressed)
                        root.updateSaturationValue(mouse.x, mouse.y, width, height)
                }
            }
        }

        Item {
            id: hueStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 30

            Item {
                id: hueStripSource
                anchors.fill: parent
                visible: false
                layer.enabled: true
                layer.smooth: true

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.00; color: Theme.colorPickerRed }
                        GradientStop { position: 0.17; color: Theme.colorPickerYellow }
                        GradientStop { position: 0.33; color: Theme.colorPickerGreen }
                        GradientStop { position: 0.50; color: Theme.colorPickerCyan }
                        GradientStop { position: 0.67; color: Theme.colorPickerBlue }
                        GradientStop { position: 0.83; color: Theme.colorPickerMagenta }
                        GradientStop { position: 1.00; color: Theme.colorPickerRed }
                    }
                }
            }

            Superellipse {
                id: hueStripMask
                anchors.fill: parent
                fillColor: Theme.colorPickerWhite
                curveDepth: height / 2
                exponent: 2.01
                visible: false
                layer.enabled: true
                layer.smooth: true
            }

            MultiEffect {
                anchors.fill: parent
                source: hueStripSource
                maskEnabled: true
                maskSource: hueStripMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
                autoPaddingEnabled: false
                antialiasing: true
            }

            Superellipse {
                width: 12
                height: hueStrip.height + 8
                curveDepth: width / 2
                exponent: 2.01
                x: root.clamp(root.hue * hueStrip.width - width / 2, -width / 2, hueStrip.width - width / 2)
                anchors.verticalCenter: parent.verticalCenter
                fillColor: root.hsvToHex(root.hue, 1, 1)
                borderWidth: 3
                borderColor: Theme.colorPickerWhite
            }

            MouseArea {
                anchors.fill: parent
                preventStealing: true
                cursorShape: Qt.PointingHandCursor
                onPressed: function(mouse) { root.updateHue(mouse.x, width) }
                onPositionChanged: function(mouse) {
                    if (pressed)
                        root.updateHue(mouse.x, width)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: [Theme.customAccentQuickRose, Theme.customAccentQuickBlush,
                        Theme.customAccentQuickOrchid, Theme.customAccentQuickMint,
                        Theme.customAccentQuickAmber, Theme.customAccentQuickInk]
                delegate: Superellipse {
                    required property var modelData
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    fillColor: modelData
                    borderWidth: String(root.draftColor).toUpperCase() === String(modelData).toUpperCase() ? 3 : Theme.strokeHairline
                    borderColor: String(root.draftColor).toUpperCase() === String(modelData).toUpperCase() ? Theme.ink : Theme.borderStrong
                    curveDepth: 10
                    exponent: 3.8
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: root.syncFromColor(String(modelData))
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            DirectionAwareTextField {
                id: hexField
                Layout.fillWidth: true
                text: root.draftColor
                placeholderText: qsTrId("genycaster.settings.custom_color_placeholder")
                font.family: Theme.latinFontFamily
                onTextEdited: root.inputError = false
                onAccepted: {
                    root.inputError = !root.syncFromColor(text)
                }
            }

            ActionButton {
                compact: true
                prominent: true
                iconGlyph: Theme.iconCheck
                text: qsTrId("genycaster.composer.apply")
                onClicked: {
                    if (!root.syncFromColor(hexField.text)) {
                        root.inputError = true
                        return
                    }
                    root.colorAccepted(root.draftColor)
                    root.close()
                }
            }
        }

        DirectionAwareLabel {
            visible: root.inputError
            Layout.fillWidth: true
            text: qsTrId("genycaster.settings.custom_color_invalid")
            color: Theme.danger
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
    }
}
