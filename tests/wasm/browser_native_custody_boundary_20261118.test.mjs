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

test('browser-native wallet never generates, retains, exports or signs custody secrets', () => {
  assert.match(browser, /bool NativeCustodyWalletController::hasWallet\(\) const noexcept \{ return false; \}/);
  assert.match(browser, /bool NativeCustodyWalletController::backupVerified\(\) const noexcept \{ return false; \}/);
  assert.match(browser, /bool NativeCustodyWalletController::generate\(\)[\s\S]*?return false;/);
  assert.match(browser, /restoreForRegistration\(QString recoveryWords\)[\s\S]*?recoveryWords\.fill\(QChar\{\}\)/);
  assert.match(browser, /verifyBackup\(QString first, QString second\)[\s\S]*?first\.fill\(QChar\{\}\);\s*second\.fill\(QChar\{\}\);\s*return false;/);
  assert.match(browser, /bool NativeCustodyWalletController::copyRecoveryWords\(\) \{ return false; \}/);
  assert.match(browser, /sealNativeOnboard\([\s\S]*?return std::nullopt;/);
  assert.match(browser, /signReservedFarcasterTransfer\([\s\S]*?return std::nullopt;/);
  assert.match(browser, /signOpenProofFarcasterChallenge\([\s\S]*?return unavailableSignature\(\);/);
  assert.match(browser, /signOpenProofWalletChallenge\([\s\S]*?return unavailableSignature\(\);/);
  assert.match(browser, /forConfirmedNativeFid\([\s\S]*?return \{\};/);
  assert.match(browser, /forConfirmedFarcasterFid\([\s\S]*?return \{\};/);
  assert.doesNotMatch(browser, /localStorage|sessionStorage|Math\.random|privateKey|seedPhrase|navigator\.clipboard/);
});

test('browser mnemonic paint remains empty while the separate browser wallet SDK stays packaged', () => {
  assert.match(browser, /NativeRecoveryWordsItem::paint\(QPainter\*\)\s*\{\s*\/\/ No native mnemonic can exist or be painted on WebAssembly\.\s*\}/);
  const main = source('ui/app/main.cpp');
  assert.match(main, /qmlRegisterType<NativeRecoveryWordsItem>\(/);
  assert.match(cmake, /miniapp-wallet-provider\.mjs/);
  const releaseGate = source('ci/verify_release_feature_contract.py');
  assert.match(releaseGate, /Android: public-only Mini App scope is not explicitly declared/);
  assert.match(releaseGate, /for feature in \("CHANNELS", "SEARCH"\)/);
  assert.match(releaseGate, /for feature in \("CHAT", "WALLET"\)/);
  assert.match(releaseGate, /GENYCASTER_ANDROID_MINIAPP_SCOPE=public-only/);
});
