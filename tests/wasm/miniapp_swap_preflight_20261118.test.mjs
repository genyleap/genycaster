import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = file => readFileSync(new URL('../../' + file, import.meta.url), 'utf8');

test('Mini App swapToken is routed only to a foreground, document-bound preflight', () => {
  const runtime = read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime, /#include "swap_intent\.hpp"/);
  assert.match(runtime, /method == QStringLiteral\("swapToken"\)/);
  assert.match(runtime, /beginSwapToken\(session, id, argument, document\)/);
  assert.match(runtime, /const auto intent = platform::miniapp::parseSwapIntent\(argument, rejection\)/);
  assert.match(runtime, /m_wallet == nullptr \|\| !m_wallet->available\(\)/);
  assert.match(runtime, /std::string\{"eip155:8453"\}/);
  assert.match(runtime, /!m_swapRequestToTask\.isEmpty\(\) \|\| m_controller\.swapRequestActive\(\)/);
  assert.match(runtime, /m_controller\.walletBrowserPickerActive\(\) \|\| m_controller\.walletBrowserWaiting\(\)/);
  assert.match(runtime, /session\.pendingSwaps\.insert\(requestId, PendingSwap/);
  assert.match(runtime, /session\.documentGeneration/);
  assert.match(runtime, /m_controller\.presentSwapRequest\(requestId, preview\)/);
  assert.match(runtime, /session\.browser->setVisible\(false\)/);
});

test('swaps and their UI expire on navigation, backgrounding, failure and teardown', () => {
  const runtime = read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime, /void revokeSwapIntents\(Session& session\)/);
  assert.match(runtime, /m_controller\.clearSwapRequest\(it\.key\(\)\)/);
  assert.match(runtime, /callbacks\.navigationStarted = \[this, taskId\] \{[\s\S]*?revokeSwapIntents\(\*active\)/);
  assert.match(runtime, /void fail\(Session& session, const QString& error\)[\s\S]*?revokeSwapIntents\(session\)/);
  assert.match(runtime, /void terminate\(const QString& taskId\)[\s\S]*?revokeSwapIntents\(session\)/);
  assert.match(runtime, /void terminateAll\(\)[\s\S]*?revokeSwapIntents\(\*session\)/);
  assert.match(runtime, /void minimize\(const QString& taskId\)[\s\S]*?cancelSwapToken/);
  assert.match(runtime, /session->documentGeneration != response\.generation/);
  assert.match(runtime, /!m_controller\.swapRequestActive\(\)/);
});

test('declined swaps follow the official rejection union and never fabricate transactions', () => {
  const runtime = read('ui/app/miniapp/runtime.cpp');
  const body = runtime.slice(runtime.indexOf('void beginSwapToken(Session&'),
                             runtime.indexOf('void composeCast(Session&'));
  assert.match(body, /"success"\), false/);
  assert.match(body, /"reason"\), QStringLiteral\("swap_failed"\)/);
  assert.match(body, /"reason"\), QStringLiteral\("rejected_by_user"\)/);
  assert.match(body, /void cancelSwapToken\(const QString& requestId\)/);
  assert.doesNotMatch(body, /"transactions"\)|eth_sendTransaction|approve\(/);
  const caps = runtime.slice(runtime.indexOf('[[nodiscard]] QJsonArray capabilities('),
                             runtime.indexOf('[[nodiscard]] QJsonArray chains('));
  assert.doesNotMatch(caps, /actions\.swapToken/);
});

test('first-party QML preflight has no blind-approve button or injected transaction data', () => {
  const host = read('ui/app/miniapp/host.hpp');
  const impl = read('ui/app/miniapp/host.cpp');
  const shell = read('ui/components/shell/AppShell.qml');
  const dialog = read('ui/components/apps/MiniAppSwapPreflight.qml');
  const cmake = read('ui/CMakeLists.txt');
  assert.match(host, /Q_PROPERTY\(bool swapRequestActive READ swapRequestActive NOTIFY swapRequestChanged\)/);
  assert.match(host, /void swapTokenCancelled\(const QString& requestId\)/);
  assert.match(impl, /if \(requestId\.isEmpty\(\) \|\| requestId != m_swapRequestId\) return/);
  assert.match(impl, /clearSwapRequest\(requestId\);\s*emit swapTokenCancelled\(requestId\)/);
  assert.match(shell, /MiniAppSwapPreflight \{\s*id: miniAppSwapPreflight/);
  assert.match(shell, /parent: Overlay\.overlay/);
  assert.match(cmake, /components\/apps\/MiniAppSwapPreflight\.qml/);
  assert.match(dialog, /textFormat: Text\.PlainText/);
  assert.match(dialog, /miniAppHost\.cancelSwapToken\(miniAppHost\.swapRequestId\)/);
  assert.match(dialog, /genycaster\.miniapp\.swap\.quote_unavailable/);
  assert.doesNotMatch(dialog, /sendTransaction|eth_sendTransaction|onClicked:.*approve/i);
});

test('validation accepts only Base CAIP-19, uint256 string amounts and optional prefill', () => {
  const validator = read('ui/app/miniapp/swap_intent.hpp');
  assert.match(validator, /\^eip155:8453\/erc20:0x\[0-9a-fA-F\]\{40\}\$/);
  assert.match(validator, /eip155:8453\/native/);
  assert.match(validator, /unsigned 256-bit integer/);
  assert.match(validator, /value <= maximum/);
  assert.match(validator, /Cannot swap a token for itself/);
  assert.match(validator, /Invalid sell amount in token base units/);
  assert.match(validator, /Unsupported swap option/);
});
