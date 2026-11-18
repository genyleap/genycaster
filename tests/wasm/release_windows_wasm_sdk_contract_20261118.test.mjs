import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=file=>readFileSync(new URL('../../'+file,import.meta.url),'utf8');

test('Windows uses a real origin-locked native Qt WebEngine SDK host',()=>{
  const cmake=read('ui/CMakeLists.txt');
  const main=read('ui/app/main.cpp');
  const win=cmake.slice(cmake.indexOf('elseif(FARCASTER_ENABLE_MINIAPPS AND WIN32)'),
                        cmake.indexOf('if(_genycasterMiniAppBrowserAvailable)'));
  assert.ok(win.length>300);
  assert.match(win,/find_package\(Qt6 6\.12 REQUIRED COMPONENTS[\s\S]*Widgets WebChannel WebEngineWidgets WebEngineQuick/);
  assert.match(win,/app\/miniapp\/browser_linux\.cpp/);
  assert.match(win,/app\/miniapp\/browser_linux_quick\.cpp/);
  assert.match(win,/Qt6::WebChannel Qt6::WebEngineWidgets Qt6::WebEngineQuick/);
  assert.match(win,/GENYCASTER_HAS_WINDOWS_MINIAPP_HOST=1/);
  assert.match(win,/qt_add_resources\(genycaster genycaster_windows_miniapp_quick/);
  assert.match(main,/defined\(Q_OS_WIN\) && defined\(GENYCASTER_HAS_WINDOWS_MINIAPP_HOST\)/);
  assert.match(main,/QtWebEngineQuick::initialize\(\)/);
  assert.match(main,/QApplication application\(argc, argv\)/);
  assert.match(main,/setLinuxMiniAppQuickHost\(mainWindow, &engine\)/);
});

test('native transport confines document token, network origin, JS and privileges',()=>{
  const source=read('ui/app/miniapp/browser_linux.cpp');
  const quick=read('ui/app/miniapp/browser_linux_quick.cpp');
  const qml=read('ui/app/miniapp/EmbeddedBrowser.qml');
  assert.match(source,/bool publicHttps\(const QUrl& url\)/);
  assert.match(source,/url\.scheme\(\) == QStringLiteral\("https"\)/);
  assert.match(source,/url\.port\(443\) == 443/);
  assert.match(source,/!ip\.setAddress\(host\)/);
  assert.match(source,/isMainFrame && url\.host\(\)\.compare\(m_origin\.host\(\)/);
  assert.match(source,/if \(m_document\.isEmpty\(\)\) m_document = document/);
  assert.match(source,/if \(document != m_document\) return/);
  assert.match(source,/m_requests > 120/);
  assert.match(source,/QWebEngineScript::ApplicationWorld/);
  assert.match(source,/QWebEngineScript::MainWorld/);
  assert.match(source,/JavascriptCanOpenWindows, false/);
  assert.match(source,/request\.deny\(\)/);
  assert.match(quick,/linuxBrowserPublicHttps/);
  assert.match(quick,/QtWebEngine Quick|Qt WebEngine Quick/);
  assert.match(qml,/offTheRecord: true/);
  assert.match(qml,/registerObject\("genyMiniAppBridge"/);
});

test('Windows release installs and deploys an SDK host instead of a browser placeholder',()=>{
  const installer=read('ci/install_qt_windows.py');
  const workflow=read('.github/workflows/ci.yml');
  const gate=read('ci/verify_release_feature_contract.py');
  const windows=workflow.slice(workflow.indexOf('\n  windows:\n'),
                               workflow.indexOf('\n  android:\n'));
  assert.match(installer,/addons\.qtwebchannel/);
  // Qt WebEngine 6.140+ is a separately versioned extension rather
  // than a Qt 6.12.0 addon. Check an explicit, fail-closed install step.
  assert.doesNotMatch(installer,/addons\.qtwebengine/);
  assert.match(windows,/Install separately versioned Qt WebEngine extension/);
  assert.match(windows,/aqtinstall==3\.3\.0/);
  assert.match(windows,/--modules qtwebengine --noarchives --outputdir Qt/);
  assert.match(windows,/Qt6WebEngineWidgets/);
  assert.match(windows,/Qt6WebEngineQuick/);
  assert.match(windows,/-DFARCASTER_ENABLE_MINIAPPS=ON/);
  assert.match(windows,/--target farcaster_miniapp_browser/);
  assert.match(windows,/QtWebEngineProcess\.exe/);
  assert.match(windows,/qtwebenginequickplugin\.dll/);
  assert.doesNotMatch(windows, /'app\/miniapp\/EmbeddedBrowser\.qml',\s*'components\/apps\/AndroidWebView\.qml'/);
  assert.match(gate,/GENYCASTER_HAS_WINDOWS_MINIAPP_HOST=1/);
  assert.match(gate,/qtwebenginequickplugin\.dll/);
});

test('WASM advertises the actual isolated JS mini app SDK without enabling native signing',()=>{
  const configure=read('cmake/platform/WebAssembly/configure.cmake');
  const app=read('src/application/CMakeLists.txt');
  const source=read('src/application/configured_features.cpp.in');
  const web=read('ui/CMakeLists.txt');
  const sdk=read('ui/wasm/miniapp-host-bridge-entry.mjs');
  assert.match(configure,/set\(FARCASTER_ENABLE_MINIAPPS OFF CACHE BOOL "" FORCE\)/);
  assert.match(configure,/set\(FARCASTER_ENABLE_PROTOCOL_SIGNING OFF CACHE BOOL "" FORCE\)/);
  assert.match(app,/if\(EMSCRIPTEN AND FARCASTER_BUILD_UI\)[\s\S]*?set\(FARCASTER_ENABLE_MINIAPPS_CPP true\)/);
  assert.match(source,/\.hasMiniApps = @FARCASTER_ENABLE_MINIAPPS_CPP@/);
  assert.match(web,/--pre-js=\$\{CMAKE_CURRENT_SOURCE_DIR\}\/wasm\/miniapp-host-pre\.js/);
  assert.match(web,/\$<TARGET_FILE_DIR:genycaster>\/miniapp-host-bridge\.mjs/);
  assert.match(sdk,/exposeToIframe/);
  assert.match(sdk,/miniAppOrigin:url\.origin/);
  assert.match(sdk,/window\.__genycasterMiniAppSdk = Object\.freeze/);
  assert.match(sdk,/ethProviderRequest: unsupported/);
  assert.match(sdk,/createMiniAppEthereumProvider\(url\.origin/);
  assert.match(sdk,/injected\.dispose\(\)/);
});

test('unsupported platform SDK capability still blocks public release',()=>{
  const code=read('ci/verify_release_feature_contract.py');
  assert.match(code,/GENYCASTER_HAS_ANDROID_MINIAPP_SDK_HOST=1/);
  assert.match(code,/Android: only unprivileged public WebView, no Mini App SDK host/);
  assert.match(code,/if args\.strict:/);
  assert.match(code,/return 1/);
});
