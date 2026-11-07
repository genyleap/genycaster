import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Superellipse {
    id: root

    property string title: ""
    property string description: ""
    property string iconGlyph: Theme.iconSystem
    property real value: 320
    property real from: 264
    property real to: 620
    property real stepSize: 8
    signal valueRequested(real value)

    function steppedValue(delta) {
        const raw = Math.max(from, Math.min(to, value + delta * stepSize))
        return Math.round(raw / stepSize) * stepSize
    }

    Layout.fillWidth: true
    implicitHeight: body.implicitHeight + Theme.space3 * 2
    fillColor: Theme.surfaceMuted
    borderWidth: Theme.strokeHairline
    borderColor: Theme.borderSubtle
    curveDepth: Theme.curveContainer

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

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
                    text: root.iconGlyph
                    color: Theme.accent
                    preferredSize: Theme.iconSizeSmall
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: root.title
                    color: Theme.ink
                    font.pixelSize: Theme.typeMetaSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }
                DirectionAwareLabel {
                    visible: root.description.length > 0
                    Layout.fillWidth: true
                    text: root.description
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            Superellipse {
                Layout.preferredWidth: Math.max(62, valueLabel.implicitWidth + Theme.space3)
                Layout.preferredHeight: 28
                fillColor: Theme.primarySurface
                borderWidth: Theme.strokeHairline
                borderColor: Theme.accent
                curveDepth: 14

                DirectionAwareLabel {
                    id: valueLabel
                    anchors.centerIn: parent
                    text: LocaleFormat.formatInteger(Math.round(root.value)) + " px"
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
                enabled: root.value > root.from + 0.5
                onClicked: root.valueRequested(root.steppedValue(-1))
            }

            Slider {
                id: widthSlider
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
                    x: widthSlider.leftPadding
                    y: Math.round((widthSlider.height - height) / 2)
                    width: widthSlider.availableWidth
                    height: 16

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
                        width: Math.max(5, parent.width * widthSlider.visualPosition)
                        height: 5
                        fillColor: Theme.accent
                        borderWidth: 0
                        curveDepth: 2.5
                    }
                }

                handle: Superellipse {
                    x: widthSlider.leftPadding
                       + widthSlider.visualPosition * (widthSlider.availableWidth - width)
                    y: Math.round((widthSlider.height - height) / 2)
                    width: widthSlider.pressed ? 22 : 20
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
                enabled: root.value < root.to - 0.5
                onClicked: root.valueRequested(root.steppedValue(1))
            }
        }
    }
}
