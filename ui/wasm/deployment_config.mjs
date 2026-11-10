// Source-tree test fallback. Shipping WASM replaces this with the validated,
// CMake-generated module; never put credentials or signing keys here.
export const deployment = Object.freeze({
  publicOrigin: 'https://caster.genyleap.com',
  siwfOrigin: 'https://genycaster.genyleap.com',
  openProofIssuer: 'https://openproof.genyleap.com',
  openProofBrowserBase: 'https://genyleap.com/openproof',
  browserGateway: 'https://storage-farcaster.genyleap.com',
  mediaPublicOrigin: 'https://media-farcaster.genyleap.com',
  snapchainOrigin: 'https://snapchain.genyleap.com',
  hypersnapOrigin: 'https://haatz.quilibrium.com',
  companyOrigin: 'https://genyleap.com',
});
