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
    get firstChild(){return this.children[0];}
    removeChild(child){this.children=this.children.filter(x=>x!==child);child.parentNode=null;return child;}
    replaceChildren(...children){this.children=[];for(const child of children)this.appendChild(child);}
    dispatch(name,options={}) {
        const event={preventDefault(){},target:this,...options};
        for(const fn of this.listeners.get(name)||[])fn(event);
    }
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
test('secure browser presents numbered 12-word fields without persisting secrets',async()=>{
    const b=browser(),result=promptForFarcasterRecovery(b);
    const overlay=b.document.getElementById('genycaster-farcaster-recovery');
    assert.ok(overlay);
    const sheet=overlay.children[0];
    assert.equal(sheet.dir,'rtl');
    const grid=b.document.getElementById('genycaster-recovery-words');
    assert.equal(grid.children.length,12);
    const words=grid.children.map(cell=>cell.children[1]);
    assert.equal(words[0].attributes['aria-label'].endsWith('1'),true);
    assert.equal(words[0].autocomplete,'off');
    const vector=('test '.repeat(11)+'junk').trim().split(' ');
    for(let i=0;i<12;i++)words[i].value=vector[i];
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();
    assert.equal(await result,vector.join(' '));
    assert.equal(words.every(word=>word.value===''),true);
    assert.equal(b.document.getElementById('genycaster-farcaster-recovery'),undefined);
    assert.equal(b.document.body.style.overflow,'');
});
test('toggle 24 words and paste recovery words in numbered order',async()=>{
    const b=browser(),result=promptForFarcasterRecovery(b);
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
    const chooser=sheet.children.find(item=>item.attributes?.role==='group');
    chooser.children[1].click();
    const grid=b.document.getElementById('genycaster-recovery-words');
    assert.equal(grid.children.length,24);
    const words=grid.children.map(cell=>cell.children[1]);
    const vector=[...Array(23).fill('abandon'),'art'];
    words[0].dispatch('paste',{clipboardData:{getData:()=>vector.join(' ')}});
    const pasted=grid.children.map(cell=>cell.children[1]);
    assert.deepEqual(pasted.map(word=>word.value),vector);
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();
    assert.equal((await result).split(' ').length,24);
    assert.equal(pasted.every(word=>word.value===''),true);
});
test('cancel clears every numbered input',async()=>{
    const b=browser(),result=promptForFarcasterRecovery(b);
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
    const grid=b.document.getElementById('genycaster-recovery-words');
    const words=grid.children.map(cell=>cell.children[1]);
    words[0].value='never';words[1].value='persist';
    const cancel=sheet.children.at(-1).children[0];
    cancel.click();
    await assert.rejects(result,/cancelled/);
    assert.equal(words.every(word=>word.value===''),true);
});
test('sign-out AbortSignal closes and wipes recovery fields',async()=>{
    const b=browser(),controller=new AbortController();
    const result=promptForFarcasterRecovery(b,controller.signal);
    const grid=b.document.getElementById('genycaster-recovery-words');
    const words=grid.children.map(cell=>cell.children[1]);
    words[0].value='secret';
    controller.abort();
    await assert.rejects(result,/cancelled/);
    assert.equal(words.every(word=>word.value===''),true);
    assert.equal(b.document.getElementById('genycaster-farcaster-recovery'),undefined);
});
test('insecure origin refuses recovery phrase input entirely',async()=>{
    const b=browser(false);
    await assert.rejects(promptForFarcasterRecovery(b),/secure HTTPS/);
    assert.equal(b.document.body.children.length,0);
});

const settle = () => new Promise(resolve=>setTimeout(resolve,55));
test('unknown BIP39 word is highlighted and can be fixed without closing dialog',async()=>{
    const b=browser();
    const result=promptForFarcasterRecovery(b);
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
    const grid=b.document.getElementById('genycaster-recovery-words');
    const words=grid.children.map(cell=>cell.children[1]);
    const vector=[...Array(11).fill('abandon'),'about'];
    vector[4]='notaword';
    for(let i=0;i<12;i++)words[i].value=vector[i];
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();
    await settle();
    assert.equal(words[4].attributes['aria-invalid'],'true');
    assert.equal(words[3].attributes['aria-invalid'],'false');
    assert.equal(words[4].value,'notaword');
    assert.ok(b.document.getElementById('genycaster-farcaster-recovery'));
    words[4].value='abandon';
    submit.click();
    assert.equal(await result,[...Array(11).fill('abandon'),'about'].join(' '));
});
test('invalid checksum explains uncertainty, preserves fields, and allows retry',async()=>{
    const b=browser();
    const result=promptForFarcasterRecovery(b);
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
    const grid=b.document.getElementById('genycaster-recovery-words');
    const words=grid.children.map(cell=>cell.children[1]);
    words.forEach(word=>word.value='abandon');
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();await settle();
    const error=sheet.children.find(item=>item.attributes?.role==='alert');
    assert.match(error.textContent,/جمع کنترل|checksum/);
    assert.equal(words.every(word=>word.value==='abandon'),true);
    assert.equal(words.every(word=>word.attributes['aria-invalid']==='false'),true);
    words[11].value='about';
    submit.click();
    assert.equal((await result).split(' ').length,12);
});
test('network verification error never dismisses words and retry can succeed',async()=>{
    const b=browser();
    let attempted=0;
    const result=promptForFarcasterRecovery(b,null,async()=> {
        attempted++;
        if(attempted===1)throw new Error('Snapchain HTTP 503: temporary failure');
        return '42';
    });
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
    const grid=b.document.getElementById('genycaster-recovery-words');
    const words=grid.children.map(cell=>cell.children[1]);
    [...Array(11).fill('abandon'),'about'].forEach((word,i)=>words[i].value=word);
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();await settle();
    assert.equal(attempted,1);
    assert.equal(words[0].value,'abandon');
    assert.ok(b.document.getElementById('genycaster-farcaster-recovery'));
    const error=sheet.children.find(item=>item.attributes?.role==='alert');
    assert.ok(!error.textContent.includes('503'));
    submit.click();
    assert.equal(await result,'42');
    assert.equal(attempted,2);
});
