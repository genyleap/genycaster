import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../../'+p,import.meta.url),'utf8');
test('temporary network buffer contention is not a per-image body limit violation',()=>{
 const cpp=read('src/transport/qt_network/async_http.cpp');
 assert.match(cpp,/responseBudgetContended = !perRequestExceeded && processExceeded/);
 assert.match(cpp,/active\.body\.size\(\) <= m_limits\.maximumBufferedResponseBytes - chunkBytes/);
 assert.match(cpp,/active\.responseBudgetContended \? ErrorCode::RateLimited : ErrorCode::Unavailable/);
});
test('asynchronous image admission pressure is retried before terminal URL failures',()=>{
 const cpp=read('ui/app/media/remote_image.cpp');
 const callback=cpp.slice(cpp.indexOf('auto operation = m_transport->start('),cpp.indexOf('if (!operation)',cpp.indexOf('auto operation = m_transport->start(')));
 assert.match(callback,/!result && \(result\.error\(\)\.code == farcaster::foundation::error::ErrorCode::RateLimited[\s\S]*?self->retryNetworkAdmission[\s\S]*?return;/);
 assert.ok(callback.indexOf('self->retryNetworkAdmission')<callback.indexOf('rememberFailure(urlText)'));
 assert.match(callback,/if \(result\) rememberFailure\(urlText\)/);
 const retry=cpp.slice(cpp.indexOf('void RemoteImageProvider::Impl::retryNetworkAdmission('),cpp.indexOf('void RemoteImageProvider::Impl::decodeNetworkResponse('));
 assert.match(retry,/maximumAdmissionRetries = 32U/);
 assert.match(retry,/admissionRetryAttempt >= maximumAdmissionRetries/);
 assert.match(retry,/control->cancelled\.load[\s\S]*?self->startNetwork/);
 assert.doesNotMatch(retry,/rememberFailure|HttpResponse|responseHolder/);
});
test('changing the load stagger does not restart active media on any backend',()=>{
 const qml=read('ui/components/visual/SuperellipseImage.qml');
 const handler=qml.slice(qml.indexOf('    onLoadDelayMsChanged:'),qml.indexOf('    // The provider owns bounded network retries'));
 assert.match(handler,/activeSource\.toString\(\)\.length > 0/);
 assert.doesNotMatch(handler,/activeSource =/);
 assert.match(qml,/rasterEligible: !root\.remoteRaster \|\| rasterScope\.eligible/);
 assert.match(qml,/rasterRetained: !root\.remoteRaster \|\| rasterScope\.retained/);
 assert.match(qml,/root\.remoteRaster && \(root\.scrollMoving \|\| !root\.rasterRetained\)/);
});
