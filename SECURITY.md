# Security policy

GenyCaster processes social identities, account sessions, signer authorizations, wallet permissions and media from external sources. Security defects should be handled privately.

## Supported versions

| Version / channel | Security status |
|---|---|
| Latest `v1.0.0-rcN` beta/RC release | Actively maintained on a best-effort basis during beta |
| Older preview builds and historical commits | No guaranteed security updates |

A release-candidate identifier does **not** mean the product has completed a third-party audit or is suitable for managing valuable funds. Platform capabilities and security acceptance differ.

## Privately report a vulnerability

1. Open the repository's [Security page](https://github.com/genyleap/genycaster/security).
2. If **Report a vulnerability** is offered, submit a **private vulnerability report** through GitHub's security-advisory flow.
3. If private reporting is not available, create a **non-sensitive** public issue asking maintainers to establish a private contact channel. **Do not** include attack steps, credentials, user information or exploit details in the issue.
4. Do not use a public pull request, discussion or issue for a working exploit.

Include the affected version/commit and platform, minimal reproduction steps, impact, affected trust boundary, and any safe proof-of-concept material. Redact wallet keys, recovery phrases, access tokens, production URLs with credentials and private account data.

Maintainers will triage privately, attempt to reproduce, and coordinate disclosure and fixes. During beta there is no guaranteed response or patch-time SLA. Please avoid actively probing production accounts, infrastructure or third-party services without authorization.

## Security boundaries

### Identity and protocol writes

- A signed-in session is **not** a protocol signer. A device-local sealed identity is not a freshly authenticated provider session or an app-cloud cookie. Private-cloud authorization uses an independently enrolled device key, not the social signer (except the bounded legacy compatibility path during migration). Farcaster signer authorization has separate custody approval, scope and expiry semantics.
- Only an authorized signer may produce protocol writes. Retries must reconcile ambiguous network results to prevent duplicate/conflicting submissions.
- Private signer material stays behind platform-specific secure-vault boundaries on supported native targets. It must not be logged or exposed to QML, Mini Apps or ordinary SQLite records.
- Unsupported signer capabilities must fail closed, not substitute plaintext persistence. Validate the complete protocol-scoped principal, cancel stale results on account switch/logout, and revoke enrolled cloud devices via signed retirement receipts.
- C++ code, JavaScript bridges and platform wallet integrations require independent review when changing signer scope or authority.

### Browser and Mini Apps

- WebAssembly is sandboxed differently from native desktop/mobile; never assume OS Keychain semantics are available in a browser.
- Mini App permissions must be bound to the **requesting origin, account, action, scope and session**, with explicit expiry/revocation rules.
- Prior consent for one Mini App must not silently grant a second origin permissions.
- Treat all embedded URLs, images, links and responses as untrusted; defend against cross-origin confusion, private-network access and malicious redirects.
- Local approvals do not replace cryptographic signatures when the active protocol requires signing.

### Services and transport

- Account synchronization, media handling, node operations and attestation services are separately deployed boundaries; source availability is **not** permission to use a production service.
- Enforce authentication/authorization on the server for sensitive API operations. **CORS is not access control**.
- Never commit deploy secrets, custody keys, signer seeds, service credentials, database passwords or plaintext production configuration.
- Do not disable TLS verification or log sensitive authorization headers for debugging.

## Safe testing and contributions

Write security regression tests with fake/test identities and disposable fixtures. Do not embed real recovery phrases, private keys, bearer tokens or user account records. Prefer narrow tests for origin isolation, signer authorization, protocol message identity, replay protection and fail-closed behavior.

See [Architecture](docs/ARCHITECTURE.md), [Authentication and signing design](docs/08-security-auth-and-signing.md), [Contributing](CONTRIBUTING.md) and the [license](LICENSE).

## Disclosure and updates

When a defect is fixed, security advisories and relevant releases should describe affected ranges, impact, remediation and known limits without exposing users to unnecessary risk. If a maintainer cannot confirm that a weakness is resolved on a given platform, that platform must remain explicitly qualified.

**Never publish a secret to demonstrate a bug.** If one is accidentally exposed, rotate/revoke it promptly; deleting it from the latest Git commit does not remove it from history.
