import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

const read = path => readFileSync(new URL("../../" + path, import.meta.url), "utf8");

test("OS-native color theme stays independently selectable and preserves saved Geny colors", () => {
  const appearance = read("src/presentation/qt/appearance_controller.cpp");
  const theme = read("ui/theme/Theme.qml");
  assert.match(appearance, /value == QStringLiteral\("platform"\)/);
  assert.match(appearance, /value == QStringLiteral\("geny"\)/);
  assert.match(appearance, /QStringLiteral\("#E95420"\)/);
  assert.match(appearance, /QStringLiteral\("#0078D4"\)/);
  assert.match(appearance, /QStringLiteral\("#007AFF"\)/);
  assert.match(appearance, /QStringLiteral\("platforms\/ubuntu"\)/);
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

test("native Qt WebEngine Mini App adapter is opt-in, never blocks proven Linux release CI", () => {
  const cmake = read("ui/CMakeLists.txt");
  const workflow = read(".github/workflows/ci.yml");
  const release = read("ci/build_local_linux_release.sh");
  assert.match(cmake, /GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST/);
  assert.match(cmake, /find_package\(Qt6 QUIET COMPONENTS Widgets WebChannel WebEngineWidgets\)/);
  assert.doesNotMatch(workflow.split("  linux:\n")[1].split("  macos:\n")[0], /qtwebengine|GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST/);
  assert.doesNotMatch(release, /GENYCASTER_REQUIRE_LINUX_MINIAPP_HOST/);
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
