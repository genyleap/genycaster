import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';

const qml = readFileSync('ui/components/apps/AndroidWebView.qml', 'utf8');

function method(name, args) {
  const start = qml.indexOf('    function ' + name + '(');
  const end = qml.indexOf('\n    }', start);
  assert.ok(start >= 0 && end > start, name + ' must be a QML function');
  const source = qml.slice(start, end + 6).trim();
  assert.ok(source.startsWith('function ' + name + '('));
  return source;
}

function fixture(launch='https://mini.example/path') {
  const sessions = [{url:launch, title:'Mini App', blocked:false}];
  const miniAppSessions = {
    get count(){return sessions.length;},
    get(index){return sessions[index];},
    setProperty(index,key,value){sessions[index][key]=value;},
  };
  const calls=[];
  const root={activeSessionIndex:0,loadError:'',publicOrigin:null};
  const miniAppHost={setState:(active,error)=>calls.push([active,error])};
  const context=vm.createContext({
    root,miniAppSessions,miniAppHost,qsTrId:()=> 'Mini App unavailable',
  });
  root.publicOrigin=vm.runInContext('('+method('publicOrigin')+')',context);
  const block=vm.runInContext('('+method('blockForeignNavigation')+')',context);
  return {block,root,sessions,calls};
}

test('HTTPS launch origin is canonicalized without accepting credentials, ports or scheme tricks',()=>{
  const {root}=fixture();
  for(const origin of [
    'https://mini.example','HTTPS://MINI.EXAMPLE:443/path',
    'https://mini.example/a?b=c','https://mini.example/#section',
  ]) assert.equal(root.publicOrigin(origin),'https://mini.example',origin);
  for(const invalid of [
    'http://mini.example','javascript:alert(1)','data:text/html,evil',
    'about:blank','https://mini.example:444/a',
    'https://mini.example@evil.example/',
    'https://mini.example\\@evil.example/',
    'https://mini.example..evil.example',
    'https://mini.example.local',
    'https://mini.example.invalid',
    'https://localhost','https://127.0.0.1',
    'https://mini.example.evil.example:8443',
    'https://mini.example:443@evil.example/',
    'https://mini.example#hi@evil.example',
  ]) {
    // A fragment on the approved origin is in fact safe, even if its text
    // resembles an email address.
    if(invalid==='https://mini.example#hi@evil.example') {
      assert.equal(root.publicOrigin(invalid),'https://mini.example');
    } else {
      assert.equal(root.publicOrigin(invalid),'',invalid);
    }
  }
});

test('same-origin SPA and launch paths remain usable; cross-origin redirects fail closed',()=>{
  const f=fixture();
  for(const page of [
    'https://mini.example/dashboard',
    'https://MINI.EXAMPLE:443/next?val=ok',
    'https://mini.example/#/settings',
  ]) assert.equal(f.block(0,page),false,page);
  assert.equal(f.sessions[0].blocked,false);
  assert.equal(f.calls.length,0);
  assert.equal(f.block(0,'https://evil.example/login'),true);
  assert.equal(f.sessions[0].blocked,true);
  assert.equal(f.root.loadError,'Mini App unavailable');
  assert.deepEqual(f.calls,[[true,'Mini App unavailable']]);
  assert.equal(f.block(0,'https://mini.example/dashboard'),true,
    'a blocked document must never auto-reactivate on callback/reload');
  assert.equal(f.calls.length,1,'no repeated host state churn from redirect loops');
});

test('URL changes cannot bypass the document gate via subdomain, redirects or invalid session index',()=>{
  for(const candidate of [
    'http://mini.example/dashboard','https://mini.example.evil.example/',
    'https://login.mini.example/','https://mini.example:8443/',
    'about:blank','javascript:alert(1)','https://evil.example@mini.example/',
  ]){
    const f=fixture();
    assert.equal(f.block(0,candidate),true,candidate);
    assert.equal(f.sessions[0].blocked,true);
  }
  assert.equal(fixture().block(2,'https://mini.example/'),true);
});

test('Android public WebView clears redirected documents and never leaks wallet/SDK privilege',()=>{
  assert.match(qml,/onUrlChanged: function\(nextUrl\)/);
  assert.match(qml,/if \(root\.blockForeignNavigation\(index, nextUrl\)\) stop\(\)/);
  assert.match(qml,/url: model\.blocked \? "" : String\(model\.url\)/);
  assert.match(qml,/visible: root\.opened && index === root\.activeSessionIndex\s*&& !Boolean\(model\.blocked\)/);
  assert.match(qml,/if \(index !== root\.activeSessionIndex \|\| Boolean\(model\.blocked\)\)/);
  assert.match(qml,/miniAppSessions\.append\(\{url:String\(url\), title:name, blocked:false\}\)/);
  assert.match(qml,/if \(Boolean\(miniAppSessions\.get\(i\)\.blocked\)\)/);
  assert.doesNotMatch(qml,/runJavaScript\(|addJavascriptInterface|nativeWallet|signOpenProofWalletChallenge/);
});
