import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";

const code=readFileSync(new URL("../../ui/wasm/genycaster-loader.js",import.meta.url),"utf8");
const start=code.indexOf("let visibleBootMs=0");
const end=code.indexOf("async function start() {",start);
assert.ok(start>0 && end>start);
const watchdog=code.slice(start,end);

function simulate({phase="download"}={}) {
  let now=0, callback=null, failure=null;
  const scope={
    window:{setInterval:fn=>(callback=fn,1)},
    performance:{now:()=>now},
    document:{visibilityState:"visible"},
    finished:false,failed:false,
    bootPhase:phase,
    fail:error=>{failure=error;},
    Error
  };
  vm.runInNewContext(watchdog,scope);
  return {
    tick(milliseconds) {
      // The production watchdog checks every 10s and caps foreground gaps.
      for(let i=0;i<milliseconds/10000;i++){now+=10000;callback();}
    },
    get error(){return failure},
    setPhase(value){scope.bootPhase=value;},
    finish(){scope.finished=true;},
    hide(){scope.document.visibilityState="hidden";}
  };
}

test("cold ~80 MB WASM download does not fail at previous 240 second cutoff",()=>{
  const s=simulate();
  s.tick(240000);
  assert.equal(s.error,null);
  s.tick(300000);
  assert.equal(s.error,null,"9 minutes can be normal on slow data plans");
});

test("no infinite hang: pending download fails with a useful message after 15 minutes",()=>{
  const s=simulate();
  s.tick(890000);
  assert.equal(s.error,null);
  s.tick(10000);
  assert.match(s.error?.message||"",/15 minutes/);
});

test("hidden tab is not counted as an active download",()=>{
  const s=simulate();
  s.tick(60000);
  s.hide();
  s.tick(900000);
  assert.equal(s.error,null);
});

test("Qt runtime and QML acknowledgements advance missing Resource Timing state",()=>{
  const runtime=code.slice(code.indexOf('onLoaded:()=>{'),code.indexOf('onExit:exit=>{'));
  const qml=code.slice(code.indexOf('window.__genycasterQmlStartup=(success,detail)=>{'),
      code.indexOf('// Fail visibly instead',code.indexOf('window.__genycasterQmlStartup')));
  assert.match(runtime,/qtRuntimeLoaded=true/);
  assert.match(runtime,/if\(bootPhase==="download"\)/);
  assert.match(runtime,/showPhase\("interface"/);
  assert.match(qml,/qmlRootReady=true/);
  assert.match(qml,/if\(bootPhase==="download"\)/);
});
