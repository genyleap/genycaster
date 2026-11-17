module;

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module farcaster.provider.compatibility.browser_wallet.bridge;

import farcaster.capability.wallet;
import farcaster.foundation.error;
import farcaster.foundation.result;

namespace farcaster::provider::compatibility::browser_wallet::bridge {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = asio::ip::tcp;
using Clock = std::chrono::steady_clock;

constexpr std::size_t TokenBytes = 32U;
constexpr std::size_t MaximumQueuedRequests = 32U;
constexpr std::chrono::milliseconds PollDelay{20};
// The local HTTP transport has a short deadline independent of the wallet
// approval window. Never allow a partial HTTP request to stall shutdown.
constexpr auto HttpTransportTimeout = std::chrono::seconds{3};

[[nodiscard]] foundation::error::Error makeError(
    const foundation::error::ErrorCode code,
    std::string message)
{
    return foundation::error::Error::make(code, std::move(message));
}

[[nodiscard]] foundation::result::Result<std::string> randomHex(const std::size_t bytes)
{
    std::vector<unsigned char> buffer(bytes);
    if (RAND_bytes(buffer.data(), static_cast<int>(buffer.size())) != 1) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "OpenSSL failed to create an EIP-1193 wallet-session capability token."));
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string result(buffer.size() * 2U, '0');
    for (std::size_t index{}; index < buffer.size(); ++index) {
        result[index * 2U] = digits[(buffer[index] >> 4U) & 0x0FU];
        result[index * 2U + 1U] = digits[buffer[index] & 0x0FU];
    }
    return result;
}

[[nodiscard]] std::string htmlEscape(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

[[nodiscard]] std::string normalizedEthereumAddress(const json::value& value)
{
    if (!value.is_string()) return {};
    std::string address{value.as_string()};
    if (address.size() != 42U || address[0] != '0'
        || (address[1] != 'x' && address[1] != 'X')) return {};
    address[1] = 'x';
    for (std::size_t i = 2; i < address.size(); ++i) {
        char& character = address[i];
        if (character >= 'A' && character <= 'F')
            character = static_cast<char>(character - 'A' + 'a');
        if (!((character >= '0' && character <= '9')
            || (character >= 'a' && character <= 'f'))) return {};
    }
    return address;
}

[[nodiscard]] bool validMethod(const std::string_view method)
{
    return method == "personal_sign" || method == "net_version" || method == "web3_clientVersion"
        || method.starts_with("eth_") || method.starts_with("wallet_");
}

[[nodiscard]] bool sensitiveMethod(
    const std::string_view method,
    const bool hasAccountAccessGrant)
{
    if (method == "eth_requestAccounts") {
        // Never silently replay an account-connection request after a cached
        // Mini App origin grant: re-entry requires a fresh explicit click.
        // The wallet extension may legitimately answer without a popup when
        // the selected account is already approved.
        return true;
    }
    if (method == "personal_sign"
        || method == "eth_decrypt" || method == "eth_getEncryptionPublicKey") {
        return true;
    }
    if (method.starts_with("eth_sign") || method.starts_with("eth_send")) {
        return true;
    }
    if (!method.starts_with("wallet_")) {
        return false;
    }
    return method != "wallet_getCapabilities" && method != "wallet_getPermissions"
        && method != "wallet_getCallsStatus";
}

[[nodiscard]] std::optional<std::string> requestedChainId(
    const std::string_view method,
    const std::string_view paramsJson)
{
    if (method != "wallet_switchEthereumChain" && method != "wallet_addEthereumChain") {
        return std::nullopt;
    }
    boost::system::error_code error{};
    const auto parsed = json::parse(paramsJson, error);
    if (error || !parsed.is_array() || parsed.as_array().empty()
        || !parsed.as_array().front().is_object()) {
        return std::string{};
    }
    const auto& object = parsed.as_array().front().as_object();
    const auto* value = object.if_contains("chainId");
    if (value == nullptr || !value->is_string()) return std::string{};
    return std::string{value->as_string()};
}

[[nodiscard]] std::optional<std::uint64_t> parseHexChainId(std::string_view value)
{
    if (!value.starts_with("0x") && !value.starts_with("0X")) return std::nullopt;
    value.remove_prefix(2U);
    if (value.empty() || value.size() > 16U) return std::nullopt;
    std::uint64_t result{};
    for (const char ch : value) {
        std::uint64_t digit{};
        if (ch >= '0' && ch <= '9') digit = static_cast<std::uint64_t>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') digit = static_cast<std::uint64_t>(10 + ch - 'a');
        else if (ch >= 'A' && ch <= 'F') digit = static_cast<std::uint64_t>(10 + ch - 'A');
        else return std::nullopt;
        if (result > (UINT64_MAX - digit) / 16U) return std::nullopt;
        result = result * 16U + digit;
    }
    return result;
}

[[nodiscard]] bool configuredChain(
    const std::vector<std::string>& supportedChains,
    const std::string_view hexadecimalChainId)
{
    const auto numeric = parseHexChainId(hexadecimalChainId);
    if (!numeric) return false;
    const std::string caip = "eip155:" + std::to_string(*numeric);
    return std::ranges::find(supportedChains, caip) != supportedChains.end();
}

[[nodiscard]] capability::wallet::EthereumRpcResponse rpcError(
    const std::uint64_t id,
    const std::int32_t code,
    std::string message,
    std::string details = {})
{
    capability::wallet::EthereumRpcResponse response{};
    response.id = id;
    response.error = capability::wallet::EthereumRpcError{
        .code = code, .message = std::move(message), .details = std::move(details)};
    return response;
}

[[nodiscard]] std::optional<capability::wallet::EthereumWalletEventKind> eventKind(
    const std::string_view name)
{
    using Kind = capability::wallet::EthereumWalletEventKind;
    if (name == "accountsChanged") return Kind::AccountsChanged;
    if (name == "chainChanged") return Kind::ChainChanged;
    if (name == "connect") return Kind::Connect;
    if (name == "disconnect") return Kind::Disconnect;
    if (name == "message") return Kind::Message;
    return std::nullopt;
}

[[nodiscard]] std::string bridgePage(
    const std::string_view basePath,
    const std::string_view nonce,
    const std::string_view appName,
    const std::string_view origin)
{
    std::string page = R"html(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Wallet approval · GenyCaster</title><style nonce=")html";
    page += nonce;
    page += R"html(">
:root{color-scheme:dark;--accent:#168bff;--accent-soft:#102c48;--canvas:#10141c;--panel:#1b2330;--raised:#242e3d;--line:#354356;--ink:#f3f7fc;--muted:#a5b3c7;--danger:#ff8f9c;--success:#87edbf;font:14px/1.48 Inter,ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,sans-serif}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;display:grid;place-items:center;padding:20px;background:radial-gradient(ellipse at 18% 6%,#163555 0%,transparent 46%),var(--canvas);color:var(--ink)}
main{width:min(100%,460px);background:var(--panel);border:1px solid var(--line);border-radius:22px;padding:26px;box-shadow:0 24px 70px #030a1588}
.brand{display:flex;align-items:center;gap:10px;color:#99c9ff;font-size:.77rem;font-weight:750;letter-spacing:.09em;text-transform:uppercase;margin-bottom:12px}
.brand-mark{display:grid;place-items:center;width:28px;height:28px;border-radius:9px;background:var(--accent-soft);border:1px solid #295b8d;font-size:16px}
h1{font-size:1.55rem;letter-spacing:-.035em;line-height:1.22;margin:0 0 5px;font-weight:750}
.subtitle{color:var(--muted);margin:0 0 19px;font-size:.87rem}
h2{font-size:1.03rem;line-height:1.4;margin:7px 0 7px}
.app{padding:14px 16px;border:1px solid var(--line);border-radius:14px;background:var(--raised);margin-bottom:20px}
#app-name{display:block;font-size:1rem;font-weight:750}
#origin{font-size:.82rem;color:var(--muted);overflow-wrap:anywhere;margin-top:3px}
.label{display:block;font-size:.77rem;font-weight:650;color:var(--muted);margin-bottom:9px}
#options{display:grid;gap:8px;max-height:225px;overflow-y:auto}
button{font:inherit;cursor:pointer;border-radius:12px;padding:11px 14px;border:1px solid var(--line);color:var(--ink);background:var(--raised);-webkit-appearance:none;appearance:none}
button:disabled{opacity:.55;cursor:not-allowed}
button:hover:enabled{border-color:#5e9cde}
button:focus-visible,summary:focus-visible{outline:2px solid #77bcff;outline-offset:2px}
.wallet-option{display:flex;align-items:center;gap:12px;width:100%;text-align:start;min-height:50px}
.wallet-option[aria-checked="true"]{background:var(--accent-soft);border-color:var(--accent);box-shadow:inset 0 0 0 1px var(--accent)}
.wallet-dot{width:16px;height:16px;border:2px solid #8597ad;border-radius:50%;display:block;flex:none}
.wallet-option[aria-checked="true"] .wallet-dot{border:5px solid var(--accent);background:white}
.wallet-name{flex:1;font-weight:650;overflow-wrap:anywhere}
.wallet-tag{font-size:.69rem;color:var(--muted);letter-spacing:.01em}
#request{margin:17px 0 0;padding:15px;border:1px solid var(--line);border-radius:14px;background:var(--raised)}
#request[hidden],#message[hidden]{display:none}
#kind{font-size:.73rem;color:#81bdff;font-weight:700}
#message{max-height:100px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;font:.79rem/1.45 ui-monospace,SFMono-Regular,Menlo,monospace;padding:11px;border-radius:9px;background:var(--canvas);color:var(--ink);margin:10px 0}
details{font-size:.8rem;color:var(--muted);margin:12px 0 0}
summary{cursor:pointer}
pre{background:var(--canvas);padding:10px;border-radius:8px;white-space:pre-wrap;overflow:auto;overflow-wrap:anywhere;max-height:160px;font:.73rem/1.45 ui-monospace,SFMono-Regular,Menlo,monospace;color:var(--ink)}
.actions{display:flex;gap:9px;margin-top:16px}
.actions button{flex:1;text-align:center;min-height:44px}
#approve{background:var(--accent);border-color:var(--accent);color:white;font-weight:750}
#approve:hover:enabled{background:#319cff}
#status,#wallet{font-size:.8rem;color:var(--muted);line-height:1.5;margin:10px 0 0;overflow-wrap:anywhere}
#status.error,#wallet.error{color:var(--danger)}
#status.ok,#wallet.ok{color:var(--success)}
.help{margin:17px 0 0;font-size:.8rem;line-height:1.5;color:var(--muted)}
@media(max-width:450px){main{padding:18px}body{padding:10px}.actions{flex-wrap:wrap}}
@media(prefers-reduced-motion:no-preference){button{transition:border-color .15s ease,background .15s ease}}
</style></head><body><main><div class="brand"><span class="brand-mark" aria-hidden="true">✦</span>GenyCaster</div><h1>Wallet approval</h1><p class="subtitle">Connect securely using your selected browser wallet.</p><section class="app"><strong id="app-name">)html";
    page += htmlEscape(appName);
    page += R"html(</strong><div id="origin">)html";
    page += htmlEscape(origin);
    page += R"html(</div></section>
<span class="label" id="wallet-label">Choose your wallet</span>
<div id="options" role="radiogroup" aria-labelledby="wallet-label" aria-busy="true"></div>
<p id="wallet" role="status" aria-live="polite">Looking for wallets…</p>
<section id="request" hidden><div id="kind">Explicit approval required</div>
<h2 id="method">Wallet request</h2>
<pre id="message" hidden></pre>
<details><summary>View technical request</summary><pre id="params"></pre></details>
<div class="actions"><button id="reject" type="button">Cancel</button>
<button id="approve" type="button">Continue to wallet</button></div></section>
<p id="status" role="status" aria-live="polite">Waiting for wallet request…</p>
<p class="help">Confirm in your wallet. Your private key stays there.</p>
<details><summary>Why is this a local page?</summary>
This one-time page passes the request to your installed wallet extension. Your wallet may display 127.0.0.1 as the requesting site; check the Mini App domain and full message before approving.
</details>
<script nonce=")html";
    page += nonce;
    page += R"html(">
const base=")html";
    page += basePath;
    page += R"html(";
const status=document.getElementById('status'),wallet=document.getElementById('wallet');
const box=document.getElementById('request'),approve=document.getElementById('approve');
const options=document.getElementById('options'),reject=document.getElementById('reject');
const method=document.getElementById('method'),params=document.getElementById('params');
const message=document.getElementById('message'),kind=document.getElementById('kind');
let current=null,busy=false,selected=null,listeners=[],undelivered=null;
let stopped=false, nextTimer=null, lastConsentResultShown=false;
const scheduleNext=(delay=750)=>{if(!stopped)nextTimer=setTimeout(poll,delay);};
const stopPolling=()=>{
  stopped=true;
  if(nextTimer!==null)clearTimeout(nextTimer);
  nextTimer=null;
  detach();
};
const providers=[];
const events=['accountsChanged','chainChanged','connect','disconnect','message'];
const show=(msg,level='')=>{status.className=level;status.textContent=msg;};
const post=async(path,body)=>{
  const response=await fetch(base+path,{method:'POST',headers:{'content-type':'application/json'},cache:'no-store',body:JSON.stringify(body)});
  if(!response.ok)throw new Error('Could not deliver request result ('+response.status+').');
};
const detach=()=>{
  if(selected&&selected.provider.removeListener)
    for(const [event,callback] of listeners)selected.provider.removeListener(event,callback);
  listeners=[];
};
const describe=async()=>{
  const chosen=selected;
  if(!chosen){wallet.className='error';wallet.textContent='Choose a wallet.';return;}
  try{
    const accounts=await chosen.provider.request({method:'eth_accounts'});
    if(stopped||selected!==chosen)return;
    wallet.className='ok';
    wallet.textContent=Array.isArray(accounts)&&accounts[0]
      ? 'Already connected · '+String(accounts[0]).slice(0,6)+'…'+String(accounts[0]).slice(-4)
      : 'Ready to connect. Select your wallet, then approve the request.';
  }catch(error){
    if(stopped||selected!==chosen)return;
    wallet.className='error';wallet.textContent=String(error?.message||error);
  }
};
const select=index=>{
  if(busy)return;
  detach();selected=providers[index];
  if(!selected)return;
  for(const button of options.querySelectorAll('.wallet-option')){
    const active=Number(button.dataset.index)===index;
    button.setAttribute('aria-checked',String(active));
    button.tabIndex=active?0:-1;
  }
  if(typeof selected.provider.on==='function')for(const event of events){
    const callback=value=>{
      if(stopped)return;
      post('/event',{event,params:[value]}).catch(()=>{});
      if(!busy&&event==='accountsChanged')describe();
    };
    selected.provider.on(event,callback);
    listeners.push([event,callback]);
  }
  describe();
  if(current&&!current.requiresApproval)execute();
};
const add=(name,provider)=>{
  if(!provider||typeof provider.request!=='function'||providers.some(item=>item.provider===provider)
      ||providers.length>=12)return;
  const entry={name:String(name||'Browser wallet').slice(0,64),provider};
  const index=providers.push(entry)-1;
  const item=document.createElement('button');
  item.type='button';
  item.className='wallet-option';
  item.setAttribute('role','radio');
  item.setAttribute('aria-checked','false');
  item.tabIndex=-1;
  item.dataset.index=String(index);
  const dot=document.createElement('span');dot.className='wallet-dot';dot.setAttribute('aria-hidden','true');
  const title=document.createElement('span');title.className='wallet-name';title.textContent=entry.name;
  const hint=document.createElement('span');hint.className='wallet-tag';hint.textContent='Extension';
  item.append(dot,title,hint);
  item.addEventListener('click',()=>select(index));
  options.append(item);
  options.setAttribute('aria-busy','false');
  if(!selected)select(index);
};
options.addEventListener('keydown',event=>{
  if(event.key!=='ArrowDown'&&event.key!=='ArrowUp'
      &&event.key!=='ArrowLeft'&&event.key!=='ArrowRight')return;
  if(!providers.length||busy)return;
  event.preventDefault();
  const index=providers.findIndex(item=>item===selected);
  const delta=(event.key==='ArrowDown'||event.key==='ArrowRight')?1:-1;
  const next=(index+delta+providers.length)%providers.length;
  select(next);
  const target=options.querySelector('[data-index="'+next+'"]');
  if(target)target.focus();
});
window.addEventListener('eip6963:announceProvider',event=>{
  const detail=event.detail;
  if(detail&&detail.info&&typeof detail.info.name==='string')
    add(detail.info.name,detail.provider);
});
window.dispatchEvent(new Event('eip6963:requestProvider'));
if(window.ethereum&&typeof window.ethereum.request==='function')
  add('Browser wallet',window.ethereum);
setTimeout(()=>{
  if(!providers.length){wallet.className='error';
    wallet.textContent='No EIP-1193 wallet detected. Reopen the connection in GenyCaster and choose a browser with a wallet extension.';}
},800);
const requestLabel=name=>{
  if(name==='eth_requestAccounts')return 'Connect wallet';
  if(name==='personal_sign'||name.startsWith('eth_sign'))return 'Sign message';
  if(name.startsWith('eth_send')||name==='wallet_sendCalls')return 'Review transaction';
  if(name==='wallet_switchEthereumChain'||name==='wallet_addEthereumChain')return 'Change network';
  if(name==='wallet_requestPermissions')return 'Review wallet permissions';
  return 'Wallet request';
};
const readable=rpc=>{
  if(rpc.method!=='personal_sign'||!Array.isArray(rpc.params))return '';
  const hex=rpc.params.filter(value=>typeof value==='string'&&/^0x(?:[0-9a-fA-F]{2}){1,4096}$/.test(value)
    && value.length>42).sort((a,b)=>b.length-a.length)[0];
  if(!hex)return '';
  try{
    const text=new TextDecoder('utf-8',{fatal:true}).decode(
      Uint8Array.from(hex.slice(2).match(/../g),value=>parseInt(value,16)));
    return /^[\x09\x0a\x0d\x20-\x7e]+$/.test(text)?text:'';
  }catch{return '';}
};
const completed=()=>{
  current=null;undelivered=null;busy=false;
  approve.disabled=false;reject.disabled=false;box.hidden=true;
  // Keep the final result visible until the next explicit consent request.
};
const execute=async()=>{
  if(!current||busy)return;
  if(!selected){show('Choose a wallet before continuing.','error');return;}
  busy=true;approve.disabled=true;reject.disabled=true;
  const request=current;
  const showApproval=Boolean(request.requiresApproval);
  if(!undelivered){
    if(showApproval)show('Opening '+selected.name+'… Check the wallet extension for any approval prompt.');
    try{
      undelivered={transportId:request.transportId,result:
        await selected.provider.request({method:request.method,params:request.params})};
      if(undelivered.result===undefined)undelivered.result=null;
    }catch(error){
      const code=Number.isInteger(Number(error?.code))?Number(error.code):4900;
      undelivered={transportId:request.transportId,error:{code,message:String(error?.message||error).slice(0,512)}};
    }
  }
  // Retrying transport delivery must not repeat a wallet approval.
  const response=undelivered;
  try{
    await post('/result',response);
    if(showApproval){
      lastConsentResultShown=true;
      show(response.error?(response.error.code===4001?'Request declined.':'Wallet request failed.')
        :(request.method==='eth_requestAccounts'
            ? 'Wallet connected. Return to GenyCaster.'
            : 'Approved. Return to GenyCaster.'),response.error?'error':'ok');
    }
    completed();
  }catch(error){
    busy=false;approve.disabled=false;reject.disabled=false;
    approve.textContent='Retry delivery';
    reject.disabled=true;
    box.hidden=false;
    show('The wallet responded. Retry delivery without signing again.','error');
  }
};
const cancel=async()=>{
  if(!current||busy||undelivered)return;
  busy=true;approve.disabled=true;reject.disabled=true;
  try{
    await post('/result',{transportId:current.transportId,
      error:{code:4001,message:'Cancelled by the user.'}});
    show('Request cancelled.');completed();
  }catch{
    busy=false;approve.disabled=false;reject.disabled=false;
    show('Could not cancel the wallet request.','error');
  }
};
approve.addEventListener('click',execute);
reject.addEventListener('click',cancel);
const poll=async()=>{
  if(stopped)return;
  if(busy||current){scheduleNext(500);return;}
  try{
    const response=await fetch(base+'/next',{cache:'no-store'});
    if(stopped)return;
    if(response.status===204){scheduleNext(1000);return;}
    if(response.status===404||response.status===410){
      stopPolling();
      box.hidden=true;
      // A normal hand-off closes its bridge session on the host. Do not
      // replace the successful acknowledgement with a new status frame.
      if(!lastConsentResultShown)
        show('Session ended. Return to GenyCaster.','ok');
      return;
    }
    if(!response.ok)throw new Error('Wallet session unavailable.');
    const rpc=await response.json();
    if(!rpc||typeof rpc.method!=='string'||!Array.isArray(rpc.params))
      throw new Error('Invalid wallet request.');
    current=rpc;
    undelivered=null;
    kind.textContent=rpc.requiresApproval?'Explicit approval required':'Read-only request';
    method.textContent=requestLabel(rpc.method);
    params.textContent=JSON.stringify({method:rpc.method,params:rpc.params},null,2);
    const text=readable(rpc);
    message.hidden=!text;
    message.textContent=text.length>240?text.slice(0,240)+'…':text;
    // Chain/account reads following sign-in are forwarded silently.
    // Reopening the consent card for each background RPC caused visible
    // layout jumps and looked like a page refresh after confirmation.
    box.hidden=!rpc.requiresApproval;
    if(rpc.requiresApproval) lastConsentResultShown=false;
    approve.textContent=rpc.method==='eth_requestAccounts'?'Connect selected wallet'
      :rpc.requiresApproval?'Open selected wallet':'Use selected wallet';
    if(rpc.requiresApproval)
      show('Select a wallet above, then choose '+approve.textContent+'.');
    if(!rpc.requiresApproval&&selected)execute();
  }catch(error){
    if(!stopped)show(String(error?.message||error),'error');
  }
  scheduleNext(800);
};
window.addEventListener('pagehide',stopPolling,{once:true});
poll();
</script></main></body></html>)html";
    return page;
}

struct Pending final {
    std::uint64_t transportId{};
    capability::wallet::EthereumRpcRequest request{};
    capability::wallet::EthereumRpcCompletion completion{};
    Clock::time_point created{};
    std::string origin{};
    bool delivered{};
};

class Bridge final : public capability::wallet::IEthereumWalletBridge {
public:
    explicit Bridge(Config config, std::shared_ptr<asio::io_context> io, tcp::acceptor acceptor, std::string token, std::string nonce)
        : m_config(std::move(config)), m_io(std::move(io)), m_acceptor(std::move(acceptor)), m_token(std::move(token)),
          m_nonce(std::move(nonce)), m_port(m_acceptor.local_endpoint().port()),
          m_worker([this](const std::stop_token stopToken) { run(stopToken); })
    {
    }

    ~Bridge() override {
        // Stop intake first. The nonblocking transport checks this token
        // every poll, preventing shutdown from waiting on a partial request.
        m_worker.request_stop();
        endSession();
    }

    [[nodiscard]] bool available() const noexcept override {
        return static_cast<bool>(m_config.launch) && !m_config.supportedChains.empty();
    }

    [[nodiscard]] std::vector<std::string> supportedChains() const override {
        return m_config.supportedChains;
    }

    [[nodiscard]] foundation::result::Status beginSession(
        std::string appName,
        std::string origin) override
    {
        if (appName.empty() || origin.empty()) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "A Mini App wallet session requires a non-empty application name and origin."));
        }
        endSession();
        auto token = randomHex(TokenBytes);
        auto nonce = randomHex(16U);
        if (!token) return std::unexpected(token.error());
        if (!nonce) return std::unexpected(nonce.error());
        std::scoped_lock lock{m_mutex};
        m_token = std::move(*token);
        m_nonce = std::move(*nonce);
        m_appName = std::move(appName);
        m_origin = std::move(origin);
        m_active = true;
        m_pageLaunched = false;
        ++m_sessionGeneration;
        return {};
    }

    void endSession() noexcept override {
        std::vector<Pending> cancelled{};
        {
            std::scoped_lock lock{m_mutex};
            m_active = false;
            m_pageLaunched = false;
            m_appName.clear();
            m_origin.clear();
            ++m_sessionGeneration;
            for (auto& [id, pending] : m_pending) cancelled.push_back(std::move(pending));
            m_pending.clear();
        }
        for (auto& pending : cancelled) {
            if (pending.completion) {
                pending.completion(rpcError(
                    pending.request.id, 4900, "The Genycaster wallet session was closed."));
            }
        }
    }

    void setEventSink(capability::wallet::EthereumWalletEventSink sink) override {
        std::scoped_lock lock{m_mutex};
        m_eventSink = std::move(sink);
    }

    void request(
        capability::wallet::EthereumRpcRequest request,
        capability::wallet::EthereumRpcCompletion completion) override
    {
        if (!completion) return;
        if (!validMethod(request.method)) {
            completion(rpcError(request.id, 4200, "Unsupported EIP-1193 method."));
            return;
        }
        if (request.paramsJson.size() > m_config.maxRequestBytes) {
            completion(rpcError(request.id, 4200, "Wallet request exceeds the host size limit."));
            return;
        }
        boost::system::error_code parseError{};
        const auto params = json::parse(request.paramsJson, parseError);
        if (parseError || !params.is_array()) {
            completion(rpcError(request.id, 4200, "Wallet request params must be a JSON array."));
            return;
        }
        if (const auto chain = requestedChainId(request.method, request.paramsJson); chain) {
            if (chain->empty() || !configuredChain(m_config.supportedChains, *chain)) {
                completion(rpcError(
                    request.id, 4901, "The requested Ethereum chain is not enabled by this host."));
                return;
            }
        }

        std::string launchUrl{};
        std::uint64_t transportId{};
        std::optional<capability::wallet::EthereumRpcResponse> immediateError{};
        {
            std::scoped_lock lock{m_mutex};
            if (!m_active || !available()) {
                immediateError = rpcError(
                    request.id, 4900, "No active Genycaster wallet session is available.");
            } else if (!m_accountAccessGrants.contains(m_origin)
                && (request.method == "eth_accounts" || request.method == "wallet_getPermissions")) {
                // A Mini App must not obtain accounts or existing wallet
                // permissions before consent for its exact origin.
                capability::wallet::EthereumRpcResponse empty{};
                empty.id = request.id;
                empty.resultJson = "[]";
                immediateError = std::move(empty);
            } else if (!m_accountAccessGrants.contains(m_origin)
                && (request.method == "eth_coinbase" || request.method == "personal_sign"
                    || request.method.starts_with("eth_sign")
                    || request.method.starts_with("eth_send")
                    || request.method == "eth_decrypt"
                    || request.method == "eth_getEncryptionPublicKey"
                    || (request.method.starts_with("wallet_")
                        && request.method != "wallet_requestPermissions"
                        && request.method != "wallet_getCapabilities"))) {
                immediateError = rpcError(
                    request.id, 4100, "Connect your wallet to this Mini App before requesting this operation.");
            } else if (!m_pageLaunched
                && request.method != "eth_requestAccounts"
                && request.method != "wallet_requestPermissions") {
                // Passive chain discovery / read-only RPC must not launch a
                // browser and steal focus whenever a Mini App opens. The
                // wallet's actual chain cannot be fabricated before connect.
                immediateError = rpcError(
                    request.id, 4900, "Connect a wallet before making this request.");
            } else if (m_pending.size() >= MaximumQueuedRequests) {
                immediateError = rpcError(request.id, 4900, "Too many wallet requests are pending.");
            } else {
                transportId = ++m_nextTransportId;
                m_pending.emplace(transportId, Pending{
                    .transportId = transportId,
                    .request = std::move(request),
                    .completion = std::move(completion),
                    .created = Clock::now(),
                    .origin = m_origin,
                    .delivered = false,
                });
                if (!m_pageLaunched) {
                    m_pageLaunched = true;
                    launchUrl = sessionUrlLocked();
                }
            }
        }
        if (immediateError) {
            completion(std::move(*immediateError));
            return;
        }
        if (!launchUrl.empty()) {
            auto launched = m_config.launch(launchUrl);
            if (!launched) {
                Pending failed{};
                bool found = false;
                {
                    std::scoped_lock lock{m_mutex};
                    m_pageLaunched = false;
                    const auto position = m_pending.find(transportId);
                    if (position != m_pending.end()) {
                        failed = std::move(position->second);
                        m_pending.erase(position);
                        found = true;
                    }
                }
                if (found && failed.completion) {
                    failed.completion(rpcError(
                        failed.request.id, 4900, "Genycaster could not open the configured wallet browser.",
                        launched.error().message));
                }
            }
        }
    }

private:
    [[nodiscard]] std::string basePathLocked() const {
        return "/session/" + m_token;
    }

    [[nodiscard]] std::string sessionUrlLocked() const {
        return "http://127.0.0.1:" + std::to_string(m_port) + basePathLocked();
    }

    template <typename Body>
    void secureHeaders(http::response<Body>& response, const std::string_view nonce = {}) const {
        response.set(http::field::cache_control, "no-store, max-age=0");
        response.set(http::field::pragma, "no-cache");
        response.set("Referrer-Policy", "no-referrer");
        response.set("X-Content-Type-Options", "nosniff");
        response.set("X-Frame-Options", "DENY");
        if (!nonce.empty()) {
            response.set("Content-Security-Policy",
                "default-src 'none'; connect-src 'self'; script-src 'nonce-" + std::string{nonce}
                    + "'; style-src 'nonce-" + std::string{nonce} + "'; base-uri 'none'; frame-ancestors 'none'");
        }
    }

    [[nodiscard]] bool authorizedTarget(const std::string_view target, std::string& suffix) const {
        std::scoped_lock lock{m_mutex};
        if (!m_active) return false;
        const std::string base = basePathLocked();
        if (!target.starts_with(base)) return false;
        suffix.assign(target.substr(base.size()));
        return suffix.empty() || suffix == "/next" || suffix == "/result" || suffix == "/event";
    }

    void run(const std::stop_token stopToken) {
        boost::system::error_code error{};
        m_acceptor.non_blocking(true, error);
        while (!stopToken.stop_requested()) {
            expireRequests();
            tcp::socket socket{m_acceptor.get_executor()};
            m_acceptor.accept(socket, error);
            if (error == asio::error::would_block || error == asio::error::try_again) {
                error.clear();
                std::this_thread::sleep_for(PollDelay);
                continue;
            }
            if (error) {
                error.clear();
                std::this_thread::sleep_for(PollDelay);
                continue;
            }
            handle(std::move(socket), stopToken);
        }
    }

    void expireRequests() {
        std::vector<Pending> expired{};
        const auto now = Clock::now();
        {
            std::scoped_lock lock{m_mutex};
            for (auto it = m_pending.begin(); it != m_pending.end();) {
                if (now - it->second.created >= m_config.requestTimeout) {
                    expired.push_back(std::move(it->second));
                    it = m_pending.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (auto& pending : expired) {
            if (pending.completion) {
                pending.completion(rpcError(
                    pending.request.id, 4900, "The wallet request timed out before completion."));
            }
        }
    }

    void handle(tcp::socket socket, const std::stop_token stopToken) {
        beast::flat_buffer buffer{};
        http::request_parser<http::string_body> parser{};
        parser.body_limit(m_config.maxRequestBytes);
        boost::system::error_code error{};
        socket.non_blocking(true, error);
        if (error) return;

        // Boost.Asio retries EWOULDBLOCK internally on blocking sockets, so
        // SO_RCVTIMEO is not sufficient. Incremental nonblocking reads keep
        // the parser state and let us honor both deadlines and shutdown.
        const auto deadline = Clock::now() + HttpTransportTimeout;
        while (!stopToken.stop_requested() && !parser.is_done()
               && Clock::now() < deadline) {
            error.clear();
            http::read_some(socket, buffer, parser, error);
            if (error == asio::error::would_block
                || error == asio::error::try_again) {
                std::this_thread::sleep_for(PollDelay);
                continue;
            }
            if (error) return;
        }
        if (stopToken.stop_requested() || !parser.is_done()) return;
        const auto request = parser.release();
        // URL capabilities alone do not protect a loopback listener from DNS
        // rebinding or cross-site requests. Only the browser document served
        // by this exact loopback authority may submit wallet responses.
        const std::string authority = "127.0.0.1:" + std::to_string(m_port);
        if (request[http::field::host] != authority) {
            writeText(socket, http::status::forbidden, "Invalid wallet bridge Host.");
            return;
        }
        if (request.method() == http::verb::post) {
            if (request[http::field::origin] != "http://" + authority) {
                writeText(socket, http::status::forbidden, "Invalid wallet bridge Origin.");
                return;
            }
            const auto site = request["Sec-Fetch-Site"];
            if (!site.empty() && site != "same-origin") {
                writeText(socket, http::status::forbidden, "Cross-site wallet request.");
                return;
            }
        }
        const std::string target{request.target()};
        std::string suffix{};
        if (!authorizedTarget(target, suffix)) {
            writeText(socket, http::status::not_found, "Not found.");
            return;
        }
        if (request.method() == http::verb::get && suffix.empty()) {
            std::string nonce{};
            std::string appName{};
            std::string origin{};
            std::string base{};
            {
                std::scoped_lock lock{m_mutex};
                nonce = m_nonce;
                appName = m_appName;
                origin = m_origin;
                base = basePathLocked();
            }
            http::response<http::string_body> response{http::status::ok, request.version()};
            response.set(http::field::content_type, "text/html; charset=utf-8");
            secureHeaders(response, nonce);
            response.body() = bridgePage(base, nonce, appName, origin);
            response.prepare_payload();
            writeBounded(socket, response);
            return;
        }
        if (request.method() == http::verb::get && suffix == "/next") {
            writeNext(socket, request.version());
            return;
        }
        if (request.method() == http::verb::post && suffix == "/result") {
            receiveResult(socket, request.version(), request.body());
            return;
        }
        if (request.method() == http::verb::post && suffix == "/event") {
            receiveEvent(socket, request.version(), request.body());
            return;
        }
        writeText(socket, http::status::method_not_allowed, "Unsupported method.");
    }

    void writeNext(tcp::socket& socket, const unsigned version) {
        std::optional<json::object> payload{};
        {
            std::scoped_lock lock{m_mutex};
            for (auto& [id, pending] : m_pending) {
                if (pending.delivered) continue;
                pending.delivered = true;
                boost::system::error_code error{};
                auto params = json::parse(pending.request.paramsJson, error);
                if (error || !params.is_array()) params = json::array{};
                payload = json::object{
                    {"transportId", id},
                    {"method", pending.request.method},
                    {"params", std::move(params)},
                    {"requiresApproval", sensitiveMethod(
                        pending.request.method,
                        m_accountAccessGrants.contains(pending.origin))},
                };
                break;
            }
        }
        if (!payload) {
            http::response<http::empty_body> response{http::status::no_content, version};
            secureHeaders(response);
            writeBounded(socket, response);
            return;
        }
        writeJson(socket, version, *payload);
    }

    void receiveResult(tcp::socket& socket, const unsigned, const std::string_view body) {
        boost::system::error_code parseError{};
        const auto parsed = json::parse(body, parseError);
        if (parseError || !parsed.is_object()) {
            writeText(socket, http::status::bad_request, "Invalid result payload.");
            return;
        }
        const auto& object = parsed.as_object();
        const auto* idValue = object.if_contains("transportId");
        if (idValue == nullptr || (!idValue->is_uint64() && !idValue->is_int64())) {
            writeText(socket, http::status::bad_request, "Missing transport id.");
            return;
        }
        const std::uint64_t transportId = idValue->is_uint64()
            ? idValue->as_uint64() : static_cast<std::uint64_t>(std::max<std::int64_t>(0, idValue->as_int64()));
        Pending pending{};
        bool found = false;
        {
            std::scoped_lock lock{m_mutex};
            const auto position = m_pending.find(transportId);
            if (position != m_pending.end()) {
                pending = std::move(position->second);
                m_pending.erase(position);
                found = true;
            }
        }
        if (!found) {
            writeText(socket, http::status::gone, "Wallet request is no longer active.");
            return;
        }

        capability::wallet::EthereumRpcResponse response{};
        response.id = pending.request.id;
        if (const auto* errorValue = object.if_contains("error"); errorValue != nullptr && errorValue->is_object()) {
            const auto& failure = errorValue->as_object();
            std::int32_t code = 4001;
            if (const auto* value = failure.if_contains("code"); value != nullptr && value->is_int64()) {
                code = static_cast<std::int32_t>(value->as_int64());
            }
            std::string message{"Wallet request failed."};
            if (const auto* value = failure.if_contains("message"); value != nullptr && value->is_string()) {
                message = std::string{value->as_string()};
            }
            std::string details{};
            if (const auto* value = failure.if_contains("details"); value != nullptr && value->is_string()) {
                details = std::string{value->as_string()};
            }
            response.error = capability::wallet::EthereumRpcError{
                .code = code, .message = std::move(message), .details = std::move(details)};
        } else if (const auto* result = object.if_contains("result")) {
            response.resultJson = json::serialize(*result);
            const std::string grantedAddress = pending.request.method == "eth_requestAccounts"
                && result->is_array() && !result->as_array().empty()
                    ? normalizedEthereumAddress(result->as_array().front())
                    : std::string{};
            bool grantedAccounts = !grantedAddress.empty();
            if (pending.request.method == "wallet_requestPermissions" && result->is_array()) {
                // EIP-2255 permissions must explicitly contain eth_accounts;
                // a successful response to an unrelated capability is not
                // permission to disclose account addresses or sign.
                for (const auto& permission : result->as_array()) {
                    if (!permission.is_object()) continue;
                    const auto* scope = permission.as_object().if_contains("parentCapability");
                    if (scope && scope->is_string() && scope->as_string() == "eth_accounts") {
                        grantedAccounts = true;
                        break;
                    }
                }
            }
            if (grantedAccounts) {
                std::scoped_lock lock{m_mutex};
                m_accountAccessGrants.insert(pending.origin);
                if (!grantedAddress.empty())
                    m_grantedAccountByOrigin[pending.origin] = grantedAddress;
                else
                    m_grantedAccountByOrigin.erase(pending.origin);
            }
        } else {
            response.error = capability::wallet::EthereumRpcError{
                .code = 4200, .message = "Wallet response omitted both result and error.", .details = {}};
        }
        if (pending.completion) pending.completion(std::move(response));
        writeText(socket, http::status::ok, "ok");
    }

    void receiveEvent(tcp::socket& socket, const unsigned, const std::string_view body) {
        boost::system::error_code parseError{};
        const auto parsed = json::parse(body, parseError);
        if (parseError || !parsed.is_object()) {
            writeText(socket, http::status::bad_request, "Invalid event payload.");
            return;
        }
        const auto& object = parsed.as_object();
        const auto* name = object.if_contains("event");
        const auto* params = object.if_contains("params");
        if (name == nullptr || !name->is_string() || params == nullptr || !params->is_array()) {
            writeText(socket, http::status::bad_request, "Invalid event fields.");
            return;
        }
        const auto kind = eventKind(name->as_string());
        if (!kind) {
            writeText(socket, http::status::bad_request, "Unsupported provider event.");
            return;
        }
        capability::wallet::EthereumWalletEventSink sink{};
        {
            std::scoped_lock lock{m_mutex};
            if (!m_active) {
                writeText(socket, http::status::gone, "Wallet session is closed.");
                return;
            }
            const bool disconnected = *kind == capability::wallet::EthereumWalletEventKind::Disconnect;
            const bool accountChanged = *kind == capability::wallet::EthereumWalletEventKind::AccountsChanged;
            bool principalChanged = disconnected;
            if (accountChanged) {
                // The provider may emit accountsChanged with the SAME account
                // immediately after a successful connect. Do not treat that
                // acknowledgement as an identity switch and re-open consent.
                std::string announced{};
                const auto& values = params->as_array();
                if (!values.empty() && values.front().is_array()
                    && !values.front().as_array().empty())
                    announced = normalizedEthereumAddress(values.front().as_array().front());
                const auto known = m_grantedAccountByOrigin.find(m_origin);
                principalChanged = announced.empty()
                    || known == m_grantedAccountByOrigin.end()
                    || known->second != announced;
            }
            if (principalChanged) {
                m_accountAccessGrants.erase(m_origin);
                m_grantedAccountByOrigin.erase(m_origin);
            }
            sink = m_eventSink;
        }
        if (sink) {
            sink(capability::wallet::EthereumWalletEvent{
                .kind = *kind,
                .paramsJson = json::serialize(*params),
            });
        }
        writeText(socket, http::status::ok, "ok");
    }

    template <class Body>
    void writeBounded(tcp::socket& socket, http::response<Body>& response) const
    {
        http::response_serializer<Body> serializer{response};
        const auto deadline = Clock::now() + HttpTransportTimeout;
        while (!serializer.is_done() && Clock::now() < deadline) {
            boost::system::error_code error{};
            http::write_some(socket, serializer, error);
            if (error == asio::error::would_block
                || error == asio::error::try_again) {
                std::this_thread::sleep_for(PollDelay);
                continue;
            }
            if (error) return;
        }
    }

    void writeText(tcp::socket& socket, const http::status status, std::string body) const {
        http::response<http::string_body> response{status, 11};
        response.set(http::field::content_type, "text/plain; charset=utf-8");
        secureHeaders(response);
        response.body() = std::move(body);
        response.prepare_payload();
        writeBounded(socket, response);
    }

    void writeJson(tcp::socket& socket, const unsigned version, const json::value& value) const {
        http::response<http::string_body> response{http::status::ok, version};
        response.set(http::field::content_type, "application/json; charset=utf-8");
        secureHeaders(response);
        response.body() = json::serialize(value);
        response.prepare_payload();
        writeBounded(socket, response);
    }

    Config m_config{};
    mutable std::mutex m_mutex{};
    std::shared_ptr<asio::io_context> m_io{};
    tcp::acceptor m_acceptor;
    std::string m_token{};
    std::string m_nonce{};
    std::string m_appName{};
    std::string m_origin{};
    std::uint16_t m_port{};
    std::uint64_t m_nextTransportId{};
    std::uint64_t m_sessionGeneration{};
    bool m_active{};
    bool m_pageLaunched{};
    std::map<std::uint64_t, Pending> m_pending{};
    std::set<std::string, std::less<>> m_accountAccessGrants{};
    // Store only the public address tied to an explicit account grant.
    // Never persist private keys, signatures or wallet permissions to disk.
    std::map<std::string, std::string, std::less<>> m_grantedAccountByOrigin{};
    capability::wallet::EthereumWalletEventSink m_eventSink{};
    std::jthread m_worker{};
};

} // namespace

foundation::result::Result<std::unique_ptr<capability::wallet::IEthereumWalletBridge>>
makeBridge(Config config)
{
    if (!config.launch) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "The EIP-1193 browser wallet bridge requires a launch callback."));
    }
    if (config.supportedChains.empty()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "The EIP-1193 browser wallet bridge requires at least one supported EVM chain."));
    }
    std::ranges::sort(config.supportedChains);
    config.supportedChains.erase(
        std::ranges::unique(config.supportedChains).begin(), config.supportedChains.end());
    for (const auto& chain : config.supportedChains) {
        if (!chain.starts_with("eip155:") || chain.size() <= 7U
            || !std::ranges::all_of(std::string_view{chain}.substr(7U), [](const char ch) {
                return ch >= '0' && ch <= '9';
            })) {
            return std::unexpected(makeError(
                foundation::error::ErrorCode::InvalidArgument,
                "Mini App wallet chains must use numeric eip155 CAIP-2 identifiers."));
        }
    }
    if (config.maxRequestBytes < 1024U || config.maxRequestBytes > 256U * 1024U) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Mini App wallet request limit must be between 1 KiB and 256 KiB."));
    }
    if (config.requestTimeout <= std::chrono::milliseconds::zero()) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::InvalidArgument,
            "Mini App wallet request timeout must be positive."));
    }

    auto token = randomHex(TokenBytes);
    auto nonce = randomHex(16U);
    if (!token) return std::unexpected(token.error());
    if (!nonce) return std::unexpected(nonce.error());

    auto io = std::make_shared<asio::io_context>();
    boost::system::error_code error{};
    tcp::acceptor acceptor{*io};
    acceptor.open(tcp::v4(), error);
    if (error) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Could not create the Mini App wallet loopback listener."));
    }
    acceptor.set_option(asio::socket_base::reuse_address(false), error);
    acceptor.bind(tcp::endpoint{asio::ip::make_address_v4("127.0.0.1"), 0}, error);
    if (error) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Could not bind the Mini App wallet loopback listener."));
    }
    acceptor.listen(asio::socket_base::max_listen_connections, error);
    if (error) {
        return std::unexpected(makeError(
            foundation::error::ErrorCode::Unavailable,
            "Could not listen for Mini App wallet loopback requests."));
    }

    // Move the acceptor together with its executor. The Bridge keeps the IO context
    // alive for the listener's complete lifetime.
    auto made = std::make_unique<Bridge>(std::move(config), std::move(io), std::move(acceptor), std::move(*token), std::move(*nonce));
    return std::unique_ptr<capability::wallet::IEthereumWalletBridge>{std::move(made)};
}

} // namespace farcaster::provider::compatibility::browser_wallet::bridge
