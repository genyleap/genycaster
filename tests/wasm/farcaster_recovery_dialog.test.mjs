import test from 'node:test';
import assert from 'node:assert/strict';
import { promptForFarcasterRecovery } from '../../ui/wasm/recovery-phrase-dialog.mjs';

class FakeElement {
    constructor(tag,document) {
        this.tagName=tag;this.document=document;this.listeners=new Map();
        this.children=[];this.style={overflow:''};this.attributes={};this.dataset={};
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
test('secure browser presents 24 masked numbered fields without persisting secrets',async()=>{
    const b=browser(),result=promptForFarcasterRecovery(b);
    const overlay=b.document.getElementById('genycaster-farcaster-recovery');
    assert.ok(overlay);
    const sheet=overlay.children[0];
    assert.equal(sheet.dir,'rtl');
    const grid=b.document.getElementById('genycaster-recovery-words');
    assert.equal(grid.children.length,24);
    const words=grid.children.map(cell=>cell.children[1]);
    assert.equal(words[0].attributes['aria-label'].endsWith('1'),true);
    assert.equal(words[0].autocomplete,'off');
    // iOS Safari auto-zooms focused form controls below 16 CSS pixels.
    assert.match(words[0].style.cssText,/font:16px system-ui/);
    assert.match(words[0].style.cssText,/touch-action:manipulation/);
    assert.equal(words[0].type,'password');
    const reveal=b.document.getElementById('genycaster-recovery-reveal');
    assert.ok(reveal);
    reveal.checked=true;reveal.dispatch('change');
    assert.equal(words[0].type,'text');
    reveal.checked=false;reveal.dispatch('change');
    assert.equal(words[0].type,'password');
    const vector=[...Array(23).fill('abandon'),'art'];
    for(let i=0;i<24;i++)words[i].value=vector[i];
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();
    assert.equal(await result,vector.join(' '));
    assert.equal(words.every(word=>word.value===''),true);
    assert.equal(b.document.getElementById('genycaster-farcaster-recovery'),undefined);
    assert.equal(b.document.body.style.overflow,'');
});
test('post-revoke account recovery uses a real full-app form, not the Setup half-pane',async()=>{
    const b=browser();
    const result=promptForFarcasterRecovery(b,null,null,{presentation:'full'});
    const root=b.document.getElementById('genycaster-farcaster-recovery');
    const card=root.children[0];
    assert.equal(root.dataset?.presentation,'full');
    assert.equal(card.attributes['role'],'dialog');
    assert.equal(card.attributes['aria-modal'],'true');
    assert.match(root.children[1].textContent,/left:/);
    root.children[0].children.at(-1).children[0].click();
    await assert.rejects(result,/cancelled/);
});

test('paste a complete 24-word phrase in numbered order',async()=>{
    const b=browser(),result=promptForFarcasterRecovery(b);
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
  // Fixed 24-word mode has no 12/24 switcher.
    const grid=b.document.getElementById('genycaster-recovery-words');
    assert.equal(grid.children.length,24);
    const words=grid.children.map(cell=>cell.children[1]);
    const vector=[...Array(23).fill('abandon'),'art'];
    words[0].dispatch('paste',{clipboardData:{getData:()=>vector.join(' ')}});
    const pasted=grid.children.map(cell=>cell.children[1]);
    assert.deepEqual(pasted.map(word=>word.value),vector);
    const submit=sheet.children.at(-1).children.at(-1);
    assert.equal(b.document.activeElement,submit,
        'full-phrase paste should focus submit instead of a narrow input');
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
    const vector=[...Array(23).fill('abandon'),'art'];
    vector[4]='notaword';
    for(let i=0;i<24;i++)words[i].value=vector[i];
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();
    await settle();
    assert.equal(words[4].attributes['aria-invalid'],'true');
    assert.equal(words[3].attributes['aria-invalid'],'false');
    assert.equal(words[4].value,'notaword');
    assert.ok(b.document.getElementById('genycaster-farcaster-recovery'));
    words[4].value='abandon';
    submit.click();
    assert.equal(await result,[...Array(23).fill('abandon'),'art'].join(' '));
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
    const error=b.document.getElementById('genycaster-recovery-error');
    assert.match(error.textContent,/جمع کنترل|checksum/);
    assert.equal(words.every(word=>word.value==='abandon'),true);
    assert.equal(words.every(word=>word.attributes['aria-invalid']==='false'),true);
    words[23].value='art';
    submit.click();
    assert.equal((await result).split(' ').length,24);
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
    [...Array(23).fill('abandon'),'art'].forEach((word,i)=>words[i].value=word);
    const submit=sheet.children.at(-1).children.at(-1);
    submit.click();await settle();
    assert.equal(attempted,1);
    assert.equal(words[0].value,'abandon');
    assert.ok(b.document.getElementById('genycaster-farcaster-recovery'));
    const error=b.document.getElementById('genycaster-recovery-error');
    assert.ok(!error.textContent.includes('503'));
    submit.click();
    assert.equal(await result,'42');
    assert.equal(attempted,2);
});

test('HyperSnap outage is visible next to retry and keeps the recovery form usable',async()=>{
    const b=browser();
    let attempt=0;
    const result=promptForFarcasterRecovery(b,null,async()=>{
        attempt++;
        if(attempt===1)throw Object.assign(new Error('ECONNRESET'),{code:'recovery-network'});
        return '4294967296';
    },{protocol:'hypersnap-v2'});
    const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
    const grid=b.document.getElementById('genycaster-recovery-words');
    const words=grid.children.map(cell=>cell.children[1]);
    const vector=[...Array(23).fill('abandon'),'art'];
    vector.forEach((word,i)=>words[i].value=word);
    const submit=sheet.children.at(-1).children.at(-1);
    const error=b.document.getElementById('genycaster-recovery-error');
    assert.equal(error.hidden,true);
    submit.click();
    await settle();
    assert.equal(error.hidden,false);
    assert.equal(error.attributes.role,'alert');
    assert.equal(error.parentNode,sheet,'keep the error outside the scrolling words');
    assert.match(error.textContent,/ارتباط با هایپراسنپ|در دسترس/);
    assert.doesNotMatch(error.textContent,/ECONNRESET|نامعتبر/);
    assert.equal(submit.textContent,'تلاش دوباره');
    assert.deepEqual(words.map(word=>word.value),vector);
    submit.click();
    assert.equal(await result,'4294967296');
    assert.equal(words.every(word=>word.value===''),true);
});

test('recovery drawer distinguishes custody ownership and signer failures without exposing secrets',async()=>{
    for(const [code,expected] of [
        ['recovery-unregistered',/مالکیت حساب|Farcaster/],
        ['recovery-ownership',/مالکیت حساب|Farcaster/],
        ['recovery-signer',/Snapchain|مجوز انتشار/],
        ['recovery-network',/بازیابی فارکستر فعلاً در دسترس نیست/],
        ['recovery-derivation',/کیف پول بازیابی/],
        ['recovery-verification',/تأیید مالکیت/],
    ]){
        const b=browser();
        let attempt=0;
        const result=promptForFarcasterRecovery(b,null,async()=>{
            attempt++;
            if(attempt===1){
                const error=new Error('Sensitive internal RPC details must not be displayed');
                error.code=code;
                throw error;
            }
            return '42';
        });
        const sheet=b.document.getElementById('genycaster-farcaster-recovery').children[0];
        const grid=b.document.getElementById('genycaster-recovery-words');
        const words=grid.children.map(cell=>cell.children[1]);
        [...Array(23).fill('abandon'),'art'].forEach((word,i)=>words[i].value=word);
        const submit=sheet.children.at(-1).children.at(-1);
        submit.click();
        await settle();
        const error=b.document.getElementById('genycaster-recovery-error');
        assert.match(error.textContent,expected,code);
        assert.doesNotMatch(error.textContent,/Sensitive internal RPC details/);
        assert.equal(words[0].value,'abandon','keep words for correction and retry');
        if(code==='recovery-network')assert.equal(submit.textContent,'تلاش دوباره');
        submit.click();
        assert.equal(await result,'42');
        assert.equal(words.every(word=>word.value===''),true,'wipe fields after success');
    }
});

test('clipboard paste keeps 24 existing browser fields stable and handles numbered exports',async()=>{
    const b=browser(),pending=promptForFarcasterRecovery(b);
    const grid=b.document.getElementById('genycaster-recovery-words');
    const original=grid.children.map(cell=>cell.children[1]);
    const vector=[...Array(23).fill('abandon'),'art'];
    const numbered=vector.map((word,i)=>String(i+1).padStart(2,'0')+'. '+word).join('\n');
    original[3].focus();
    original[3].dispatch('paste',{clipboardData:{getData:()=>numbered}});
    assert.deepEqual(grid.children.map(cell=>cell.children[1].value),vector);
    assert.equal(grid.children[0].children[1],original[0],
        'full paste must not reconstruct and drop the original input');
    const submit=b.document.getElementById('genycaster-farcaster-recovery').children[0]
        .children.at(-1).children.at(-1);
    assert.equal(b.document.activeElement,submit);
    submit.click();
    assert.equal(await pending,vector.join(' '));
});

test('partial browser paste into 01 does not steal focus and jump to 04',async()=>{
    const b=browser(),pending=promptForFarcasterRecovery(b);
    const grid=b.document.getElementById('genycaster-recovery-words');
    const inputs=grid.children.map(cell=>cell.children[1]);
    assert.equal(b.document.activeElement,inputs[0]);
    inputs[0].dispatch('paste',{clipboardData:{getData:()=> 'abandon, ability; able'}});
    assert.deepEqual(inputs.slice(0,3).map(x=>x.value),['abandon','ability','able']);
    assert.equal(inputs[3].value,'');
    assert.equal(b.document.activeElement,inputs[0]);
    b.document.getElementById('genycaster-farcaster-recovery').children[0]
      .children.at(-1).children[0].click();
    await assert.rejects(pending,/cancelled/);
});
