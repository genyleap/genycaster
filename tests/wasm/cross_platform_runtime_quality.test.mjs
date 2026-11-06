import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync, readdirSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const read = file => readFileSync(resolve(root, file), "utf8");

test("fresh installs start in English without overwriting an explicit language", () => {
    const cpp = read("src/presentation/qt/localization_controller.cpp");
    const start = cpp.slice(cpp.indexOf("const QString storedLanguage"),
                            cpp.indexOf("const QString storedRegionalLocale"));
    assert.match(start, /QStringLiteral\("en"\)/);
    assert.match(cpp, /overrides\.languageTag\.isEmpty\(\) \? storedLanguage/);
    assert.match(cpp, /setLanguageTag\(QStringLiteral\("en"\)\)/);
    assert.match(cpp, /m_impl->settings\.setValue\(QString::fromLatin1\(kLanguageSetting\)/);
    const qml = read("ui/components/InitialSetup.qml");
    assert.match(qml, /showsPrimaryCta: \[0,1,2,6,7\]\.includes\(currentStep\)/);
    assert.doesNotMatch(qml, /showsPrimaryCta: .*\.indexOf\(/);
});

test("verified GitHub calendar reuses account credentials, not a public network request", () => {
    const account = read("ui/app/openproof_connections_controller.cpp");
    const activity = read("ui/app/genypulse_activity_controller.cpp");
    const main = read("ui/app/main.cpp");
    const ownRequest = account.slice(
        account.indexOf("OpenProofConnectionsController::requestVerifiedGitHubCalendar"),
        account.indexOf("OpenProofConnectionsController::restoreAccountSession"));
    assert.match(ownRequest, /cloudAuthorizationReady\(\)/);
    assert.match(ownRequest, /m_viewerFid/);
    assert.match(ownRequest, /trustedHttpsEndpoint\(m_cloudUrl\)/);
    assert.match(ownRequest, /authorizeCloudRequest\(request\)/);
    assert.match(ownRequest, /return m_network\.get\(request\)/);
    assert.match(activity, /requestVerifiedGitHubCalendar\(login, year\)/);
    assert.match(activity, /cancelReply\(m_githubContributionsReply, this\)/);
    assert.match(activity, /activeAccountFid\(\)/);
    assert.match(activity, /cloudAuthorizationReady\(\)/);
    assert.doesNotMatch(activity, /QUrl url\{QStringLiteral\("https:\/\/caster\.genyleap\.com\/api\/v1\/cloud\/github_contributions"\)\}/);
    assert.match(main, /genyPulseActivity\.setAccountConnections\(&openProofConnections\)/);
});

test("busy or starting protocol writes never fabricate a missing signer", () => {
    const source = read("src/presentation/qt/presentation_bridge.cpp");
    for (const name of ["setReaction", "setFollowing", "setBlocked", "removeCast"]) {
        const body = source.slice(
            source.indexOf("void PresentationBridge::" + name + "("),
            source.indexOf("\nvoid PresentationBridge::", source.indexOf("void PresentationBridge::" + name + "(") + 8));
        assert.ok(body.length > 150, name);
        const busy = body.indexOf("if (m_writeBusy) return;");
        const unavailable = body.indexOf("if (!m_isAuthenticated || !m_writeReady)");
        assert.ok(busy > 0 && unavailable > busy, name + " incorrectly treats pending writes as signer failures");
        assert.match(body, /the_local_runtime_is_still_starting/);
    }
    const profile = source.slice(
        source.indexOf("bool PresentationBridge::updateProtocolProfile"),
        source.indexOf("\nvoid PresentationBridge::", source.indexOf("bool PresentationBridge::updateProtocolProfile")));
    assert.match(profile, /if \(m_writeBusy\) return false/);
});

test("market rows rearrange the sparkline and truncate names at constrained rail widths", () => {
    const rail = read("ui/components/ContextRail.qml");
    const section = rail.slice(rail.indexOf("id: marketComponent"),
                               rail.indexOf("id: suggestedComponent"));
    assert.match(section, /readonly property bool compactMarketRow: width < 330/);
    assert.match(section, /Layout\.minimumWidth: 0/);
    assert.match(section, /visible: !tokenRow\.compactMarketRow/);
    assert.match(section, /visible: tokenRow\.compactMarketRow/);
    assert.match(section, /columns: tokenRow\.compactMarketRow \? 2 : 3/);
    assert.match(section, /elide: Text\.ElideRight/);
    assert.match(section, /TokenSparkline/);
});

test("Apple SDK fixes remove blanket diagnostics suppression and imported umbrella APIs", () => {
    const native = read("ui/app/miniapp_browser_macos.mm");
    assert.doesNotMatch(native, /#pragma clang diagnostic ignored/);
    assert.doesNotMatch(native, /<AppKit\/AppKit\.h>|<WebKit\/WebKit\.h>|<QuartzCore\/QuartzCore\.h>/);
    assert.match(native, /#import <AppKit\/NSView\.h>/);
    assert.match(native, /#import <WebKit\/WKWebView\.h>/);
    assert.match(native, /#import <WebKit\/WKPreferences\.h>/);
    assert.match(native, /#import <WebKit\/WKNavigationAction\.h>/);
    assert.match(native, /#import <WebKit\/WKNavigationResponse\.h>/);
    assert.match(native, /GenyMiniAppLoadingSurface : NSView/);
    const sdkBridge = read("cmake/platform/macOS/configure.cmake");
    assert.match(sdkBridge, /OBJCXX_STANDARD 26/);
    assert.match(sdkBridge, /check_objcxx_source_compiles/);
    assert.doesNotMatch(sdkBridge, /OBJCXX_STANDARD 23|-Wno-|diagnostic ignored/);
    assert.match(read("ui/CMakeLists.txt"), /target_compile_features\(farcaster_miniapp_runtime PUBLIC cxx_std_26\)/);
    const qml = read("ui/components/InitialSetup.qml");
    assert.match(qml, /showsPrimaryCta: .*\.includes\(currentStep\)/);
});

test("platform-specific CMake and packaging stay isolated from the shared Qt product", () => {
    const cmake = read("CMakeLists.txt");
    const ui = read("ui/CMakeLists.txt");
    // A platform must have exactly one CMake entrypoint; adding one file per
    // incident/dependency is an architecture regression, not a fix.
    for (const platform of ["Android", "iOS", "macOS", "WebAssembly", "Windows", "Linux"]) {
        const platformDir = resolve(root, "cmake/platform", platform);
        assert.deepEqual(readdirSync(platformDir), ["configure.cmake"], platform);
    }
    for (const platform of ["Android", "iOS", "WebAssembly", "Windows", "Linux"]) {
        assert.match(cmake, new RegExp("cmake/platform/" + platform + "/configure\\.cmake"));
    }
    assert.match(ui, /cmake\/platform\/macOS\/configure\.cmake/);
    assert.match(ui, /genycaster_macos_setup_browser\(\)/);
    assert.match(ui, /properties\/ConfigureTarget\.cmake/);
    assert.match(cmake, /genycaster_android_setup_openssl\(\)/);
    assert.match(cmake, /genycaster_android_setup_boost_headers\(\)/);
    assert.match(read("cmake/platform/Android/configure.cmake"), /genycaster_require_android_qt_tls/);
    assert.match(read("cmake/platform/WebAssembly/configure.cmake"), /genycaster_wasm_setup_boost_headers/);
});

test("public documentation explains PostgreSQL without disclosing the deployment database name", () => {
    for (const file of [
        "README.md", "STATUS.md", "AGENTS.md", "THIRD_PARTY_NOTICES.md",
        "docs/ARCHITECTURE.md", "docs/OPENPROOF_AND_SERVICES.md",
        "services/account-sync/README.md", "services/media-gateway/README.md"
    ]) {
        const text = read(file);
        assert.match(text, /PostgreSQL/);
        assert.doesNotMatch(text, /CREATE DATABASE [a-z][a-z0-9_]* WITH/);
    }
    assert.match(read("README.md"), /native, local-first decentralized social experience/);
});
