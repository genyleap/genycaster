#!/usr/bin/env python3
"""Android Gradle 429 failover must be explicit, limited and reproducible."""
from __future__ import annotations
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent.parent
MIRROR="https://maven-central.storage-download.googleapis.com/maven2/"


class AndroidGradleMirrorTests(unittest.TestCase):
    def test_google_cloud_maven_mirror_precedes_qt_gradle_bootstrap(self):
        w=(ROOT/".github/workflows/ci.yml").read_text()
        android=w.split("\n  android:\n",1)[1].split("\n  ios:\n",1)[0]
        init=(ROOT/"ci/gradle_maven_mirror.init.gradle").read_text()
        self.assertIn("gradle.beforeProject",init)
        self.assertIn("project.buildscript.repositories.maven",init)
        self.assertIn(MIRROR,init)
        self.assertNotIn("http://",init)
        self.assertNotIn("mavenLocal()",init)
        self.assertIn("Configure Google Cloud Maven Central mirror before Android toolchains",android)
        self.assertIn(MIRROR,android)
        self.assertIn('grep -q \'<project\'',android)
        self.assertIn("ci/gradle_maven_mirror.init.gradle",android)
        self.assertLess(android.index("Configure Google Cloud Maven Central mirror"),
                        android.index("Rebuild Qt FFmpeg"))
        self.assertLess(android.index("Rebuild Qt FFmpeg"),android.index("Configure and build Android APK"))
        self.assertIn("bash ci/build_android_apk_with_retry.sh",android)
        self.assertNotIn("cmake --build build/android --target genycaster_make_apk --parallel 3",android)

    @staticmethod
    def fake_runner(error_message: str) -> tuple[int,int,str]:
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            bin=root/"bin"
            bin.mkdir()
            calls=root/"calls.txt"
            cmake=bin/"cmake"
            cmake.write_text(
                "#!/bin/sh\n"
                "count=$(cat \"$CM_CALLS_FILE\" 2>/dev/null || echo 0)\n"
                "count=$((count + 1))\n"
                "echo \"$count\" > \"$CM_CALLS_FILE\"\n"
                "if [ \"$count\" -eq 1 ]; then\n"
                "  echo \"$CM_ERROR_MESSAGE\"\n"
                "  exit 14\n"
                "fi\n"
                "echo 'Fake incremental build success'\n"
                "exit 0\n")
            cmake.chmod(0o755)
            sleep=bin/"sleep"
            sleep.write_text("#!/bin/sh\nexit 0\n")
            sleep.chmod(0o755)
            env=dict(os.environ,PATH=str(bin)+os.pathsep+os.environ.get("PATH",""),
                     RUNNER_TEMP=tmp,CM_CALLS_FILE=str(calls),
                     CM_ERROR_MESSAGE=error_message)
            process=subprocess.run(["bash","ci/build_android_apk_with_retry.sh"],
                cwd=ROOT,env=env,capture_output=True,text=True,timeout=25)
            count=int(calls.read_text())
            return process.returncode,count,process.stdout + process.stderr

    def test_429_is_retried_incrementally(self):
        code,count,output=self.fake_runner(
            "Could not GET artifact: Received status code 429 from server: Too Many Requests")
        self.assertEqual((code,count),(0,2),output)
        self.assertIn("succeeded on attempt 2",output)

    def test_compile_errors_are_never_retried(self):
        code,count,output=self.fake_runner("C++: error: no matching constructor")
        self.assertEqual((code,count),(14,1),output)
        self.assertIn("Non-network Android failure; refusing retry",output)


if __name__=="__main__":
    unittest.main()
