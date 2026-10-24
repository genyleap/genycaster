# Genycaster Docs website

Standalone, static developer documentation built for `docs.genycaster.xyz`.
Its white navigation layout follows the information architecture of `docs.genyleap.com` while using distinct Genycaster branding. It contains no WebAssembly bundle, no customer account data and no app login route.

## Contents
- Overview and capabilities
- Balaji's $2048 Farcaster client challenge: acceptance matrix and read-node blueprint
- Architecture and read/write diagrams
- Provider and client switching (distinct topics)
- Build from source, operation/production reference, candid status report
- Machine-readable `llms.txt`, sitemap and robots

## Local preview
```sh
cd docs-site
python3 -m http.server 8420
# open http://127.0.0.1:8420/
```
No runtime dependencies or external assets. Native browser JavaScript handles documentation navigation and search.

## Deployment
Serve these files from a dedicated static Nginx root for `docs.genycaster.xyz`, with HTTPS and a valid certificate that covers the exact hostname. `docs.genycaster.xyz` must point to the intended server through the DNS provider; use HTTP-01 or DNS-01 ACME validation as appropriate before enabling TLS.
Recommended deployment: publish a validated release directory, then atomically change the documentation-only `current` symlink. Never point the documentation vhost to `/var/www/caster.genyleap.com` or the WASM bundle.
Stage a committed, verified release with `python3 docs-site/deploy/release.py`.

The HTTP and TLS configuration templates are in `deploy/nginx-http-bootstrap.conf` and `deploy/nginx-tls.conf`. The domain requires a matching DNS record, a certificate explicitly valid for docs.genycaster.xyz and an administrator to enable the Nginx virtual host. The existing parent-domain certificate does not cover this subdomain. The docs application does not share the Genycaster WASM root.

Administrative sequence (review before running):

```sh
sudo sh docs-site/deploy/activate.sh bootstrap
# Confirm the HTTP-01 route is reachable on the requested DNS name.
sudo certbot certonly --webroot -w /var/www/docs.genycaster.xyz/acme -d docs.genycaster.xyz
sudo sh docs-site/deploy/activate.sh tls
curl -I https://docs.genycaster.xyz/
```

Only execute this from the repository on the intended server with authorized access; the Remote Operations connector has `/etc` read-only and cannot issue/install the dedicated certificate.
