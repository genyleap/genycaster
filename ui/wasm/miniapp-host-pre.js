// Browser-side, unprivileged Mini App surface for Qt WebAssembly.
// A cross-origin iframe cannot receive the native WKWebView document-start
// SDK injection. Do not pretend that a sandboxed iframe has custody/wallet
// privileges or protocol-write access.
(function () {
    "use strict";
    if (typeof window === "undefined") return;
    const app = {
        element: null, frame: null, warningTimer: null, sessions: [], limit: 1,
        holder: null, tabs: null, titleEl: null, warning: null,
        activeIndex: -1, activeUrl: "", mode: "window",
        fullscreen: false, backdrop: null, warning: null,
        observer: null, screenButton: null, dismissButton: null, header: null, user: {}
    };

    function approvedUrl(value) {
        try {
            const url = new URL(value);
            const host = url.hostname.toLowerCase();
            if (url.protocol !== "https:" || url.username || url.password ||
                // allow-scripts + allow-same-origin is safe only when the
                // framed document is not the Genycaster host's own origin.
                (window.location && url.origin === window.location.origin) ||
                (url.port && url.port !== "443") || !host.includes(".") ||
                host.endsWith(".local") || host.endsWith(".localhost") ||
                host.endsWith(".internal") || host.endsWith(".test") ||
                host.endsWith(".invalid") || /^\d+\.\d+\.\d+\.\d+$/.test(host) ||
                host.startsWith("[") || host.endsWith(".")) return null;
            return url;
        } catch { return null; }
    }

    function close() {
        const element = app.element;
        if (app.warningTimer !== null) clearTimeout(app.warningTimer);
        app.warningTimer = null;
        app.element = null;
        app.frame = null;
        if (app.observer) { app.observer.disconnect(); app.observer = null; }
        if (app.backdrop) { app.backdrop.remove(); app.backdrop = null; }
        if (typeof document !== "undefined" && document.fullscreenElement === element)
            document.exitFullscreen?.().catch?.(() => {});
        for (const session of app.sessions) {
            session.detach?.();
            session.frame.remove();
        }
        app.sessions = [];
        app.activeIndex = -1;
        app.activeUrl = "";
        app.holder = null;
        app.tabs = null;
        app.titleEl = null;
        app.warning = null;
        app.fullscreen = false;
        app.screenButton = null;
        app.dismissButton = null;
        app.header = null;
        if (element) element.remove();
    }

    function normalizeMode(value) {
        return ["window", "bottom", "left", "right"].includes(String(value))
            ? String(value) : "window";
    }
    function surfaceColors() {
        const root = document.documentElement;
        const mode = root && root.getAttribute
            ? root.getAttribute("data-chrome-theme") : "";
        const light = mode === "light" ||
            (mode !== "dark" && window.matchMedia?.("(prefers-color-scheme: light)")?.matches);
        return light ? {
            bg:"#f9f9fb", chrome:"#f1f2f5", text:"#1c222b",
            subtle:"#697180", border:"#dce0e6", active:"#e7eaf0",
            warn:"#99622d", backdrop:"rgba(22,28,38,.34)"
        } : {
            bg:"#15171b", chrome:"#202228", text:"#f6f6f8",
            subtle:"#b2b6c0", border:"#353940", active:"#343844",
            warn:"#efc38a", backdrop:"rgba(0,0,0,.55)"
        };
    }
    function paint() {
        if (!app.element) return;
        const colors = surfaceColors();
        Object.assign(app.element.style,{
            background:colors.bg,color:colors.text,
            border:"1px solid "+colors.border
        });
        if(app.header) Object.assign(app.header.style,{
            background:colors.chrome,borderBottom:"1px solid "+colors.border
        });
        if(app.dismissButton) Object.assign(app.dismissButton.style,{
            borderColor:colors.border,color:colors.text
        });
        if(app.backdrop)app.backdrop.style.background=colors.backdrop;
        if(app.tabs)app.tabs.style.borderBottom="1px solid "+colors.border;
        if(app.warning)Object.assign(app.warning.style,{
            background:colors.chrome,color:colors.warn,
            borderTop:"1px solid "+colors.border
        });
        renderTabs();
    }
    function applyPresentation() {
        if (!app.element) return;
        const sheet=app.element;
        // On mobile every presentation mode becomes a true viewport-filling app.
        // Desktop retains the selected floating/docked presentation.
        const narrow=(window.innerWidth || 1024)<640;
        const effective=(app.fullscreen || narrow)?"fullscreen":app.mode;
        const base={
            position:"fixed",zIndex:"2147483000",
            display:"flex",flexDirection:"column",
            maxWidth:"none",maxHeight:"none",
            boxShadow:"0 22px 72px rgba(0,0,0,.28)",
            overflow:"hidden"
        };
        if(effective==="fullscreen")Object.assign(base,{
            inset:"0",width:"100%",height:"100%",borderRadius:"0"
        });
        else if(effective==="bottom")Object.assign(base,{
            inset:"auto 0 0 0",width:"min(424px,100%)",
            left:"50%",right:"auto",
            transform:"translateX(-50%)",
            height:"min(755px,94dvh)",
            borderRadius:"20px 20px 0 0"
        });
        else if(effective==="left")Object.assign(base,{
            inset:"0 auto 0 0",width:"min(424px,90vw)",
            height:"100%",borderRadius:"0 20px 20px 0"
        });
        else if(effective==="right")Object.assign(base,{
            inset:"0 0 0 auto",width:"min(424px,90vw)",
            height:"100%",borderRadius:"20px 0 0 20px"
        });
        else Object.assign(base,{
            inset:"50% auto auto 50%",
            transform:"translate(-50%,-50%)",
            // Match the native 424 x 755 Mini App window instead of a
            // broad desktop dialog that stretches mobile-oriented content.
            width:"min(424px,calc(100vw - 32px))",
            height:"min(755px,calc(100dvh - 32px))",
            borderRadius:"20px"
        });
        // Reset properties to avoid stale values after changing presentation
        // from a drawer to centered window or Full Screen.
        for(const key of ["inset","top","right","bottom","left","width",
                          "height","transform","borderRadius"])sheet.style[key]="";
        Object.assign(sheet.style,base);
        // iOS standalone browser safe-area belongs to the overlay chrome.
        if(app.header) app.header.style.paddingTop = narrow
            ? "calc(8px + env(safe-area-inset-top,0px))" : "8px";
        if(app.holder) app.holder.style.paddingBottom = narrow
            ? "env(safe-area-inset-bottom,0px)" : "0";
        if(app.backdrop)app.backdrop.style.display=app.fullscreen?"none":"block";
        if(app.screenButton){
            app.screenButton.textContent=app.fullscreen?"⤡":"⤢";
            app.screenButton.setAttribute("aria-label",
                app.fullscreen?"Exit full screen":"Full screen");
            app.screenButton.title=app.fullscreen?"Exit full screen":"Full screen";
        }
        paint();
    }
    function setMode(mode) {
        app.mode=normalizeMode(mode);
        applyPresentation();
    }
    async function toggleFullscreen() {
        if(!app.element)return false;
        const sheet=app.element;
        if(app.fullscreen){
            app.fullscreen=false;
            if(document.fullscreenElement===sheet && document.exitFullscreen){
                try { await document.exitFullscreen(); } catch {}
            }
        }else{
            app.fullscreen=true;
            if(sheet.requestFullscreen){
                try { await sheet.requestFullscreen(); } catch {
                    // Presentation remains viewport-filling when browser
                    // fullscreen permission is unavailable.
                }
            }
        }
        applyPresentation();
        return app.fullscreen;
    }

    function renderTabs() {
        if (!app.tabs) return;
        if (typeof app.tabs.replaceChildren === "function") app.tabs.replaceChildren();
        else if (Array.isArray(app.tabs.children)) app.tabs.children=[];
        app.tabs.style.display = app.sessions.length > 1 ? "flex" : "none";
        if (app.sessions.length <= 1) return;
        for (let index = 0; index < app.sessions.length; index++) {
            const session = app.sessions[index];
            const tab = document.createElement("button");
            tab.type = "button";
            tab.textContent = session.title;
            tab.setAttribute("role", "tab");
            tab.setAttribute("aria-selected",String(index === app.activeIndex));
            const palette=surfaceColors();
            Object.assign(tab.style,{
                border:"1px solid "+palette.border,borderRadius:"9px",
                background:index === app.activeIndex ? palette.active : palette.chrome,
                color:palette.text,padding:"7px 11px",cursor:"pointer",
                whiteSpace:"nowrap",maxWidth:"180px",overflow:"hidden",
                textOverflow:"ellipsis",flex:"0 1 auto"
            });
            tab.addEventListener("click",()=>select(index));
            const dismiss = document.createElement("button");
            dismiss.type = "button";
            dismiss.textContent = "×";
            dismiss.setAttribute("aria-label","Close "+session.title);
            Object.assign(dismiss.style,{border:0,background:"transparent",
                color:palette.text,cursor:"pointer",padding:"4px 9px"});
            dismiss.addEventListener("click",()=>closeTab(index));
            const group=document.createElement("span");
            Object.assign(group.style,{display:"flex",alignItems:"center",gap:"2px"});
            group.append(tab,dismiss);
            app.tabs.append(group);
        }
    }
    function select(index) {
        if(index < 0 || index >= app.sessions.length)return;
        app.activeIndex = index;
        const session = app.sessions[index];
        app.frame=session.frame;
        app.activeUrl=session.url;
        for(let n=0;n<app.sessions.length;n++)
            app.sessions[n].frame.style.display=n===index?"block":"none";
        if(app.titleEl)app.titleEl.textContent=session.title;
        if(app.warning)app.warning.style.display="none";
        renderTabs();
    }
    function closeTab(index) {
        if(index < 0 || index >= app.sessions.length)return;
        const previousActive=app.activeIndex;
        app.sessions[index].detach?.();
        app.sessions[index].frame.remove();
        app.sessions.splice(index,1);
        if(!app.sessions.length){close();return;}
        const nextIndex=index<previousActive ? previousActive-1
            : index===previousActive ? Math.min(index,app.sessions.length-1)
            : previousActive;
        select(nextIndex);
    }
    function setLimit(value) {
        const n=Number(value);
        app.limit=Number.isFinite(n) ? Math.max(1,Math.min(4,Math.floor(n))) : 1;
        while(app.sessions.length>app.limit) {
            // Keep the foreground session if the membership is downgraded.
            closeTab(app.activeIndex===0 ? app.sessions.length-1 : 0);
        }
    }
    function attachSession(session) {
        if (session.detach) return;
        const sdk = window.__genycasterMiniAppSdk;
        if (!sdk || typeof sdk.attach !== "function") return;
        try {
            session.detach = sdk.attach(session.frame, session.url, app.user, {
                ready: () => {
                    session.ready = true;
                    if (app.frame === session.frame && app.warning)
                        app.warning.style.display = "none";
                },
                close: () => {
                    const index = app.sessions.indexOf(session);
                    if (index >= 0) closeTab(index);
                },
                openMiniApp: url => open(url, new URL(url).hostname, "Close"),
                fullscreen: requested => {
                    if (app.fullscreen !== requested) void toggleFullscreen();
                }
            });
        } catch (error) {
            console.warn("[GenyCaster] Mini App SDK host unavailable", error);
            if (app.frame === session.frame && app.warning)
                app.warning.style.display = "block";
        }
    }
    function enableSdk() {
        app.sessions.forEach(attachSession);
    }
    function setUser(user) {
        const next = user && typeof user === "object" ? user : {};
        const previousFid = Number(app.user?.fid || 0);
        const nextFid = Number(next.fid || 0);
        app.user = next;
        // ReactNativeWebView/native hosts hydrate the account independently
        // from Mini App navigation. Keep already-attached, same-FID SDK
        // contexts current without reloading apps or overwriting identity on
        // a different FID. Native host closes sessions on an account switch.
        if (previousFid > 0 && previousFid === nextFid)
            for (const session of app.sessions)
                session.detach?.updateUser?.(next);
    }
    function makeFrame(url,title) {
        const frame=document.createElement("iframe");
        frame.title=String(title||"Mini App").slice(0,160);
        frame.setAttribute("sandbox","allow-scripts allow-same-origin allow-forms allow-popups");
        frame.setAttribute("referrerpolicy","no-referrer");
        frame.setAttribute("loading","eager");
        frame.setAttribute("allow","fullscreen");
        Object.assign(frame.style,{display:"block",width:"100%",flex:"1",
            minHeight:"0",background:"#fff",border:"0"});
        frame.addEventListener("error",()=>{
            if(app.frame===frame && app.warning)app.warning.style.display="block";
        });
        frame.src=url.href;
        return frame;
    }

    function open(address, title, closeLabel, presentation) {
        if (presentation !== undefined) setMode(presentation);
        const url = approvedUrl(address);
        if (!url || !document.body) return false;
        const appTitle=String(title || url.hostname).slice(0,160);
        // The official host Comlink endpoint scopes messages by origin.
        // Never expose two sessions at the same origin simultaneously.
        const existing=app.sessions.findIndex(s=>new URL(s.url).origin===url.origin);
        if(existing>=0){select(existing);return true;}
        if (app.element && app.element.isConnected) {
            if(app.sessions.length>=app.limit) {
                if(app.limit===1) close();
                else return false;
            } else {
                const frame=makeFrame(url,appTitle);
                app.holder.append(frame);
                const session = {url:url.href,title:appTitle,frame,detach:null,ready:false};
                app.sessions.push(session);
                attachSession(session);
                select(app.sessions.length-1);
                return true;
            }
        }
        close();
        const sheet = document.createElement("section");
        sheet.id = "genycaster-wasm-miniapp-host";
        sheet.setAttribute("role", "dialog");
        sheet.setAttribute("aria-modal", "true");
        sheet.setAttribute("aria-label", String(title || url.hostname).slice(0, 160));
        Object.assign(sheet.style, {
            display:"flex",flexDirection:"column",
            fontFamily:"system-ui,-apple-system,sans-serif"
        });
        const top = document.createElement("header");
        Object.assign(top.style, {
            display: "flex", alignItems: "center", gap: "8px",
            minHeight: "54px", boxSizing: "border-box", padding: "8px 12px"
        });
        const name = document.createElement("strong");
        name.textContent = String(title || url.hostname).slice(0, 160);
        Object.assign(name.style, { overflow: "hidden", textOverflow: "ellipsis",
            whiteSpace: "nowrap", flex: "1" });
        const domain = document.createElement("span");
        domain.textContent = url.hostname;
        Object.assign(domain.style, { fontSize: "12px", color: "#afb2ba" });
        const isPersian = String(window.__genycasterPreferredLanguage || navigator.language || '')
            .toLowerCase().startsWith('fa');
        // Mini Apps open inside the selected GenyCaster presentation.
        // An unrelated "Open in Farcaster/browser" toolbar is not part
        // of the native experience and must not interrupt the user here.
        const dismiss = document.createElement("button");
        dismiss.type = "button";
        dismiss.textContent = "×";
        dismiss.setAttribute("aria-label", String(closeLabel || "Close").slice(0, 80));
        Object.assign(dismiss.style, {
            display:"flex",alignItems:"center",justifyContent:"center",
            boxSizing:"border-box",flex:"0 0 40px",padding:"0",
            color:"inherit",background:"transparent",
            border:"1px solid",borderRadius:"10px",
            cursor:"pointer",fontFamily:"inherit",fontWeight:"400",
            fontSize:"25px",lineHeight:"1",width:"40px",height:"40px"
        });
        dismiss.addEventListener("click", close);
        const fullscreenButton = document.createElement("button");
        fullscreenButton.type="button";
        fullscreenButton.textContent="⤢";
        fullscreenButton.setAttribute("aria-label","Full screen");
        Object.assign(fullscreenButton.style,{
            color:"inherit",background:"transparent",border:"0",
            borderRadius:"10px",cursor:"pointer",fontSize:"24px",
            boxSizing:"border-box",display:"flex",alignItems:"center",
            justifyContent:"center",flex:"0 0 40px",padding:"0",
            width:"40px",height:"40px"
        });
        fullscreenButton.addEventListener("click",()=>{void toggleFullscreen();});
        top.append(name, fullscreenButton, dismiss);
        const tabs=document.createElement("nav");
        tabs.setAttribute("role","tablist");
        tabs.setAttribute("aria-label",isPersian ? "مینی‌اپ‌های باز" : "Open Mini Apps");
        Object.assign(tabs.style,{display:"none",gap:"6px",overflowX:"auto",
            boxSizing:"border-box",padding:"7px 12px",
            borderBottom:"1px solid #35383f"});
        const holder=document.createElement("div");
        Object.assign(holder.style,{display:"flex",flex:"1",minHeight:"0"});
        const frame = makeFrame(url,appTitle);
        holder.append(frame);
        const warning=document.createElement('div');
        warning.setAttribute('role','status');
        warning.textContent = isPersian
            ? 'این مینی‌اپ در مرورگر بارگذاری نشد. بعضی برنامه‌ها به قابلیت‌های SDK میزبان نیاز دارند.'
            : 'This Mini App did not load in the browser. Some apps require host SDK capabilities.';
        Object.assign(warning.style,{
            display:'none',padding:'12px 16px',fontSize:'13px'
        });
        frame.addEventListener('error',()=>{warning.style.display='block';});
        sheet.append(top, tabs, holder, warning);
        const backdrop=document.createElement("div");
        Object.assign(backdrop.style,{
            position:"fixed",inset:"0",zIndex:"2147482999"
        });
        backdrop.addEventListener("click",close);
        document.body.append(backdrop);
        document.body.append(sheet);
        app.backdrop=backdrop;
        app.element = sheet;
        app.holder = holder;
        app.tabs = tabs;
        app.titleEl = name;
        app.header=top;
        app.warning = warning;
        app.screenButton = fullscreenButton;
        app.dismissButton = dismiss;
        const session = {url:url.href,title:appTitle,frame,detach:null,ready:false};
        app.sessions = [session];
        attachSession(session);
        app.fullscreen=false;
        applyPresentation();
        select(0);
        // The browser-owned Mini App presentation uses the same restrained
        // entry rhythm as Qt drawers without changing its fixed geometry.
        if(typeof sheet.animate==="function"
             && !(window.matchMedia?.("(prefers-reduced-motion: reduce)")?.matches)) {
            sheet.animate([{opacity:0.3},{opacity:1}],{
                duration:260,easing:"cubic-bezier(.2,.75,.2,1)"
            });
        }
        if(typeof MutationObserver!=="undefined" && document.documentElement){
            app.observer=new MutationObserver(()=>paint());
            app.observer.observe(document.documentElement,{
                attributes:true,attributeFilter:["data-chrome-theme"]
            });
        }
        dismiss.focus();
        return true;
    }

    // Recompute on rotation and browser viewport changes without reloading the iframe.
    if (typeof window.addEventListener === "function")
        window.addEventListener("resize", () => { if (app.element) applyPresentation(); });

    window.__genycasterMiniAppOverlay = Object.freeze({
        open, close, setLimit, setMode, toggleFullscreen, enableSdk, setUser,
        mode:()=>app.mode,
        fullscreen:()=>app.fullscreen,
        active: () => Boolean(app.element && app.element.isConnected)
    });
}());
