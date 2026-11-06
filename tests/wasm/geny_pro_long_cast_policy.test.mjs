import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";

const raw=readFileSync(new URL("../../ui/components/ProCastPolicy.js",import.meta.url),"utf8")
  .replace(/^\.pragma library\s*/,"");
const context=vm.createContext({});
vm.runInContext(raw,context);
const {publicationPlan,characterCount,utf8Length}=context;
const normalized=value=>JSON.parse(JSON.stringify(value));

test("Basic 1024 and Pro 4096 count Unicode scalars, not UTF-8 bytes",()=>{
 assert.equal(characterCount("سلام"),4);
 assert.equal(utf8Length("سلام"),8);
 assert.equal(characterCount("😀"),1);
 assert.equal(utf8Length("😀"),4);
 assert.equal(characterCount("👩‍👩‍👧‍👧"),1);
 assert.equal(characterCount("a\u0301"),1);
 assert.equal(characterCount("🇮🇷"),1);
 assert.equal(publicationPlan("x".repeat(1024),[],[],[],[],false).parts.length,1);
 assert.equal(publicationPlan("x".repeat(1025),[],[],[],[],false).error,"character_limit");
 assert.equal(publicationPlan("x".repeat(4096),[],[],[],[],true).parts.length,4);
 assert.equal(publicationPlan("x".repeat(4097),[],[],[],[],true).error,"character_limit");
});
test("Persian and astral emoji are not lost or split inside a thread",()=>{
 for(const input of ["سلام جهان ".repeat(380),"😀".repeat(4096),"a ".repeat(2048)]){
  const plan=publicationPlan(input,[],[],[],[],true);
  assert.equal(plan.error,undefined);
  assert.equal(plan.requiresThread,true);
  assert.equal(plan.parts.map(x=>x.text).join(""),input);
  assert.ok(plan.parts.every(x=>utf8Length(x.text)<=1024));
 }
});
test("mentions stay attached to correct segment at byte-relative positions",()=>{
 const content="a".repeat(1100)+" @user";
 const result=publicationPlan(content,["42"],[1101],[],[],true);
 assert.equal(result.parts.length,2);
 const carrying=result.parts.filter(x=>x.mentionFids.length);
 assert.equal(carrying.length,1);
 assert.deepEqual(normalized(carrying[0].mentionFids),["42"]);
 assert.equal(carrying[0].text.slice(carrying[0].mentionPositions[0]),"@user");
});
test("Pro can attach four items in confirmed two-embed thread, Basic only one",()=>{
 const embeds=["a","b","c","d"].map(x=>({kind:"url",value:"https://cdn.test/"+x}));
 const pro=publicationPlan("Album",[],[],embeds,[],true);
 assert.equal(pro.parts.length,2);
 assert.equal(pro.parts[0].embeds.length,2);
 assert.equal(pro.parts[1].embeds.length,2);
 assert.deepEqual(normalized(pro.parts.flatMap(x=>x.embeds)),embeds);
 assert.equal(publicationPlan("Album",[],[],embeds,[],false).error,"media_limit");
});
test("Additional user-authored thread casts are preserved after planned body",()=>{
 const plan=publicationPlan("First",[],[],[],["Second","Third"],true);
 assert.deepEqual(normalized(plan.parts.map(x=>x.text)),["First","Second","Third"]);
 assert.equal(publicationPlan("",[],[],[],[],true).error,"thread_limit");
});

test("composer visibly confirms every multi-cast plan before sending",()=>{
 const composer=readFileSync(new URL("../../ui/components/ComposerDrawer.qml",import.meta.url),"utf8");
 const controller=readFileSync(new URL("../../src/presentation/qt/rich_composer_controller.cpp",import.meta.url),"utf8");
 const bridge=readFileSync(new URL("../../src/presentation/qt/presentation_bridge.cpp",import.meta.url),"utf8");
 assert.match(composer,/ProCastPolicy\.publicationPlan/);
 assert.match(composer,/objectName: "genyProExplicitThreadConfirmation"/);
 assert.match(composer,/planned\.requiresThread[\s\S]*?threadPublicationConfirmation\.open\(\)/);
 assert.match(composer,/root\.threadRequested\(casts\)/);
 assert.match(composer,/root\.withinAccountCharacterLimit/);
 assert.match(composer,/maxMediaUploads: verifiedGenyPro/);
 assert.match(controller,/result\.reserve\(4\)/);
 assert.match(controller,/if \(result\.size\(\) >= 4\) break/);
 assert.match(bridge,/casts\.size\(\) > 24/);
 assert.match(bridge,/text\.toUtf8\(\)\.size\(\) > 1024/);
});
test("mini app quotas are enforced in native runtime and web/Android host",()=>{
 const host=readFileSync(new URL("../../ui/app/miniapp_host_controller.cpp",import.meta.url),"utf8");
 const runtime=readFileSync(new URL("../../ui/app/miniapp_runtime.cpp",import.meta.url),"utf8");
 const android=readFileSync(new URL("../../ui/components/AndroidMiniAppWebView.qml",import.meta.url),"utf8");
 const main=readFileSync(new URL("../../ui/app/main.cpp",import.meta.url),"utf8");
 assert.match(host,/setSessionLimit\(const int limit\)/);
 assert.match(runtime,/m_allowedSessions\{1U\}/);
 assert.match(runtime,/m_sessions\.size\(\) >= m_allowedSessions/);
 assert.match(runtime,/while \(m_sessions\.size\(\) > m_allowedSessions\)/);
 assert.match(android,/miniAppSessions\.count >= miniAppHost\.sessionLimit/);
 assert.match(android,/onSessionLimitChanged/);
 assert.match(main,/miniAppRuntimeOwner->setSessionLimit\(verified \? 4 : 1\)/);
 assert.match(main,/miniAppHost\.setSessionLimit\(verified \? 4 : 1\)/);
});
