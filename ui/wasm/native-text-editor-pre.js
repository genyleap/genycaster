// Persistent browser-owned text inputs. The overlay is a sibling of the Qt
// canvas, never inside Qt's touch-action:none window. No gesture synthesizing.
(() => {
    const fields=new Map();
    let layer=null,refreshFrame=0;
    const registeredFonts=new Map();
    function registerFont(family,bytes,weight){
        const key=family+":"+weight;
        if(registeredFonts.has(key)||typeof FontFace!=="function")return;
        const face=new FontFace(family,bytes,{weight:String(weight)});
        registeredFonts.set(key,face);
        document.fonts.add(face);
        face.load().then(refresh,()=>{document.fonts.delete(face);registeredFonts.delete(key);});
    }
    function fontStack(family){
        if(!family||/^sans serif$|^sans-serif$|^system-ui$/i.test(family))
            return '"DejaVu Sans", sans-serif';
        if(/^monospace$/i.test(family))return '"DejaVu Sans Mono", monospace';
        return '"'+String(family).replace(/["\\]/g,"")+'", "DejaVu Sans", sans-serif';
    }
    function canvasRect(screen){
        // Qt's canvas can be offset within its host on WebKit (safe areas,
        // visual viewport panning and decorated QQuickWindows).
        const shadow=screen.querySelector("#qt-shadow-container")?.shadowRoot;
        const nodes=[...(shadow||screen).querySelectorAll(".qt-window-canvas")];
        const boxes=nodes.map(n=>n.getBoundingClientRect()).filter(b=>b.width>0&&b.height>0);
        return boxes.sort((a,b)=>b.width*b.height-a.width*a.height)[0]||screen.getBoundingClientRect();
    }
    function host(){
        const screen=document.getElementById('qt-screen');
        if(!screen)return null;
        if(!layer){
            layer=document.createElement('div');
            layer.id='genycaster-native-input-layer';
            layer.style.cssText='position:absolute;inset:0;z-index:2;pointer-events:none;touch-action:auto;overflow:visible;';
            screen.after(layer);
            const style=document.createElement('style');
            style.textContent='.genycaster-native-input::placeholder{color:var(--placeholder);opacity:1}';
            layer.append(style);
        }
        return screen;
    }
    function remove(field){
        fields.delete(field.token);
        if(document.activeElement===field.input)field.input.blur();
        if(field.input instanceof HTMLInputElement||field.input instanceof HTMLTextAreaElement)field.input.value='';
        else field.input.textContent='';
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
        const b=canvasRect(screen),origin=layer.getBoundingClientRect();
        const sx=b.width/g.windowWidth,sy=b.height/g.windowHeight;
        const o=f.options;
        const x=g.x+(o.leftPadding||0),y=g.y+(o.topPadding||0);
        const w=g.width-(o.leftPadding||0)-(o.rightPadding||0),h=g.height-(o.topPadding||0)-(o.bottomPadding||0);
        const left=Math.max(0,g.clipX-x)*sx,top=Math.max(0,g.clipY-y)*sy;
        const right=Math.max(0,x+w-g.clipX-g.clipWidth)*sx,bottom=Math.max(0,y+h-g.clipY-g.clipHeight)*sy;
        if(w*sx<=left+right||h*sy<=top+bottom)return false;
        // Keep the CSS input at the iOS non-auto-zoom threshold, then scale
        // its entire box back to the exact QML font and content geometry.
        const size=Math.max(1,(o.fontSize||16)*sx);
        const scale=o.readOnlyCast?1:Math.min(1,size/16);
        const style={left:(b.left-origin.left+x*sx)+'px',top:(b.top-origin.top+y*sy)+'px',
            width:w*sx/scale+'px',height:h*sy/scale+'px',
            transform:scale===1?'none':'scale('+scale+')',transformOrigin:'top left',
            clipPath:left||top||right||bottom?'inset('+top/scale+'px '+right/scale+'px '+bottom/scale+'px '+left/scale+'px)':'none',
            fontFamily:fontStack(o.fontFamily),fontSize:size/scale+'px',
            fontWeight:String(o.fontWeight||400),caretColor:o.accent,
            lineHeight:o.lineHeight?String(o.lineHeight):'normal',
            textAlign:o.rtl?'right':'left'};
        for(const [key,value] of Object.entries(style)){
            if(f.style[key]!==value){f.input.style[key]=value;f.style[key]=value;}
        }
        const i=f.input;
        if(o.readOnlyCast){
            i.dir=o.rtl?'rtl':'ltr';i.style.color=o.ink;
            i.style.setProperty('--cast-accent',o.accent);
            const content=i.firstElementChild;
            if(content){
                const measured=Math.ceil(content.getBoundingClientRect().height/sy);
                if(measured>0&&measured!==f.measuredHeight){
                    f.measuredHeight=measured;
                    queueMicrotask(()=>{if(fields.get(f.token)===f)f.reply(f.token,JSON.stringify({type:'layout',height:measured}),true);});
                }
            }
            return true;
        }
        if(i.dir!==(o.rtl?'rtl':'ltr'))i.dir=o.rtl?'rtl':'ltr';
        if(i.placeholder!==(o.placeholder||''))i.placeholder=o.placeholder||'';
        if(i.tagName==='INPUT'&&i.type!==(o.masked?'password':'text'))i.type=o.masked?'password':'text';
        paint(f);
        if(i.readOnly!==!!o.readOnly)i.readOnly=!!o.readOnly;
        const limit=Math.max(1,Math.min(262144,Number(o.maxLength)||262144));
        if(i.maxLength!==limit)i.maxLength=limit;
        return true;
    }
    function renderCast(f,value){
        const fragment=document.createDocumentFragment();
        const o=f.options;
        const spans=Array.isArray(o.spans)&&o.spans.length?o.spans:[{kind:'text',text:value}];
        for(const span of spans){
            if(!span)continue;
            if(typeof span==='string'){fragment.append(document.createTextNode(span));continue;}
            const kind=String(span.kind||'text');
            const target=kind==='mention'&&span.username?'@'+String(span.username):
                kind==='link'&&/^https?:\/\//i.test(String(span.target||''))?String(span.target):'';
            const node=document.createElement(target?'a':kind==='strong'?'strong':kind==='emphasis'?'em':kind==='inline-code'?'code':'span');
            node.textContent=kind==='mention'&&span.username?'@'+String(span.username):String(span.text||'');
            if(target){node.dataset.castTarget=target;node.style.color='var(--cast-accent)';node.style.textDecoration='none';node.setAttribute('role','link');}
            if(span.color&&CSS.supports('color',String(span.color)))node.style.color=String(span.color);
            fragment.append(node);
        }
        const content=document.createElement('div');
        content.style.cssText='display:block;margin:0;padding:0;white-space:pre-wrap;overflow-wrap:anywhere;';
        content.append(fragment);
        f.input.replaceChildren(content);
        f.castSignature=JSON.stringify([value,o.spans]);
    }
    function mountCast(f,value){
        const node=f.input;
        node.className='genycaster-native-cast';
        node.style.cssText='position:absolute;pointer-events:auto;user-select:text;-webkit-user-select:text;-webkit-touch-callout:default;touch-action:pan-y;white-space:pre-wrap;overflow-wrap:anywhere;box-sizing:border-box;overflow:hidden;';
        node.setAttribute('role','article');
        renderCast(f,value);
        const send=(type,extra={})=>{if(fields.get(f.token)===f)f.reply(f.token,JSON.stringify({type,...extra}),true);};
        let gesture=null,suppressClickUntil=0;
        const selected=()=>{const selection=window.getSelection();return selection&&!selection.isCollapsed&&(node.contains(selection.anchorNode)||node.contains(selection.focusNode));};
        node.addEventListener('touchstart',e=>{if(e.touches.length===1)gesture={x:e.touches[0].clientX,y:e.touches[0].clientY,lastY:e.touches[0].clientY,at:performance.now(),scrolling:false};},{passive:true});
        node.addEventListener('touchmove',e=>{
            if(!gesture||e.touches.length!==1||selected())return;
            const t=e.touches[0],dy=t.clientY-gesture.y;
            if(!gesture.scrolling&&performance.now()-gesture.at<450&&Math.abs(dy)>10&&Math.abs(dy)>Math.abs(t.clientX-gesture.x))gesture.scrolling=true;
            if(!gesture.scrolling)return;
            if(e.cancelable)e.preventDefault();
            const delta=gesture.lastY-t.clientY;gesture.lastY=t.clientY;
            send('scroll',{delta});
        },{passive:false});
        const end=()=>{if(gesture&&(gesture.scrolling||performance.now()-gesture.at>450))suppressClickUntil=performance.now()+500;gesture=null;};
        node.addEventListener('touchend',end);node.addEventListener('touchcancel',end);
        node.addEventListener('wheel',e=>{if(e.cancelable)e.preventDefault();send('scroll',{delta:e.deltaY});},{passive:false});
        node.addEventListener('click',e=>{
            if(selected()||performance.now()<suppressClickUntil)return;
            const target=e.target.closest('[data-cast-target]')?.dataset.castTarget;
            send(target?'link':'activate',target?{target}:{});
        });
    }
    function open(token,value,options,reply,selectionReply){
        if(!isSecureContext||!host()||typeof token!=='string'||!token.length||token.length>128||
            typeof value!=='string'||value.length>262144||typeof reply!=='function')return false;
        if(fields.has(token))return sync(token,value,options);
        const multiline=!!options.multiline&&!options.masked;
        const input=document.createElement(options.readOnlyCast?'div':multiline?'textarea':'input');
        if(!multiline&&!options.readOnlyCast)input.type=options.masked?'password':'text';
        if(options.readOnlyCast){
            const f={token,input,options,reply,selectionReply,style:Object.create(null),composing:false};
            fields.set(token,f);layer.append(input);
            mountCast(f,value);
            if(!layout(f)){remove(f);return false;}
            return true;
        }
        input.className='genycaster-native-input';
        input.autocomplete='off';input.spellcheck=!options.sensitive;
        input.setAttribute('autocapitalize',options.sensitive?'none':'sentences');
        input.setAttribute('autocorrect',options.sensitive?'off':'on');
        input.setAttribute('aria-label',options.placeholder||'Text input');
        input.style.cssText='position:absolute;pointer-events:auto;touch-action:auto;user-select:text;-webkit-user-select:text;-webkit-touch-callout:default;box-sizing:border-box;border:0;outline:0;margin:0;padding:0;background:transparent;resize:none;appearance:none;-webkit-appearance:none;line-height:1.25;-webkit-text-size-adjust:100%;text-size-adjust:100%;';
        input.value=value;value='';
        const f={token,input,options,reply,selectionReply,style:Object.create(null),composing:false};
        fields.set(token,f);layer.append(input);
        if(!layout(f)){remove(f);return false;}
        const selection=()=>{if(fields.get(token)===f&&!f.composing)selectionReply?.(token,input.selectionStart||0,input.selectionEnd||0);};
        const publish=()=>{if(fields.get(token)===f&&!f.composing){reply(token,input.value,true);selection();}};
        input.addEventListener('focus',()=>{
            if(fields.get(token)!==f)return;
            paint(f);
            selectionReply?.(token,-1,1);
        });
        input.addEventListener('blur',()=>{
            if(fields.get(token)!==f)return;
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
        // Cast frame updates carry only geometry; retain formatting and spans.
        if(f.options.readOnlyCast&&options.readOnlyCast&&options.geometryOnly)
            options={...f.options,geometry:options.geometry,geometryOnly:true};
        f.options=options;
        if(!layout(f)){remove(f);return false;}
        if(options.readOnlyCast){
            if(!options.geometryOnly&&f.castSignature!==JSON.stringify([value,options.spans])){
                renderCast(f,value);
                layout(f);
            }
            return true;
        }
        if(!options.geometryOnly&&!f.composing&&f.input.value!==value){
            const start=f.input.selectionStart,end=f.input.selectionEnd;
            f.input.value=value;
            f.input.setSelectionRange(Math.min(start,value.length),Math.min(end,value.length));
        }
        if(options.focusRequested&&document.activeElement!==f.input)f.input.focus({preventScroll:true});
        return true;
    }
    function refresh(){
        if(refreshFrame)return;
        refreshFrame=requestAnimationFrame(()=>{
            refreshFrame=0;
            for(const f of [...fields.values()])if(!layout(f)){remove(f);f.reply(f.token,'',false);}
        });
    }
    // Release DOM focus after a Qt canvas gesture, not before its press and
    // release pair. This avoids moving the close button during a keyboard
    // resize and leaves the next canvas click independent of editor text.
    let outsidePointer=null;
    document.addEventListener('pointerdown',event=>{
        const active=document.activeElement;
        if(!active?.classList.contains('genycaster-native-input'))return;
        if(event.composedPath().includes(active))return;
        if(event.composedPath().includes(document.getElementById('qt-screen'))){
            outsidePointer={id:event.pointerId,active};
            if(event.cancelable)event.preventDefault();
        }
    },true);
    function finishOutside(event){
        if(!outsidePointer||outsidePointer.id!==event.pointerId)return;
        const {active}=outsidePointer;outsidePointer=null;
        requestAnimationFrame(()=>{if(document.activeElement===active)active.blur();});
    }
    document.addEventListener('pointerup',finishOutside,true);
    document.addEventListener('pointercancel',finishOutside,true);
    window.addEventListener('resize',refresh);
    window.visualViewport?.addEventListener('resize',refresh);
    window.visualViewport?.addEventListener('scroll',refresh);
    window.addEventListener('pagehide',()=>{for(const f of [...fields.values()])remove(f);});
    window.__genycasterSystemEditor=Object.freeze({open,sync,refresh,registerFont,cancel(token){const f=fields.get(token);if(f)remove(f);}});
})();
