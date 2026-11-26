import test from 'node:test';
import assert from 'node:assert/strict';
import {promptCreateBrowserWallet} from '../../ui/wasm/browser_account_create_dialog.mjs';

const PHRASE = Array.from({length:24},()=> 'abandon').join(' ');

function fakeBrowser() {
  const nodes=[];
  const make=tag=>{
    const node={
      tagName:tag.toUpperCase(), style:{cssText:''}, textContent:'', children:[],
      value:'', checked:false, disabled:false, hidden:false, parentElement:null,
      isConnected:true, dataset:{}, attributes:{},
      append(...children) {
        for(const child of children){child.parentElement=this;this.children.push(child);}
      },
      appendChild(child) {this.append(child);return child;},
      replaceChildren(...children) {
        this.children=[];this.textContent='';
        if(children.length)this.append(...children);
      },
      setAttribute(key,value){this.attributes[key]=value;},
      remove(){
        this.isConnected=false;
        if(this.parentElement) {
          const index=this.parentElement.children.indexOf(this);
          if(index>=0)this.parentElement.children.splice(index,1);
        }
      },
      focus(){},scrollTop:10
    };
    nodes.push(node);
    return node;
  };
  const body=make('body');
  body.style.overflow='';
  const document={
    body,documentElement:{dataset:{theme:'dark'}},activeElement:{focus(){}},
    createElement:make,
    getElementById:id=>nodes.find(x=>x.id===id&&x.isConnected)||null
  };
  const browser={
    document,isSecureContext:true,
    crypto:{getRandomValues(array){array.fill(0);return array;}},
    navigator:{language:'en',clipboard:{writeText:async()=>{}}}
  };
  const one=text=>nodes.find(n=>n.tagName==='BUTTON'&&n.textContent===text);
  return {browser,nodes,one};
}

async function start(onRegister,other={}) {
  const ui=fakeBrowser();
  const dialog=promptCreateBrowserWallet({
    browser:ui.browser,generatePhrase:async()=>PHRASE,
    protocol:'hypersnap-v2',onRegister,...other
  });
  await new Promise(resolve=>setImmediate(resolve));
  for(const node of ui.nodes) {
    if(node.tagName==='INPUT'&&node.type==='password')node.value='abandon';
    if(node.tagName==='INPUT'&&node.type==='checkbox')node.checked=true;
  }
  return {dialog,...ui};
}

test('backup words are masked, explicitly revealable and can be hidden again',async()=>{
  const ui=await start(async()=>{throw Error('registration should not run')});
  const words=ui.nodes.filter(n=>n.className==='wallet-word');
  assert.equal(words.length,24);
  assert.ok(words.every(n=>n.textContent==='••••••'));
  ui.one('Show words').onclick();
  assert.ok(words.every(n=>n.textContent==='abandon'));
  ui.one('Hide words').onclick();
  assert.ok(words.every(n=>n.textContent==='••••••'));
  ui.one('Cancel').onclick();
  await assert.rejects(ui.dialog,/cancelled/);
});
test('backup cards disappear in favor of interactive progress; cancellation before POST aborts without writing', async()=>{
  let abortObserved=false;
  const ui=await start((phrase,{signal,onProgress})=>{
    assert.equal(phrase,PHRASE);
    onProgress(5,25,'proof');
    return new Promise((resolve,reject)=>{
      signal.addEventListener('abort',()=>{
        abortObserved=true;
        reject(new Error('Registration cancelled before submission'));
      },{once:true});
    });
  });
  const create=ui.one('Create account'),cancel=ui.one('Cancel');
  assert.ok(create&&cancel);
  void create.onclick();
  await new Promise(resolve=>setImmediate(resolve));
  const grid=ui.nodes.find(x=>x.tagName==='DIV'&&x.children.length===24);
  const progress=ui.nodes.find(x=>x.tagName==='SECTION'&&x.style.display==='block');
  assert.ok(progress,'The real progress section must be visible');
  assert.equal(grid.style.display,'none');
  assert.equal(grid.children.every(x=>x.children.length===0),true);
  assert.equal(progress.style.display,'block');
  assert.equal(progress.hidden,false);
  assert.equal(cancel.disabled,false,'Cancel remains usable while only computing PoW');
  assert.ok(ui.nodes.some(x=>/Computing the registration proof locally/.test(x.textContent)));
  const selected=ui.nodes.find(x=>x.tagName==='LI' && x.dataset.state==='active');
  assert.ok(selected);
  assert.match(selected.textContent,/Request/);
  assert.ok(ui.nodes.some(x=>x.tagName==='STYLE' && /:focus-visible/.test(x.textContent)
    && /:active:not\(:disabled\)/.test(x.textContent) && /wallet-verification-label input/.test(x.textContent)));
  assert.equal(create.disabled,true);
  assert.equal(create.dataset.busy,'true');
  cancel.onclick();
  await assert.rejects(ui.dialog,/cancelled before submission/);
  assert.equal(abortObserved,true);
  assert.equal(ui.browser.document.body.style.overflow,'');
});

test('after irreversible POST begins, cancel cannot trigger a duplicate or falsely report cancellation',async()=>{
  let resolvePost;
  let activeSignal;
  const ui=await start((_phrase,{signal,onProgress})=>{
    activeSignal=signal;
    onProgress(0,1,'submitting');
    return new Promise(resolve=>{resolvePost=resolve;});
  });
  const cancel=ui.one('Cancel');
  void ui.one('Create account').onclick();
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(cancel.disabled,true);
  cancel.onclick();
  assert.equal(activeSignal.aborted,false);
  const resolved={state:'confirmed',fid:'4294967333'};
  resolvePost(resolved);
  assert.deepEqual(await ui.dialog,resolved);
});

test('a network outcome that may have been accepted stays pending, never silently resubmits',async()=>{
  const ui=await start((_phrase,{onProgress})=>{
    onProgress(0,1,'submitting');
    const error=new Error('submission unconfirmed');
    error.outcomeUnknown=true;
    return Promise.reject(error);
  });
  void ui.one('Create account').onclick();
  await new Promise(resolve=>setImmediate(resolve));
  const close=ui.one('Close');
  assert.equal(close.disabled,false);
  assert.equal(ui.nodes.filter(n=>String(n.textContent).includes('Do not retry')).length,1);
  close.onclick();
  await assert.rejects(ui.dialog,/submission unconfirmed/);
});

test('pre-submission network failures stay visible and retry the same backed-up wallet safely',async()=>{
 let calls=0;
 const ui=await start((_phrase,{onProgress})=>{
   calls++;
   onProgress(0,1,'network');
   return calls===1?Promise.reject(new Error('blockchain readiness could not be verified'))
     :Promise.resolve({state:'confirmed',fid:'4294967333'});
 });
 const create=ui.one('Create account');
 void create.onclick();
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(create.disabled,false,'pre-write failure must remain recoverable');
 assert.equal(create.textContent,'Try again');
 assert.ok(ui.nodes.some(x=>/Registration was not submitted/.test(x.textContent)));
 void create.onclick();
 assert.deepEqual(await ui.dialog,{state:'confirmed',fid:'4294967333'});
 assert.equal(calls,2,'only pre-submission retries are allowed');
});

test('Farcaster FID reservation is clearly irreversible and keeps cancellation disabled',async()=>{
  let resolveReservation;
  const ui=await start((_phrase,{signal,onProgress})=>{
    onProgress(0,4,'reserve');
    return new Promise(resolve=>{resolveReservation=resolve;});
  },{protocol:'farcaster-snapchain'});
  void ui.one('Create account').onclick();
  await new Promise(resolve=>setImmediate(resolve));
  const cancel=ui.one('Cancel');
  assert.equal(cancel.disabled,true,'paid reserve POST is an irreversible boundary');
  assert.ok(ui.nodes.some(n=>String(n.textContent).includes('Preparing sponsored registration')));
  resolveReservation({state:'confirmed',fid:'101'});
  await ui.dialog;
});

test('Farcaster sponsorship code is explained with a verified official signup link',async()=>{
 const ui=await start(()=>Promise.resolve({fid:'101'}),{protocol:'farcaster-snapchain'});
 const links=ui.nodes.filter(n=>n.tagName==='A');
 assert.equal(links.length,1);
 assert.equal(links[0].href,'https://farcaster.xyz/');
 assert.equal(links[0].target,'_blank');
 assert.equal(links[0].rel,'noopener noreferrer');
 assert.ok(ui.nodes.some(n=>String(n.textContent).includes('Genycaster operator')));
 ui.one('Cancel').onclick();
 await assert.rejects(ui.dialog,/cancelled/);
});
