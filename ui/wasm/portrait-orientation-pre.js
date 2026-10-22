// Portrait-only mobile PWA display. Browser tabs cannot request hard OS locks.
(function () {
    "use strict";
    if (typeof document === "undefined" || typeof window === "undefined") return;
    const mobileLandscape = () => window.matchMedia &&
        window.matchMedia("(pointer: coarse) and (max-height: 600px) and (orientation: landscape)").matches;
    const notice = document.createElement("div");
    notice.id = "genycaster-portrait-orientation-notice";
    notice.setAttribute("role", "status");
    Object.assign(notice.style, {
        position: "fixed", inset: "0", zIndex: "2147483645",
        display: "none", alignItems: "center", justifyContent: "center",
        padding: "24px", boxSizing: "border-box", textAlign: "center",
        fontFamily: "system-ui, sans-serif", fontSize: "17px",
        background: "#0b0c0f", color: "#f5f5f6"
    });
    const label = document.createElement("span");
    notice.append(label);
    const sync = () => {
        if (!notice.isConnected && document.body) document.body.append(notice);
        notice.style.display = mobileLandscape() ? "flex" : "none";
        label.textContent = String(window.__genycasterPreferredLanguage || navigator.language || "")
            .toLowerCase().startsWith("fa")
            ? "برای استفاده از جنی‌کستر، گوشی را عمودی نگه دارید."
            : "Genycaster uses portrait mode. Rotate your phone upright.";
    };
    const lock = () => {
        if (!window.matchMedia || !window.matchMedia("(display-mode: standalone)").matches) return;
        const orientation = window.screen && window.screen.orientation;
        if (orientation && typeof orientation.lock === "function")
            Promise.resolve(orientation.lock("portrait")).catch(() => {});
    };
    window.addEventListener("resize", sync, {passive:true});
    window.addEventListener("orientationchange", sync, {passive:true});
    document.addEventListener("DOMContentLoaded", sync, {once:true});
    document.addEventListener("pointerdown", lock, {once:true,passive:true});
    sync();
}());
