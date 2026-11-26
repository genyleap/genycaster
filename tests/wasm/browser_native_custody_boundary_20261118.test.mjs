import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source = path => readFileSync(path, 'utf8');
const cmake = source('ui/CMakeLists.txt');
const browser = source('ui/app/identity/native_wallet_browser.cpp');
const native = source('ui/app/identity/native_wallet.cpp');

test('WebAssembly selects the isolated browser custody ABI, never the OpenSSL desktop controller', () => {
  assert.match(cmake, /if\(EMSCRIPTEN\)\s*set\(_genycasterCustodyController app\/identity\/native_wallet_browser\.cpp\)\s*else\(\)\s*set\(_genycasterCustodyController app\/identity\/native_wallet\.cpp\)/);
  assert.match(cmake, /qt_add_executable\(genycaster[\s\S]*?\$\{_genycasterCustodyController\}/);
  assert.doesNotMatch(browser, /#include\s*<openssl\//);
  assert.doesNotMatch(browser, /import farcaster\.platform\.custody_recovery/);
  assert.match(native, /#include\s*<openssl\/crypto\.h>/);
  assert.match(native, /import farcaster\.platform\.custody_recovery/);
  const preparation = source('ui/app/identity/native_prepare.cpp');
  assert.match(preparation, /#if !defined\(Q_OS_WASM\)\s*import farcaster\.platform\.custody_recovery;\s*#endif/);
  assert.match(preparation, /bool NativeRegistrationPreparationController::prepare\(\)\s*\{\s*#if defined\(Q_OS_WASM\)[\s\S]*?erasePrepared\(\);\s*m_failed = true;\s*emit changed\(\);\s*return false;\s*#else/);
  // Native builds retain their real, bounded and cancellable PoW engine.
  assert.match(preparation, /solveNativeOnboardPow\(/);
});

test('browser adapter reads metadata but native signature ports fail closed', () => {
  assert.match(browser, /QVariantMap readState/);
  assert.match(browser, /JSON\.stringify\(bridge\.state\(\)\)/);
  assert.match(browser, /beginBrowserWallet/);
  assert.match(browser, /dispatch\(QStringLiteral\("verify"\)/);
  assert.match(browser, /dispatch\(QStringLiteral\("copy"\)/);
  assert.match(browser, /x\.fill\('\\0'\); y\.fill/);
  const adapter=source('ui/wasm/browser_setup_wallet.mjs');
  assert.match(adapter,/const snapshot=\(\)=>\(\{\.\.\.state\}\)/);
  assert.doesNotMatch(adapter,/setItem\([^,]+,\s*(phrase|voucher)/);
  assert.match(browser, /sealNativeOnboard\([\s\S]*?return std::nullopt;/);
  assert.match(browser, /signReservedFarcasterTransfer\([\s\S]*?return std::nullopt;/);
  assert.match(browser, /signOpenProofFarcasterChallenge\([\s\S]*?return unavailableSignature\(\);/);
  assert.match(browser, /signOpenProofWalletChallenge\([\s\S]*?return unavailableSignature\(\);/);
  assert.match(browser, /forConfirmedNativeFid\([\s\S]*?return \{\};/);
  assert.match(browser, /forConfirmedFarcasterFid\([\s\S]*?return \{\};/);
  assert.doesNotMatch(browser, /localStorage|sessionStorage|Math\.random|privateKey|seedPhrase|navigator\.clipboard/);
});

test('browser mnemonic paint is bounded, transient and shared with native QML', () => {
  assert.match(browser,/if\(!m_wallet\|\|!m_wallet->recoveryVisible\(\)\)return/);
  assert.match(browser,/if\(!m_masked\)/);
  assert.match(browser,/std::array<char,512>/);
  assert.match(browser,/std::fill\(buffer.begin\(\),buffer.end\(\),'\\0'\)/);
  const setup=source('ui/components/setup/InitialSetup.qml');
  assert.match(setup,/NativeRecoveryWords \{/);
  assert.match(setup,/masked: !backupWordVisibility.checked/);
  assert.doesNotMatch(source('ui/app/identity/native_wallet.hpp'),/Q_PROPERTY\(QString (phrase|mnemonic|recoveryWords)/);
  const main = source('ui/app/main.cpp');
  assert.match(main, /qmlRegisterType<NativeRecoveryWordsItem>\(/);
  assert.match(cmake, /miniapp-wallet-provider\.mjs/);
  const releaseGate = source('ci/verify_release_feature_contract.py');
  assert.match(releaseGate, /Android: public-only Mini App scope is not explicitly declared/);
  assert.match(releaseGate, /for feature in \("CHANNELS", "SEARCH"\)/);
  assert.match(releaseGate, /for feature in \("CHAT", "WALLET"\)/);
  assert.match(releaseGate, /GENYCASTER_ANDROID_MINIAPP_SCOPE=public-only/);
});
