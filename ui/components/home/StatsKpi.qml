import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

// Reusable integer KPI with locale-aware exact/compact formatting.
Superellipse {
    id: root
    property string title: ""
    property string iconName: ""
    property string footnote: ""
    property real amount: 0
    property bool available: true
    property real animatedAmount: 0
    readonly property string exactValue: Number(Math.max(0, Math.round(amount))).toLocaleString(Qt.locale(), "f", 0)
    readonly property string currentValue: {
        if (!available) return "—"
        const number = Math.max(0, animatedAmount)
        if (number >= 1000000)
            return Number(number / 1000000).toLocaleString(Qt.locale(), "f", 1) + "M"
        if (number >= 10000)
            return Number(number / 1000).toLocaleString(Qt.locale(), "f", 1) + "K"
        return Number(Math.round(number)).toLocaleString(Qt.locale(), "f", 0)
    }
    function updateAnimatedValue() {
        numberTween.stop()
        if (!available) {
            animatedAmount = 0
        } else if (Theme.reduceMotion) {
            animatedAmount = Math.max(0, amount)
        } else {
            numberTween.from = 0
            numberTween.to = Math.max(0, amount)
            numberTween.start()
        }
    }
    onAmountChanged: updateAnimatedValue()
    onAvailableChanged: updateAnimatedValue()
    Component.onCompleted: updateAnimatedValue()

    Layout.fillWidth: true
    Layout.preferredWidth: 0
    Layout.preferredHeight: 104
    fillColor: Theme.surface
    borderWidth: 1
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveContainer

    NumberAnimation {
        id: numberTween
        target: root
        property: "animatedAmount"
        duration: Theme.reduceMotion ? 0 : 520
        easing.type: Easing.OutCubic
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space1

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space1
            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Theme.typeCaptionSize
                color: Theme.textSecondary
                elide: Text.ElideRight
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
            text: root.currentValue
            font.pixelSize: Theme.typeTitle1Size
            font.weight: Theme.weightBold
            color: Theme.textPrimary
            elide: Text.ElideRight
            Accessible.name: root.title + ": " + (root.available ? root.exactValue : "—")
        }

        DirectionAwareLabel {
            Layout.fillWidth: true
            text: root.available && root.amount >= 10000
                  ? root.exactValue : root.footnote
            Accessible.description: root.footnote
            color: Theme.textTertiary
            font.pixelSize: Theme.typeCaptionSize
            elide: Text.ElideRight
        }
    }
}
