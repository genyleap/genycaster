/* Fallback for the legacy Nginx 302: /download -> /about/#downloads.
 *
 * Bootstrap runs synchronously in the historical /about head, before layout.
 * It hides the About DOM, loads our dedicated download page into this SAME
 * document, then replaces the visible URL with /download. There is no second
 * navigation to /download/index.html and no flash of the About page.
 *
 * Once the canonical Nginx location is installed, this legacy-only path is
 * bypassed entirely. The actual /about page remains unchanged without the
 * #downloads fragment.
 */
(() => {
  "use strict";
  const onLegacyRoute = () =>
    (location.pathname === "/about/" || location.pathname === "/about") &&
    location.hash === "#downloads";
  if (!onLegacyRoute()) return;

  const root = document.documentElement;
  // Runs in the parser-blocking <head> before the About page can be painted.
  root.style.visibility = "hidden";
  root.style.opacity = "0";
  // Update the address bar immediately, before fetching the download UI.
  // Even slow connections must not expose /about/#downloads to the visitor.
  let canonicalized = false;
  function canonicalize() {
    if (canonicalized) return;
    try {
      history.replaceState(history.state, "", "/download");
      canonicalized = true;
    } catch (_) {
      // History APIs may be restricted in embedded browser environments.
    }
  }
  canonicalize();
  let swapping = false;

  async function mountDownload() {
    if (swapping) return;
    swapping = true;
    try {
      const response = await fetch("/download/index.html", {
        credentials: "same-origin", redirect: "error", cache: "no-store"
      });
      if (!response.ok) throw new Error("Download document unavailable");
      const source = await response.text();
      if (!source.includes('id="download-primary"') ||
          source.length > 120000) throw new Error("Unexpected download document");

      const parsed = new DOMParser().parseFromString(source, "text/html");
      if (!parsed.getElementById("download-primary") ||
          !parsed.getElementById("platform-list")) {
        throw new Error("Missing download controls");
      }
      const cssHref = parsed.querySelector('link[rel="stylesheet"]')?.getAttribute("href");
      const scriptSrc = parsed.querySelector('script[src*="/download/download.js"]')?.getAttribute("src");
      if (cssHref !== "/download/download.css?v=6" ||
          scriptSrc !== "/download/download.js?v=6") {
        throw new Error("Download asset version mismatch");
      }

      const stylesheet = document.createElement("link");
      stylesheet.rel = "stylesheet";
      stylesheet.href = cssHref;
      const cssReady = new Promise((resolve, reject) => {
        stylesheet.onload = resolve;
        stylesheet.onerror = reject;
      });
      document.head.appendChild(stylesheet);
      await cssReady;

      // Retire the historical landing's styles before showing the new body.
      for (const node of document.querySelectorAll('link[rel="stylesheet"]')) {
        if (node !== stylesheet) node.remove();
      }
      document.body.replaceWith(document.importNode(parsed.body, true));
      document.documentElement.lang = parsed.documentElement.lang || "en";
      document.documentElement.dir = "ltr";
      document.title = parsed.title;
      const canonical = document.querySelector('link[rel="canonical"]');
      if (canonical) canonical.href = "https://genycaster.xyz/download";
      const description = document.querySelector('meta[name="description"]');
      if (description) {
        description.content = parsed.querySelector('meta[name="description"]')?.content ||
          "Download GenyCaster for your device.";
      }

      // Scripts in DOMParser documents are intentionally inert. Start only
      // the trusted download controller after the controls are in the DOM.
      const script = document.createElement("script");
      script.src = scriptSrc;
      script.onload = () => {
        canonicalize();
        root.style.visibility = "";
        root.style.opacity = "";
        root.removeAttribute("data-genycaster-download-routing");
      };
      script.onerror = () => location.replace("/download/index.html");
      document.head.appendChild(script);
    } catch (_) {
      // This is fail-safe. A broken inline route must not strand the user.
      location.replace("/download/index.html");
    }
  }

  root.setAttribute("data-genycaster-download-routing", "true");
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", mountDownload, {once: true});
  } else {
    mountDownload();
  }
})();
