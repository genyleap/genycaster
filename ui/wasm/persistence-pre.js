// Executed before the Qt/WebAssembly application enters main().
// Populate the virtual app data directory from IndexedDB before opening SQLite.
// Emscripten's IDBFS is asynchronous, so a run dependency guards startup.
Module['preRun'] = Module['preRun'] || [];

// Serialize browser durability writes. Emscripten IDBFS rejects overlapping
// syncfs calls; a navigation must await the last successful flush rather
// than racing the periodic cache/database persistence timer.
Module['genycasterSyncBrowserData'] = (function () {
    let previous = Promise.resolve();
    return function () {
        const next = previous.then(() => new Promise((resolve, reject) => {
            try {
                FS.syncfs(false, error => error ? reject(error) : resolve());
            } catch (error) {
                reject(error);
            }
        }));
        previous = next.catch(() => {});
        return next;
    };
})();
Module['preRun'].push(function () {
    const directory = '/home/web_user/.local/share';
    try {
        FS.mkdirTree(directory);
        FS.mount(IDBFS, {}, directory);
        addRunDependency('genycaster-idbfs');
        FS.syncfs(true, function (error) {
            if (error) {
                console.error('[Genycaster] IndexedDB restore failed; local writes may not survive reload:', error);
            }
            removeRunDependency('genycaster-idbfs');
        });
    } catch (error) {
        console.error('[Genycaster] Browser persistence unavailable:', error);
        // No bogus durable-writing claim if IDBFS mount is rejected.
    }
});
