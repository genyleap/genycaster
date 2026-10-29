# WebAssembly size optimization

## Production baseline (before optimization)

Build `daf4afa0`, WASM SHA filename `farcaster_client.975b06f2c9cfc85c5438.wasm`:

- Raw: 79,134,243 bytes (75.47 MiB).
- HTTP Brotli response: 31,158,407 bytes (29.72 MiB), measured via `curl -H 'Accept-Encoding: br'` against the published CDN.
- Binary sections: code 33.94 MiB, data 41.14 MiB.
- Pinned `NotoSansCJK-Regular.ttc`: 19,484,784 bytes (18.58 MiB), 10 faces, 44,810 Unicode code points per face.

## Safe, lossless initial optimization

The production client's supported localized CJK UI uses the **JP/KR/SC** regional faces, not TC/HK or the five monospace variants. `ci/optimize_cjk_collection.py` generates a TTC containing precisely JP, KR and SC. It **does not subset glyphs** or change regional shaping and verifies source SHA-256, family names and full code-point mapping. The resulting font is 17,371,156 bytes, **2,113,628 bytes smaller** than the source. Native builds preserve their original configuration.

WASM CMake uses this TTC when `fontTools` is available. Configure with `-DGENYCASTER_CJK_FONTTOOLS_PYTHON=<python-with-fontTools>` to make this deterministic. If the tool is unavailable, use the complete upstream collection; never ship an incomplete font. The checked source SHA-256 is part of the script; neither the font binary nor a fontTools dependency is committed.

The first release builder on the server pins its `fontTools` Python venv and caches the verified compact TTC. The program retains the same canonical resource alias, so no QML or native text-rendering behavior changes.

## Further possibilities, not yet accepted

`wasm-opt -Oz` on the complete 75 MiB linked binary exceeded a 220-second CPU-bound trial on the deployment VPS, and therefore was **not** included in the production build. `-Os` / `-Oz` compiler settings should be A/B measured on an isolated build including startup timings, before deployment. Extracting font faces to lazy-loaded HTTP resources could reduce initial WASM more substantially, but requires explicit loading and offline-cache behavior to ensure arbitrary Chinese, Japanese, and Korean cast texts always render correctly. Do not remove coverage based only on the chosen UI language.

Every performance release needs a verified binary, UI localization/smoke tests, transfer-size measurements from origin and CDN, and rollback-safe deployment.
