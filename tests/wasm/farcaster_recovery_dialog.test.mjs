import test from 'node:test';
import assert from 'node:assert/strict';
import { promptForFarcasterRecovery } from '../../ui/wasm/recovery-phrase-dialog.mjs';

class FakeElement {
    constructor(tag,document) {
        this.tagName=tag;this.document=document;this.listeners=new Map();
        this.children=[];this.style={overflow:''};this.attributes={};
        this.value='';this.textContent='';this.placeholder='';
    }
    setAttribute(name,value){this.attributes[name]=value;}
    addEventListener(name,fn){const current=this.listeners.get(name)||[];current.push(fn);this.listeners.set(name,current);}
    removeEventListener(name,fn){this.listeners.set(name,(this.listeners.get(name)||[]).filter(x=>x!==fn));}
    appendChild(element){element.parentNode=this;this.children.push(element);return element;}
    remove(){if(this.parentNode)this.parentNode.children=this.parentNode.children.filter(x=>x!==this);}
    focus(){this.document.activeElement=this;}
    click(){for(const fn of this.listeners.get('click')||[])fn({target:this});}
    get id(){return this._id;}
    set id(v){this._id=v;}
}
class FakeDocument {
    constructor(){
        this.body=new FakeElement('body',this);
        this.activeElement=new FakeElement('previous',this);
    }
    createElement(tag){return new FakeElement(tag,this);}
    getElementById(id){const find=e=>(e.id===id?e:
        e.children.map(find).find(Boolean));return find(this.body);}
}
function browser(secure=true) {
    return {document:new FakeDocument(),isSecureContext:secure,
        navigator:{language:'fa-IR'}};
}
test('secure browser presents localized dialog and never persists the phrase',async()=>{
    const b=browser(),result=promptForFarcasterRecovery(b);
    const overlay=b.document.getElementById('genycaster-farcaster-recovery');
    assert.ok(overlay);
    const sheet=overlay.children[0];
    assert.equal(sheet.dir,'rtl');
    const area=b.document.getElementById('genycaster-recovery-words');
    assert.ok(area);
    assert.equal(area.autocomplete,'off');
    area.value='test '.repeat(11)+'junk';
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();
    assert.equal(await result,'test '.repeat(11)+'junk');
    assert.equal(area.value,'');
    assert.equal(b.document.getElementById('genycaster-farcaster-recovery'),undefined);
    assert.equal(b.document.body.style.overflow,'');
});
test('cancel wipes input and rejects without returning a phrase',async()=>{
    const b=browser(),result=promptForFarcasterRecovery(b);
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
    const area=b.document.getElementById('genycaster-recovery-words');
    area.value='never save my phrase';
    const cancel=sheet.children.at(-1).children[0];
    cancel.click();
    await assert.rejects(result,/cancelled/);
    assert.equal(area.value,'');
});
test('sign-out AbortSignal closes and wipes recovery dialog',async()=>{
    const b=browser(),controller=new AbortController();
    const result=promptForFarcasterRecovery(b,controller.signal);
    const area=b.document.getElementById('genycaster-recovery-words');
    area.value='not stored';
    controller.abort();
    await assert.rejects(result,/cancelled/);
    assert.equal(area.value,'');
    assert.equal(b.document.getElementById('genycaster-farcaster-recovery'),undefined);
});
test('insecure origin refuses recovery phrase input entirely',async()=>{
    const b=browser(false);
    await assert.rejects(promptForFarcasterRecovery(b),/secure HTTPS/);
    assert.equal(b.document.body.children.length,0);
});
