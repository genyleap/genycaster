import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconGlyph: ""
    property string trailingText: ""
    property bool primary: false
    property bool complete: false
    property bool busy: false
    property bool showChevron: true
    property bool compactCard: false

    hoverEnabled: true
    implicitHeight: compactCard ? 62 : 78
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    topPadding: 0
    bottomPadding: 0

    readonly property color foreground: primary ? Theme.contrastInk(Theme.ink) : Theme.ink
    readonly property color secondaryForeground: primary
                                                 ? Qt.rgba(foreground.r, foreground.g, foreground.b, 0.68)
                                                 : Theme.inkMuted

    background: Superellipse {
        fillColor: !root.enabled
                   ? Theme.surfaceMuted
                   : root.primary
                     ? (root.down ? Qt.darker(Theme.ink, 1.06)
                                  : root.hovered ? Qt.lighter(Theme.ink, 1.10) : Theme.ink)
                     : root.down ? Theme.surfacePressed
                                 : root.hovered ? Theme.surfaceHover : Theme.surface
        borderWidth: root.primary ? 0 : Theme.strokeStandard
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveContainer
        elevation: root.primary && root.enabled ? Theme.elevationControl : Theme.elevationNone

        Behavior on fillColor {
            ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }
    }

    contentItem: RowLayout {
        spacing: Theme.space3

        GenySpinner {
            visible: root.busy
            running: visible
            color: root.primary ? root.foreground : Theme.inkMuted
            Layout.preferredWidth: Theme.iconSize
            Layout.preferredHeight: Theme.iconSize
        }

        Icon {
            visible: !root.busy && root.iconGlyph.length > 0
            text: root.iconGlyph
            color: root.primary ? root.foreground : Theme.inkMuted
            preferredSize: Theme.iconSize
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.title
                color: root.foreground
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                elide: Text.ElideRight
            }

            Label {
                visible: root.subtitle.length > 0
                Layout.fillWidth: true
                text: root.subtitle
                color: root.secondaryForeground
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightMedium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        Label {
            visible: root.trailingText.length > 0
            text: root.trailingText
            color: root.primary ? root.secondaryForeground : Theme.inkSubtle
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightDemiBold
        }

        Icon {
            visible: root.complete
            text: Theme.iconCheck
            color: root.primary ? root.foreground : Theme.accent
            preferredSize: Theme.iconSizeSmall
        }

        Icon {
            visible: root.showChevron && !root.complete && !root.busy
            text: Theme.iconArrowRight
            color: root.primary ? root.secondaryForeground : Theme.inkSubtle
            preferredSize: Theme.iconSizeSmall
        }
    }
}
