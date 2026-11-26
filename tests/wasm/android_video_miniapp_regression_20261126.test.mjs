import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
const read=p=>fs.readFileSync(new URL('../../'+p,import.meta.url),'utf8');

test('Android lets Qt FFmpeg own the media request without an independent HEAD veto',()=>{
  const source=read('src/presentation/qt/media_playback_coordinator.cpp');
  const fn=source.slice(source.indexOf('qulonglong MediaPlaybackCoordinator::prepareSource'));
  const android=fn.split('#if !defined(Q_OS_WASM)')[1].split('#else')[0];
  assert.match(android,/!url.userInfo\(\).isEmpty\(\)/);
  assert.match(android,/emit guard->sourcePrepared\(requestId, source\)/);
  assert.doesNotMatch(android,/m_sourcePreparationBackend/);
  const player=read('ui/components/media/GenyVideoPlayer.qml');
  assert.match(player,/startupWatchdog.restart\(\)[\s\S]{0,220}player.source = target/);
  assert.match(player,/Android media startup timeout/);
  assert.match(player,/Android media playback stalled/);
  assert.match(player,/activation.resumeAfterLoad\s+&& !root.failed/);
});

test('native SDK exposes only bounded foreground external HTTPS launches',()=>{
  const runtime=read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime,/QStringLiteral\("actions.openUrl"\)/);
  const attention=runtime.slice(runtime.indexOf('const bool requiresForegroundAttention'),runtime.indexOf('const QJsonValue arguments = message.value'));
  assert.match(attention,/method == QStringLiteral\("openUrl"\)/);
  assert.match(attention,/!session.foreground/);
  const action=runtime.split('} else if (method == QStringLiteral("openUrl")) {')[1].split('} else if (method == QStringLiteral("openMiniApp"))')[0];
  assert.match(action,/argument.isString\(\)/);
  assert.match(action,/QUrl::StrictMode/);
  assert.match(action,/!publicHttps\(destination\)/);
  assert.match(action,/externalLaunchWindow.elapsed\(\) < 1500/);
  assert.match(action,/QDesktopServices::openUrl\(destination\)/);
});

test('Android draws chrome before creating isolated WebView and validates each RPC batch',()=>{
  const java=read('properties/Android/src/com/genyleap/genycaster/MiniAppBrowser.java');
  assert.match(java,/dialog.setContentView\(outer\);[\s\S]{0,400}UI.postDelayed/);
  assert.match(java,/pendingUrl=url;pendingReferrer=referrer/);
  assert.match(java,/new LinearLayout.LayoutParams\(-1,dp\(52\)\)/);
  assert.match(java,/tool\(activity,"share"/);
  assert.match(java,/new RippleDrawable/);
  assert.match(java,/web.onPause\(\)/);
  assert.match(java,/web.onResume\(\)/);
  assert.match(java,/new ArrayList<>\(queuedRpc\)/);
  assert.match(java,/epoch==generation/);
  assert.match(java,/rpc.epoch!=epoch \|\| !JSONObject.quote\(rpc.token\).equals\(result\)/);
  assert.match(java,/!isMainFrame \|\| !allowed\(sourceOrigin\)/);
  assert.match(java,/generation\+\+; queuedRpc.clear\(\)/);
  assert.match(java,/queuedRpc.size\(\)>=120/);
});

test('WalletConnect verification frame is narrowly allowed but loopback stays top-level only',()=>{
  const java=read('properties/Android/src/com/genyleap/genycaster/MiniAppBrowser.java');
  const ios=read('ui/app/miniapp/browser_ios.m');
  const wallet=read('src/providers/compatibility/browser_wallet/bridge.cpp');
  assert.match(wallet,/mobileWallet \? "; frame-src https:\/\/verify.walletconnect.org" : ""/);
  assert.doesNotMatch(wallet,/frame-src \*/);
  assert.match(java,/if\(!request.isForMainFrame\(\)\)[\s\S]{0,250}"verify.walletconnect.org".equalsIgnoreCase/);
  assert.match(java,/if \(wallet\) return uri!=null && uri.toString\(\).equals\(origin\)/);
  assert.match(ios,/if\(!mainFrame\) return \[url.scheme.lowercaseString isEqualToString:@"https"\]/);
  assert.match(ios,/\[url.host.lowercaseString isEqualToString:@"verify.walletconnect.org"\]/);
  assert.match(ios,/return \[url isEqual:self.origin\] && self.callbacks.allowed_url/);
});

test('mobile consent skips desktop browser selection and cancellation stops automatic reopening',()=>{
  const host=read('ui/app/miniapp/host.cpp');
  const request=host.split('void MiniAppHostController::requestWalletBrowser(')[1].split('void MiniAppHostController::chooseWalletBrowser(')[0];
  assert.match(request,/if \(m_nativeSdkAvailable\) \{[\s\S]{0,350}chooseWalletBrowser\(QStringLiteral\("system"\)\);[\s\S]{0,150}return;/);
  assert.ok(request.indexOf('chooseWalletBrowser(QStringLiteral("system"))')<request.indexOf('installedWalletBrowsers()'));
  const runtime=read('ui/app/miniapp/runtime.cpp');
  assert.match(runtime,/session->walletApprovalDismissed = true;[\s\S]{0,400}session->pendingSignInId, \{\}, \{\}, false, true/);
  assert.match(runtime,/if \(session.walletApprovalDismissed\) \{[\s\S]{0,300}rejected_by_user/);
  assert.match(runtime,/rpcError\(rpcIdValue, 4001, QStringLiteral\("Wallet approval dismissed/);
  assert.doesNotMatch(runtime,/walletApprovalDismissed = false/);
  assert.match(runtime,/#if !defined\(Q_OS_ANDROID\) && !defined\(Q_OS_IOS\)\s+if \(invalidProof\) m_controller.reportMiniAppSignInVerificationFailed/);
});
test('Android toolbar uses shared Genyleap SVG assets, not hand-drawn glyphs',()=>{
  const cpp=read('ui/app/miniapp/browser_android.cpp');
  const java=read('properties/Android/src/com/genyleap/genycaster/MiniAppBrowser.java');
  const theme=read('ui/theme/Theme.qml');
  for(const [name,path] of [['back','navigation/arrow-left'],['refresh','actions/repost'],['share','actions/share'],['minimize','navigation/chevron-down'],['close','system/close']]){
    assert.ok(cpp.includes('toolbarIcon(QStringLiteral("'+path+'.svg"))'));
    assert.match(read('ui/assets/icons/genyleap/icons/'+path+'.svg'),/<svg/);
    assert.ok(theme.includes('"'+path+'"'));
  }
  assert.match(cpp,/QSvgRenderer renderer/);
  assert.match(java,/BitmapFactory.decodeByteArray/);
  assert.match(java,/setPadding\(dp\(13\),dp\(13\),dp\(13\),dp\(13\)\)/);
  assert.doesNotMatch(java,/canvas.draw|new Path\(/);
  assert.match(java,/Intent.CATEGORY_BROWSABLE/);
  assert.match(java,/ActivityNotFoundException \| SecurityException/);
});
