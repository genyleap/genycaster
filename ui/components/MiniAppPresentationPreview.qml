import QtQuick

Item {
    id: root

    property string mode: "window"
    property bool selected: false

    readonly property bool sidePanel: mode === "left" || mode === "right"
    readonly property bool bottomPanel: mode === "bottom"
    readonly property bool floatingWindow: mode === "window"

    implicitHeight: 96

    Superellipse {
        id: desktopFrame
        anchors.fill: parent
        fillColor: Theme.background
        borderWidth: Theme.strokeHairline
        borderColor: Theme.borderMuted
        curveDepth: Theme.curveControl

        // Keep all chrome inset from the outer superellipse. Nothing is clipped
        // against the curved edge, so the preview remains clean at every DPR.
        Superellipse {
            id: titleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 3
            height: 14
            fillColor: Theme.surfaceRaised
            borderWidth: 0
            curveDepth: 6

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Repeater {
                    model: 3
                    delegate: Superellipse {
                        required property int index
                        width: 4
                        height: 4
                        fillColor: index === 0 ? Theme.danger
                                  : index === 1 ? Theme.warning
                                                : Theme.success
                        borderWidth: 0
                        curveDepth: 2
                        opacity: 0.70
                    }
                }
            }

            Superellipse {
                anchors.centerIn: parent
                width: Math.min(50, parent.width * 0.34)
                height: 4
                fillColor: Theme.borderMuted
                borderWidth: 0
                curveDepth: 2
            }
        }

        Item {
            id: workspace
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: titleBar.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            anchors.topMargin: 3
            anchors.bottomMargin: 4

            Superellipse {
                id: navRail
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.max(16, parent.width * 0.14)
                fillColor: Theme.surfaceMuted
                borderWidth: 0
                curveDepth: 6

                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 7
                    spacing: 5

                    Repeater {
                        model: 4
                        delegate: Superellipse {
                            required property int index
                            width: index === 0 ? 8 : 6
                            height: width
                            fillColor: index === 0 ? Theme.accent : Theme.borderStrong
                            borderWidth: 0
                            curveDepth: width / 2
                            opacity: index === 0 ? 0.88 : 0.38
                        }
                    }
                }
            }

            Item {
                id: feedMock
                anchors.left: navRail.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 6

                Superellipse {
                    x: 0
                    y: 2
                    width: parent.width * 0.48
                    height: 5
                    fillColor: Theme.borderStrong
                    borderWidth: 0
                    curveDepth: 2.5
                    opacity: 0.56
                }
                Superellipse {
                    x: 0
                    y: 12
                    width: parent.width * 0.74
                    height: 3
                    fillColor: Theme.borderMuted
                    borderWidth: 0
                    curveDepth: 1.5
                }
                Superellipse {
                    x: 0
                    y: 19
                    width: parent.width * 0.58
                    height: 3
                    fillColor: Theme.borderMuted
                    borderWidth: 0
                    curveDepth: 1.5
                }
                Superellipse {
                    x: 0
                    y: 29
                    width: parent.width
                    height: Math.max(18, parent.height - 31)
                    fillColor: Theme.surfaceRaised
                    borderWidth: Theme.strokeHairline
                    borderColor: Theme.borderSubtle
                    curveDepth: 6
                }
            }

            Rectangle {
                visible: !root.floatingWindow
                anchors.fill: parent
                color: Theme.backdropScrim
                opacity: 0.14
            }

            Superellipse {
                id: miniAppPanel

                width: root.sidePanel ? workspace.width * 0.42
                       : root.bottomPanel ? workspace.width * 0.56
                                          : workspace.width * 0.48
                height: root.sidePanel ? workspace.height - 2
                        : root.bottomPanel ? workspace.height * 0.58
                                           : workspace.height * 0.72
                x: root.mode === "left" ? 0
                   : root.mode === "right" ? workspace.width - width
                                           : Math.round((workspace.width - width) / 2)
                y: root.bottomPanel ? workspace.height - height
                                   : Math.round((workspace.height - height) / 2)
                fillColor: Theme.surfaceElevated
                borderWidth: Theme.strokeEmphasis
                borderColor: Theme.accent
                curveDepth: root.floatingWindow ? 9 : 8
                curveTopLeft: root.mode !== "left"
                curveBottomLeft: root.floatingWindow || root.mode === "right"
                curveTopRight: root.mode !== "right"
                curveBottomRight: root.floatingWindow || root.mode === "left"
                elevation: root.floatingWindow ? Theme.elevationPopover : Theme.elevationControl

                Superellipse {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 3
                    anchors.rightMargin: 3
                    anchors.topMargin: 3
                    height: root.floatingWindow ? 11 : 8
                    fillColor: Theme.primarySurface
                    borderWidth: 0
                    curveDepth: 4

                    Row {
                        visible: root.floatingWindow
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Repeater {
                            model: 2
                            delegate: Superellipse {
                                width: 3
                                height: 3
                                fillColor: Theme.accent
                                borderWidth: 0
                                curveDepth: 1.5
                                opacity: 0.62
                            }
                        }
                    }

                    Superellipse {
                        anchors.centerIn: parent
                        width: Math.min(parent.width * 0.40, 30)
                        height: 3
                        fillColor: Theme.accent
                        borderWidth: 0
                        curveDepth: 1.5
                        opacity: 0.72
                    }
                }

                Superellipse {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: root.floatingWindow ? 19 : 16
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    height: 4
                    fillColor: Theme.borderStrong
                    borderWidth: 0
                    curveDepth: 2
                    opacity: 0.48
                }
                Superellipse {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: root.floatingWindow ? 27 : 24
                    anchors.leftMargin: 6
                    anchors.rightMargin: 14
                    height: 3
                    fillColor: Theme.borderMuted
                    borderWidth: 0
                    curveDepth: 1.5
                }
            }
        }

    }
}
