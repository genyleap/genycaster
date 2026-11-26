// The only DOM editing surface used for Qt canvas inputs. The browser owns
// selection, IME, keyboard shortcuts and native Copy/Paste. No account logic,
// storage, networking, or programmatic clipboard permission is involved.
(() => {
    let active=null;
    function finish(session,ok,notify=true) {
        if(active!==session)return;
        active=null;
        const {dialog,input,token,reply,layout,previousFocus}=session;
        const value=ok?input.value:'';
        input.value='';
        window.visualViewport?.removeEventListener('resize',layout);
        window.visualViewport?.removeEventListener('scroll',layout);
        window.removeEventListener('resize',layout);
        try{dialog.close();}catch{}
        dialog.remove();
        if(previousFocus?.isConnected && previousFocus.focus)
            try{previousFocus.focus({preventScroll:true});}catch{}
        if(notify)reply(token,value,ok);
    }
    function open(token,value,options,reply) {
        if(!window.isSecureContext || typeof token!=='string' || token.length>128 ||
            typeof value!=='string' || value.length>262144 || typeof reply!=='function')return false;
        if(active)finish(active,false);
        const dialog=document.createElement('dialog');
        if(typeof dialog.showModal!=='function')return false;
        dialog.id='genycaster-system-text-editor';
        dialog.setAttribute('aria-label',String(options.title||'Edit'));
        const safeColor=(v,fallback)=>/^#[a-f0-9]{6}$/i.test(v||'')?v:fallback;
        const surface=safeColor(options.surface,'#202124'),ink=safeColor(options.ink,'#f5f5f5');
        const border=safeColor(options.border,'#626262');
        const accent=safeColor(options.accent,'#167aff');
        dialog.style.cssText='position:fixed;inset:auto;margin:0;box-sizing:border-box;display:flex;flex-direction:column;gap:12px;padding:16px;border:1px solid '+border+';border-radius:18px;background:'+surface+';color:'+ink+';font:16px system-ui;max-width:none;max-height:none;overflow:auto;overscroll-behavior:contain;z-index:2147483647';
        dialog.dir=options.rtl?'rtl':'ltr';
        const heading=document.createElement('div');
        heading.textContent=String(options.title||'Edit');
        heading.style.cssText='font-size:18px;font-weight:600';
        const multiline=!!options.multiline&&!options.sensitive;
        const input=document.createElement(multiline?'textarea':'input');
        if(!multiline)input.type=options.sensitive?'password':'text';
        input.value=value;
        value='';
        input.placeholder=String(options.placeholder||'');
        input.setAttribute('aria-label',String(options.title||'Edit'));
        input.maxLength=Math.max(1,Math.min(262144,Number(options.maxLength)||262144));
        input.autocomplete='off';
        input.spellcheck=!options.sensitive;
        input.setAttribute('autocapitalize',options.sensitive?'none':'sentences');
        if(options.sensitive){
            input.setAttribute('autocorrect','off');
            input.setAttribute('data-lpignore','true');
            input.setAttribute('data-1p-ignore','true');
            // Password inputs mask on every engine. Normalize a multiline
            // phrase only in the real user-generated paste event.
            input.addEventListener('paste',e=>{
                const text=e.clipboardData?.getData('text/plain');
                if(typeof text!=='string')return;
                e.preventDefault();
                const normalized=text.replace(/\s+/g,' ');
                const start=input.selectionStart,end=input.selectionEnd;
                const available=Math.max(0,input.maxLength-(input.value.length-(end-start)));
                input.setRangeText(normalized.slice(0,available),start,end,'end');
            });
            input.addEventListener('copy',e=>e.preventDefault());
            input.addEventListener('cut',e=>e.preventDefault());
        }
        input.style.cssText+=';box-sizing:border-box;width:100%;min-width:0;min-height:'+(multiline?'100':'48')+'px;flex:'+(multiline?'1 1 auto':'0 0 auto')+';padding:12px;border:1px solid '+border+';border-radius:10px;background:transparent;color:'+ink+';font:16px/1.5 system-ui;outline-color:'+accent+';resize:none;touch-action:auto;user-select:text;-webkit-user-select:text;-webkit-touch-callout:default;';
        const hint=document.createElement('div');
        hint.textContent=String(options.hint||'');
        hint.style.cssText='font-size:12px;line-height:1.4;color:'+safeColor(options.muted,'#a8a8a8');
        const actions=document.createElement('div');
        actions.style.cssText='display:flex;justify-content:flex-end;gap:10px;flex:none';
        function button(label,primary) {
            const el=document.createElement('button');
            el.type='button';el.textContent=String(label);
            el.style.cssText='min-height:44px;padding:10px 18px;border:1px solid '+(primary?accent:border)+';border-radius:10px;background:'+(primary?accent:'transparent')+';color:'+(primary?'#ffffff':ink)+';font:500 16px system-ui';
            return el;
        }
        const cancel=button(options.cancel||'Cancel',false),apply=button(options.apply||'Apply',true);
        actions.append(cancel,apply);dialog.append(heading,hint,input,actions);
        const previousFocus=document.activeElement;
        const layout=()=>{
            const vp=window.visualViewport;
            const width=vp?.width||window.innerWidth,height=vp?.height||window.innerHeight;
            const w=Math.min(560,Math.max(160,width-24));
            dialog.style.width=w+'px';
            dialog.style.left=((vp?.offsetLeft||0)+(width-w)/2)+'px';
            dialog.style.top=((vp?.offsetTop||0)+Math.min(12,Math.max(0,height/20)))+'px';
            dialog.style.maxHeight=Math.max(120,height-24)+'px';
            if(multiline)input.style.height=Math.max(100,Math.min(220,height-155))+'px';
        };
        const session={dialog,input,token,reply,layout,previousFocus};
        active=session;
        cancel.addEventListener('click',()=>finish(session,false));
        apply.addEventListener('click',()=>finish(session,true));
        dialog.addEventListener('cancel',e=>{e.preventDefault();finish(session,false);});
        dialog.addEventListener('close',()=>finish(session,false));
        input.addEventListener('keydown',e=>{
            if(e.key==='Enter'&&(!multiline||e.ctrlKey||e.metaKey)){
                e.preventDefault();finish(session,true);
            }
        });
        // Stop Qt's global keyboard listeners from consuming browser editing
        // events; do not prevent native beforeinput/paste/selection actions.
        for(const name of ['keydown','keyup','keypress','paste','copy','cut','beforeinput','input','compositionstart','compositionupdate','compositionend'])
            dialog.addEventListener(name,e=>e.stopPropagation());
        try{
            document.body.append(dialog);layout();dialog.showModal();
            input.focus({preventScroll:true});
            const start=Math.min(input.value.length,Math.max(0,Number(options.start)||0));
            const end=Math.min(input.value.length,Math.max(start,Number(options.end)||start));
            input.setSelectionRange(start,end);
            window.visualViewport?.addEventListener('resize',layout);
            window.visualViewport?.addEventListener('scroll',layout);
            window.addEventListener('resize',layout);
            return true;
        }catch{
            finish(session,false,false);return false;
        }
    }
    window.addEventListener('pagehide',()=>{if(active)finish(active,false,false);});
    window.__genycasterSystemEditor=Object.freeze({
        open,
        cancel(token){if(active?.token===token)finish(active,false,false);}
    });
})();
