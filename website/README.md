# GenyCaster download website

The static, dependency-free landing page at https://genycaster.xyz/ is stored in this directory and deployed to the server's \`/var/www/genycaster.xyz/public/\`.

- \`index.html\`: page structure, SEO tags, genuine GenyCaster brand assets and illustrative (not real-screenshot) app mockup.
- \`site-download.css\`: responsive styling including mobile, RTL and reduced-motion behavior.
- \`site-download.js\`: English/Persian text and safe release-link rendering.
- \`releases.json\`: explicit source of truth for public native installers.

The Web button intentionally opens https://caster.genyleap.com/ (the independently deployed WebAssembly app).

## Publishing native download links

Only publish after each target installer has successfully passed the platform's release checks, is publicly accessible without a private GitHub login, and has been uploaded to a stable HTTPS URL.

1. Distribute the verified artifact from \`https://genycaster.xyz/downloads/…\` or a **public** GitHub Release asset under \`https://github.com/genyleap/genycaster/releases/download/…\`.
2. Update the matching entry in \`releases.json\` with a real version string and a list of objects with \`label\` (for example \`Windows x64 · .exe\`) and \`url\`.
3. Deploy \`releases.json\` to the same site, then verify that each actual URL responds successfully and serves the expected artifact.
4. The page automatically shows only approved release links in the manifest; all other targets continue to show their honest preparation status.

**Never:** link GitHub Actions artifacts from private runs, fabricate versions, or conflate a successful CI build with a public release. The script deliberately accepts only HTTPS downloads at the first-party \`/downloads/\` path or the public GenyCaster GitHub Release path; do not widen the allowlist casually.

The live server uses Nginx with a restrictive CSP that must permit \`script-src 'self'\` and \`connect-src 'self'\` for the local script and manifest. External fonts, analytics, runtime libraries and third-party trackers are not required.
