import QtQuick
import QtQuick.Controls

/**
 * Adaptive transient surface. Mobile presentations use a true bottom Drawer;
 * desktop presentations use the standard centered GenyModal.
 */
Item {
    id: control

    property Item contentItem: null
    property Item blurSourceItem: null
    property int contentPadding: Theme.space3
    property bool dimBackground: true
    property bool showHandle: true
    property real mobileHeightRatio: 0.90
    property real popupZ: 1000
    readonly property bool compactPresentation: parent ? parent.width < Theme.breakpointMobile : false
    readonly property bool opened: desktopDialog.opened || mobileDrawer.opened

    signal didOpen()
    signal didClose()

    property bool switchingPresentation: false

    function attachContent(host) {
        if (!control.contentItem || !host)
            return
        control.contentItem.parent = host
        control.contentItem.x = 0
        control.contentItem.y = 0
        control.contentItem.width = Qt.binding(() => host.width)
        control.contentItem.height = Qt.binding(() => host.height)
    }

    function open() {
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
        desktopDialog.close()
        mobileDrawer.close()
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
        z: control.popupZ
        width: Math.min(control.width, parent ? Math.max(320, parent.width - Theme.space5 * 2) : control.width)
        height: Math.min(control.height, parent ? Math.max(320, parent.height - Theme.space5 * 2) : control.height)

        // Desktop dialog stays centered; the backdrop samples the actual
        // application, rather than replacing it with a flat dark scrim.
        Overlay.modal: DrawerBackdrop {
            sourceItem: control.blurSourceItem
            active: desktopDialog.opened
            addScrim: control.dimBackground
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
        parent: control.parent
        bottomSheet: true
        dimBackground: control.dimBackground
        blurSourceItem: control.blurSourceItem
        showHandle: control.showHandle
        contentPadding: control.contentPadding
        z: control.popupZ
        width: parent ? parent.width : control.width
        height: Math.min(control.height,
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
