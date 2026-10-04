import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var model: []
    property alias currentIndex: carousel.currentIndex
    property string selectedLabel: ""
    property string selectedDetail: ""
    property bool userInteracted: false
    signal activated(int index, var entry)

    readonly property int itemCount: model && model.length !== undefined ? model.length : 0
    readonly property var currentEntry: currentIndex >= 0 && currentIndex < itemCount
                                        ? model[currentIndex] : null

    implicitHeight: 184

    function wrappedOffset(index) {
        if (itemCount <= 0)
            return 0
        let delta = index - carousel.currentIndex
        if (delta > itemCount / 2)
            delta -= itemCount
        else if (delta < -itemCount / 2)
            delta += itemCount
        return delta
    }

    function activateCurrent() {
        if (!userInteracted || !currentEntry)
            return
        activated(currentIndex, currentEntry)
    }

    function previous() {
        if (itemCount <= 0)
            return
        userInteracted = true
        carousel.decrementCurrentIndex()
    }

    function next() {
        if (itemCount <= 0)
            return
        userInteracted = true
        carousel.incrementCurrentIndex()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space3

        Superellipse {
            Layout.fillWidth: true
            Layout.preferredHeight: 124
            fillColor: Theme.surface
            borderWidth: Theme.strokeStandard
            borderColor: Theme.borderMuted
            curveDepth: Theme.curveContainer

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 56
                anchors.rightMargin: 56
                height: Theme.strokeHairline
                color: Theme.borderMuted
                opacity: 0.72
            }

            Button {
                id: previousButton
                width: 34
                height: 34
                anchors.left: parent.left
                anchors.leftMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                hoverEnabled: true
                onClicked: root.previous()

                background: Rectangle {
                    radius: width / 2
                    color: previousButton.hovered ? Theme.surfaceHover : "transparent"
                    border.width: 0
                }

                contentItem: Icon {
                    text: Theme.iconArrowLeft
                    color: Theme.inkMuted
                    preferredSize: Theme.iconSizeSmall
                }
            }

            Button {
                id: nextButton
                width: 34
                height: 34
                anchors.right: parent.right
                anchors.rightMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                hoverEnabled: true
                onClicked: root.next()

                background: Rectangle {
                    radius: width / 2
                    color: nextButton.hovered ? Theme.surfaceHover : "transparent"
                    border.width: 0
                }

                contentItem: Icon {
                    text: Theme.iconArrowRight
                    color: Theme.inkMuted
                    preferredSize: Theme.iconSizeSmall
                }
            }

            PathView {
                id: carousel
                anchors.left: previousButton.right
                anchors.right: nextButton.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: Theme.space2
                anchors.rightMargin: Theme.space2
                model: root.model
                pathItemCount: Math.min(root.itemCount, 5)
                preferredHighlightBegin: 0.5
                preferredHighlightEnd: 0.5
                highlightRangeMode: PathView.StrictlyEnforceRange
                snapMode: PathView.SnapToItem
                maximumFlickVelocity: 1050

                onMovementStarted: root.userInteracted = true
                onCurrentIndexChanged: Qt.callLater(root.activateCurrent)

                path: Path {
                    startX: 20
                    startY: carousel.height * 0.58

                    PathCubic {
                        control1X: carousel.width * 0.24
                        control1Y: carousel.height * 0.44
                        control2X: carousel.width * 0.36
                        control2Y: carousel.height * 0.44
                        x: carousel.width / 2
                        y: carousel.height * 0.50
                    }

                    PathCubic {
                        control1X: carousel.width * 0.64
                        control1Y: carousel.height * 0.44
                        control2X: carousel.width * 0.76
                        control2Y: carousel.height * 0.44
                        x: carousel.width - 20
                        y: carousel.height * 0.58
                    }
                }

                delegate: Item {
                    required property int index
                    required property var modelData

                    readonly property int offsetFromCenter: root.wrappedOffset(index)
                    readonly property int distanceFromCenter: Math.abs(offsetFromCenter)

                    width: 154
                    height: 64
                    scale: PathView.isCurrentItem ? 1.0
                           : distanceFromCenter <= 1 ? 0.86 : 0.74
                    opacity: PathView.isCurrentItem ? 1.0
                             : distanceFromCenter <= 1 ? 0.66 : 0.28
                    z: PathView.isCurrentItem ? 3 : 1

                    Behavior on scale {
                        NumberAnimation {
                            duration: Theme.reduceMotion ? 0 : Theme.motionStandard
                            easing.type: Easing.OutCubic
                        }
                    }

                    Superellipse {
                        anchors.fill: parent
                        visible: PathView.isCurrentItem
                        fillColor: Theme.ink
                        borderWidth: 0
                        borderColor: "transparent"
                        curveDepth: Theme.curveControl
                        elevation: Theme.elevationControl
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 1

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.hello
                            color: PathView.isCurrentItem
                                   ? Theme.contrastInk(Theme.ink) : Theme.inkMuted
                            font.family: Theme.fontFamilyForText(modelData.hello)
                            font.pixelSize: PathView.isCurrentItem
                                            ? Theme.typeTitle3Size : Theme.typeLabelSize
                            font.weight: PathView.isCurrentItem
                                         ? Theme.weightBold : Theme.weightMedium
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: PathView.isCurrentItem
                            text: modelData.language
                            color: Qt.rgba(Theme.contrastInk(Theme.ink).r,
                                           Theme.contrastInk(Theme.ink).g,
                                           Theme.contrastInk(Theme.ink).b, 0.68)
                            font.family: Theme.fontFamilyForText(modelData.language)
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightMedium
                        }
                    }

                    TapHandler {
                        onTapped: {
                            root.userInteracted = true
                            carousel.currentIndex = index
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space1
            Layout.rightMargin: Theme.space1
            spacing: Theme.space2

            Icon {
                text: Theme.iconLanguage
                color: Theme.accent
                preferredSize: Theme.iconSize
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: root.selectedLabel.length > 0
                          ? root.selectedLabel
                          : root.currentEntry ? root.currentEntry.language : ""
                    color: Theme.ink
                    font.pixelSize: Theme.typeLabelSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.selectedDetail.length > 0
                          ? root.selectedDetail
                          : root.currentEntry ? root.currentEntry.tag : ""
                    color: Theme.inkSubtle
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Theme.typeCaptionSize
                    elide: Text.ElideRight
                }
            }

            Label {
                visible: root.userInteracted
                text: qsTrId("genycaster.setup.language_applies_now")
                color: Theme.inkSubtle
                font.pixelSize: Theme.typeCaptionSize
                elide: Text.ElideRight
            }
        }
    }
}
