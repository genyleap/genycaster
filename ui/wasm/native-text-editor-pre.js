// Persistent browser-owned text inputs. The overlay is a sibling of the Qt
// canvas, never inside Qt's touch-action:none window. No gesture synthesizing.
(() => {
    const fields=new Map();
    let layer=null;
    function host(){
        const screen=document.getElementById('qt-screen');
        if(!screen)return null;
        if(!layer){
            layer=document.createElement('div');
            layer.id='genycaster-native-input-layer';
            layer.style.cssText='position:fixed;inset:0;z-index:2;pointer-events:none;touch-action:auto;overflow:visible;';
            screen.after(layer);
            const style=document.createElement('style');
            style.textContent='.genycaster-native-input::placeholder{color:var(--placeholder);opacity:1}';
            layer.append(style);
        }
        return screen;
    }
    function remove(field){
        fields.delete(field.token);
        field.input.value='';
        field.input.remove();
    }
    function paint(f){
        const active=document.activeElement===f.input;
        const ink=(active||f.options.paintIdle)?f.options.ink:'transparent';
        const placeholder=(active||f.options.paintIdle)?(f.options.placeholderInk||'#999999'):'transparent';
        for(const [key,value] of Object.entries({color:ink,webkitTextFillColor:ink})){
            if(f.style[key]!==value){f.input.style[key]=value;f.style[key]=value;}
        }
        if(f.style.placeholderInk!==placeholder){
            f.input.style.setProperty('--placeholder',placeholder);
            f.style.placeholderInk=placeholder;
        }
    }
    function layout(f){
        const screen=host(),g=f.options.geometry;
        if(!screen||!g||!g.windowWidth||!g.windowHeight||g.clipWidth<=0||g.clipHeight<=0)return false;
        const b=screen.getBoundingClientRect(),sx=b.width/g.windowWidth,sy=b.height/g.windowHeight;
        const o=f.options;
        const x=g.x+(o.leftPadding||0),y=g.y+(o.topPadding||0);
        const w=g.width-(o.leftPadding||0)-(o.rightPadding||0),h=g.height-(o.topPadding||0)-(o.bottomPadding||0);
        const left=Math.max(0,g.clipX-x)*sx,top=Math.max(0,g.clipY-y)*sy;
        const right=Math.max(0,x+w-g.clipX-g.clipWidth)*sx,bottom=Math.max(0,y+h-g.clipY-g.clipHeight)*sy;
        if(w*sx<=left+right||h*sy<=top+bottom)return false;
        const style={left:(b.left+x*sx)+'px',top:(b.top+y*sy)+'px',width:w*sx+'px',height:h*sy+'px',
            clipPath:left||top||right||bottom?'inset('+top+'px '+right+'px '+bottom+'px '+left+'px)':'none',
            fontFamily:o.fontFamily||'system-ui',fontSize:Math.max(16,(o.fontSize||16)*sx)+'px',
            fontWeight:String(o.fontWeight||400),caretColor:o.accent,
            textAlign:o.rtl?'right':'left'};
        for(const [key,value] of Object.entries(style)){
            if(f.style[key]!==value){f.input.style[key]=value;f.style[key]=value;}
        }
        const i=f.input;
        if(i.dir!==(o.rtl?'rtl':'ltr'))i.dir=o.rtl?'rtl':'ltr';
        if(i.placeholder!==(o.placeholder||''))i.placeholder=o.placeholder||'';
        if(i.tagName==='INPUT'&&i.type!==(o.masked?'password':'text'))i.type=o.masked?'password':'text';
        paint(f);
        if(i.readOnly!==!!o.readOnly)i.readOnly=!!o.readOnly;
        const limit=Math.max(1,Math.min(262144,Number(o.maxLength)||262144));
        if(i.maxLength!==limit)i.maxLength=limit;
        return true;
    }
    function open(token,value,options,reply,selectionReply){
        if(!isSecureContext||!host()||typeof token!=='string'||!token.length||token.length>128||
            typeof value!=='string'||value.length>262144||typeof reply!=='function')return false;
        if(fields.has(token))return sync(token,value,options);
        const multiline=!!options.multiline&&!options.masked;
        const input=document.createElement(multiline?'textarea':'input');
        if(!multiline)input.type=options.masked?'password':'text';
        input.className='genycaster-native-input';
        input.autocomplete='off';input.spellcheck=!options.sensitive;
        input.setAttribute('autocapitalize',options.sensitive?'none':'sentences');
        input.setAttribute('autocorrect',options.sensitive?'off':'on');
        input.setAttribute('aria-label',options.placeholder||'Text input');
        input.style.cssText='position:absolute;pointer-events:auto;touch-action:auto;user-select:text;-webkit-user-select:text;-webkit-touch-callout:default;box-sizing:border-box;border:0;outline:0;margin:0;padding:0;background:transparent;resize:none;appearance:none;-webkit-appearance:none;line-height:1.25;';
        input.value=value;value='';
        const f={token,input,options,reply,selectionReply,style:Object.create(null),composing:false};
        fields.set(token,f);layer.append(input);
        if(!layout(f)){remove(f);return false;}
        const selection=()=>{if(fields.get(token)===f&&!f.composing)selectionReply?.(token,input.selectionStart||0,input.selectionEnd||0);};
        const publish=()=>{if(fields.get(token)===f&&!f.composing){reply(token,input.value,true);selection();}};
        input.addEventListener('focus',()=>{
            paint(f);
            selectionReply?.(token,-1,1);
        });
        input.addEventListener('blur',()=>{
            paint(f);
            selectionReply?.(token,-1,0);
        });
        input.addEventListener('select',selection);
        input.addEventListener('selectionchange',selection);
        input.addEventListener('click',selection);
        input.addEventListener('keyup',selection);
        input.addEventListener('compositionstart',()=>{f.composing=true;});
        input.addEventListener('compositionend',()=>{f.composing=false;publish();});
        input.addEventListener('input',publish);
        input.addEventListener('keydown',e=>{
            if(e.key==='Enter'&&!multiline&&!f.composing){e.preventDefault();selectionReply?.(token,-2,0);}
        });
        if(options.sensitive){
            input.setAttribute('data-lpignore','true');input.setAttribute('data-1p-ignore','true');
            input.addEventListener('copy',e=>e.preventDefault());
            input.addEventListener('cut',e=>e.preventDefault());
            input.addEventListener('paste',e=>{
                const raw=e.clipboardData?.getData('text/plain');
                if(typeof raw!=='string')return;
                e.preventDefault();
                const start=input.selectionStart,end=input.selectionEnd;
                const limit=Math.max(0,input.maxLength-(input.value.length-(end-start)));
                input.setRangeText(raw.replace(/\s+/g,' ').slice(0,limit),start,end,'end');publish();
            });
        }
        // Native default actions are never prevented for ordinary text.
        for(const name of ['keydown','keyup','keypress','paste','copy','cut','beforeinput','input','compositionstart','compositionupdate','compositionend','contextmenu','pointerdown','pointerup','touchstart','touchmove','touchend'])
            input.addEventListener(name,e=>e.stopPropagation());
        return true;
    }
    function sync(token,value,options){
        const f=fields.get(token);if(!f)return false;
        f.options=options;
        if(!layout(f)){remove(f);return false;}
        if(!options.geometryOnly&&!f.composing&&f.input.value!==value){
            const start=f.input.selectionStart,end=f.input.selectionEnd;
            f.input.value=value;
            f.input.setSelectionRange(Math.min(start,value.length),Math.min(end,value.length));
        }
        if(options.focusRequested)f.input.focus({preventScroll:true});
        return true;
    }
    function refresh(){for(const f of [...fields.values()])if(!layout(f)){remove(f);f.reply(f.token,'',false);}}
    window.addEventListener('resize',refresh);
    window.visualViewport?.addEventListener('resize',refresh);
    window.visualViewport?.addEventListener('scroll',refresh);
    window.addEventListener('pagehide',()=>{for(const f of [...fields.values()])remove(f);});
    window.__genycasterSystemEditor=Object.freeze({open,sync,cancel(token){const f=fields.get(token);if(f)remove(f);}});
})();
