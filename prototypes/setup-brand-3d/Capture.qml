import QtQuick
Prototype {
    id: preview
    Timer {
        interval: 4500
        running: true
        repeat: false
        onTriggered: preview.brandSidebar.grabToImage(function(result) {
            console.log("CAPTURE_DARK",result.saveToFile("assets/native-dark.png"))
            preview.dark = false
            lightTimer.start()
        })
    }
    Timer {
        id: lightTimer
        interval: 2500
        onTriggered: preview.brandSidebar.grabToImage(function(result) {
            console.log("CAPTURE_LIGHT",result.saveToFile("assets/native-light.png"))
            Qt.quit()
        })
    }
}
