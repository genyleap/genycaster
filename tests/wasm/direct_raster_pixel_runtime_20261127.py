#!/usr/bin/env python3
"""Render the actual shared direct-raster material on a Qt Quick OpenGL scene.

No user media or hardware-performance claim. The supplied QSB is the native
package; software-driver OpenGL is sufficient for pixel/lifetime contracts.
"""
from pathlib import Path
import os
import struct
import subprocess
import tempfile
import zlib

PROJECT = Path(__file__).resolve().parents[2]
qml = (PROJECT / "ui/components/visual/SuperellipseImage.qml").read_text()
start = qml.index("    Loader {\n        id: directRaster")
end = qml.index("    // GIF animation", start)
material = qml[start:end].replace("qrc:/genycaster/effects/raster-contour.frag.qsb",
                                 "raster-contour.frag.qsb")

def png(path, width, height, transparent=False):
    def chunk(name, data):
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name+data))
    row = b"".join(bytes((255,0,0,255) if x<width/4 else
                         (0,255,0,0 if transparent else 255) if x<width*3/4 else
                         (0,0,255,255)) for x in range(width))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,6,0,0,0))
                     +chunk(b"IDAT",zlib.compress((b"\0"+row)*height))
                     +chunk(b"IEND",b""))

fixture = """import QtQuick
import QtQuick.Window
import QtTest
Rectangle {
    id: scene
    color: "white"
    width: 180; height: 180
    Flickable {
        id: scroll
        anchors.fill: parent
        clip: true
        contentHeight: 400
        Item {
            id: root
            x: 20; y: 80
            width: 96; height: 96
            property bool directRasterFailed: false
            property bool directMask: !directRasterFailed
            property bool stillVisible: imageSource.status === Image.Ready
            property url activeSource: "pattern.png"
            property int fillMode: Image.PreserveAspectFit
            property real curveDepth: 16
            property real exponent: 4
            readonly property real paintedWidth: imageSource.paintedWidth
            readonly property real paintedHeight: imageSource.paintedHeight
            readonly property rect visibleContentRect: fillMode === Image.PreserveAspectFit
                ? Qt.rect((width-imageSource.paintedWidth)/2,(height-imageSource.paintedHeight)/2,
                          Math.max(1,imageSource.paintedWidth),Math.max(1,imageSource.paintedHeight))
                : Qt.rect(0,0,width,height)
            Image {
                id: imageSource
                anchors.fill: parent
                source: root.activeSource
                fillMode: root.fillMode
                visible: false
                cache: true
                asynchronous: false
                smooth: false
                mipmap: false
            }
""" + material + """
        }
    }
    TestCase {
        name: "DirectRasterMaterial"
        when: windowShown
        function colorAt(x,y,r,g,b) {
            const frame = grabImage(scene)
            // grabImage captures physical pixels; the assertions are in the
            // scene's logical coordinates, including fractional display DPR.
            x = Math.floor((x + 0.5) * frame.width / scene.width)
            y = Math.floor((y + 0.5) * frame.height / scene.height)
            verify(Math.abs(frame.red(x,y)-r)<=12, "red "+x+","+y+" "+frame.red(x,y))
            verify(Math.abs(frame.green(x,y)-g)<=12, "green "+x+","+y+" "+frame.green(x,y))
            verify(Math.abs(frame.blue(x,y)-b)<=12, "blue "+x+","+y+" "+frame.blue(x,y))
        }
        function test_fitCropAlphaScroll() {
            console.log("ACTUAL_DPR", Screen.devicePixelRatio)
            tryCompare(imageSource,"status",Image.Ready)
            scroll.contentY = 60
            wait(200)
            verify(directRaster.item !== null)
            compare(directRaster.item.status,ShaderEffect.Compiled)
            // Actual fitted rectangle is (20,44)-(116,92).
            colorAt(30,68,255,0,0)
            colorAt(68,68,0,255,0)
            colorAt(106,68,0,0,255)
            colorAt(21,45,255,255,255)
            colorAt(68,30,255,255,255)
            const material = directRaster.item
            for (const offset of [30,70,40,80,60]) {
                scroll.contentY = offset
                wait(40)
                compare(directRaster.item,material)
                colorAt(68,128-offset,0,255,0)
            }
            root.fillMode = Image.PreserveAspectCrop
            wait(100)
            colorAt(28,68,0,255,0)
            colorAt(108,68,0,255,0)
            colorAt(21,21,255,255,255)
            compare(directRaster.item,material)
            root.fillMode = Image.PreserveAspectFit
            root.activeSource = "alpha.png"
            wait(150)
            tryCompare(imageSource,"status",Image.Ready)
            colorAt(68,68,255,255,255)
            colorAt(30,68,255,0,0)
            compare(directRaster.item,material)
            root.activeSource = "pattern.png"
            wait(150)
            colorAt(68,68,0,255,0)
        }
    }
}
"""

def main():
    runner = os.environ.get("QML_TEST_RUNNER", "/opt/genycaster/Qt/6.12.0/gcc_64/bin/qmltestrunner")
    env = dict(os.environ, QT_QPA_PLATFORM="xcb", QSG_RHI_BACKEND="opengl",
               LIBGL_ALWAYS_SOFTWARE="1", QSG_INFO="1")
    with tempfile.TemporaryDirectory(prefix="genycaster-direct-raster-") as tmp:
        path = Path(tmp)
        png(path / "pattern.png",96,48)
        png(path / "alpha.png",96,48,True)
        (path / "raster-contour.frag.qsb").write_bytes((PROJECT/"ui/shaders/raster-contour.frag.qsb").read_bytes())
        (path / "tst_direct_raster.qml").write_text(fixture)
        run = subprocess.run(["xvfb-run","-a",runner,"-input",tmp],env=env,cwd=PROJECT,
                             capture_output=True,text=True,timeout=35)
    output=run.stdout+run.stderr
    print(output)
    if run.returncode or "Binding loop detected" in output or "Failed to find shader" in output:
        raise SystemExit(1)

if __name__=="__main__":
    main()
