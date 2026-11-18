import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
const file = path => readFileSync(new URL('../../'+path, import.meta.url),'utf8');

test('SDK swap capability is advertised only with a reachable authenticated-quote backend and Base wallet',()=>{
  const api=file('services/account-sync/server.mjs');
  const controller=file('ui/app/identity/connections.cpp');
  const host=file('ui/app/miniapp/host.hpp');
  const main=file('ui/app/main.cpp');
  const runtime=file('ui/app/miniapp/runtime.cpp');
  assert.match(api,/path === '\/api\/v1\/cloud\/swap\/status'/);
  assert.match(api,/available:await readyToSwap\(\)/);
  assert.match(api,/swapQuoteStatus\(\)/);
  assert.match(api,/swapRpc\('eth_chainId'/);
  assert.match(api,/swapRpc\('eth_getCode',\s*\[swapContracts.ROUTER/);
  assert.match(controller,/void OpenProofConnectionsController::refreshSwapAvailability\(\)/);
  assert.match(controller,/QStringLiteral\("\/api\/v1\/cloud\/swap\/status"\)/);
  assert.match(controller,/ManualRedirectPolicy/);
  assert.match(controller,/swapAvailabilityChanged\(status==200/);
  assert.match(host,/Q_PROPERTY\(bool swapAvailable READ swapAvailable NOTIFY swapAvailabilityChanged\)/);
  assert.match(main,/&OpenProofConnectionsController::swapAvailabilityChanged,[\s\S]*?&MiniAppHostController::setSwapAvailable/);
  assert.match(runtime,/capabilities\(m_wallet, signInAvailable\(\), m_controller.swapAvailable\(\)\)/);
  assert.match(runtime,/if \(!m_controller.swapAvailable\(\)\)/);
  assert.match(runtime,/result.push_back\(QStringLiteral\("actions.swapToken"\)\)/);
});

test('user must separately confirm exact ERC-20 approval and the refreshed quote swap',()=>{
  const host=file('ui/app/miniapp/host.cpp');
  const runtime=file('ui/app/miniapp/runtime.cpp');
  const qml=file('ui/components/apps/MiniAppSwapPreflight.qml');
  assert.match(host,/void MiniAppHostController::executeSwapStep\(const QString& requestId\)/);
  assert.match(host,/m_swapStage != QStringLiteral\("review"\)/);
  assert.match(host,/emit swapExecutionRequested\(requestId\)/);
  assert.match(runtime,/void executeSwapStep\(const QString& requestId\)/);
  assert.match(runtime,/void sendSwapTransaction\(const QString& taskId,const QString& requestId/);
  assert.match(runtime,/\.method="eth_accounts"/);
  assert.match(runtime,/\.method="eth_chainId"/);
  assert.match(runtime,/\.method="eth_sendTransaction"/);
  assert.match(runtime,/pending->approvalReceipt=approval/);
  assert.match(runtime,/if \(!approval\) \{[\s\S]*?runtime.finishSwap\(requestId,true\)/);
  assert.match(runtime,/pending->quote.reset\(\);[\s\S]*?QStringLiteral\("refreshing"\)/);
  assert.match(runtime,/runtime.m_controller.dispatchSwapQuote\(requestId/);
  assert.match(runtime,/pending->quote && pending->quote->requiresApproval/);
  assert.match(qml,/genycaster\.miniapp\.swap\.approve_exact/);
  assert.match(qml,/genycaster\.miniapp\.swap\.confirm_swap/);
  assert.match(qml,/onClicked: miniAppHost.executeSwapStep\(miniAppHost.swapRequestId\)/);
  assert.doesNotMatch(qml,/eth_sendTransaction|privateKey|mnemonic/);
});

test('success requires mined successful receipt and returns official SDK transactions union',()=>{
  const receipt=file('ui/app/miniapp/swap_execution.hpp');
  const runtime=file('ui/app/miniapp/runtime.cpp');
  assert.match(receipt,/enum class ReceiptStatus \{ Pending, Success, Reverted, Invalid \}/);
  assert.match(receipt,/value\("transactionHash"\)\.compare\(expectedHash/);
  assert.match(receipt,/value\("from"\)\.compare\(sender/);
  assert.match(receipt,/value\("to"\)\.compare\(recipient/);
  assert.match(receipt,/value\("blockHash"\)/);
  assert.match(receipt,/value\("status"\)==QStringLiteral\("0x1"\)/);
  assert.match(receipt,/value\("status"\)==QStringLiteral\("0x0"\)/);
  assert.match(runtime,/\.method="eth_getTransactionReceipt"/);
  assert.match(runtime,/pending->receiptAttempts>80/);
  assert.match(runtime,/runtime.finishSwap\(requestId,true\)/);
  assert.match(runtime,/"swap"\),QJsonObject\{\{QStringLiteral\("transactions"\),transactions\}\}/);
  assert.match(runtime,/QStringLiteral\("rejected_by_user"\)/);
  assert.match(runtime,/QStringLiteral\("swap_failed"\)/);
});

test('no revoked document, changed wallet, expired quote or foreground switch can start a new tx',()=>{
  const runtime=file('ui/app/miniapp/runtime.cpp');
  const host=file('ui/app/miniapp/host.cpp');
  assert.match(runtime,/active->documentGeneration!=generation/);
  assert.match(runtime,/m_walletTaskId!=taskId/);
  assert.match(runtime,/pending->quote->expiresAtMs<=QDateTime::currentMSecsSinceEpoch/);
  assert.match(runtime,/pending->taker,Qt::CaseInsensitive\)==0/);
  assert.match(runtime,/QStringLiteral\("0x2105"\)/);
  assert.match(runtime,/if \(std::ranges::any_of\(target->pendingSwaps/);
  assert.match(host,/if \(m_swapStage == QStringLiteral\("swap_receipt"\)/);
  assert.match(host,/if \(m_swapStage == QStringLiteral\("review"\)\)\s*m_swapStage = QStringLiteral\("editing"\)/);
  assert.match(host,/QTimer::singleShot\(static_cast<int>\(remaining\), this/);
});

test('wallet transactions include Base chain ID and exact approval address, not an arbitrary spender',()=>{
  const quote=file('ui/app/miniapp/swap_quote.hpp');
  assert.equal((quote.match(/QStringLiteral\("0x2105"\)/g)||[]).length,2);
  assert.match(quote,/QStringLiteral\("0x095ea7b3"\)/);
  assert.match(quote,/exactApprovalCalldata\(/);
  assert.match(quote,/baseSwapRouter/);
  assert.match(quote,/decimalToHex256\(/);
});

test('slippage is editable, bounded, preserved after approval and bound to returned quote',()=>{
  const runtime=file('ui/app/miniapp/runtime.cpp');
  const dialog=file('ui/components/apps/MiniAppSwapPreflight.qml');
  const server=file('services/swap-quote/quote.mjs');
  assert.match(dialog,/property string slippageInput: "100"/);
  assert.match(dialog,/Number\(control.slippageInput\) >= 10/);
  assert.match(dialog,/Number\(control.slippageInput\) <= 300/);
  assert.match(dialog,/slippageBps: Number\(control.slippageInput\)/);
  assert.match(runtime,/int slippageBps\{100\}/);
  assert.match(runtime,/slippage<10 \|\| slippage>300/);
  assert.match(runtime,/pending->slippageBps=slippage/);
  assert.match(runtime,/quoteSlippage!=pending->slippageBps/);
  assert.match(runtime,/QStringLiteral\("slippageBps"\),pending->slippageBps/);
  assert.match(runtime,/QStringLiteral\("slippageBps"\),pendingQuote->slippageBps/);
  assert.match(server,/slippageBps < 10 \|\| slippageBps > 300/);
});
