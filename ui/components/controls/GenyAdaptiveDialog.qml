import QtQuick
import QtQuick.Controls
import Farcaster.Client

/**
 * Adaptive transient surface. Mobile presentations use a true bottom Drawer;
 * desktop presentations use the standard centered GenyModal.
 */
Item {
    id: control

    property Item contentItem: null
    property Item blurSourceItem: null
    // Opt-in: static, downsampled backdrop for expensive desktop inspectors.
    // Other dialogs keep their existing Gaussian presentation.
    property bool efficientBackdropBlur: false
    property int contentPadding: Theme.space3
    property bool dimBackground: true
    property int closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    property real mobileBottomPaddingExtra: 0
    // Explicit presentation contract: compact confirmation prompts remain
    // short gesture-dismissable sheets, while scrollable forms opt into a
    // full-screen inspector with their own Close control and no pull handle.
    property bool mobileFullScreen: false
    property bool desktopFullScreen: false
    property bool showHandle: true
    property real mobileHeightRatio: 0.90
    property real popupZ: 1000
    readonly property bool compactPresentation: Qt.platform.os === "android"
        || Qt.platform.os === "ios"
        || (parent ? parent.width < Theme.breakpointMobile : false)
    readonly property bool opened: (desktopDialog && desktopDialog.opened)
                                  || (mobileDrawer && mobileDrawer.opened)

    signal didOpen()
    signal didClose()

    property bool switchingPresentation: false

    function attachContent(host) {
        if (!control.contentItem || !host)
            return
        control.contentItem.parent = host
        control.contentItem.x = 0
        control.contentItem.y = 0
        control.contentItem.width = Qt.binding(() => host ? host.width : 0)
        control.contentItem.height = Qt.binding(() => host ? host.height : 0)
    }

    function open() {
        if (!desktopDialog || !mobileDrawer) return
        if (control.compactPresentation) {
            control.switchingPresentation = desktopDialog.opened
            if (desktopDialog.opened)
                desktopDialog.close()
            control.attachContent(mobileContentHost)
            mobileDrawer.open()
        } else {
            control.switchingPresentation = mobileDrawer.opened
            if (mobileDrawer.opened)
                mobileDrawer.close()
            control.attachContent(desktopContentHost)
            desktopDialog.open()
        }
        control.switchingPresentation = false
    }

    function close() {
        if (desktopDialog) desktopDialog.close()
        if (mobileDrawer) mobileDrawer.close()
    }

    onCompactPresentationChanged: {
        if (control.opened)
            Qt.callLater(control.open)
    }

    Component.onCompleted: control.attachContent(stagingHost)

    Item {
        id: stagingHost
        visible: false
        width: Math.max(1, control.width)
        height: Math.max(1, control.height)
    }

    GenyModal {
        id: desktopDialog
        parent: control.parent
        padding: control.contentPadding
        fullScreen: control.desktopFullScreen
        z: control.popupZ
        closePolicy: control.closePolicy
        width: control.desktopFullScreen ? (parent ? parent.width : control.width)
            : Math.min(control.width, parent ? Math.max(320, parent.width - Theme.space5 * 2) : control.width)
        height: control.desktopFullScreen ? (parent ? parent.height : control.height)
            : Math.min(control.height, parent ? Math.max(320, parent.height - Theme.space5 * 2) : control.height)

        // The modal is edge-to-edge only when the caller opts in. Standard
        // dialogs continue to use their existing centered frosted backdrop.
        Overlay.modal: DrawerBackdrop {
            sourceItem: control.desktopFullScreen ? null : control.blurSourceItem
            efficientNativeBlur: control.efficientBackdropBlur
            snapshotBlurOnBrowser: true
            active: !!desktopDialog && desktopDialog.opened
            addScrim: !control.desktopFullScreen && control.dimBackground
            blockInput: true
            onBackgroundTapped: control.close()
        }

        contentItem: Item {
            id: desktopContentHost
        }

        onOpened: {
            control.attachContent(desktopContentHost)
            control.didOpen()
        }
        onClosed: {
            if (!control.switchingPresentation && !mobileDrawer.visible)
                control.didClose()
        }
    }

    GenyDrawer {
        id: mobileDrawer
        parent: control.mobileFullScreen && control.Overlay.overlay
                ? control.Overlay.overlay : control.parent
        bottomSheet: true
        fullScreen: control.mobileFullScreen
        interactive: !control.mobileFullScreen
        dimBackground: control.dimBackground
        blurSourceItem: control.blurSourceItem
        efficientBackdropBlur: control.efficientBackdropBlur
        showHandle: !control.mobileFullScreen && control.showHandle
        contentPadding: control.contentPadding
        bottomPadding: control.contentPadding + Math.max(control.mobileBottomPaddingExtra,
            parent ? parent.SafeArea.margins.bottom : 0)
        closePolicy: control.closePolicy
        z: control.popupZ
        width: parent ? parent.width : control.width
        height: control.mobileFullScreen
                ? (parent ? parent.height : control.height)
                : Math.min(control.height,
                           parent ? Math.max(280, parent.height * control.mobileHeightRatio) : control.height)

        contentItem: Item {
            id: mobileContentHost
        }

        onOpened: {
            control.attachContent(mobileContentHost)
            control.didOpen()
        }
        onClosed: {
            if (!control.switchingPresentation && !desktopDialog.visible)
                control.didClose()
        }
    }
}
