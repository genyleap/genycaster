/* GenyCaster native release invitation; no tracking, fingerprinting or installs. */
(() => {
    "use strict";
    const byId = (id) => document.getElementById(id);
    const dialog = byId("native-downloads-dialog");
    const trigger = byId("native-downloads-trigger");
    const backdrop = byId("native-downloads-backdrop");
    if (!dialog || !trigger || !backdrop) return;

    const close = byId("native-downloads-close");
    const platformName = byId("native-downloads-platform");
    const architecture = byId("native-downloads-architecture");
    const architectureOptions = byId("native-downloads-options");
    const architectureValue = byId("native-downloads-architecture-value");
    const selection = Object.create(null);
    const status = byId("native-downloads-status");
    const download = byId("native-downloads-action");
    const prompt = byId("native-downloads-nudge");
    const promptOpen = byId("native-downloads-nudge-open");
    const promptClose = byId("native-downloads-nudge-close");
    const tabs = [...dialog.querySelectorAll("[data-native-platform]")];
    const supported = Object.freeze({
        windows: { label: "Windows", arches: {x64: "x86_64", arm64: "ARM64"} },
        macos:   { label: "macOS", arches: {arm64: "Apple Silicon", x64: "Intel"} },
        linux:   { label: "Linux", arches: {x64: "x86_64", arm64: "ARM64"} },
        android: { label: "Android", arches: {arm64: "ARM64-v8a"} }
    });
    // Qt Superellipse.qml uses an exponent-4 continuous cubic contour.
    // CSS corner-shape is not implemented consistently by web engines.
    // Reproduce the QML geometry via lightweight SVG once per element;
    // ResizeObserver changes only the path, never the HTML controls.
    // Exactly one custom SVG surface per major container. Smaller controls
    // use the matching CSS corner family rather than nesting extra strokes.
    const shapeSpecs = [
        ["#native-downloads-dialog", 43, "sheet"],
        ["#native-downloads-options", 23],
        [".native-github-cta", 26]
    ];
    const shapeNamespace = "http://www.w3.org/2000/svg";
    const shapeNodes = new WeakMap();
    const shapeObserver = typeof ResizeObserver === "function"
        ? new ResizeObserver(entries => {
            for (const entry of entries) drawShape(entry.target);
        }) : null;
    const shapeMobile = typeof window.matchMedia === "function"
        ? window.matchMedia("(max-width: 640px)") : null;

    function superellipsePath(width, height, depth, topOnly) {
        if (!(width > 0 && height > 0)) return "";
        const inset = 0.65;
        const left = inset, top = inset;
        const right = Math.max(left, width - inset);
        const bottom = Math.max(top, height - inset);
        const radius = Math.max(0, Math.min(depth - inset,
            (right - left) / 2 - 0.15,
            topOnly ? right - left : (bottom - top) / 2 - 0.15));
        const r = Math.max(0, radius);
        // Mirror Qt's cubicFactor = (8 * (0.5 ^ (1/exponent)) - 4)/3.
        const k = (8 * Math.pow(0.5, 0.25) - 4) / 3;
        const f = (n) => Number(n.toFixed(3));
        const tl = r, tr = r, br = topOnly ? 0 : r, bl = topOnly ? 0 : r;
        return [
            "M", f(left + tl), f(top),
            "L", f(right - tr), f(top),
            "C", f(right - tr + k * tr), f(top),
                 f(right), f(top + tr - k * tr),
                 f(right), f(top + tr),
            "L", f(right), f(bottom - br),
            "C", f(right), f(bottom - br + k * br),
                 f(right - br + k * br), f(bottom),
                 f(right - br), f(bottom),
            "L", f(left + bl), f(bottom),
            "C", f(left + bl - k * bl), f(bottom),
                 f(left), f(bottom - bl + k * bl),
                 f(left), f(bottom - bl),
            "L", f(left), f(top + tl),
            "C", f(left), f(top + tl - k * tl),
                 f(left + tl - k * tl), f(top),
                 f(left + tl), f(top),
            "Z"
        ].join(" ");
    }

    function drawShape(element) {
        const info = shapeNodes.get(element);
        if (!info) return;
        const width = element.clientWidth, height = element.clientHeight;
        if (!width || !height) return;
        const mobileSheet = info.mode === "sheet" &&
            Boolean(shapeMobile?.matches);
        const depth = mobileSheet ? 41 : info.depth;
        const dimensions = width + ":" + height + ":" + depth + ":" + mobileSheet;
        if (info.last === dimensions) return;
        info.last = dimensions;
        info.svg.setAttribute("viewBox", "0 0 " + width + " " + height);
        info.path.setAttribute("d",
            superellipsePath(width, height, depth, mobileSheet));
    }

    function mountShape(element, depth, mode = "") {
        if (shapeNodes.has(element)) return;
        const svg = document.createElementNS(shapeNamespace, "svg");
        const path = document.createElementNS(shapeNamespace, "path");
        svg.classList.add("nd-squircle-svg");
        svg.setAttribute("aria-hidden", "true");
        svg.setAttribute("focusable", "false");
        svg.setAttribute("preserveAspectRatio", "none");
        path.setAttribute("vector-effect", "non-scaling-stroke");
        path.setAttribute("stroke-linejoin", "round");
        svg.append(path);
        element.prepend(svg);
        element.classList.add("nd-squircle");
        shapeNodes.set(element, {svg, path, depth, mode, last:""});
        shapeObserver?.observe(element);
        drawShape(element);
    }

    for (const [selector, depth, mode] of shapeSpecs) {
        for (const element of document.querySelectorAll(selector))
            mountShape(element, depth, mode);
    }
    if (!shapeObserver) {
        window.addEventListener("resize", () => {
            for (const [selector] of shapeSpecs)
                for (const element of document.querySelectorAll(selector))
                    drawShape(element);
        }, {passive:true});
    }
    if (shapeMobile) {
        const refreshShapes = () => {
            for (const [selector] of shapeSpecs)
                for (const element of document.querySelectorAll(selector))
                    drawShape(element);
        };
        if (shapeMobile.addEventListener)
            shapeMobile.addEventListener("change", refreshShapes);
        else if (shapeMobile.addListener)
            shapeMobile.addListener(refreshShapes);
    }

    let platform = "windows";
    let catalog = Object.create(null);
    let lastFocused = null;
    let userActivated = false;
    const releaseBase = "https://github.com/genyleap/genycaster/releases/download/";

    function detectPlatform() {
        const ua = navigator.userAgent || "";
        const hint = String(navigator.userAgentData?.platform ||
                            navigator.platform || "").toLowerCase();
        if (/android/i.test(ua) || hint.includes("android")) return "android";
        if (/iphone|ipad|ipod/i.test(ua) || (hint.includes("mac") &&
                navigator.maxTouchPoints > 1)) return null;
        if (hint.includes("win") || /windows/i.test(ua)) return "windows";
        if (hint.includes("mac") || /macintosh/i.test(ua)) return "macos";
        if (hint.includes("linux") || /linux/i.test(ua)) return "linux";
        return null;
    }

    function verifiedRelease(record) {
        if (!record || typeof record !== "object") return null;
        if (typeof record.url !== "string" || typeof record.sha256 !== "string" ||
            typeof record.version !== "string" ||
            !/^[a-f0-9]{64}$/.test(record.sha256) ||
            !/^v?[0-9]+\.[0-9]+\.[0-9]+(?:-[a-z0-9.]+)?$/i.test(record.version)) return null;
        let url;
        try { url = new URL(record.url); } catch (_) { return null; }
        if (url.origin !== "https://github.com" ||
            !url.href.startsWith(releaseBase) ||
            url.username || url.password || url.search || url.hash ||
            /%2f|%5c|\.\./i.test(url.pathname)) return null;
        return {url: url.href, version: record.version, sha256: record.sha256};
    }

    async function loadCatalog() {
        try {
            const response = await fetch("/releases/native.json",
                {cache: "no-store", credentials: "omit", redirect: "error"});
            if (!response.ok || Number(response.headers.get("content-length") || 0) > 32768)
                throw new Error("Catalog unavailable");
            const body = await response.text();
            if (body.length > 32768) throw new Error("Catalog too large");
            const data = JSON.parse(body);
            if (data.schema !== 1 || typeof data.platforms !== "object" ||
                !data.platforms || Array.isArray(data.platforms)) throw new Error("Bad catalog");
            const safe = Object.create(null);
            for (const [os, info] of Object.entries(supported)) {
                safe[os] = Object.create(null);
                for (const arch of Object.keys(info.arches)) {
                    const entry = verifiedRelease(data.platforms[os]?.[arch]);
                    if (entry) safe[os][arch] = entry;
                }
            }
            catalog = safe;
        } catch (_) {
            // A missing, invalid or unavailable release catalog NEVER enables
            // an invented, pre-release, unsigned or unverified download link.
            catalog = Object.create(null);
        }
        render();
        maybeNudge();
    }

    function selectedArchitecture() {
        const choices = supported[platform].arches;
        const preferred = selection[platform];
        if (preferred && Object.hasOwn(choices, preferred)) return preferred;
        return Object.keys(choices)[0];
    }

    function closeOptions(restoreFocus = false) {
        if (architectureOptions.hidden) return;
        architectureOptions.hidden = true;
        architecture.setAttribute("aria-expanded", "false");
        architecture.classList.remove("is-open");
        architecture.closest(".native-downloads-configuration")?.classList.remove("is-dropdown-open");
        if (restoreFocus) architecture.focus({preventScroll: true});
    }

    function openOptions(focusSelected = false) {
        if (!architectureOptions.hidden) return;
        architectureOptions.hidden = false;
        architecture.setAttribute("aria-expanded", "true");
        architecture.classList.add("is-open");
        architecture.closest(".native-downloads-configuration")?.classList.add("is-dropdown-open");
        // Clicking keeps keyboard focus on the trigger; it should not paint
        // a second focus ring around the preselected option. Keyboard
        // activation still moves focus to the selected option.
        if (focusSelected)
            architectureOptions.querySelector('[aria-selected="true"]')?.focus({preventScroll:true});
    }

    function chooseArchitecture(value, restoreFocus = true) {
        if (!Object.hasOwn(supported[platform].arches, value)) return;
        selection[platform] = value;
        closeOptions(restoreFocus);
        render();
    }

    function render() {
        const info = supported[platform];
        platformName.textContent = info.label;
        for (const tab of tabs) {
            const active = tab.dataset.nativePlatform === platform;
            tab.setAttribute("aria-pressed", String(active));
            tab.classList.toggle("selected", active);
        }
        const arch = selectedArchitecture();
        architectureValue.textContent = info.arches[arch];
        architectureOptions.replaceChildren();
        const options = Object.entries(info.arches);
        for (const [code, label] of options) {
            const option = document.createElement("button");
            option.type = "button";
            option.className = "native-downloads-option";
            option.setAttribute("role", "option");
            option.setAttribute("aria-selected", String(arch === code));
            option.tabIndex = arch === code ? 0 : -1;
            option.dataset.architecture = code;
            option.textContent = label;
            option.addEventListener("click", () => chooseArchitecture(code));
            architectureOptions.append(option);
        }
        const entry = catalog[platform]?.[arch] || null;
        download.hidden = !entry;
        if (entry) {
            download.href = entry.url;
            download.textContent = "Download " + info.label + " · " + entry.version;
            download.setAttribute("aria-label", "Download verified " + info.label +
                " " + info.arches[arch] + " release " + entry.version);
            status.textContent = "Published build · SHA-256 " +
                entry.sha256.slice(0, 12) + "…";
        } else {
            download.removeAttribute("href");
            download.removeAttribute("aria-label");
            status.textContent = "No verified download for this system yet. " +
                "The browser edition remains available.";
        }
    }


    function setOpen(open) {
        if (open) {
            closeOptions();
            lastFocused = document.activeElement;
            userActivated = true;
            hideNudge();
            dialog.hidden = false;
            backdrop.hidden = false;
            drawShape(dialog);
            trigger.setAttribute("aria-expanded", "true");
            close.focus({preventScroll: true});
        } else {
            closeOptions();
            dialog.hidden = true;
            backdrop.hidden = true;
            trigger.setAttribute("aria-expanded", "false");
            (lastFocused?.isConnected ? lastFocused : trigger).focus({preventScroll: true});
        }
    }

    function hideNudge() {
        prompt.hidden = true;
    }

    function maybeNudge() {
        const device = detectPlatform();
        const hasRelease = device && Object.keys(catalog[device] || {}).length;
        if (!hasRelease || userActivated || !prompt.hidden) return;
        try {
            if (localStorage.getItem("genycaster.native-downloads.dismissed.v1") === "1") return;
        } catch (_) { /* private mode: show at most once this page */ }
        const boot = byId("genycaster-boot");
        if (!boot) return;
        const reveal = () => {
            if (boot.classList.contains("finished") && !userActivated) {
                prompt.hidden = false;
                observer.disconnect();
            }
        };
        const observer = new MutationObserver(reveal);
        observer.observe(boot, {attributes: true, attributeFilter: ["class"]});
        reveal();
    }

    trigger.addEventListener("click", () => { hideNudge(); setOpen(true); });
    close.addEventListener("click", () => setOpen(false));
    backdrop.addEventListener("click", () => setOpen(false));
    tabs.forEach(tab => tab.addEventListener("click", () => {
        platform = tab.dataset.nativePlatform;
        render();
    }));
    architecture.addEventListener("click", () => {
        if (architectureOptions.hidden) openOptions();
        else closeOptions(true);
    });
    architecture.addEventListener("keydown", event => {
        if (["ArrowDown", "ArrowUp", "Home", "End", " ", "Enter"].includes(event.key)) {
            event.preventDefault();
            openOptions(true);
            const choices = [...architectureOptions.querySelectorAll('[role="option"]')];
            const option = event.key === "End" ? choices[choices.length - 1]
                         : event.key === "Home" ? choices[0]
                         : architectureOptions.querySelector('[aria-selected="true"]') || choices[0];
            option?.focus();
        } else if (event.key === "Escape") {
            event.preventDefault();
            event.stopPropagation();
            closeOptions(true);
        }
    });
    architectureOptions.addEventListener("keydown", event => {
        const choices = [...architectureOptions.querySelectorAll('[role="option"]')];
        const current = choices.indexOf(document.activeElement);
        let next = -1;
        if (event.key === "ArrowDown") next = (current + 1) % choices.length;
        else if (event.key === "ArrowUp") next = (current - 1 + choices.length) % choices.length;
        else if (event.key === "Home") next = 0;
        else if (event.key === "End") next = choices.length - 1;
        if (next >= 0) {
            event.preventDefault();
            choices[next]?.focus();
        } else if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            chooseArchitecture(document.activeElement?.dataset.architecture);
        } else if (event.key === "Escape") {
            event.preventDefault();
            event.stopPropagation();
            closeOptions(true);
        } else if (event.key === "Tab") {
            closeOptions();
        }
    });
    document.addEventListener("pointerdown", event => {
        if (!architectureOptions.hidden &&
                !architectureOptions.contains(event.target) &&
                !architecture.contains(event.target)) closeOptions();
    });
    dialog.addEventListener("keydown", event => {
        if (event.key === "Escape") {
            if (!architectureOptions.hidden) {
                event.preventDefault(); event.stopPropagation(); closeOptions(true); return;
            }
            event.preventDefault(); setOpen(false);
        }
        if (event.key !== "Tab") return;
        const items = [...dialog.querySelectorAll(
            'button:not([disabled]):not([hidden]),a[href]:not([hidden])')]
            .filter(item => !architectureOptions.contains(item) || !architectureOptions.hidden);
        if (!items.length) return;
        const first = items[0], last = items[items.length - 1];
        if (event.shiftKey && document.activeElement === first) {
            event.preventDefault(); last.focus();
        } else if (!event.shiftKey && document.activeElement === last) {
            event.preventDefault(); first.focus();
        }
    });
    document.addEventListener("keydown", event => {
        if (event.key === "Escape" && !dialog.hidden && architectureOptions.hidden)
            setOpen(false);
    });
    promptOpen.addEventListener("click", () => {
        const current = detectPlatform();
        if (current) platform = current;
        render();
        setOpen(true);
    });
    promptClose.addEventListener("click", () => {
        hideNudge();
        try { localStorage.setItem("genycaster.native-downloads.dismissed.v1", "1"); }
        catch (_) { /* optional dismissal preference */ }
    });
    const platformFromUA = detectPlatform();
    if (platformFromUA) platform = platformFromUA;
    render();
    loadCatalog();
    // Public entry point for GenyCaster's native/WebAssembly chrome. No
    // credentials, private account data or Qt identity data cross this API.
    window.__genycasterOpenNativeDownloads = () => setOpen(true);
})();
