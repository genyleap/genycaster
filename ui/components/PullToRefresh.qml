import QtQuick

// Shared pull-to-refresh gesture and small viewport indicator. Attach this
// *outside* the ListView's content so it does not affect delegate geometry.
// DragOverBounds permits a real touch/mouse pull at the top; wheel scrolling
// never arms a refresh because Flickable.dragging remains false.
Item {
    id: control
    objectName: "pullToRefresh"
    required property var flickable
    property bool busy: false
    property bool refreshEnabled: true
    property real activationDistance: 80
    property bool startedAtTop: false
    property real pulledDistance: 0

    signal refreshRequested()

    readonly property real offset: flickable
        ? Math.max(0, flickable.originY - flickable.contentY) : 0
    readonly property bool armed: control.startedAtTop
        && control.pulledDistance >= control.activationDistance

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    height: 44
    z: 35
    // Pagination deep in the feed must not create a fake top refresh spinner.
    visible: (busy && flickable && flickable.contentY <= flickable.originY + activationDistance)
             || (startedAtTop && offset > 8)
    opacity: visible ? 1 : 0

    Connections {
        target: control.flickable
        function onDraggingChanged() {
            if (!control.flickable) return
            if (control.flickable.dragging) {
                control.startedAtTop = control.refreshEnabled && !control.busy
                    && control.flickable.contentY <= control.flickable.originY + 4
                control.pulledDistance = 0
            } else {
                const shouldRefresh = control.armed && control.refreshEnabled && !control.busy
                control.startedAtTop = false
                control.pulledDistance = 0
                if (shouldRefresh)
                    control.refreshRequested()
            }
        }
        function onContentYChanged() {
            if (control.startedAtTop && control.flickable && control.flickable.dragging)
                control.pulledDistance = Math.max(control.pulledDistance, control.offset)
        }
    }

    Rectangle {
        width: 40
        height: 40
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        radius: 20
        color: Theme.primarySurface
        border.color: Theme.borderSubtle
        border.width: 1

        Icon {
            anchors.centerIn: parent
            text: control.busy ? Theme.iconRefresh
                : control.armed ? Theme.iconCheck : Theme.iconChevronDown
            color: Theme.accent
            preferredSize: 20
        }
    }
}
