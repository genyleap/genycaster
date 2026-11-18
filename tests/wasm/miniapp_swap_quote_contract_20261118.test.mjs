import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=file=>readFileSync(new URL('../../'+file,import.meta.url),'utf8');

test('only the account-owned Qt network client can request a swap quote',()=>{
  const main=read('ui/app/main.cpp');
  const header=read('ui/app/identity/connections.hpp');
  const implementation=read('ui/app/identity/connections.cpp');
  assert.match(main,/&MiniAppHostController::swapQuoteNetworkRequested,[\s\S]*?&OpenProofConnectionsController::requestSwapQuote/);
  assert.match(main,/&OpenProofConnectionsController::swapQuoteReceived,[\s\S]*?&MiniAppHostController::receiveSwapQuoteResult/);
  assert.match(header,/void requestSwapQuote\(const QString& requestId, const QVariantMap& parameters\)/);
  assert.match(implementation,/cloudAuthorizationReady\(\)/);
  assert.match(implementation,/authorizeCloudRequest\(request\)/);
  assert.match(implementation,/QStringLiteral\("\/api\/v1\/cloud\/swap\/quote"\)/);
  assert.match(implementation,/QNetworkRequest::ManualRedirectPolicy/);
  assert.match(implementation,/const auto generation=m_grantGeneration/);
  assert.match(implementation,/if \(viewer!=m_viewerFid \|\| generation!=m_grantGeneration/);
  assert.doesNotMatch(implementation.slice(
    implementation.indexOf('void OpenProofConnectionsController::requestSwapQuote('),
    implementation.indexOf('void OpenProofConnectionsController::authorizeCloudRequest(')),
    /GENYCASTER_0X_API_KEY|0x-api-key|eth_sendTransaction|privateKey|mnemonic/);
});

test('quote is bound to current Mini App document, account, chain, amount and token pair',()=>{
  const runtime=read('ui/app/miniapp/runtime.cpp');
  const validation=read('ui/app/miniapp/swap_quote.hpp');
  assert.match(runtime,/void prepareSwapQuote\(const QString& requestId/);
  assert.match(runtime,/method="eth_requestAccounts"/);
  assert.match(runtime,/method="eth_chainId"/);
  assert.match(runtime,/QStringLiteral\("0x2105"\)/);
  assert.match(runtime,/pendingQuote->taker=account/);
  assert.match(runtime,/m_controller\.dispatchSwapQuote\(requestId/);
  assert.match(runtime,/void receiveSwapQuote\(const QString& requestId/);
  assert.match(runtime,/platform::miniapp::validateSwapQuote\([\s\S]*?pending->intent,pending->taker/);
  assert.match(runtime,/session->documentGeneration/);
  assert.match(validation,/field\("taker"\)\.compare\(taker/);
  assert.match(validation,/field\("sellToken"\)\.compare\(intent\.sellToken/);
  assert.match(validation,/field\("buyToken"\)\.compare\(intent\.buyToken/);
  assert.match(validation,/field\("sellAmount"\) != intent\.sellAmount/);
  assert.match(validation,/expiry <= nowMs \|\| expiry > nowMs \+ 25'000/);
  assert.match(validation,/baseAllowanceHolder/);
});

test('UI offers editable quote request but no invisible execution or token approvals',()=>{
  const dialog=read('ui/components/apps/MiniAppSwapPreflight.qml');
  const runtime=read('ui/app/miniapp/runtime.cpp');
  assert.match(dialog,/BidiTextField/);
  assert.match(dialog,/miniAppHost\.beginSwapQuote\(miniAppHost\.swapRequestId/);
  assert.match(dialog,/genycaster\.miniapp\.swap\.min_receive/);
  assert.match(dialog,/genycaster\.miniapp\.swap\.network_fee/);
  assert.match(dialog,/genycaster\.miniapp\.swap\.execution_notice/);
  assert.match(dialog,/miniAppHost\.executeSwapStep\(miniAppHost\.swapRequestId\)/);
  assert.match(dialog,/openProofConnections\.cloudAuthorizationReady/);
  assert.match(dialog,/miniAppHost\.invalidateSwapQuote\(miniAppHost\.swapRequestId\)/);
  const host = read('ui/app/miniapp/host.cpp');
  assert.match(host,/void MiniAppHostController::invalidateSwapQuote\(const QString& requestId\)/);
  assert.match(host,/QTimer::singleShot\(static_cast<int>\(remaining\), this/);
  assert.match(host,/expiry - QDateTime::currentMSecsSinceEpoch\(\)/);
  assert.doesNotMatch(dialog,/eth_sendTransaction|wallet_sendCalls|approve\(/);
  const capabilities=runtime.slice(runtime.indexOf('[[nodiscard]] QJsonArray capabilities('),
    runtime.indexOf('[[nodiscard]] QJsonArray chains('));
  assert.match(capabilities,/if \(supportsSwap && wallet != nullptr && wallet->available\(\)\)/);
  assert.match(capabilities,/actions\.swapToken/);
});

test('0x permissions are always scoped to AllowanceHolder, execution address remains dynamic',()=>{
  const quote=read('services/swap-quote/quote.mjs');
  const cpp=read('ui/app/miniapp/swap_quote.hpp');
  assert.match(quote,/quote\.allowanceTarget, ALLOWANCE_HOLDER/);
  assert.match(quote,/issues\.allowance/);
  assert.match(quote,/eq\(allowance\.spender, ALLOWANCE_HOLDER\)/);
  assert.match(quote,/transaction: \{to: tx\.to\.toLowerCase\(\)/);
  assert.match(cpp,/QStringLiteral\("0x095ea7b3"\)/);
  assert.match(cpp,/baseAllowanceHolder/);
  assert.match(cpp,/validAddress\(to\)/);
});
