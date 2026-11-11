import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";

const read = path => readFileSync(new URL("../../" + path, import.meta.url), "utf8");

test("OS-native color theme stays independently selectable and preserves saved Geny colors", () => {
  const appearance = read("src/presentation/qt/appearance_controller.cpp");
  const theme = read("ui/theme/Theme.qml");
  assert.match(appearance, /value == QStringLiteral\("platform"\)/);
  assert.match(appearance, /value == QStringLiteral\("geny"\)/);
  assert.match(appearance, /"ubuntu", "Ubuntu", "#E95420"/);
  assert.match(appearance, /QStringLiteral\("#0078D4"\)/);
  assert.match(appearance, /QStringLiteral\("#007AFF"\)/);
  assert.match(appearance, /"ubuntu", "Ubuntu", "#E95420", "platforms\/ubuntu"/);
  assert.match(theme, /colorThemeId === "platform" \? platformAccentColor/);
});

test("native platform swatch shows OS name and icon in Setup and Settings", () => {
  const settings = read("ui/components/settings/SettingsPopup.qml");
  const setup = read("ui/components/setup/InitialSetup.qml");
  assert.match(settings, /\{ id: "platform", label: Theme\.platformName, color: Theme\.swatchPlatform, icon: Theme\.platformIcon \}/);
  assert.match(setup, /id: setupPlatformColor/);
  assert.match(setup, /Theme\.platformAccentHex/);
  assert.match(setup, /Theme\.platformIcon/);
});

test("Linux Mini App Open never silently delegates to the default external browser", () => {
  const host = read("ui/app/miniapp/host.cpp");
  const section = host.slice(host.indexOf("if (!m_available) {", host.indexOf("void MiniAppHostController::openWithMetadata")));
  assert.match(section, /#if defined\(Q_OS_LINUX\) && !defined\(Q_OS_ANDROID\)/);
  assert.match(section, /setState\(false, qtTrId\("genycaster\.miniapp\.unavailable"\)\);/);
});

test("Linux release CI enables the native Qt WebEngine Mini App host instead of omitting it", () => {
  const cmake = read("ui/CMakeLists.txt");
  const workflow = read(".github/workflows/ci.yml");
  const release = read("ci/build_local_linux_release.sh");
  assert.match(cmake, /GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST/);
  assert.match(cmake, /find_package\(Qt6 QUIET COMPONENTS Widgets WebChannel WebEngineWidgets WebEngineQuick\)/);
  assert.match(workflow.split("  linux:\n")[1].split("  macos:\n")[0], /qtwebengine/);
  assert.match(workflow.split("  linux:\n")[1].split("  macos:\n")[0], /GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST=ON/);
  assert.match(release, /GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST/);
});

test("experimental Linux SDK channel is isolated from untrusted WebEngine main-world scripts", () => {
  const browser = read("ui/app/miniapp/browser_linux.cpp");
  assert.match(browser, /setWebChannel\(m_channel, QWebEngineScript::ApplicationWorld\)/);
  assert.match(browser, /transport\.setRunsOnSubFrames\(false\)/);
  assert.match(browser, /publicSdk\.setRunsOnSubFrames\(false\)/);
  assert.match(browser, /event\.source !== window/);
  assert.match(browser, /event\.origin !== window\.location\.origin/);
  assert.match(browser, /request\.deny\(\)/);
  assert.match(browser, /return nullptr;/);
  assert.match(browser, /m_page->url\(\)\.host\(\)\.compare\(m_origin\.host\(\)/);
});

test("broken media origin warnings are throttled without changing URL retries", () => {
  const image = read("ui/app/media/remote_image.cpp");
  assert.match(image, /warnedDecodeOrigins\{64\}/);
  assert.match(image, /std::chrono::minutes\{1\}/);
  assert.match(image, /if \(shouldWarnAboutDecodeOrigin\(urlText\)\)/);
  assert.match(image, /rememberFailure\(urlText\)/);
});

test("browser host OS controls WASM native preset rather than the Emscripten build host", () => {
  const appearance = read("src/presentation/qt/appearance_controller.cpp");
  const body = appearance.match(/EM_JS\(int, browserHostPlatform, \(\), \{([\s\S]*?)\n\}\);/);
  assert.ok(body, "host detection must be an actual WASM-to-browser adapter");
  const detect = navigator => vm.runInNewContext("(function() {" + body[1] + "\n})()", { navigator });
  assert.equal(detect({platform:"MacIntel",userAgent:"Mozilla/5.0 (Macintosh; Intel Mac OS X)"}),1);
  assert.equal(detect({platform:"MacIntel",maxTouchPoints:5,userAgent:"Mozilla/5.0 (Macintosh)"}),2);
  assert.equal(detect({platform:"iPhone",userAgent:"Mozilla/5.0 (iPhone)"}),2);
  assert.equal(detect({userAgentData:{platform:"macOS"},platform:"Linux x86_64"}),1);
  assert.equal(detect({platform:"Win32",userAgent:"Mozilla/5.0"}),3);
  assert.equal(detect({platform:"Linux x86_64",userAgent:"Mozilla/5.0 (X11; Linux)"}),4);
  assert.equal(detect({platform:"Linux armv8",userAgent:"Mozilla/5.0 (Linux; Android 14)"}),5);
  assert.equal(detect({platform:"Linux x86_64",userAgent:"Mozilla/5.0 (X11; CrOS x86_64)"}),6);
  assert.equal(detect({platform:"",userAgent:""}),0);
  assert.match(appearance,/QString AppearanceController::platformName\(\) const \{\s*#if defined\(Q_OS_WASM\)/);
  assert.match(appearance,/case 1: return QStringLiteral\("platforms\/macos"\)/);
  assert.match(appearance,/default: return QStringLiteral\("globe"\)/);
});
