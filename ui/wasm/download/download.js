/* GenyCaster download hub: strict release manifest and explicit architectures. */
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const version = "1.0.0";
  const releasePrefix = "https://github.com/genyleap/genycaster/releases/download/v1.0.0/";
  const releasePage = "https://github.com/genyleap/genycaster/releases/tag/v1.0.0";
  const supported = Object.freeze({
    windows: {name:"Windows", desc:"descWindows", format:"EXE", arches:{x64:"Windows x64",arm64:"Windows ARM64"}},
    macos: {name:"macOS",desc:"descMac",format:"DMG",arches:{arm64:"Apple Silicon",x64:"Intel"}},
    linux: {name:"Linux",desc:"descLinux",format:"AppImage",arches:{x64:"Linux x86_64",arm64:"Linux ARM64"}},
    android: {name:"Android",desc:"descAndroid",format:"APK",arches:{arm64:"Android ARM64-v8a"}},
    ios: {name:"iOS / iPadOS",desc:"descIos",format:"—",arches:{}}
  });
  // Explicit filenames observed in the existing GitHub v1.0.0 Release.
  // Never build a live installer link from an untrusted catalog filename.
  const names = Object.freeze({
    windows:{x64:"GenyCaster-v1.0.0-windows-x86_64.exe",arm64:"GenyCaster-v1.0.0-windows-arm64.exe"},
    macos:{arm64:"GenyCaster-v1.0.0-macos-arm64-adhoc-UNNOTARIZED.dmg",x64:"GenyCaster-v1.0.0-macos-x86_64-adhoc-UNNOTARIZED.dmg"},
    linux:{x64:"GenyCaster-v1.0.0-linux-x86_64.AppImage",arm64:"GenyCaster-v1.0.0-linux-arm64.AppImage"},
    android:{arm64:"GenyCaster-v1.0.0-android-arm64-v8a.apk"}
  });
  const debNames = Object.freeze({
    x64:"GenyCaster-v1.0.0-linux-x86_64.deb",
    arm64:"GenyCaster-v1.0.0-linux-arm64.deb"
  });
  const copy = {
    en:{
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
    },
    fa:{
      skip:"رفتن به بخش دریافت",webLink:"نسخهٔ وب",eyebrow:"جنی‌کستر / انتشار عمومی",
      headlineFirst:"جنی‌کستر شما.",headlineSecond:"روی هر دستگاه.",intro:"سیستم‌عامل و معماری دستگاه را انتخاب کنید و مستقیم دریافت کنید.",
      free:"دریافت رایگان",open:"متن‌باز",native:"نسخه‌های بومی",sectionKicker:"۰۱ / دریافت برنامه",choose:"انتخاب سیستم‌عامل",soon:"بعداً",
      selected:"سیستم‌عامل انتخاب‌شده",processor:"معماری پردازنده",checking:"بررسی نسخه",
      readyTitle:"متناسب با دستگاه شما.",readyDescription:"فایل مناسب، مستقیم از انتشار رسمی گیت‌هاب.",
      webTitle:"مرورگر را ترجیح می‌دهید؟",webDescription:"بدون نصب، جنی‌کستر را همین حالا باز کنید.",webAction:"اجرای نسخهٔ وب ↗",
      verification:"فایل‌ها از انتشار رسمی گیت‌هاب هستند. با چک‌سام SHA-256 می‌توانید صحت آن‌ها را بررسی کنید.",
      checksum:"چک‌سام‌ها ↗",about:"درباره",release:"مشاهدهٔ انتشار ↗",
      detected:"شناسایی شد",available:"آمادهٔ دریافت",unavailable:"دریافت موقتاً در دسترس نیست",iosUnavailable:"نسخهٔ iOS هنوز منتشر نشده؛ از نسخهٔ وب استفاده کنید.",
      download:"دریافت برای",releaseUnavailable:"تأیید فایل نصب امکان‌پذیر نیست. انتشار رسمی را در گیت‌هاب ببینید.",
      defaultArch:"معماری متناسب با دستگاهتان را انتخاب کنید.",macArch:"در About This Mac مشخص کنید پردازندهٔ مک Apple Silicon است یا Intel.",
      linuxAlt:"اوبونتو / دبیان (.deb)",otherFormat:"فرمت دیگر:",format:"فایل نصب",processorInfo:"معماری",
      descWindows:"فایل نصب دسکتاپ ویندوز ۶۴ بیتی.",descMac:"نسخهٔ دسکتاپ برای macOS 15 و بالاتر.",descLinux:"AppImage برای لینوکس، به‌همراه بستهٔ دبیان.",descAndroid:"فایل APK اندروید ۶۴ بیتی برای نصب مستقیم.",descIos:"نسخهٔ آیفون و آیپد هنوز منتشر نشده است.",
      warningMac:"نسخهٔ مک امضای ad-hoc دارد و تأیید رسمی Apple Notarization ندارد. ممکن است نیاز به تأیید در Privacy & Security باشد.",
      warningAndroid:"فایل APK خارج از پلی‌استور نصب می‌شود. فقط فایل انتشار رسمی گیت‌هاب را نصب کنید.",
      statusWait:"در حال بررسی انتشار رسمی…"
    }
  };
  let language = "en";
  let platform = "windows";
  let detected = null;
  const chosen = Object.create(null);
  let releases = Object.create(null);
  let loaded = false;
  let failed = false;

  function dict(key) { return (copy[language] || copy.en)[key] || copy.en[key] || key; }
  function storedLanguage() {
    try {
      const remembered = localStorage.getItem("genycaster.site.language");
      if (remembered === "fa" || remembered === "en") return remembered;
    } catch (_) { /* user may disable localStorage */ }
    return /^fa(?:-|$)/i.test(navigator.language || "") ? "fa" : "en";
  }
  function setLanguage(next) {
    language = next === "fa" ? "fa" : "en";
    document.documentElement.lang = language;
    document.documentElement.dir = language === "fa" ? "rtl" : "ltr";
    document.querySelectorAll("[data-i18n]").forEach(element => {
      element.textContent = dict(element.dataset.i18n);
    });
    $("language").textContent = language === "fa" ? "EN" : "FA";
    $("language").setAttribute("aria-label",
      language === "fa" ? "Switch language to English" : "تغییر زبان به فارسی");
    render();
  }
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
    return Object.freeze({url:exact,sha256:record.sha256});
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
    });
    $("selected-name").textContent = info.name;
    $("selected-icon").src = "/assets/platforms/" + platform + ".svg?v=1";
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
    $("release-bullet").classList.toggle("available",ready);
    $("release-label").textContent = platform === "ios" ? dict("unavailable") :
      !loaded ? dict("checking") : ready ? dict("available") : dict("unavailable");
    $("format-text").textContent = info.format;
    $("arch-text").textContent = arch ? info.arches[arch] : "—";
    const main = $("download-primary");
    main.hidden = !ready;
    if (ready) {
      main.href = entry.url;
      $("primary-text").textContent = dict("download") + " " + info.name;
      main.setAttribute("aria-label",dict("download") + " " + info.name + " · " + info.arches[arch]);
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
  $("language").addEventListener("click", () => {
    const next = language === "fa" ? "en" : "fa";
    try { localStorage.setItem("genycaster.site.language",next); } catch (_) {}
    setLanguage(next);
  });
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
  setLanguage(storedLanguage());
  loadRelease();
})();
