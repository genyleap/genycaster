# Update delivery and verification

GenyCaster's update manager uses a fail-closed release discovery and
verification pipeline designed for its native platforms.

- Asynchronous, quiet check after startup and every 24h, with opt-out in
  Settings > Updates and an explicit Check now button.
- Official GitHub Releases `/repos/genyleap/genycaster/releases/latest`;
  no arbitrary update origin. Draft/prerelease metadata is rejected.
- Semantic version comparison against the CMake project version built into
  the executable; no downgrade and no rollback to older versions.
- Only native platform-matching release assets are selected. A complete
  SHA-256 from GitHub's asset `digest` is required before enabling the
  download (missing checksum => release-page-only, never execute).
- Download into app-local `.partial` file; stream hash, verify, atomically
  rename, and only then allow the user to open the installer.
- No silent execution of downloaded installers. The OS installation / package
  manager is deliberately invoked by user action; self-replacing running
  binaries would require a separately signed, platform-specific helper
  before automatic replacement could be considered safe.
- iOS and WebAssembly have no desktop package install action; users can visit
  releases. Signing/codesign/notarization checks and platform updater helpers
  should be added to the packaging pipeline before describing silent updates
  as supported.

Security limitations: the digest is delivered through GitHub's release
metadata (TLS); it detects mismatches but is not an independent signed
supply-chain attestation. Release automation should sign packages and publish
checksums. Never download executables merely by constructing a tag URL.
