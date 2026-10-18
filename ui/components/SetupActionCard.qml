import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconGlyph: ""
    property string trailingText: ""
    // Optional measured response-time hint used by read-node selection.
    property int latencyMs: -1
    property bool primary: false
    property bool complete: false
    // Selection is NOT task completion. This is a single-choice radio indicator.
    property bool selected: false
    property bool busy: false
    property bool showChevron: true
    property bool compactCard: false

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(compactCard ? 62 : 78,
                             contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    topPadding: Theme.space3
    bottomPadding: Theme.space3

    Accessible.name: title
    Accessible.description: subtitle
    Accessible.checked: root.selected

    // Disabled primary actions use a neutral card background; do not paint
    // white-on-white labels just because the enabled state was prominent.
    // An in-flight action remains visually active while clicks are disabled.
    // Unrelated sign-in options stay neutral, never inheriting its spinner.
    readonly property bool visuallyDisabled: !enabled && !busy
    readonly property color foreground: visuallyDisabled ? Theme.inkMuted
                                      : primary ? Theme.accentInk : Theme.ink
    readonly property color secondaryForeground: visuallyDisabled ? Theme.inkMuted
                                               : primary ? Qt.rgba(foreground.r, foreground.g, foreground.b, 0.84)
                                               : Theme.inkMuted

    background: Superellipse {
        fillColor: root.visuallyDisabled
                   ? Theme.surfaceMuted
                   : root.primary
                     ? (root.down ? Theme.primaryPressed
                                  : root.hovered ? Theme.primaryFillHover : Theme.primaryFill)
                     : root.down ? Theme.surfacePressed
                                 : root.hovered ? Theme.surfaceHover : Theme.surface
        borderWidth: root.primary && !root.visuallyDisabled ? 0 : (Theme.keyboardFocusVisible(root) ? 2 : Theme.strokeStandard)
        borderColor: Theme.keyboardFocusVisible(root) ? Theme.focus : Theme.borderMuted
        curveDepth: Theme.curveContainer
        elevation: root.primary && !root.visuallyDisabled ? Theme.elevationControl : Theme.elevationNone

        Behavior on fillColor {
            ColorAnimation { duration: Theme.reduceMotion ? 0 : Theme.motionFast }
        }
    }

    contentItem: RowLayout {
        spacing: Theme.space3
        layoutDirection: Theme.uiDirection

        GenySpinner {
            visible: root.busy
            running: visible
            color: root.foreground
            Layout.preferredWidth: Theme.iconSize
            Layout.preferredHeight: Theme.iconSize
        }

        Icon {
            visible: !root.busy && root.iconGlyph.length > 0
            text: root.iconGlyph
            color: !root.visuallyDisabled && !root.primary ? Theme.inkMuted : root.foreground
            preferredSize: Theme.iconSize
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.title
                color: root.foreground
                font.pixelSize: Theme.typeLabelSize
                font.weight: Theme.weightDemiBold
                horizontalAlignment: Theme.textStartAlignment
                elide: Text.ElideRight
            }

            DirectionAwareLabel {
                visible: root.subtitle.length > 0
                Layout.fillWidth: true
                text: root.subtitle
                color: root.secondaryForeground
                font.pixelSize: Theme.typeCaptionSize
                font.weight: Theme.weightMedium
                horizontalAlignment: Theme.textStartAlignment
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: root.compactCard ? 1 : 2
            }
        }

        Label {
            visible: root.trailingText.length > 0
            text: root.trailingText
            color: root.primary ? root.secondaryForeground
                   : root.latencyMs < 0 ? Theme.inkSubtle
                   : root.latencyMs <= 300 ? Theme.success
                   : root.latencyMs <= 800 ? Theme.warning : Theme.danger
            font.pixelSize: Theme.typeCaptionSize
            font.weight: Theme.weightDemiBold
        }

        Icon {
            visible: root.selected
            text: Theme.iconCheck
            color: root.primary ? root.foreground : Theme.accent
            preferredSize: Theme.iconSize
        }

        Icon {
            visible: root.complete && !root.selected
            text: Theme.iconCheck
            color: root.foreground
            preferredSize: Theme.iconSizeSmall
        }

        Icon {
            visible: root.showChevron && !root.complete && !root.selected && !root.busy
            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
            color: root.visuallyDisabled ? Theme.inkMuted
                   : root.primary ? root.secondaryForeground : Theme.inkSubtle
            preferredSize: Theme.iconSizeSmall
        }
    }
}
