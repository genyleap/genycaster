import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Superellipse {
    id: root

    property string title: ""
    property string previewText: ""
    property string iconGlyph: ""
    property real value: 1.0
    property real from: 0.85
    property real to: 1.30
    property real stepSize: 0.05
    signal valueRequested(real value)

    readonly property int stepCount: Math.max(1, Math.round((to - from) / stepSize))
    readonly property int valuePercent: Math.round(value * 100)

    function steppedValue(delta) {
        const raw = Math.max(from, Math.min(to, value + delta * stepSize))
        return Math.round(raw / stepSize) * stepSize
    }

    Layout.fillWidth: true
    implicitHeight: scaleBody.implicitHeight + Theme.space3 * 2
    fillColor: Theme.surfaceMuted
    borderWidth: Theme.strokeHairline
    borderColor: Theme.borderSubtle
    curveDepth: Theme.curveContainer

    ColumnLayout {
        id: scaleBody
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space3
        spacing: Theme.space3

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Superellipse {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                fillColor: Theme.surfaceRaised
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveControl

                Icon {
                    anchors.centerIn: parent
                    visible: root.iconGlyph.length > 0
                    text: root.iconGlyph
                    color: Theme.accent
                    preferredSize: Theme.iconSizeSmall
                }

                DirectionAwareLabel {
                    anchors.centerIn: parent
                    visible: root.iconGlyph.length === 0
                    text: root.previewText
                    color: Theme.ink
                    font.pixelSize: Theme.typeBodySize
                    font.weight: Theme.weightBold
                }
            }

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.title
                color: Theme.ink
                font.pixelSize: Theme.typeMetaSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }

            Superellipse {
                Layout.preferredWidth: Math.max(52, percentageLabel.implicitWidth + Theme.space3)
                Layout.preferredHeight: 28
                fillColor: Theme.primarySurface
                borderWidth: Theme.strokeHairline
                borderColor: Theme.accent
                curveDepth: 14

                DirectionAwareLabel {
                    id: percentageLabel
                    anchors.centerIn: parent
                    text: LocaleFormat.formatInteger(root.valuePercent) + "%"
                    color: Theme.accent
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightBold
                    forceLeftToRight: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            layoutDirection: Theme.uiDirection

            ActionButton {
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: "actions/minus"
                text: "−"
                enabled: root.value > root.from + 0.001
                onClicked: root.valueRequested(root.steppedValue(-1))
            }

            Slider {
                id: scaleSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                from: root.from
                to: root.to
                stepSize: root.stepSize
                snapMode: Slider.SnapAlways
                value: root.value
                hoverEnabled: true
                Accessible.name: root.title
                onMoved: root.valueRequested(value)

                background: Item {
                    x: scaleSlider.leftPadding
                    y: Math.round((scaleSlider.height - height) / 2)
                    width: scaleSlider.availableWidth
                    height: 18

                    Superellipse {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 5
                        fillColor: Theme.borderMuted
                        borderWidth: 0
                        curveDepth: 2.5
                    }

                    Superellipse {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(5, parent.width * scaleSlider.visualPosition)
                        height: 5
                        fillColor: Theme.accent
                        borderWidth: 0
                        curveDepth: 2.5
                    }

                    Repeater {
                        model: root.stepCount + 1
                        delegate: Superellipse {
                            required property int index
                            readonly property real position: index / root.stepCount
                            x: Math.round(position * parent.width) - width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            width: index === Math.round((1.0 - root.from) / root.stepSize) ? 3 : 2
                            height: width
                            fillColor: index <= Math.round(scaleSlider.position * root.stepCount)
                                       ? Theme.accentInk : Theme.inkSubtle
                            borderWidth: 0
                            curveDepth: width / 2
                            opacity: index === Math.round((1.0 - root.from) / root.stepSize) ? 0.95 : 0.62
                        }
                    }
                }

                handle: Superellipse {
                    x: scaleSlider.leftPadding
                       + scaleSlider.visualPosition * (scaleSlider.availableWidth - width)
                    y: Math.round((scaleSlider.height - height) / 2)
                    width: scaleSlider.pressed ? 22 : 20
                    height: width
                    fillColor: Theme.surfaceElevated
                    borderWidth: 2
                    borderColor: Theme.accent
                    curveDepth: width / 2
                    elevation: Theme.elevationControl
                    Behavior on width {
                        NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
                    }
                }
            }

            ActionButton {
                compact: true
                quiet: true
                iconOnly: true
                iconGlyph: Theme.iconPlus
                text: "+"
                enabled: root.value < root.to - 0.001
                onClicked: root.valueRequested(root.steppedValue(1))
            }
        }
    }
}
