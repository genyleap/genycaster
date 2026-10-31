#!/usr/bin/env python3
"""Exercise the real Qt Quick 3D QML scene on an isolated virtual X display."""
import os, subprocess, time, pathlib, signal, sys
home=pathlib.Path(__file__).resolve().parent
q="/opt/Qt/6.11.2/gcc_64/bin/qmlscene"
env=os.environ.copy()
env.update({"DISPLAY":":92","QSG_RHI_BACKEND":"opengl","LIBGL_ALWAYS_SOFTWARE":"1",
            "QT_LOGGING_RULES":"qt.quick3d.assetutils=true"})
xvfb=subprocess.Popen(["/usr/bin/Xvfb",":92","-screen","0","1440x900x24","-nolisten","tcp"],
                      stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
try:
    time.sleep(1.0)
    p=subprocess.Popen([q,str(home/(sys.argv[1] if len(sys.argv)>1 else "Prototype.qml"))],cwd=home,env=env,
                       stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        out,err=p.communicate(timeout=18)
        print("qmlscene exit",p.returncode)
    except subprocess.TimeoutExpired:
        p.terminate()
        out,err=p.communicate(timeout=5)
        print("qmlscene ran 18s, then terminated for smoke test")
    print("STDOUT",out[-1500:]);print("STDERR",err[-6000:])
    if "module \"QtQuick3D\" is not installed" in err or "Type RuntimeLoader unavailable" in err:
        sys.exit(2)
    if "Brand3D GLB load:" in err:
        sys.exit(3)
finally:
    xvfb.terminate()
    try: xvfb.wait(timeout=2)
    except subprocess.TimeoutExpired: xvfb.kill()
