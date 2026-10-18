// Browser-specific Farcaster HLS playback overlay.
//
// Qt Multimedia's WebAssembly backend delegates codec support to the browser,
// which is inconsistent for adaptive HLS. Use the browser's native HLS support
// (Safari), or pinned local hls.js/MSE (Chromium/Firefox). The ordinary
// GenyVideoPlayer remains the one implementation for native platforms.
(function () {
  "use strict";
  if (typeof window === "undefined") return;

  const state = { overlay: null, video: null, hls: null, lastFocus: null };
  const hlsScript = "vendor/hls-1.6.15.min.js";

  function approvedUrl(input) {
    if (typeof input !== "string" || input.length > 2048) return null;
    try {
      const url = new URL(input);
      const host = url.hostname.toLowerCase();
      if (url.protocol !== "https:" || url.username || url.password ||
          (url.port && url.port !== "443") ||
          !/(?:^|\/)[^/]*\.m3u8$/i.test(url.pathname) ||
          !host.includes(".") || host.startsWith("[") ||
          /(?:^|\.)(?:localhost|local|internal|test|invalid)$/.test(host) ||
          /^\d+\.\d+\.\d+\.\d+$/.test(host)) return null;
      return url;
    } catch { return null; }
  }

  function close() {
    const old = state.overlay;
    state.overlay = null;
    if (state.hls) {
      try { state.hls.destroy(); } catch {}
      state.hls = null;
    }
    if (state.video) {
      state.video.pause();
      state.video.removeAttribute("src");
      state.video.load();
      state.video = null;
    }
    if (old) old.remove();
    if (state.lastFocus?.focus) state.lastFocus.focus();
    state.lastFocus = null;
  }

  let scriptLoading = null;
  function loadHls() {
    if (window.Hls) return Promise.resolve(window.Hls);
    if (scriptLoading) return scriptLoading;
    scriptLoading = new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = hlsScript;
      script.async = true;
      script.onload = () => window.Hls ? resolve(window.Hls) : reject(new Error("HLS library missing"));
      script.onerror = () => reject(new Error("Cannot load local HLS engine"));
      document.head.append(script);
    }).catch(error => {
      scriptLoading = null;
      throw error;
    });
    return scriptLoading;
  }

  function open(input, title) {
    const url = approvedUrl(input);
    if (!url || !document.body) return false;
    close();
    state.lastFocus = document.activeElement;
    const persian = String(window.__genycasterPreferredLanguage || navigator.language || "")
      .toLowerCase().startsWith("fa");
    const sheet = document.createElement("section");
    sheet.id = "genycaster-browser-hls";
    sheet.setAttribute("role", "dialog");
    sheet.setAttribute("aria-modal", "true");
    sheet.setAttribute("aria-label", String(title || "Video").slice(0, 100));
    sheet.className = "genycaster-hls-modal";
    const style = document.createElement("style");
    style.textContent = `
      #genycaster-browser-hls .gc-hls-header { flex-shrink:0; }
      #genycaster-browser-hls .gc-hls-stage { flex:1; min-height:0; display:flex;
        align-items:center; justify-content:center; padding:12px;
        box-sizing:border-box; overflow:hidden; }
      #genycaster-browser-hls .gc-hls-video { width:100%; max-width:1100px;
        height:auto; max-height:100%; aspect-ratio:16/9; object-fit:contain;
        border-radius:12px; background:#000; }
      #genycaster-browser-hls .gc-hls-status { min-height:22px; flex-shrink:0; }
      @media (orientation:landscape) and (max-height:520px) {
        #genycaster-browser-hls .gc-hls-header { min-height:44px !important; padding:4px 14px !important; }
        #genycaster-browser-hls .gc-hls-stage { padding:4px 12px !important; }
        #genycaster-browser-hls .gc-hls-video { max-width:min(100%, 900px); }
        #genycaster-browser-hls .gc-hls-status { padding:4px 12px !important; }
      }`;
    sheet.append(style);
    Object.assign(sheet.style, {
      position: "fixed", inset: "0", zIndex: "2147483001",
      background: "#0b0c0f", color: "#f8f9fc", display: "flex",
      flexDirection: "column", fontFamily: "system-ui, sans-serif",
      paddingTop: "env(safe-area-inset-top, 0px)",
      paddingBottom: "env(safe-area-inset-bottom, 0px)"
    });
    const header = document.createElement("header");
    header.className = "gc-hls-header";
    Object.assign(header.style, {
      display: "flex", alignItems: "center", gap: "12px", minHeight: "58px",
      padding: "8px 16px", borderBottom: "1px solid #282c35", boxSizing: "border-box",
      background: "#17191f"
    });
    const heading = document.createElement("strong");
    // Avoid the backend's generic 'Farcaster video' caption for a public
    // stream. A hostname subtitle makes provenance visible to the user.
    heading.textContent = String(title && title !== 'Farcaster video'
      ? title : (persian ? 'پخش ویدیو' : 'Video player')).slice(0, 100);
    Object.assign(heading.style, { flex: "1", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" });
    const external = document.createElement("button");
    external.textContent = persian ? "باز کردن لینک" : "Open original";
    const dismiss = document.createElement("button");
    dismiss.textContent = "×";
    dismiss.setAttribute("aria-label", persian ? "بستن ویدیو" : "Close video");
    for (const button of [external, dismiss]) {
      Object.assign(button.style, {
        border: "1px solid #434752", borderRadius: "12px", minHeight: "44px",
        minWidth: button === dismiss ? "44px" : "96px",
        padding: "8px 12px", color: "#f8f9fc", background: "#262a33",
        cursor: "pointer", fontSize: button === dismiss ? "24px" : "14px"
      });
    }
    external.addEventListener("click", () => window.open(url.href, "_blank", "noopener,noreferrer"));
    dismiss.addEventListener("click", close);
    header.append(heading, external, dismiss);

    const video = document.createElement("video");
    video.controls = true;
    video.playsInline = true;
    video.autoplay = true;
    video.preload = "metadata";
    video.setAttribute("webkit-playsinline", "");
    video.setAttribute("controlsList", "nodownload");
    video.crossOrigin = "anonymous";
    video.className = "gc-hls-video";
    // A Cloudflare Stream HLS manifest has a canonical poster available
    // without decoding video frames during feed scrolling.
    const streamId = /^\/([a-zA-Z0-9_-]{20,128})\/manifest\/video\.m3u8$/i.exec(url.pathname);
    if (streamId && /^(?:videodelivery\.net|[^.]+\.videodelivery\.net|[^.]+\.cloudflarestream\.com)$/.test(url.hostname))
      video.poster = url.origin + '/' + streamId[1] + '/thumbnails/thumbnail.jpg?time=1s&height=720';
    const status = document.createElement("div");
    status.setAttribute("role", "status");
    status.className = "gc-hls-status";
    status.textContent = persian ? "در حال آماده‌سازی ویدیو…" : "Preparing video…";
    Object.assign(status.style, {
      textAlign: "center", padding: "10px 16px", color: "#b7bed1", fontSize: "13px"
    });
    video.addEventListener("playing", () => { status.textContent = ""; });
    video.addEventListener("error", () => {
      status.textContent = persian
        ? "پخش در این مرورگر ممکن نشد؛ لینک اصلی را باز کنید."
        : "This browser cannot play the stream. Open the original link.";
    });
    const stage = document.createElement("main");
    stage.className = "gc-hls-stage";
    stage.append(video);
    sheet.append(header, stage, status);
    document.body.append(sheet);
    state.overlay = sheet;
    state.video = video;

    const requestPlay = () => video.play().catch(() => {
      status.textContent = persian ? "برای پخش، دکمهٔ ویدیو را لمس کنید." : "Tap Play to start the video.";
    });
    if (video.canPlayType("application/vnd.apple.mpegurl") ||
        video.canPlayType("application/x-mpegURL")) {
      video.src = url.href;
      requestPlay();
    } else {
      loadHls().then(Hls => {
        if (state.video !== video || !Hls.isSupported()) {
          status.textContent = persian ? "پخش HLS در این مرورگر پشتیبانی نمی‌شود." :
            "HLS playback is not supported in this browser.";
          return;
        }
        const instance = new Hls({
          enableWorker: true, autoStartLoad: true, maxBufferLength: 24,
          maxMaxBufferLength: 48, backBufferLength: 12
        });
        state.hls = instance;
        instance.attachMedia(video);
        instance.on(Hls.Events.MEDIA_ATTACHED, () => instance.loadSource(url.href));
        instance.on(Hls.Events.MANIFEST_PARSED, requestPlay);
        instance.on(Hls.Events.ERROR, (_event, details) => {
          if (details?.fatal) status.textContent = persian
            ? "دریافت ویدیو ناموفق بود؛ لینک اصلی را باز کنید."
            : "Unable to play this stream. Open the original link.";
        });
      }).catch(() => {
        if (state.video === video)
          status.textContent = persian ? "موتور پخش در دسترس نیست." : "Video engine unavailable.";
      });
    }

    sheet.addEventListener("keydown", event => {
      if (event.key === "Escape") { event.preventDefault(); close(); }
      if (event.key === "Tab") {
        const tabs = [external, dismiss, video];
        const current = tabs.indexOf(document.activeElement);
        if (current !== -1) {
          event.preventDefault();
          tabs[(current + (event.shiftKey ? tabs.length - 1 : 1)) % tabs.length].focus();
        }
      }
    });
    dismiss.focus();
    return true;
  }

  window.__genycasterBrowserHls = {open, close};
})();
