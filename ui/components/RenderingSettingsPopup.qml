import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GenyAdaptiveDialog {
    id: root

    readonly property real hostWidth: parent ? parent.width : 760
    readonly property real hostHeight: parent ? parent.height : 720
    readonly property bool compactLayout: root.compactPresentation || width < 700
    readonly property int qualityColumns: compactLayout ? 1 : 3
    readonly property int requestedSamples: renderingController.preferredSampleCount
    readonly property string activeMsaaText: renderingController.activeSampleCount > 1
                                                 ? LocaleFormat.formatInteger(renderingController.activeSampleCount) + "×"
                                                 : qsTrId("genycaster.rendering.off")
    readonly property string requestedMsaaText: root.requestedSamples > 1
                                                    ? LocaleFormat.formatInteger(root.requestedSamples) + "×"
                                                    : qsTrId("genycaster.rendering.off")

    contentPadding: root.compactPresentation ? Theme.space3 : Theme.space4
    mobileHeightRatio: 0.88
    width: root.compactPresentation
           ? root.hostWidth
           : Math.min(Math.max(420, root.hostWidth - Theme.space6 * 2),
                      root.hostWidth < Theme.breakpointNarrow ? 620 : 780)
    height: root.compactPresentation
            ? Math.min(740, Math.max(380, root.hostHeight * 0.88))
            : Math.min(Math.max(460, contentColumn.implicitHeight + Theme.space4 * 2),
                       Math.max(360, root.hostHeight - Theme.space5 * 2))

    contentItem: ScrollView {
        id: viewport
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: GenyScrollBar {}

        ColumnLayout {
            id: contentColumn
            width: viewport.availableWidth
            spacing: root.compactLayout ? Theme.space3 : Theme.space4

            ModalHeader {
                Layout.fillWidth: true
                title: qsTrId("genycaster.rendering.title")
                subtitle: qsTrId("genycaster.rendering.subtitle")
                onCloseRequested: root.close()
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: experimentalRow.implicitHeight + Theme.space3 * 2
                fillColor: Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: Theme.warning
                curveDepth: Theme.curveControl

                RowLayout {
                    id: experimentalRow
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space2
                    layoutDirection: Theme.uiDirection

                    Superellipse {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        fillColor: Theme.surfaceRaised
                        borderWidth: Theme.strokeHairline
                        borderColor: Theme.warning
                        curveDepth: Theme.curveCompact

                        Icon {
                            anchors.centerIn: parent
                            text: Theme.iconWarning
                            color: Theme.warning
                            preferredSize: Theme.iconSizeSmall
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.rendering.experimental")
                            color: Theme.warning
                            font.pixelSize: Theme.typeCaptionSize
                            font.weight: Theme.weightBold
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.rendering.experimental_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.qualityColumns
                uniformCellWidths: true
                layoutDirection: Theme.uiDirection
                rowSpacing: Theme.space2
                columnSpacing: Theme.space2

                Repeater {
                    model: [
                        { id: "efficiency", title: qsTrId("genycaster.rendering.efficiency"),
                          description: qsTrId("genycaster.rendering.efficiency_description"), icon: Theme.iconEnergy },
                        { id: "adaptive", title: qsTrId("genycaster.rendering.adaptive"),
                          description: qsTrId("genycaster.rendering.adaptive_description"), icon: Theme.iconSystem },
                        { id: "fidelity", title: qsTrId("genycaster.rendering.fidelity"),
                          description: qsTrId("genycaster.rendering.fidelity_description"), icon: Theme.iconFidelity }
                    ]

                    delegate: Superellipse {
                        id: optionCard
                        required property var modelData
                        readonly property bool selected: renderingController.profile === modelData.id
                        readonly property bool recommended: modelData.id === "adaptive"

                        Layout.fillWidth: true
                        Layout.minimumHeight: root.compactLayout ? 104 : 142
                        Layout.preferredHeight: Math.max(Layout.minimumHeight, optionContent.implicitHeight + Theme.space3 * 2)
                        fillColor: selected ? Theme.primarySurface : optionHover.hovered ? Theme.surfaceInteractive : Theme.surfaceRaised
                        borderWidth: selected ? Theme.strokeEmphasis : Theme.strokeHairline
                        borderColor: selected ? Theme.accent : recommended ? Theme.borderStrong : Theme.borderSubtle
                        curveDepth: Theme.curveContainer

                        Behavior on fillColor { ColorAnimation { duration: Theme.motionFast } }
                        Behavior on borderColor { ColorAnimation { duration: Theme.motionFast } }

                        ColumnLayout {
                            id: optionContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Theme.space3
                            spacing: Theme.space2

                            RowLayout {
                                Layout.fillWidth: true
                                layoutDirection: Theme.uiDirection
                                spacing: Theme.space2

                                Superellipse {
                                    Layout.preferredWidth: root.compactLayout ? 38 : 42
                                    Layout.preferredHeight: Layout.preferredWidth
                                    fillColor: optionCard.selected ? Theme.surfaceSelected : Theme.surfaceMuted
                                    borderWidth: Theme.strokeHairline
                                    borderColor: optionCard.selected ? Theme.accent : Theme.borderSubtle
                                    curveDepth: Theme.curveControl

                                    Icon {
                                        anchors.centerIn: parent
                                        text: optionCard.modelData.icon
                                        color: optionCard.selected ? Theme.accent : Theme.textSecondary
                                        preferredSize: Theme.iconSizeLarge
                                    }
                                }

                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: optionCard.modelData.title
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeBodySize
                                    font.weight: Theme.weightBold
                                    elide: Text.ElideRight
                                }

                                Icon {
                                    visible: optionCard.recommended && !optionCard.selected
                                    text: Theme.iconCheck
                                    color: Theme.positive
                                    preferredSize: Theme.iconSizeSmall
                                    Accessible.name: qsTrId("genycaster.rendering.adaptive_description")
                                }

                                GenyRadioIndicator {
                                    checked: optionCard.selected
                                }
                            }

                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: optionCard.modelData.description
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                wrapMode: Text.Wrap
                                maximumLineCount: root.compactLayout ? 4 : 5
                                elide: Text.ElideRight
                            }
                        }

                        HoverHandler { id: optionHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: renderingController.setProfile(optionCard.modelData.id)
                        }
                    }
                }
            }

            Superellipse {
                Layout.fillWidth: true
                Layout.preferredHeight: telemetryGrid.implicitHeight + Theme.space3 * 2
                fillColor: Theme.surfaceMuted
                borderWidth: Theme.strokeHairline
                borderColor: Theme.borderSubtle
                curveDepth: Theme.curveContainer

                GridLayout {
                    id: telemetryGrid
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    columns: root.compactLayout ? 2 : 4
                    uniformCellWidths: false
                    rowSpacing: Theme.space2
                    columnSpacing: root.compactLayout ? Theme.space3 : Theme.space4
                    layoutDirection: Theme.uiDirection

                    DirectionAwareLabel { text: qsTrId("genycaster.rendering.backend"); color: Theme.inkMuted; font.pixelSize: Theme.typeCaptionSize }
                    Label { text: renderingController.graphicsApi; color: Theme.ink; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold }
                    // FPS is recognizable to readers; milliseconds are a
                    // pacing interval, not an observed benchmark frame rate.
                    DirectionAwareLabel { text: qsTrId("genycaster.rendering.frame_target"); color: Theme.inkMuted; font.pixelSize: Theme.typeCaptionSize }
                    Label { text: renderingController.frameIntervalMs > 0
                                 ? LocaleFormat.formatInteger(Math.round(1000 / renderingController.frameIntervalMs)) + " FPS"
                                 : "—"; color: Theme.ink; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold }
                    DirectionAwareLabel { text: qsTrId("genycaster.rendering.display"); color: Theme.inkMuted; font.pixelSize: Theme.typeCaptionSize }
                    Label { text: LocaleFormat.formatInteger(Math.round(renderingController.refreshRate)) + " Hz · DPR " + LocaleFormat.formatNumber(renderingController.devicePixelRatio, 1) + "×"; color: Theme.ink; font.family: Theme.fontFamilyForText(text); font.pixelSize: Theme.typeCaptionSize; font.weight: Theme.weightDemiBold }
                    DirectionAwareLabel { text: qsTrId("genycaster.rendering.msaa"); color: Theme.inkMuted; font.pixelSize: Theme.typeCaptionSize }
                    Label {
                        text: renderingController.restartRequired
                              ? root.activeMsaaText + "  →  " + root.requestedMsaaText
                              : root.activeMsaaText
                        color: renderingController.restartRequired ? Theme.warning : Theme.ink
                        font.family: Theme.fontFamilyForText(text)
                        font.pixelSize: Theme.typeCaptionSize
                        font.weight: Theme.weightDemiBold
                    }
                }
            }

            DirectionAwareLabel {
                visible: renderingController.restartRequired
                Layout.fillWidth: true
                text: qsTrId("genycaster.rendering.restart_required")
                color: Theme.warning
                font.pixelSize: Theme.typeCaptionSize
                wrapMode: Text.Wrap
            }
        }
    }
}
