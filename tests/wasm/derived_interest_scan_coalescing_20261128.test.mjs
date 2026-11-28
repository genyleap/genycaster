import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
for (const [stem,klass,enabled] of [
    ['ui/app/social/cast_views','CastViewController','m_networkEnabled'],
    ['ui/app/browser/reactions','BrowserReactionController','m_enabled']
]) {
    const read=ext=>readFileSync(new URL('../../'+stem+ext,import.meta.url),'utf8');
    const cpp=read('.cpp'),hpp=read('.hpp');
    const method=name=>cpp.slice(cpp.indexOf('void '+klass+'::'+name+'('),cpp.indexOf('\n}',cpp.indexOf('void '+klass+'::'+name+'('))+2);
    test(klass+' coalesces interest scans while retaining immediate cancellation',()=>{
        for(const name of ['retain','release','ensure']){
            assert.match(method(name),/scheduleInterestScan\(\)/);
            assert.doesNotMatch(method(name),/queueInterested\(\)/);
        }
        assert.match(method('release'),/m_pending\.remove(?:All)?\(/);
        assert.match(method('scheduleInterestScan'),new RegExp('if \\('+enabled+' && !m_interestScanTimer\\.isActive\\(\\)\\)'));
        assert.match(method('queueInterested'),/m_interestScanTimer\.stop\(\)/);
        assert.match(hpp,/QTimer m_interestScanTimer\{this\}/);
        assert.match(cpp,/m_interestScanTimer.setSingleShot\(true\)/);
        assert.match(cpp,/m_interestScanTimer.setInterval\(0\)/);
    });
}
