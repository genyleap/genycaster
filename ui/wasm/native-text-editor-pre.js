// Native browser input over the existing Qt field, never a dialog or a
// separate authentication surface. Qt retains the field data and geometry.
(() => {
    let active=null;
    // Qt owns a hidden keyboard input inside its shadow-DOM window. Keep
    // the real editor inside that same window, otherwise Qt observes a
    // window deactivation and its input context can reclaim native focus.
    function deepFocus(){
        let node=document.activeElement;
        while(node?.shadowRoot?.activeElement)node=node.shadowRoot.activeElement;
        return node;
    }
    function qtWindow(){
        const focused=deepFocus();
        const direct=focused?.closest?.('.qt-window');
        if(direct)return direct;
        const screen=document.getElementById('qt-screen');
        if(!screen)return null;
        const visit=root=>{
            const candidates=[...root.querySelectorAll('.qt-window')].filter(el=>{
                const b=el.getBoundingClientRect();
                return b.width>0&&b.height>0&&!el.closest('[inert]');
            });
            if(candidates.length)return candidates.at(-1);
            for(const el of root.querySelectorAll('*')){
                if(el.shadowRoot){const result=visit(el.shadowRoot);if(result)return result;}
            }
            return null;
        };
        return (screen.shadowRoot && visit(screen.shadowRoot)) || visit(screen);
    }
    function setStyle(session,key,value){
        // Comparing CSSOM strings is insufficient: browsers normalize
        // hex colors and font names. Cache authored values instead.
        if(session.appliedStyle[key]===value)return;
        session.input.style[key]=value;
        session.appliedStyle[key]=value;
    }
    function ready(session){
        if(active!==session||!session.input.isConnected||deepFocus()!==session.input)return false;
        const b=session.input.getBoundingClientRect();
        if(b.width<=0||b.height<=0)return false;
        const style=getComputedStyle(session.input);
        if(style.display==='none'||style.visibility!=='visible'||Number(style.opacity)<=0)return false;
        const visible=session.visibleRect;
        if(!visible||visible.width<=0||visible.height<=0)return false;
        const root=session.input.getRootNode();
        const x=visible.x+visible.width/2,y=visible.y+visible.height/2;
        // WebKit versions lacking ShadowRoot.elementFromPoint must not
        // claim ownership based on the outer shadow host alone.
        const hit=root.elementFromPoint?.(x,y) || root.elementsFromPoint?.(x,y)?.[0];
        return hit===session.input;
    }
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
        const screen=session.host;
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
        const viewport=window.visualViewport;
        const vx=viewport?.offsetLeft||0,vy=viewport?.offsetTop||0;
        const vr=vx+(viewport?.width||window.innerWidth),vb=vy+(viewport?.height||window.innerHeight);
        const px=b.left+x*sx,py=b.top+y*sy;
        const cx=Math.max(px+left,vx),cy=Math.max(py+top,vy);
        session.visibleRect={x:cx,y:cy,width:Math.max(0,Math.min(px+w*sx-right,vr)-cx),
            height:Math.max(0,Math.min(py+h*sy-bottom,vb)-cy)};
        const style={
            left:(x*sx)+'px',top:(y*sy)+'px',
            width:w*sx+'px',height:h*sy+'px',
            // Avoid clip-path on fully visible text inputs: WebKit's
            // native selection UI can be affected by clipping.
            clipPath:left||top||right||bottom?'inset('+top+'px '+right+'px '+bottom+'px '+left+'px)':'none',
            fontFamily:session.options.fontFamily||'system-ui',
            fontSize:Math.max(16,(session.options.fontSize||16)*sx)+'px',
            fontWeight:String(session.options.fontWeight||400),
            color:color(session.options.ink,'#f4f4f4'),
            webkitTextFillColor:color(session.options.ink,'#f4f4f4'),
            caretColor:color(session.options.accent,'#167aff'),
            textAlign:session.options.rtl?'right':'left'
        };
        for(const [key,val] of Object.entries(style))setStyle(session,key,val);
        const dir=session.options.rtl?'rtl':'ltr';
        if(i.dir!==dir)i.dir=dir;
        const placeholder=String(session.options.placeholder||'');
        if(i.placeholder!==placeholder)i.placeholder=placeholder;
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
        const host=qtWindow();
        if(!host)return false;
        const multiline=!!options.multiline&&!options.masked;
        const input=document.createElement(multiline?'textarea':'input');
        if(!multiline)input.type=options.masked?'password':'text';
        input.id='genycaster-inline-text-editor';
        input.setAttribute('aria-label',String(options.placeholder||'Text input'));
        input.autocomplete='off';
        input.draggable=false;
        input.spellcheck=!options.sensitive;
        input.setAttribute('autocapitalize',options.sensitive?'none':'sentences');
        input.setAttribute('autocorrect',options.sensitive?'off':'on');
        input.maxLength=Math.max(1,Math.min(262144,Number(options.maxLength)||262144));
        input.value=value;
        value='';
        input.style.cssText='position:absolute;z-index:5;opacity:1;visibility:visible;display:block;pointer-events:auto;box-sizing:border-box;padding:0;margin:0;border:0;outline:0;border-radius:0;box-shadow:none;background:transparent;appearance:none;-webkit-appearance:none;resize:none;line-height:1.25;user-select:text;-webkit-user-select:text;-webkit-touch-callout:default;touch-action:auto;';
        input.style.setProperty('--genycaster-placeholder',color(options.placeholderInk,'#a8a8a8'));
        // Keep the placeholder visually aligned with the existing Qt field.
        const tree=host.getRootNode();
        if(!tree.querySelector('#genycaster-inline-editor-style')){
            const style=document.createElement('style');
            style.id='genycaster-inline-editor-style';
            style.textContent='#genycaster-inline-text-editor::placeholder{color:var(--genycaster-placeholder);opacity:1}';
            if(tree===document)document.head.append(style);else tree.append(style);
        }
        const session={token,input,host,options,reply,composing:false,layout:null,appliedStyle:Object.create(null),visibleRect:null};
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
        input.addEventListener('blur',()=>{
            // A native context menu / keyboard transient may not have a
            // related target. Don't destroy a selected input synchronously.
            queueMicrotask(()=>{
                if(active===session&&deepFocus()!==input)close(session);
            });
        });
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
        for(const name of ['keydown','keyup','keypress','paste','copy','cut','beforeinput','input','compositionstart','compositionupdate','compositionend','contextmenu','pointerdown','pointermove','pointerup','pointercancel','touchstart','touchmove','touchend','touchcancel','dragstart','dblclick','click'])
            input.addEventListener(name,e=>e.stopPropagation());
        try{
            host.append(input);
            if(!layout(session))return false;
            input.focus({preventScroll:true});
            if(!ready(session)){close(session,false);return false;}
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
        if(!ready(session)){close(session);return;}
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
        if(active&&!e.composedPath().includes(active.input))close(active);
    },true);
    window.addEventListener('pagehide',()=>{if(active)close(active,false);});
    window.__genycasterSystemEditor=Object.freeze({
        open,sync,
        cancel(token){if(active?.token===token)close(active,false);}
    });
})();
