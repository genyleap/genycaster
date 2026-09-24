import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string statusText: ""
    property string errorText: ""
    property bool runtimeReady: false
    property bool offline: false
    property bool sourceUnavailable: false
    property bool networkKnown: false
    property bool stale: false
    property bool busy: false
    property bool hasUsableContent: false
    property bool compactMode: false
    readonly property bool compactMetadata: root.compactMode || root.width < 900
    readonly property bool iconOnlyMetadata: root.width < 360

    signal retryRequested()
    signal renderingRequested()
    signal taskManagerRequested()
    signal feedbackRequested()

    readonly property bool warningState: !root.runtimeReady || root.offline || root.sourceUnavailable || root.errorText.length > 0
    readonly property string stateLabel: !root.runtimeReady ? qsTrId("genycaster.status.setup_issue")
                                         : root.offline ? qsTrId("genycaster.status.offline")
                                         : root.sourceUnavailable ? qsTrId("genycaster.status.source_issue")
                                         : root.errorText.length > 0 ? qsTrId("genycaster.status.data_issue")
                                         : qsTrId("genycaster.status.live")
    readonly property color stateColor: root.warningState ? Theme.warning : Theme.positive
    readonly property string profileLabel: renderingController.profile === "efficiency"
                                           ? qsTrId("genycaster.rendering.efficiency")
                                           : renderingController.profile === "fidelity"
                                             ? qsTrId("genycaster.rendering.fidelity")
                                             : qsTrId("genycaster.rendering.adaptive")

    implicitHeight: root.compactMode ? 32 : 38
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceRaised
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.strokeHairline
        color: Theme.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compactMode ? Theme.space2 : Theme.space3
        anchors.rightMargin: root.compactMode ? Theme.space2 : Theme.space3
        layoutDirection: Qt.LeftToRight
        spacing: root.compactMode ? Theme.space1 : Theme.space2

        RowLayout {
            Layout.preferredWidth: root.iconOnlyMetadata ? 12 : 112
            Layout.minimumWidth: Layout.preferredWidth
            Layout.maximumWidth: Layout.preferredWidth
            layoutDirection: Theme.uiDirection
            spacing: Theme.space1
            Accessible.role: Accessible.StaticText
            Accessible.name: root.stateLabel

            Superellipse {
                implicitWidth: 7
                implicitHeight: 7
                fillColor: root.stateColor
                curveDepth: 3.5
                exponent: 2.01
            }

            Label {
                visible: !root.iconOnlyMetadata
                Layout.fillWidth: true
                text: root.stateLabel
                color: root.warningState ? Theme.warning : Theme.textSecondary
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }
        }

        Rectangle {
            visible: !root.compactMetadata
            Layout.preferredWidth: 1
            Layout.preferredHeight: 16
            color: Theme.divider
        }

        RowLayout {
            layoutDirection: Theme.uiDirection
            visible: !root.compactMetadata
            Layout.preferredWidth: 120
            Layout.maximumWidth: 120
            spacing: Theme.space1

            Icon { text: nodeProviderIcon; color: Theme.textTertiary; preferredSize: 13 }
            Label {
                Layout.fillWidth: true
                text: nodeProviderLabel
                color: Theme.textTertiary
                font.family: Theme.latinFontFamily
                font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                elide: Text.ElideRight
            }
        }

        Item { Layout.fillWidth: true }

        ActionButton {
            readonly property bool retryable: root.runtimeReady
                && (root.sourceUnavailable || root.errorText.length > 0 || root.offline)
            Layout.preferredWidth: 28
            Layout.minimumWidth: 28
            Layout.maximumWidth: 28
            Layout.preferredHeight: 28
            Layout.maximumHeight: 28
            opacity: retryable ? 1 : 0
            compact: true
            quiet: true
            iconOnly: true
            iconGlyph: Theme.iconRefresh
            text: qsTrId("genycaster.status.retry")
            enabled: retryable && !root.busy
            Accessible.ignored: !retryable
            onClicked: root.retryRequested()
        }

        Superellipse {
            id: taskManagerChip
            visible: miniAppTaskManager.taskCount > 0
            Layout.preferredHeight: 26
            Layout.preferredWidth: root.iconOnlyMetadata ? 32 : root.compactMetadata ? 82 : 150
            Layout.minimumWidth: Layout.preferredWidth
            Layout.maximumWidth: Layout.preferredWidth
            fillColor: taskHover.hovered ? Theme.primarySurface : "transparent"
            borderWidth: Theme.strokeHairline
            borderColor: miniAppTaskManager.hasProcessTelemetry && miniAppTaskManager.totalCpuPercent >= 80
                ? Theme.warning : Theme.borderSubtle
            curveDepth: 13

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space1
                anchors.rightMargin: Theme.space1
                spacing: Theme.space1
                layoutDirection: Theme.uiDirection

                GenySpinner {
                    visible: miniAppTaskManager.launchingCount > 0
                    running: visible
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    color: Theme.accent
                }
                Icon {
                    visible: miniAppTaskManager.launchingCount === 0
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    text: Theme.iconCpu
                    color: miniAppTaskManager.hasProcessTelemetry && miniAppTaskManager.totalCpuPercent >= 80
                        ? Theme.warning : Theme.textSecondary
                    preferredSize: 14
                }
                Label {
                    visible: !root.iconOnlyMetadata
                    Layout.fillWidth: true
                    text: miniAppTaskManager.launchingCount > 0
                        ? qsTrId("genycaster.miniapp.loading") + " · "
                          + LocaleFormat.formatInteger(miniAppTaskManager.launchingCount)
                        : miniAppTaskManager.hasProcessTelemetry
                          ? LocaleFormat.formatNumber(miniAppTaskManager.totalCpuPercent, 1) + "% · "
                            + LocaleFormat.formatInteger(miniAppTaskManager.taskCount)
                          : LocaleFormat.formatInteger(miniAppTaskManager.totalRpcPerSecond) + "/s · "
                            + LocaleFormat.formatInteger(miniAppTaskManager.taskCount)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    font.features: ({ "tnum": 1 })
                    elide: Text.ElideRight
                }
            }

            HoverHandler { id: taskHover; cursorShape: Qt.PointingHandCursor }
            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.taskManagerRequested()
            }
            Accessible.name: qsTrId("genycaster.task_manager.title")
        }

        Superellipse {
            id: renderingChip
            Layout.preferredHeight: 26
            Layout.preferredWidth: root.iconOnlyMetadata ? 28 : root.compactMetadata ? 110 : 250
            Layout.minimumWidth: Layout.preferredWidth
            Layout.maximumWidth: Layout.preferredWidth
            fillColor: renderingHover.hovered ? Theme.surfaceMuted : "transparent"
            borderWidth: Theme.strokeHairline
            borderColor: renderingController.restartRequired ? Theme.warning : Theme.borderSubtle
            curveDepth: 13

            RowLayout {
                layoutDirection: Theme.uiDirection
                id: renderingRow
                anchors.fill: parent
                anchors.leftMargin: Theme.space1
                anchors.rightMargin: Theme.space1
                spacing: Theme.space1

                Icon {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    text: renderingController.profile === "fidelity" ? Theme.iconFidelity
                          : renderingController.profile === "efficiency" ? Theme.iconEnergy
                          : Theme.iconSystem
                    color: renderingController.profile === "fidelity" ? Theme.accent : Theme.textSecondary
                    preferredSize: 14
                }
                Label {
                    visible: !root.iconOnlyMetadata
                    Layout.fillWidth: true
                    text: root.profileLabel
                    color: Theme.textSecondary
                    font.pixelSize: Theme.typeCaptionSize
                    font.weight: Theme.weightDemiBold
                    elide: Text.ElideRight
                }
                Label {
                    visible: !root.compactMetadata && renderingController.profile === "adaptive"
                    text: "· " + renderingController.effectiveQuality
                    color: Theme.textTertiary
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                }
                Label {
                    visible: !root.compactMetadata
                    text: renderingController.graphicsApi
                          + (renderingController.activeSampleCount > 1
                             ? " · " + renderingController.activeSampleCount + "× AA" : "")
                    color: Theme.textTertiary
                    font.family: Theme.latinFontFamily
                    font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
                }
            }

            HoverHandler { id: renderingHover }
            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.renderingRequested()
            }
        }

        Label {
            visible: !root.compactMode && root.width >= 1200
            Layout.preferredWidth: 230
            Layout.minimumWidth: 230
            Layout.maximumWidth: 230
            text: LocaleFormat.formatNumber(renderingController.frameIntervalMs, 1) + " ms · "
                  + LocaleFormat.formatInteger(Math.round(renderingController.refreshRate)) + " Hz · DPR "
                  + LocaleFormat.formatNumber(renderingController.devicePixelRatio, 1) + "×"
            color: Theme.textTertiary
            font.family: Theme.fontFamilyForText(text)
            font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
            font.features: ({ "tnum": 1 })
            elide: Text.ElideRight
        }

        Superellipse {
            id: feedbackButton
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            fillColor: feedbackHover.hovered ? Theme.primarySurface : "transparent"
            borderWidth: Theme.strokeHairline
            borderColor: feedbackHover.hovered ? Theme.accent : Theme.borderSubtle
            curveDepth: 14

            Icon {
                anchors.centerIn: parent
                text: Theme.iconFeedback
                color: feedbackHover.hovered ? Theme.accent : Theme.textSecondary
                preferredSize: Theme.iconSizeSmall
            }

            HoverHandler { id: feedbackHover }
            TapHandler {
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: root.feedbackRequested()
            }
            Accessible.name: qsTrId("genycaster.feedback.title")
        }

        Label {
            visible: !root.compactMetadata
            text: "© " + new Date().getFullYear() + " Genyleap"
            color: Theme.textTertiary
            font.family: Theme.latinFontFamily
            font.pixelSize: Math.max(9, Theme.typeCaptionSize - 1)
        }
    }
}
