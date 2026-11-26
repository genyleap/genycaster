// Native browser input over the existing Qt field, never a dialog or a
// separate authentication surface. Qt retains the field data and geometry.
(() => {
    let active=null;
    const color=(v,fallback)=>/^#[a-f0-9]{6}$/i.test(v||'')?v:fallback;
    function close(session,notify=true) {
        if(active!==session)return;
        active=null;
        session.input.value='';
        session.input.remove();
        window.visualViewport?.removeEventListener('resize',session.layout);
        window.visualViewport?.removeEventListener('scroll',session.layout);
        window.removeEventListener('resize',session.layout);
        if(notify)session.reply(session.token,'',false);
    }
    function layout(session) {
        const screen=document.getElementById('qt-screen');
        const g=session.options.geometry;
        if(!screen||!g||!g.windowWidth||!g.windowHeight||
           g.clipWidth<=0||g.clipHeight<=0){
            close(session);return false;
        }
        const b=screen.getBoundingClientRect(),sx=b.width/g.windowWidth,sy=b.height/g.windowHeight;
        const x=g.x+(session.options.leftPadding||0),y=g.y+(session.options.topPadding||0);
        const w=Math.max(1,g.width-(session.options.leftPadding||0)-(session.options.rightPadding||0));
        const h=Math.max(1,g.height-(session.options.topPadding||0)-(session.options.bottomPadding||0));
        const i=session.input;
        const left=Math.max(0,g.clipX-x)*sx,top=Math.max(0,g.clipY-y)*sy;
        const right=Math.max(0,x+w-g.clipX-g.clipWidth)*sx;
        const bottom=Math.max(0,y+h-g.clipY-g.clipHeight)*sy;
        if(left+right>=w*sx||top+bottom>=h*sy){close(session);return false;}
        Object.assign(i.style,{
            left:(b.left+x*sx)+'px',top:(b.top+y*sy)+'px',
            width:w*sx+'px',height:h*sy+'px',
            clipPath:'inset('+top+'px '+right+'px '+bottom+'px '+left+'px)',
            fontFamily:session.options.fontFamily||'system-ui',
            fontSize:Math.max(16,(session.options.fontSize||16)*sx)+'px',
            fontWeight:String(session.options.fontWeight||400),
            color:color(session.options.ink,'#f4f4f4'),
            caretColor:color(session.options.accent,'#167aff'),
            textAlign:session.options.rtl?'right':'left'
        });
        i.dir=session.options.rtl?'rtl':'ltr';
        i.placeholder=String(session.options.placeholder||'');
        if(i.tagName==='INPUT'){
            const type=session.options.masked?'password':'text';
            if(i.type!==type)i.type=type;
        }
        return true;
    }
    function open(token,value,options,reply,selectionReply) {
        if(!window.isSecureContext||typeof token!=='string'||!token.length||token.length>128||
           typeof value!=='string'||value.length>262144||typeof reply!=='function')return false;
        if(active)close(active);
        const multiline=!!options.multiline&&!options.masked;
        const input=document.createElement(multiline?'textarea':'input');
        if(!multiline)input.type=options.masked?'password':'text';
        input.id='genycaster-inline-text-editor';
        input.setAttribute('aria-label',String(options.placeholder||'Text input'));
        input.autocomplete='off';
        input.spellcheck=!options.sensitive;
        input.setAttribute('autocapitalize',options.sensitive?'none':'sentences');
        input.setAttribute('autocorrect',options.sensitive?'off':'on');
        input.maxLength=Math.max(1,Math.min(262144,Number(options.maxLength)||262144));
        input.value=value;
        value='';
        input.style.cssText='position:fixed;z-index:5;box-sizing:border-box;padding:0;margin:0;border:0;outline:0;border-radius:0;box-shadow:none;background:transparent;appearance:none;-webkit-appearance:none;resize:none;line-height:1.25;user-select:text;-webkit-user-select:text;-webkit-touch-callout:default;touch-action:auto;';
        input.style.setProperty('--genycaster-placeholder',color(options.placeholderInk,'#a8a8a8'));
        // Keep the placeholder visually aligned with the existing Qt field.
        if(!document.getElementById('genycaster-inline-editor-style')){
            const style=document.createElement('style');
            style.id='genycaster-inline-editor-style';
            style.textContent='#genycaster-inline-text-editor::placeholder{color:var(--genycaster-placeholder);opacity:1}';
            document.head.append(style);
        }
        const session={token,input,options,reply,composing:false,layout:null};
        session.layout=()=>{if(active===session)layout(session);};
        active=session;
        const publishSelection=()=>{
            if(active===session&&!session.composing&&typeof selectionReply==='function')
                selectionReply(token,input.selectionStart||0,input.selectionEnd||0);
        };
        const publish=()=>{
            if(active===session&&!session.composing){
                reply(token,input.value,true);
                publishSelection();
            }
        };
        input.addEventListener('select',publishSelection);
        input.addEventListener('selectionchange',publishSelection);
        input.addEventListener('keyup',publishSelection);
        input.addEventListener('click',publishSelection);
        input.addEventListener('compositionstart',()=>{session.composing=true;});
        input.addEventListener('compositionend',()=>{session.composing=false;publish();});
        input.addEventListener('input',publish);
        input.addEventListener('blur',()=>close(session));
        if(options.sensitive){
            input.setAttribute('data-lpignore','true');
            input.setAttribute('data-1p-ignore','true');
            input.addEventListener('copy',e=>e.preventDefault());
            input.addEventListener('cut',e=>e.preventDefault());
            input.addEventListener('paste',e=>{
                const text=e.clipboardData?.getData('text/plain');
                if(typeof text!=='string')return;
                e.preventDefault();
                const normalized=text.replace(/\s+/g,' ');
                const start=input.selectionStart,end=input.selectionEnd;
                const available=Math.max(0,input.maxLength-(input.value.length-(end-start)));
                input.setRangeText(normalized.slice(0,available),start,end,'end');
                publish();
            });
        }
        input.addEventListener('keydown',e=>{
            if(e.key==='Escape'){e.preventDefault();close(session);}
            if(e.key==='Enter'&&!multiline&&!session.composing){
                e.preventDefault();close(session);
            }
        });
        // Stop the Qt canvas from consuming editing events. Default browser
        // selection, context menu, keyboard and clipboard behavior remain.
        for(const name of ['keydown','keyup','keypress','paste','copy','cut','beforeinput','input','compositionstart','compositionupdate','compositionend','contextmenu','pointerdown','pointerup','touchstart','touchend'])
            input.addEventListener(name,e=>e.stopPropagation());
        try{
            document.body.append(input);
            if(!layout(session))return false;
            input.focus({preventScroll:true});
            if(active!==session)return false;
            const start=Math.min(input.value.length,Math.max(0,Number(options.start)||0));
            const end=Math.min(input.value.length,Math.max(start,Number(options.end)||start));
            input.setSelectionRange(start,end);
            window.visualViewport?.addEventListener('resize',session.layout);
            window.visualViewport?.addEventListener('scroll',session.layout);
            window.addEventListener('resize',session.layout);
            return true;
        }catch{close(session,false);return false;}
    }
    function sync(token,value,options){
        const session=active;
        if(!session||session.token!==token)return;
        session.options=options;
        if(!layout(session))return;
        // Qt validation/distribution may change the field. Never overwrite
        // the browser's in-flight IME composition or move an unchanged caret.
        if(!options.geometryOnly&&!session.composing&&session.input.value!==value){
            const start=session.input.selectionStart,end=session.input.selectionEnd;
            session.input.value=value;
            session.input.setSelectionRange(Math.min(start,value.length),Math.min(end,value.length));
        }
    }
    // A tap on a different Qt control releases this overlay before that
    // control takes focus. Native selection inside the input is not intercepted.
    document.addEventListener('pointerdown',e=>{
        if(active&&e.target!==active.input)close(active);
    },true);
    window.addEventListener('pagehide',()=>{if(active)close(active,false);});
    window.__genycasterSystemEditor=Object.freeze({
        open,sync,
        cancel(token){if(active?.token===token)close(active,false);}
    });
})();
