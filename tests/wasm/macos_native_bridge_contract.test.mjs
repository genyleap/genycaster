import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';

const read = relative => readFileSync(new URL('../../' + relative, import.meta.url), 'utf8');

test('macOS WebKit is Objective-C with a C ABI, not Objective-C++ SDK patches', () => {
    const cmake = read('cmake/platform/macOS/configure.cmake');
    const native = read('ui/app/miniapp/browser_macos.m');
    const cpp = read('ui/app/miniapp/browser_macos.cpp');
    const header = read('ui/app/miniapp/browser_macos_native.h');
    const workflows = read('.github/workflows/ci.yml');
    assert.match(cmake, /enable_language\(OBJC\)/);
    assert.match(cmake, /OBJC_STANDARD 11/);
    assert.match(cmake, /CXX_STANDARD 26/);
    assert.match(cmake, /browser_macos\.m"/);
    assert.match(cmake, /browser_macos\.cpp"/);
    assert.match(cmake, /-fobjc-arc/);
    assert.doesNotMatch(cmake, /OBJCXX_STANDARD|-ivfsoverlay|prepare_macos_sdk_cpp26|Python3_EXECUTABLE/);
    assert.doesNotMatch(native, /#include <Q|namespace farcaster|std::|QJson|QString|QUrl|\.mm\b/);
    assert.match(header, /extern "C"/);
    assert.match(cpp, /qtTrId/);
    assert.match(cpp, /static_assert\(static_cast<int>\(PresentationMode::Right\) == GENY_MAC_RIGHT\)/);
    assert.match(cpp, /QT_TRID_NOOP\("genycaster\.miniapp\.navigation_blocked"\)/);
    assert.doesNotMatch(workflows, /test_macos_cpp26_sdk_overlay/);
    assert.equal(existsSync(new URL('../../ci/prepare_macos_sdk_cpp26.py',import.meta.url)),false);
    assert.equal(existsSync(new URL('../../ui/app/miniapp/browser_macos.mm',import.meta.url)),false);
});

test('native browser exposes a complete ABI with explicit ownership and scoped messaging', () => {
    const header = read('ui/app/miniapp/browser_macos_native.h');
    const native = read('ui/app/miniapp/browser_macos.m');
    const cpp = read('ui/app/miniapp/browser_macos.cpp');
    const functions = [
      'create', 'valid', 'destroy', 'load', 'status', 'ready',
      'set_back', 'evaluate', 'set_visible'
    ];
    for (const func of functions) {
        assert.match(header, new RegExp('geny_mac_browser_' + func + '\\('));
        assert.match(native, new RegExp('geny_mac_browser_' + func + '\\('));
        assert.match(cpp, new RegExp('geny_mac_browser_' + func + '\\('));
    }
    assert.match(native, /__bridge_retained/);
    assert.match(native, /__bridge_transfer/);
    assert.match(native, /removeScriptMessageHandlerForName/);
    assert.match(native, /message\.frameInfo\.isMainFrame/);
    assert.match(native, /securityOrigin\.host/);
    assert.match(cpp, /allowedWebUrl\(QUrl::fromEncoded/);
    assert.match(cpp, /payload\.isObject\(\)/);
    assert.match(cpp, /GENY_MAC_CLOSED/);
    assert.match(native, /GENY_MAC_NAVIGATION_STARTED/);
    assert.match(native, /GENY_MAC_ACTIVATED/);
    assert.match(cpp, /__genyMiniAppEthProviderEvent/);
    assert.match(cpp, /__genyMiniAppReply/);
    assert.match(native, /forMainFrameOnly:YES/);
    assert.match(native, /nonPersistentDataStore/);
});
