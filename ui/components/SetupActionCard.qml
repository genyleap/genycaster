import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Farcaster.Client

Button {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconGlyph: ""
    property url brandImageSource: ""
    property string trailingText: ""
    // Optional measured response-time hint used by read-node selection.
    property int latencyMs: -1
    property bool primary: false
    property bool complete: false
    // Selection is NOT task completion. This is a single-choice radio indicator.
    property bool selected: false
    property bool busy: false
    property bool showChevron: true
    property bool showSelectionMark: true
    property bool compactCard: false
    property bool onboardingStyle: false
    property bool neutralPrimary: false

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: Math.max(compactCard ? 62 : onboardingStyle ? 78 : 96,
                             contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    topPadding: compactCard || onboardingStyle ? Theme.space3 : Theme.space4
    bottomPadding: compactCard || onboardingStyle ? Theme.space3 : Theme.space4

    Accessible.name: title
    Accessible.description: subtitle
    Accessible.checked: root.selected

    // Disabled primary actions use a neutral card background; do not paint
    // white-on-white labels just because the enabled state was prominent.
    // An in-flight action remains visually active while clicks are disabled.
    // Unrelated sign-in options stay neutral, never inheriting its spinner.
    readonly property bool visuallyDisabled: !enabled && !busy
    readonly property color foreground: visuallyDisabled ? Theme.inkMuted
                                      : primary ? (neutralPrimary ? Theme.surface : Theme.accentInk) : Theme.ink
    readonly property color secondaryForeground: visuallyDisabled ? Theme.inkMuted
                                               : primary ? Qt.rgba(foreground.r, foreground.g, foreground.b, 0.84)
                                               : Theme.inkMuted

    background: Superellipse {
        fillColor: root.visuallyDisabled
                   ? Theme.surfaceMuted
                   : root.primary
                     ? (root.neutralPrimary ? Theme.ink
                        : root.down ? Theme.primaryPressed
                                    : root.hovered ? Theme.primaryFillHover : Theme.primaryFill)
                     : root.down ? Theme.surfacePressed
                                 : root.hovered ? Theme.surfaceHover : Theme.surface
        borderWidth: Theme.keyboardFocusVisible(root) ? 2
                     : root.selected ? 2
                     : root.primary && !root.visuallyDisabled ? 0 : Theme.strokeStandard
        borderColor: Theme.keyboardFocusVisible(root) ? Theme.focus
                     : root.selected ? Theme.accent : Theme.borderMuted
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
            visible: !root.busy && root.iconGlyph.length > 0 && root.brandImageSource.toString().length === 0
            text: root.iconGlyph
            color: !root.visuallyDisabled && !root.primary ? Theme.inkMuted : root.foreground
            preferredSize: Theme.iconSize
        }

        Image {
            visible: !root.busy && root.brandImageSource.toString().length > 0
            source: root.brandImageSource
            Layout.preferredWidth: Theme.iconSize + 4
            Layout.preferredHeight: Theme.iconSize + 4
            sourceSize: Qt.size(96, 96)
            fillMode: Image.PreserveAspectFit
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.onboardingStyle ? Theme.space1 : 2

            DirectionAwareLabel {
                Layout.fillWidth: true
                text: root.title
                color: root.foreground
                font.pixelSize: root.compactCard || root.onboardingStyle ? Theme.typeLabelSize : Math.max(Theme.typeLabelSize, 17)
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
                maximumLineCount: root.compactCard ? 1 : root.onboardingStyle ? 3 : 2
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
            visible: root.selected && root.showSelectionMark
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
            visible: root.showChevron && !root.complete
                     && (!root.selected || !root.showSelectionMark) && !root.busy
            text: Theme.rtl ? Theme.iconArrowLeft : Theme.iconArrowRight
            color: root.visuallyDisabled ? Theme.inkMuted
                   : root.primary ? root.secondaryForeground : Theme.inkSubtle
            preferredSize: Theme.iconSizeSmall
        }
    }
}
