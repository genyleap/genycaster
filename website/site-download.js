"use strict";
(() => {
  const strings = {
    en: {
      skip: "Skip to content",
      navDownloads: "Downloads", navFeatures: "Why GenyCaster", navWeb: "Open web app", navGetApp: "Get the app ",
      eyebrow: "A NEW HOME FOR YOUR SOCIAL WORLD", heroLine1: "Your social world.", heroLine2: "One place.",
      heroDescription: "Farcaster conversations, discovery, and Mini Apps — connected in a native, local-first experience. Your space, your pace, your choice of protocol.",
      heroDownload: "Explore downloads", heroTry: "Try in your browser", metaNative: "Native experience", metaLocal: "Local-first", metaProtocol: "Multi-protocol",
      previewNote: "INTERFACE CONCEPT · FOR ILLUSTRATION", sectionApp: "THE APP",
      downloadsHeading: "Find your GenyCaster.", downloadsDescription: "On your desk, in your pocket, or right in your browser. Choose the experience that fits you.",
      releaseLegend: "Downloads reflect verified public releases", availableNow: "AVAILABLE NOW",
      webDescription: "Your social world in a browser. No installation required.", launchWeb: "Launch web app",
      nativeHeading: "Native applications", nativeDescription: "Installers will appear as soon as they are ready for public download.",
      pending: "In preparation", noRelease: "Public download not yet available", released: "Available to download", yourDevice: "Your device",
      windowsDescription: "A focused desktop experience, built for Windows.",
      macDescription: "Native windowing and a smooth, familiar Mac feel.",
      linuxDescription: "An open desktop, with an app that belongs there.",
      androidDescription: "Made for touch, ready to move with you.",
      iosDescription: "A mobile-first social space for iPhone.",
      comingNext: "More ways to connect.", comingNextDescription: "We're bringing GenyCaster to more screens. Check back here for public builds and supported formats.",
      useWebInstead: "Use the web app today",
      downloadNote: "Only publicly available, verified release files will be linked here. Private CI artifacts are not public installers.",
      sectionDifferent: "BUILT DIFFERENT", experienceHeading1: "Less friction.", experienceHeading2: "More connection.",
      experienceDescription: "A thoughtfully connected social workspace, with the freedom to make it yours.",
      feature1Heading: "Your conversations, closer.", feature1Description: "Feeds, profiles, channels, notifications and Mini Apps, together in one familiar space.",
      feature2Heading: "Local-first by design.", feature2Description: "Cached content can be available while fresh data arrives, making every return feel more natural.",
      feature3Heading: "Your protocol, your choice.", feature3Description: "HyperSnap is preferred by default, while compatible Snapchain routes remain selectable.",
      closingKicker: "THE NEXT CHAPTER IS YOURS", closingHeading: "A social space that feels like home.",
      closingDescription: "Start in the browser today. Discover native releases here when they're available.",
      seePlatforms: "See all platforms", footerMade: "A Genyleap project", download: "Download"
    },
    fa: {
      skip: "رفتن به محتوای اصلی",
      navDownloads: "دریافت", navFeatures: "چرا جنی‌کستر؟", navWeb: "نسخهٔ وب", navGetApp: "دریافت برنامه ",
      eyebrow: "خانه‌ای تازه برای دنیای اجتماعی شما", heroLine1: "دنیای اجتماعی شما.", heroLine2: "در یک جا.",
      heroDescription: "گفت‌وگوهای فارکستر، اکتشاف و مینی‌اپ‌ها در تجربه‌ای بومی و محلی‌محور کنار هم قرار می‌گیرند. فضای شما، به سبک شما، با پروتکل انتخابی شما.",
      heroDownload: "مشاهدهٔ نسخه‌ها", heroTry: "اجرای نسخهٔ وب", metaNative: "تجربهٔ بومی", metaLocal: "محلی‌محور", metaProtocol: "چندپروتکلی",
      previewNote: "پیش‌نمایش مفهومی رابط کاربری", sectionApp: "برنامه",
      downloadsHeading: "جنی‌کستر برای دستگاه شما.", downloadsDescription: "روی دسکتاپ، در موبایل یا مستقیم در مرورگر؛ نسخهٔ مناسب خودتان را انتخاب کنید.",
      releaseLegend: "فقط نسخه‌های عمومی تأییدشده برای دریافت نمایش داده می‌شوند", availableNow: "همین حالا در دسترس",
      webDescription: "دنیای اجتماعی شما در مرورگر، بدون نیاز به نصب.", launchWeb: "اجرای نسخهٔ وب",
      nativeHeading: "نسخه‌های بومی", nativeDescription: "فایل‌های نصب به‌محض انتشار عمومی در همین صفحه قرار می‌گیرند.",
      pending: "در دست آماده‌سازی", noRelease: "فایل دریافت عمومی هنوز منتشر نشده", released: "آمادهٔ دریافت", yourDevice: "دستگاه شما",
      windowsDescription: "تجربه‌ای متمرکز و متناسب با دسکتاپ ویندوز.",
      macDescription: "پنجره‌های بومی و تجربه‌ای روان و آشنا روی مک.",
      linuxDescription: "برنامه‌ای سازگار با روح دسکتاپ آزاد.",
      androidDescription: "طراحی‌شده برای لمس و استفاده در حرکت.",
      iosDescription: "فضایی اجتماعی با طراحی موبایل‌محور برای آیفون.",
      comingNext: "راه‌های بیشتری برای اتصال.", comingNextDescription: "جنی‌کستر را به نمایشگرهای بیشتری می‌آوریم. انتشارهای عمومی و فرمت‌های پشتیبانی‌شده را اینجا دنبال کنید.",
      useWebInstead: "فعلاً از نسخهٔ وب استفاده کنید",
      downloadNote: "در این صفحه فقط فایل‌های انتشار عمومی و تأییدشده لینک می‌شوند. خروجی‌های خصوصی CI فایل نصب عمومی نیستند.",
      sectionDifferent: "متفاوت از پایه", experienceHeading1: "دردسر کمتر.", experienceHeading2: "ارتباط بیشتر.",
      experienceDescription: "یک فضای اجتماعی یکپارچه که آزادی شخصی‌سازی را در اختیار شما می‌گذارد.",
      feature1Heading: "گفت‌وگوها نزدیک‌تر از همیشه.", feature1Description: "فیدها، پروفایل‌ها، کانال‌ها، اعلان‌ها و مینی‌اپ‌ها در فضایی آشنا کنار هم قرار دارند.",
      feature2Heading: "محلی‌محور از ابتدا.", feature2Description: "محتوای کش‌شده می‌تواند پیش از رسیدن اطلاعات تازه نمایش داده شود تا بازگشت به برنامه روان‌تر باشد.",
      feature3Heading: "پروتکل شما، انتخاب شما.", feature3Description: "هایپراسنپ مسیر پیش‌فرض ترجیحی است و مسیرهای سازگار اسنپ‌چین نیز قابل انتخاب‌اند.",
      closingKicker: "فصل بعدی را شما می‌سازید", closingHeading: "فضایی اجتماعی با حس خانه.",
      closingDescription: "همین امروز از مرورگر آغاز کنید؛ نسخه‌های بومی بعد از انتشار از همین صفحه در دسترس خواهند بود.",
      seePlatforms: "مشاهدهٔ همهٔ نسخه‌ها", footerMade: "پروژه‌ای از جنی‌لیپ", download: "دریافت"
    }
  };

  const supported = ["windows", "macos", "linux", "android", "ios"];
  const cards = new Map(supported.map((key) => [key, document.querySelector('[data-platform="' + key + '"]')]));
  const toggle = document.getElementById("language-toggle");
  let language = "en";
  let releaseData = null;

  function getStoredLanguage() {
    try {
      const value = window.localStorage.getItem("genycaster.site.language");
      if (value === "en" || value === "fa") return value;
    } catch (_) { /* Browsers may block storage. */ }
    return /^fa(?:-|$)/i.test(navigator.language || "") ? "fa" : "en";
  }

  function translate(key) {
    return (strings[language] || strings.en)[key] || strings.en[key] || key;
  }

  function platformFromUserAgent() {
    const platform = (navigator.userAgentData && navigator.userAgentData.platform || navigator.platform || "").toLowerCase();
    const agent = navigator.userAgent.toLowerCase();
    if (platform.includes("android") || agent.includes("android")) return "android";
    if (/iphone|ipad|ipod/.test(agent)) return "ios";
    if (platform.includes("win")) return "windows";
    if (platform.includes("mac")) return "macos";
    if (platform.includes("linux")) return "linux";
    return null;
  }

  function allowedDownloadUrl(rawUrl) {
    if (typeof rawUrl !== "string") return null;
    try {
      const url = new URL(rawUrl, window.location.origin);
      if (url.protocol !== "https:") return null;
      if (url.origin === window.location.origin && url.pathname.startsWith("/downloads/")) return url.href;
      if (url.origin === "https://github.com" && url.pathname.startsWith("/genyleap/genycaster/releases/download/")) return url.href;
    } catch (_) { /* Malformed URLs must not produce download links. */ }
    return null;
  }

  function renderReleases() {
    const guessedPlatform = platformFromUserAgent();
    for (const [name, card] of cards) {
      if (!card) continue;
      card.classList.toggle("recommended", guessedPlatform === name);
      const badge = card.querySelector("[data-badge]");
      const status = card.querySelector("[data-status]");
      const container = card.querySelector("[data-links]");
      container.replaceChildren();
      card.classList.remove("is-available");

      const config = releaseData && releaseData.platforms && releaseData.platforms[name];
      const assets = config && Array.isArray(config.assets) ? config.assets : [];
      let visibleAssets = 0;
      for (const asset of assets) {
        if (!asset || typeof asset.label !== "string" || asset.label.length > 64) continue;
        const url = allowedDownloadUrl(asset.url);
        if (!url) continue;
        const anchor = document.createElement("a");
        anchor.href = url;
        anchor.rel = "noopener noreferrer";
        const label = document.createElement("span");
        label.textContent = translate("download") + " · " + asset.label;
        const arrow = document.createElement("span");
        arrow.textContent = "↓";
        arrow.setAttribute("aria-hidden", "true");
        anchor.append(label, arrow);
        container.append(anchor);
        visibleAssets += 1;
      }

      if (visibleAssets > 0) {
        card.classList.add("is-available");
        badge.textContent = translate("released") + (config.version ? " · " + String(config.version).slice(0, 24) : "");
        status.hidden = true;
      } else {
        badge.textContent = translate("pending");
        status.hidden = false;
        const text = status.querySelector("span");
        text.textContent = translate("noRelease");
      }
    }
  }

  function applyLanguage(nextLanguage) {
    language = nextLanguage === "fa" ? "fa" : "en";
    document.documentElement.lang = language;
    document.documentElement.dir = language === "fa" ? "rtl" : "ltr";
    document.querySelectorAll("[data-i18n]").forEach((element) => {
      element.textContent = translate(element.getAttribute("data-i18n"));
    });
    toggle.textContent = language === "en" ? "FA" : "EN";
    toggle.setAttribute("aria-label", language === "en" ? "تغییر زبان به فارسی" : "Switch language to English");
    renderReleases();
  }

  if (toggle) {
    toggle.addEventListener("click", () => {
      const next = language === "en" ? "fa" : "en";
      try { window.localStorage.setItem("genycaster.site.language", next); } catch (_) { /* Optional preference. */ }
      applyLanguage(next);
    });
  }
  applyLanguage(getStoredLanguage());

  fetch("/releases.json", {cache: "no-store"})
    .then((response) => {
      if (!response.ok) throw new Error("Release manifest unavailable");
      return response.json();
    })
    .then((manifest) => {
      if (!manifest || manifest.schemaVersion !== 1 || !manifest.platforms || typeof manifest.platforms !== "object") return;
      releaseData = manifest;
      renderReleases();
    })
    .catch(() => {
      // A missing manifest never creates a guessed or broken download link.
    });
})();
