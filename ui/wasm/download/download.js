/* GenyCaster download hub: strict release manifest and explicit architectures. */
(() => {
  "use strict";
  // Until the canonical Nginx location can be installed, its legacy 302
  // reaches this exact static entry through /about/#downloads. Preserve the
  // public address in the browser without adding a navigation/history entry.
  // All assets use absolute URLs, so subsequent fetches are unaffected.
  if (typeof location !== "undefined" && typeof history !== "undefined" &&
      (location.pathname === "/download/index.html" ||
       location.pathname === "/download/")) {
    history.replaceState(history.state, "", "/download" +
      location.search + location.hash);
  }
  const $ = (id) => document.getElementById(id);
  const version = "1.0.512";
  const releasePrefix = "https://github.com/genyleap/genycaster/releases/download/v1.0.512/";
  const supported = Object.freeze({
    windows: {name:"Windows", desc:"descWindows", format:"EXE", arches:{x64:"Windows x64",arm64:"Windows ARM64"}},
    macos: {name:"macOS",desc:"descMac",format:"DMG",arches:{arm64:"Apple Silicon",x64:"Intel"}},
    linux: {name:"Linux",desc:"descLinux",format:"AppImage",arches:{x64:"Linux x86_64",arm64:"Linux ARM64"}},
    android: {name:"Android",desc:"descAndroid",format:"APK",arches:{arm64:"Android ARM64-v8a"}},
    ios: {name:"iOS / iPadOS",desc:"descIos",format:"—",arches:{}}
  });
  // Explicit filenames observed in the existing GitHub v1.0.512 Release.
  // Never build a live installer link from an untrusted catalog filename.
  const names = Object.freeze({
    windows:{x64:"GenyCaster-v1.0.512-windows-x86_64.exe",arm64:"GenyCaster-v1.0.512-windows-arm64.exe"},
    macos:{arm64:"GenyCaster-v1.0.512-macos-arm64-adhoc-UNNOTARIZED.dmg",x64:"GenyCaster-v1.0.512-macos-x86_64-adhoc-UNNOTARIZED.dmg"},
    linux:{x64:"GenyCaster-v1.0.512-linux-x86_64.AppImage",arm64:"GenyCaster-v1.0.512-linux-arm64.AppImage"},
    android:{arm64:"GenyCaster-v1.0.512-android-arm64-v8a.apk"}
  });
  const debNames = Object.freeze({
    x64:"GenyCaster-v1.0.512-linux-x86_64.deb",
    arm64:"GenyCaster-v1.0.512-linux-arm64.deb"
  });
  const copy = Object.freeze({
      skip:"Skip to downloads",webLink:"Open web app",eyebrow:"GENYCASTER / PUBLIC RELEASE",
      headlineFirst:"Your GenyCaster.",headlineSecond:"Any device.",intro:"Pick your platform, choose the processor, and download. No account needed to get the app.",
      free:"Free to download",open:"Open source",native:"Native apps",sectionKicker:"01 / GET THE APP",choose:"Choose your platform",soon:"Later",
      selected:"SELECTED PLATFORM",processor:"Processor / architecture",checking:"Checking release",
      readyTitle:"Made for your device.",readyDescription:"The right build, straight from our GitHub Release.",
      webTitle:"Prefer the browser?",webDescription:"Open GenyCaster instantly, without installing anything.",webAction:"Launch web app ↗",
      verification:"Files come from the official GitHub Release. Verify downloads with SHA-256 checksums.",
      checksum:"Checksums ↗",about:"About",release:"View release ↗",
      detected:"Detected",available:"Available to download",unavailable:"Download temporarily unavailable",iosUnavailable:"No iOS release yet. Try the web app.",
      download:"Download for",releaseUnavailable:"Installer verification unavailable. See the official GitHub Release.",
      defaultArch:"Select the processor that matches your device.",macArch:"Choose Apple Silicon (M-series) or Intel in About This Mac.",
      linuxAlt:"Ubuntu / Debian (.deb)",otherFormat:"Other format:",format:"INSTALLER",processorInfo:"ARCHITECTURE",
      descWindows:"Desktop installer for Windows 64-bit.",descMac:"Desktop build for macOS 15 or later.",descLinux:"AppImage for Linux desktops, with a Debian package option.",descAndroid:"64-bit Android APK, installed directly.",descIos:"The iPhone and iPad edition is not published yet.",
      warningMac:"macOS builds use ad-hoc signing and are not Apple notarized. macOS may require approval in Privacy & Security.",
      warningAndroid:"Android APK installs outside the Play Store. Only install the file from the verified GitHub Release.",
      statusWait:"Checking verified public release…"
  });
  let platform = "windows";
  let detected = null;
  const chosen = Object.create(null);
  let releases = Object.create(null);
  let loaded = false;
  let failed = false;

  function dict(key) { return copy[key] || key; }
  function detectOS() {
    const ua = String(navigator.userAgent || "").toLowerCase();
    const hint = String(navigator.userAgentData?.platform ||
                      navigator.platform || "").toLowerCase();
    if (ua.includes("android") || hint.includes("android")) return "android";
    if (/iphone|ipad|ipod/.test(ua) || (hint.includes("mac") &&
        navigator.maxTouchPoints > 1)) return "ios";
    if (hint.includes("win") || ua.includes("windows")) return "windows";
    if (hint.includes("mac") || ua.includes("macintosh")) return "macos";
    if (hint.includes("linux") || ua.includes("linux")) return "linux";
    return null;
  }
  function verifiedRecord(record, os, arch) {
    if (!record || typeof record !== "object" || record.version !== version ||
        !/^[a-f0-9]{64}$/.test(record.sha256 || "")) return null;
    const filename = names[os]?.[arch];
    if (!filename || typeof record.url !== "string") return null;
    const exact = releasePrefix + filename;
    if (record.url !== exact) return null;
    if (os === "macos" && record.distribution !== "ad-hoc-unnotarized") return null;
    try {
      const url = new URL(record.url);
      if (url.protocol !== "https:" || url.origin !== "https://github.com" ||
          url.username || url.password || url.hash || url.search) return null;
    } catch (_) { return null; }
    return Object.freeze({url:exact,sha256:record.sha256,version:record.version});
  }
  function preferredArch() {
    const options = supported[platform].arches;
    const arch = chosen[platform];
    return arch && Object.hasOwn(options,arch) ? arch : Object.keys(options)[0] || null;
  }
  function render() {
    const info = supported[platform];
    if (!info) return;
    document.querySelectorAll("[data-platform]").forEach(button => {
      button.setAttribute("aria-pressed", String(button.dataset.platform === platform));
      const label = button.querySelector?.("[data-version]");
      if (!label) return;
      const versions = Object.values(releases[button.dataset.platform] || {});
      const available = versions.find(record => record?.version === version);
      label.textContent = !loaded ? "Checking…" :
        available ? "v" + available.version : "Not available";
    });
    $("selected-name").textContent = info.name;
    // CSS SVG mask picks the platform silhouette while theme tokens own its color.
    $("selected-icon").dataset.platform = platform;
    $("selected-description").textContent = dict(info.desc);
    const auto = Boolean(detected);
    $("detected").hidden = !auto;
    if (auto) $("detected-text").textContent = dict("detected") + ": " + supported[detected].name;
    const arch = preferredArch();
    $("arch-block").hidden = !arch;
    const options = $("architectures");
    options.replaceChildren();
    for (const [key,label] of Object.entries(info.arches)) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "arch-button";
      button.textContent = label;
      button.dataset.arch = key;
      button.setAttribute("aria-pressed",String(key===arch));
      options.append(button);
    }
    $("arch-tip").textContent = platform === "macos" ? dict("macArch") : dict("defaultArch");
    const entry = arch && releases[platform]?.[arch];
    const available = Boolean(entry);
    const ready = loaded && available;
    $("selected-version").textContent = ready ? "v" + entry.version :
      !loaded ? "Checking…" : "Not available";
    $("release-number").textContent = ready ? "v" + entry.version + " · Stable" : "—";
    $("file-name").textContent = ready ? entry.url.slice(releasePrefix.length) :
      platform === "ios" ? "No iOS build" : "Installer not available";
    $("file-hash").textContent = ready ? entry.sha256.slice(0,16) + "…" : "—";
    $("file-hash").setAttribute("title",ready ? entry.sha256 : "");
    $("release-bullet").classList.toggle("available",ready);
    $("release-label").textContent = platform === "ios" ? dict("unavailable") :
      !loaded ? dict("checking") : ready ? dict("available") : dict("unavailable");
    $("format-text").textContent = info.format;
    $("arch-text").textContent = arch ? info.arches[arch] : "—";
    const main = $("download-primary");
    main.hidden = !ready;
    if (ready) {
      main.href = entry.url;
      $("primary-text").textContent = "Download " + info.name + " · v" + entry.version;
      main.setAttribute("aria-label",
        "Download GenyCaster " + info.name + " " + info.arches[arch] +
        " version " + entry.version);
    } else {
      main.removeAttribute("href");
    }
    const pending = $("download-pending");
    pending.hidden = ready;
    if (!ready) pending.textContent = platform === "ios" ? dict("iosUnavailable") :
      failed ? dict("releaseUnavailable") : dict("statusWait");
    const alternatives = $("alternatives");
    alternatives.replaceChildren();
    alternatives.hidden = true;
    if (ready && platform === "linux") {
      const altName = debNames[arch];
      const alt = document.createElement("a");
      alt.href = releasePrefix + altName;
      alt.target = "_blank";
      alt.rel = "noopener noreferrer";
      alt.textContent = dict("linuxAlt") + " ↗";
      const label = document.createElement("span");
      label.textContent = dict("otherFormat");
      alternatives.append(label,alt);
      alternatives.hidden = false;
    }
    const trust = $("trust-notice");
    const trustNeeded = ready && (platform === "macos" || platform === "android");
    trust.hidden = !trustNeeded;
    if (trustNeeded) $("trust-text").textContent =
      dict(platform === "macos" ? "warningMac" : "warningAndroid");
    mountShapes();
  }


  // Match the cubic geometry of ui/components/visual/Superellipse.qml
  // (Qt high-quality path, exponent=4) rather than relying on browser
  // corner-shape, which is inconsistent across current engines.
  const shapes = new WeakMap();
  const shapeTargets = Object.freeze([
    [".platform", 19], [".arch-button",14], [".download-panel",30],
    [".selected-logo",20], [".release-side",23], [".side-graphic",19],
    [".primary-download",17], [".web-card",22], [".web-action",15],
    [".version",16], [".intro-tags span",15], [".selected-version",12],
    [".release-number",15], [".trust-notice",16]
  ]);
  const svgNamespace = "http://www.w3.org/2000/svg";
  const shapeObserver = typeof ResizeObserver === "function" ?
    new ResizeObserver(entries => {
      for (const entry of entries) drawShape(entry.target);
    }) : null;

  function superellipsePath(width, height, depth) {
    if (!(width > 0 && height > 0)) return "";
    // Same path inset and physical-pixel guard as Qt's Shape renderer.
    const inset = .5;
    const right = Math.max(inset,width - inset);
    const bottom = Math.max(inset,height - inset);
    const edgeGuard = .25 / Math.max(1,window.devicePixelRatio || 1);
    const radius = Math.max(0, Math.min(
      depth - inset, (right-inset)/2 - edgeGuard,
      (bottom-inset)/2 - edgeGuard
    ));
    const safeExponent = 4.0;
    const diagonal = Math.pow(.5, 1 / safeExponent);
    const cubicFactor = Math.max(.5522847498, Math.min(.985,(8 * diagonal - 4) / 3));
    const r = radius, k = cubicFactor, l = inset, t = inset, b = bottom, x = right;
    const f = n => Number(n.toFixed(3));
    return [
      "M",f(l+r),f(t),
      "L",f(x-r),f(t),
      "C",f(x-r+k*r),f(t),f(x),f(t+r-k*r),f(x),f(t+r),
      "L",f(x),f(b-r),
      "C",f(x),f(b-r+k*r),f(x-r+k*r),f(b),f(x-r),f(b),
      "L",f(l+r),f(b),
      "C",f(l+r-k*r),f(b),f(l),f(b-r+k*r),f(l),f(b-r),
      "L",f(l),f(t+r),
      "C",f(l),f(t+r-k*r),f(l+r-k*r),f(t),f(l+r),f(t),
      "Z"
    ].join(" ");
  }
  function drawShape(element) {
    const entry = shapes.get(element);
    if (!entry) return;
    const width = element.clientWidth, height = element.clientHeight;
    if (!(width > 0 && height > 0)) return;
    const key = width + ":" + height;
    if (entry.last === key) return;
    entry.last = key;
    entry.svg.setAttribute("viewBox","0 0 " + width + " " + height);
    entry.path.setAttribute("d",superellipsePath(width,height,entry.depth));
  }
  function mountShapes() {
    if (typeof document.createElementNS !== "function") return;
    for (const [selector,depth] of shapeTargets) {
      for (const element of document.querySelectorAll(selector)) {
        if (shapes.has(element)) continue;
        const svg = document.createElementNS(svgNamespace,"svg");
        const path = document.createElementNS(svgNamespace,"path");
        svg.setAttribute("aria-hidden","true");
        svg.setAttribute("focusable","false");
        svg.setAttribute("preserveAspectRatio","none");
        svg.classList.add("gs-shape");
        path.setAttribute("vector-effect","non-scaling-stroke");
        path.setAttribute("stroke-linejoin","round");
        svg.append(path);
        element.prepend(svg);
        shapes.set(element,{svg,path,depth,last:""});
        element.classList.add("gs-shaped");
        element.dataset.shapeReady = "true";
        shapeObserver?.observe(element);
        drawShape(element);
      }
    }
  }

  async function loadRelease() {
    try {
      const response = await fetch("/releases/native.json",
        {cache:"no-store",credentials:"omit",redirect:"error"});
      if (!response.ok || Number(response.headers.get("content-length") || 0) > 32768)
        throw new Error("Unverified public release catalog");
      const raw = await response.text();
      if (raw.length > 32768) throw new Error("Catalog too large");
      const data = JSON.parse(raw);
      if (data.schema !== 1 || !data.platforms ||
          typeof data.platforms !== "object" || Array.isArray(data.platforms))
        throw new Error("Unexpected release catalog");
      const safe = Object.create(null);
      for (const [os, info] of Object.entries(supported)) {
        safe[os] = Object.create(null);
        for (const arch of Object.keys(info.arches)) {
          const entry = verifiedRecord(data.platforms[os]?.[arch],os,arch);
          if (entry) safe[os][arch] = entry;
        }
      }
      releases = safe;
      loaded = true;
    } catch (_) {
      // Fail closed on missing or malformed release metadata.
      failed = true;
      loaded = true;
    }
    render();
  }

  detected = detectOS();
  if (detected) platform = detected;
  $("platform-list").addEventListener("click", (event) => {
    const button = event.target.closest("button[data-platform]");
    if (!button || !supported[button.dataset.platform]) return;
    platform = button.dataset.platform;
    render();
  });
  $("architectures").addEventListener("click", (event) => {
    const button = event.target.closest("button[data-arch]");
    if (!button || !Object.hasOwn(supported[platform].arches,button.dataset.arch)) return;
    chosen[platform] = button.dataset.arch;
    render();
  });
  render();
  loadRelease();
})();
