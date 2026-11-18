import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Item {
    id: root

    readonly property bool compactPresentation: parent ? parent.width < Theme.breakpointMobile : false
    readonly property bool opened: (desktopDialog && desktopDialog.opened)
                                  || (mobileDrawer && mobileDrawer.opened)

    function open() {
        if (!desktopDialog || !mobileDrawer) return
        if (root.compactPresentation) {
            desktopDialog.close()
            mobileDrawer.open()
        } else {
            mobileDrawer.close()
            desktopDialog.open()
        }
    }

    function close() {
        if (desktopDialog) desktopDialog.close()
        if (mobileDrawer) mobileDrawer.close()
    }

    function memoryText(bytes) {
        const value = Number(bytes || 0)
        if (value <= 0)
            return "—"
        if (value >= 1073741824)
            return LocaleFormat.formatNumber(value / 1073741824, 1) + " GB"
        return LocaleFormat.formatNumber(value / 1048576, 1) + " MB"
    }

    function stateText(state) {
        switch (String(state || "")) {
        case "foreground": return qsTrId("genycaster.task_manager.foreground")
        case "minimized": return qsTrId("genycaster.task_manager.minimized")
        case "background": return qsTrId("genycaster.feed_header.open")
        case "resolving":
        case "loading": return qsTrId("genycaster.miniapp.loading")
        case "error": return qsTrId("genycaster.task_manager.error")
        default: return String(state || "")
        }
    }

    Component {
        id: taskManagerContent

        Item {
            id: contentRoot

            property bool compactMode: false
            readonly property bool narrow: compactMode || width < Theme.scaleUi(470)
            readonly property real maximumListHeight: narrow
                ? Theme.scaleUi(302) : Theme.scaleUi(366)
            readonly property real listHeight: miniAppTaskManager.taskCount > 0
                                                   ? Math.min(taskList.contentHeight, maximumListHeight)
                                                   : 0

            implicitWidth: 500
            implicitHeight: contentLayout.implicitHeight

            ColumnLayout {
                id: contentLayout
                width: parent.width
                spacing: Theme.space3

                ModalHeader {
                    Layout.fillWidth: true
                    title: qsTrId("genycaster.task_manager.title")
                    subtitle: qsTrId("genycaster.task_manager.subtitle")
                    onCloseRequested: root.close()
                }

                // Summary and session list use the same horizontal inset.
                // The live state is informational, not a competing action.
                Superellipse {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.scaleUi(62)
                    fillColor: Theme.surfaceMuted
                    borderColor: Theme.borderSubtle
                    borderWidth: Theme.strokeHairline
                    curveDepth: Theme.curveControl

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space3
                        layoutDirection: Theme.uiDirection

                        Superellipse {
                            Layout.preferredWidth: Theme.scaleUi(36)
                            Layout.preferredHeight: Theme.scaleUi(36)
                            fillColor: Theme.primarySurface
                            borderWidth: 0
                            curveDepth: Theme.curveCompact
                            Icon {
                                anchors.centerIn: parent
                                text: Theme.iconCpu
                                color: Theme.accent
                                preferredSize: Theme.iconSizeSmall
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 0
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: LocaleFormat.formatInteger(miniAppTaskManager.taskCount)
                                      + " · " + LocaleFormat.formatInteger(miniAppTaskManager.totalRpcPerSecond)
                                      + "/s"
                                color: Theme.ink
                                font.pixelSize: Theme.typeMetaSize
                                font.weight: Theme.weightDemiBold
                            }
                            DirectionAwareLabel {
                                Layout.fillWidth: true
                                text: miniAppTaskManager.hasProcessTelemetry
                                      ? LocaleFormat.formatNumber(miniAppTaskManager.totalCpuPercent, 1) + "% · "
                                        + root.memoryText(miniAppTaskManager.totalMemoryBytes)
                                      : qsTrId("genycaster.task_manager.sdk_rate")
                                color: Theme.inkSubtle
                                font.pixelSize: Theme.typeCaptionSize
                                elide: Text.ElideRight
                            }
                        }

                        RowLayout {
                            layoutDirection: Theme.uiDirection
                            Layout.alignment: Qt.AlignVCenter
                            spacing: Theme.space1
                            Superellipse {
                                Layout.preferredWidth: Theme.scaleUi(6)
                                Layout.preferredHeight: Theme.scaleUi(6)
                                fillColor: miniAppTaskManager.taskCount > 0 ? Theme.success : Theme.inkSubtle
                                borderWidth: 0
                                curveDepth: 3
                            }
                            DirectionAwareLabel {
                                text: qsTrId("genycaster.status.live")
                                color: Theme.inkMuted
                                font.pixelSize: Theme.typeCaptionSize
                                font.weight: Theme.weightMedium
                            }
                        }
                    }
                }

                Item {
                    visible: miniAppTaskManager.taskCount === 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: contentRoot.compactMode ? 116 : 132

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - Theme.space6, 320)
                        spacing: Theme.space1

                        Icon {
                            Layout.alignment: Qt.AlignHCenter
                            text: Theme.iconCpu
                            color: Theme.inkSubtle
                            preferredSize: Theme.iconSizeLarge
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.task_manager.empty")
                            color: Theme.ink
                            font.pixelSize: Theme.typeMetaSize
                            font.weight: Theme.weightDemiBold
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: qsTrId("genycaster.task_manager.empty_description")
                            color: Theme.inkMuted
                            font.pixelSize: Theme.typeCaptionSize
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }
                    }
                }

                ListView {
                    id: taskList
                    visible: miniAppTaskManager.taskCount > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: contentRoot.listHeight
                    clip: true
                    spacing: Theme.space2
                    model: miniAppTaskManager.tasks
                    boundsBehavior: Flickable.StopAtBounds
                    reuseItems: true
                    ScrollBar.vertical: GenyScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Superellipse {
                        id: taskCard
                        required property var modelData
                        readonly property bool foreground: Boolean(modelData.foreground)

                        width: taskList.width
                        height: Math.max(Theme.scaleUi(contentRoot.narrow ? 88 : 82),
                                         taskRow.implicitHeight + Theme.space3 * 2)
                        fillColor: foreground ? Theme.primarySurface : Theme.surfaceRaised
                        borderWidth: foreground ? Theme.strokeEmphasis : Theme.strokeHairline
                        borderColor: foreground ? Theme.accent : Theme.borderSubtle
                        curveDepth: Theme.curveControl

                        RowLayout {
                            id: taskRow
                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space2
                            layoutDirection: Theme.uiDirection

                            SuperellipseImage {
                                Layout.preferredWidth: Theme.scaleUi(contentRoot.narrow ? 36 : 42)
                                Layout.preferredHeight: Layout.preferredWidth
                                Layout.alignment: Qt.AlignVCenter
                                source: String(taskCard.modelData.iconSource || "")
                                fillMode: Image.PreserveAspectFit
                                mediaCategory: "miniapps"
                                forceAutomaticDownload: true
                                showManualLoadIndicator: false
                                fillColor: Theme.surfaceMuted
                                borderWidth: 0
                                curveDepth: Theme.curveControl
                                Icon {
                                    z: 10
                                    anchors.centerIn: parent
                                    visible: String(taskCard.modelData.iconSource || "").length === 0
                                    text: Theme.iconApps
                                    color: taskCard.foreground ? Theme.accent : Theme.inkMuted
                                    preferredSize: Theme.iconSizeSmall
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 1
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: String(taskCard.modelData.title || taskCard.modelData.domain || "Mini App")
                                    color: Theme.ink
                                    font.pixelSize: Theme.typeMetaSize
                                    font.weight: Theme.weightDemiBold
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: root.stateText(taskCard.modelData.state)
                                          + " · " + String(taskCard.modelData.domain || "")
                                    forceLeftToRight: false
                                    color: taskCard.foreground ? Theme.accent : Theme.inkMuted
                                    font.pixelSize: Theme.typeCaptionSize
                                    elide: Text.ElideMiddle
                                    maximumLineCount: 1
                                }
                                DirectionAwareLabel {
                                    Layout.fillWidth: true
                                    text: Boolean(taskCard.modelData.resourceTelemetryAvailable)
                                          ? LocaleFormat.formatInteger(Number(taskCard.modelData.rpcPerSecond || 0)) + "/s · "
                                            + LocaleFormat.formatNumber(Number(taskCard.modelData.cpuPercent || 0), 1) + "% · "
                                            + root.memoryText(taskCard.modelData.memoryBytes)
                                          : LocaleFormat.formatInteger(Number(taskCard.modelData.rpcPerSecond || 0)) + "/s"
                                    color: Theme.inkSubtle
                                    font.pixelSize: Theme.typeCaptionSize
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignVCenter
                                spacing: Theme.space1
                                layoutDirection: Theme.uiDirection
                                ActionButton {
                                    visible: !taskCard.foreground
                                    compact: true
                                    quiet: true
                                    iconOnly: true
                                    iconGlyph: Theme.iconPlay
                                    text: qsTrId("genycaster.task_manager.focus")
                                    onClicked: {
                                        const taskId = String(taskCard.modelData.taskId || "")
                                        root.close()
                                        Qt.callLater(() => miniAppTaskManager.focusTask(taskId))
                                    }
                                }
                                ActionButton {
                                    visible: taskCard.foreground
                                    compact: true
                                    quiet: true
                                    positive: taskCard.foreground
                                    iconOnly: true
                                    iconGlyph: Theme.iconCompress
                                    text: qsTrId("genycaster.task_manager.minimize")
                                    onClicked: miniAppTaskManager.minimizeTask(
                                        String(taskCard.modelData.taskId || ""))
                                }
                                ActionButton {
                                    compact: true
                                    quiet: true
                                    destructive: true
                                    iconOnly: true
                                    iconGlyph: Theme.iconTrash
                                    text: qsTrId("genycaster.task_manager.terminate")
                                    onClicked: miniAppTaskManager.terminateTask(
                                        String(taskCard.modelData.taskId || ""))
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    GenyModal {
        id: desktopDialog
        parent: root.parent
        padding: Theme.space4
        width: Math.min(parent ? parent.width - Theme.space6 * 2 : 560, 560)
        height: Math.min(parent ? parent.height - Theme.space6 * 2 : 620,
                         Math.max(220, desktopContent.implicitHeight + topPadding + bottomPadding))

        contentItem: Loader {
            id: desktopContent
            sourceComponent: taskManagerContent
            onLoaded: item.compactMode = false
        }
    }

    GenyDrawer {
        id: mobileDrawer
        parent: root.parent
        bottomSheet: true
        dimBackground: true
        showHandle: true
        contentPadding: Theme.space3
        width: parent ? parent.width : 0
        height: Math.min(parent ? parent.height * 0.72 : 520,
                         Math.max(230, mobileContent.implicitHeight + topPadding + bottomPadding))

        contentItem: Loader {
            id: mobileContent
            sourceComponent: taskManagerContent
            onLoaded: item.compactMode = true
        }
    }
}
