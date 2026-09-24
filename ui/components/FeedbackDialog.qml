import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GenyAdaptiveDialog {
    id: root

    property string category: "improvement"
    property int sentiment: 3

    readonly property var sentimentModel: [
        { value: 1, icon: Theme.iconMoodFrustrated },
        { value: 2, icon: Theme.iconMoodSad },
        { value: 3, icon: Theme.iconMoodNeutral },
        { value: 4, icon: Theme.iconMoodPositive },
        { value: 5, icon: Theme.iconMoodExcited }
    ]
    readonly property var categoryModel: [
        { id: "bug", label: qsTrId("genycaster.feedback.bug"), icon: Theme.iconWarning },
        { id: "performance", label: qsTrId("genycaster.feedback.performance"), icon: Theme.iconChart },
        { id: "improvement", label: qsTrId("genycaster.feedback.improvement"), icon: Theme.iconFidelity },
        { id: "idea", label: qsTrId("genycaster.feedback.idea"), icon: Theme.iconInfo }
    ]

    contentPadding: root.compactPresentation ? Theme.space3 : Theme.space4
    mobileHeightRatio: 0.90
    width: root.compactPresentation
           ? (parent ? parent.width : 420)
           : Math.min(parent ? parent.width - Theme.space6 * 2 : 620, 620)
    height: root.compactPresentation
            ? Math.min(parent ? parent.height * 0.90 : 690, 720)
            : Math.min(parent ? parent.height - Theme.space6 * 2 : 690, 690)

    function structuredFeedback() {
        return "Category: " + root.category
             + "\nSentiment: " + root.sentiment + "/5"
             + "\n\n" + feedbackField.text.trim()
    }

    onDidOpen: feedbackField.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: Theme.space4

        ModalHeader {
            Layout.fillWidth: true
            title: qsTrId("genycaster.feedback.title")
            subtitle: qsTrId("genycaster.feedback.subtitle")
            onCloseRequested: root.close()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: qsTrId("genycaster.feedback.subtitle")
                color: Theme.inkMuted
                font.pixelSize: Theme.typeCaptionSize
            }

            RowLayout {
                Layout.fillWidth: true
                layoutDirection: Theme.uiDirection
                spacing: Theme.space2

                Repeater {
                    model: root.sentimentModel
                    delegate: Superellipse {
                        id: moodButton
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        fillColor: root.sentiment === modelData.value ? Theme.primarySurface : Theme.surfaceMuted
                        borderWidth: root.sentiment === modelData.value ? Theme.strokeEmphasis : Theme.strokeHairline
                        borderColor: root.sentiment === modelData.value ? Theme.accent : Theme.borderSubtle
                        curveDepth: Theme.curveControl
                        scale: root.sentiment === modelData.value ? 1.02 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast; easing.type: Easing.OutCubic }
                        }

                        Icon {
                            anchors.centerIn: parent
                            text: moodButton.modelData.icon
                            color: root.sentiment === moodButton.modelData.value ? Theme.accent : Theme.inkMuted
                            preferredSize: Theme.iconSizeLarge
                        }
                        HoverHandler { id: moodHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: root.sentiment = moodButton.modelData.value }
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width < 430 ? 1 : 2
            rowSpacing: Theme.space2
            columnSpacing: Theme.space2
            layoutDirection: Theme.uiDirection

            Repeater {
                model: root.categoryModel
                delegate: Superellipse {
                    id: categoryCard
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    fillColor: root.category === modelData.id ? Theme.primarySurface : Theme.surfaceMuted
                    borderWidth: root.category === modelData.id ? Theme.strokeEmphasis : Theme.strokeHairline
                    borderColor: root.category === modelData.id ? Theme.accent : Theme.borderSubtle
                    curveDepth: Theme.curveControl

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2
                        layoutDirection: Theme.uiDirection
                        Icon {
                            text: categoryCard.modelData.icon
                            color: root.category === categoryCard.modelData.id ? Theme.accent : Theme.inkMuted
                            preferredSize: Theme.iconSizeSmall
                        }
                        DirectionAwareLabel {
                            Layout.fillWidth: true
                            text: categoryCard.modelData.label
                            color: root.category === categoryCard.modelData.id ? Theme.accent : Theme.ink
                            font.pixelSize: Theme.typeLabelSize
                            font.weight: Theme.weightDemiBold
                        }
                        Icon {
                            visible: root.category === categoryCard.modelData.id
                            text: Theme.iconCheck
                            color: Theme.accent
                            preferredSize: Theme.iconSizeSmall
                        }
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.category = categoryCard.modelData.id }
                }
            }
        }

        Superellipse {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 170
            fillColor: Theme.editorSurface
            borderWidth: feedbackField.activeFocus ? Theme.strokeEmphasis : Theme.strokeHairline
            borderColor: feedbackField.activeFocus ? Theme.accent : Theme.borderSubtle
            curveDepth: Theme.curveLarge

            DirectionAwareTextArea {
                id: feedbackField
                anchors.fill: parent
                anchors.margins: Theme.space3
                placeholderText: qsTrId("genycaster.feedback.placeholder")
                wrapMode: TextEdit.Wrap
                background: null
            }
        }

        Superellipse {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            fillColor: Theme.surfaceMuted
            borderWidth: Theme.strokeHairline
            borderColor: Theme.borderSubtle
            curveDepth: Theme.curveControl

            RowLayout {
                layoutDirection: Theme.uiDirection
                anchors.fill: parent
                anchors.margins: Theme.space3
                spacing: Theme.space2
                Icon { text: Theme.iconInfo; color: Theme.accent; preferredSize: Theme.iconSizeSmall }
                DirectionAwareLabel {
                    Layout.fillWidth: true
                    text: qsTrId("genycaster.feedback.backend_pending")
                    color: Theme.inkMuted
                    font.pixelSize: Theme.typeCaptionSize
                    wrapMode: Text.Wrap
                }
            }
        }

        RowLayout {
            layoutDirection: Theme.uiDirection
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            ActionButton {
                text: qsTrId("genycaster.feedback.copy")
                iconGlyph: Theme.iconCopy
                enabled: feedbackField.text.trim().length > 0
                onClicked: presentationBridge.copyText(root.structuredFeedback())
            }
            ActionButton {
                text: qsTrId("genycaster.feedback.done")
                onClicked: root.close()
            }
        }
    }
}
