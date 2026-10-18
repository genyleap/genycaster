// Browser-side, unprivileged Mini App surface for Qt WebAssembly.
// A cross-origin iframe cannot receive the native WKWebView document-start
// SDK injection. Do not pretend that a sandboxed iframe has custody/wallet
// privileges or protocol-write access.
(function () {
    "use strict";
    if (typeof window === "undefined") return;
    const app = { element: null, frame: null, warningTimer: null };

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
        if (element) element.remove();
    }

    function open(address, title, closeLabel) {
        const url = approvedUrl(address);
        if (!url || !document.body) return false;
        close();
        const sheet = document.createElement("section");
        sheet.id = "genycaster-wasm-miniapp-host";
        sheet.setAttribute("role", "dialog");
        sheet.setAttribute("aria-modal", "true");
        sheet.setAttribute("aria-label", String(title || url.hostname).slice(0, 160));
        Object.assign(sheet.style, {
            position: "fixed", inset: "0", zIndex: "2147483000",
            display: "flex", flexDirection: "column",
            background: "#121317", color: "#f7f7f8",
            fontFamily: "system-ui, sans-serif"
        });
        const top = document.createElement("header");
        Object.assign(top.style, {
            display: "flex", alignItems: "center", gap: "12px",
            minHeight: "54px", boxSizing: "border-box", padding: "8px 16px",
            borderBottom: "1px solid #35383f", background: "#1b1d23"
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
        const browserButton = document.createElement('button');
        browserButton.textContent = isPersian ? 'باز کردن در مرورگر' : 'Open in browser';
        browserButton.setAttribute('aria-label', browserButton.textContent);
        Object.assign(browserButton.style, {
            color:'#a4b8ff',background:'#292d3c',border:'1px solid #434d70',
            borderRadius:'9px',fontSize:'13px',padding:'8px 12px',cursor:'pointer',
        });
        browserButton.addEventListener('click', () => {
            window.open(url.href,'_blank','noopener,noreferrer');
        });
        const dismiss = document.createElement("button");
        dismiss.textContent = "×";
        dismiss.setAttribute("aria-label", String(closeLabel || "Close").slice(0, 80));
        Object.assign(dismiss.style, {
            color: "inherit", background: "transparent",
            border: "1px solid #484b54", borderRadius: "10px",
            cursor: "pointer", fontSize: "24px", minWidth: "42px",
            height: "38px"
        });
        dismiss.addEventListener("click", close);
        top.append(name, domain, browserButton, dismiss);

        const frame = document.createElement("iframe");
        frame.title = String(title || "Mini App").slice(0, 160);
        frame.setAttribute("sandbox", "allow-scripts allow-same-origin allow-forms allow-popups");
        frame.setAttribute("referrerpolicy", "no-referrer");
        frame.setAttribute("loading", "eager");
        frame.setAttribute("allow", "fullscreen");
        Object.assign(frame.style, {
            display: "block", width: "100%", flex: "1",
            minHeight: "0", background: "#fff", border: "0"
        });
        const warning=document.createElement('div');
        warning.setAttribute('role','status');
        warning.textContent = isPersian
          ? 'اگر برنامه بارگذاری نشد، آن را در مرورگر باز کنید.'
          : 'If the app does not load, open it in your browser.';
        Object.assign(warning.style, {
          display:'none',padding:'12px 16px',background:'#282321',
          borderTop:'1px solid #5d4d39',color:'#ead6b5',fontSize:'13px',
        });
        frame.addEventListener('error',()=>{warning.style.display='block';});
        frame.addEventListener('load',()=>{
          if(app.warningTimer!==null)clearTimeout(app.warningTimer);
          app.warningTimer=null;
        });
        // Cross-origin CSP/frame-ancestors refusals cannot be inspected. An
        // explicit external fallback must remain reachable at all times.
        app.warningTimer=setTimeout(()=>{warning.style.display='block';},12000);
        // Set the URL only after sandbox and origin restrictions exist.
        frame.src = url.href;
        sheet.append(top, frame, warning);
        document.body.append(sheet);
        app.element = sheet;
        app.frame = frame;
        dismiss.focus();
        return true;
    }

    window.__genycasterMiniAppOverlay = Object.freeze({
        open, close,
        active: () => Boolean(app.element && app.element.isConnected)
    });
}());
