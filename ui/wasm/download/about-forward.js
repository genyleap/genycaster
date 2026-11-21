/* Preserve the canonical /download entry while Nginx owns its legacy 302.
 * Runs only on the historic /about landing; no redirect loop. */
(() => {
  "use strict";
  function openDedicatedDownloads() {
    if ((location.pathname === "/about/" || location.pathname === "/about") &&
        location.hash === "#downloads") {
      location.replace("/download/index.html");
    }
  }
  addEventListener("hashchange", openDedicatedDownloads);
  openDedicatedDownloads();
})();
