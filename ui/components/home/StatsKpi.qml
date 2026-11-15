import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Responsive locale-aware KPI. All rows take their actual implicit height,
// including multi-line footnotes in Arabic/Persian or large accessibility type.
Superellipse {
    id: root
    property string title: ""
    property string iconName: ""
    property string footnote: ""
    property real amount: 0
    property bool available: true
    property real animatedAmount: 0
    readonly property string exactValue: LocaleFormat.formatInteger(Math.max(0, Math.round(amount)))
    readonly property string currentValue: available
        ? LocaleFormat.formatCompactNumber(Math.max(0, animatedAmount)) : "—"

    function updateAnimatedValue() {
        numberTween.stop()
        if (!available) animatedAmount = 0
        else if (Theme.reduceMotion) animatedAmount = Math.max(0, amount)
        else {
            numberTween.from = 0
            numberTween.to = Math.max(0, amount)
            numberTween.start()
        }
    }
    onAmountChanged: updateAnimatedValue()
    onAvailableChanged: updateAnimatedValue()
    Component.onCompleted: updateAnimatedValue()

    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.preferredWidth: 0
    Layout.preferredHeight: Math.max(140, cardContent.implicitHeight + Theme.space3 * 2)
    fillColor: Theme.surface
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveContainer

    // These few summary cards are persistent, high-salience surfaces.
    // A stationary rim is visible on desktop and touch without a hover event.
    NeonHalo {
        anchors.fill: parent
        ambient: true
        cornerRadius: root.curveDepth
        borderInset: root.borderWidth / 2
    }

    NumberAnimation {
        id: numberTween
        target: root
        property: "animatedAmount"
        duration: Theme.reduceMotion ? 0 : 520
        easing.type: Easing.OutCubic
    }

    ColumnLayout {
        id: cardContent
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            layoutDirection: Theme.uiDirection
            spacing: Theme.space1
            DirectionAwareLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                alignToUi: true
                text: root.title
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.textSecondary
                wrapMode: Text.Wrap
            }
            Icon {
                visible: root.iconName.length > 0
                text: root.iconName
                color: Theme.textTertiary
                preferredSize: Theme.iconSizeLarge
            }
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            alignToUi: true
            text: root.currentValue
            font.pixelSize: root.width < 220 ? Math.max(Theme.typeMetaSize+5, Theme.typeTitle1Size-5)
                                             : Theme.typeTitle1Size
            font.weight: Theme.weightBold
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
            Accessible.name: root.title + ": " + (root.available ? root.exactValue : "—")
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            alignToUi: true
            text: root.available && root.amount >= 10000
                  ? root.exactValue : root.footnote
            Accessible.description: root.footnote
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            wrapMode: Text.Wrap
        }
    }
}
