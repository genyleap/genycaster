import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const read = file => readFileSync(file, 'utf8');

test('native OpenProof-enabled composition routes both interactive sign-in choices to OPP', () => {
    const composition = read('ui/app/runtime/composition.cpp');
    const session = read('src/application/session.cpp');
    const adapter = read('src/providers/openproof_auth/auth.cpp');

    // The direct-Farcaster route used to pass the protocol-only compatibility
    // provider even in production builds composed with OpenProof. Never let
    // that silent split return.
    assert.match(composition,
        /primaryAuthentication,\s*[\r\n]+\s*farcasterIdentityResolver,\s*[\r\n]+#if GENYCASTER_HAS_OPENPROOF_AUTH\s*[\s\S]*?runtime->openProofAuthentication\.get\(\)\s*#elif FARCASTER_HAS_FARCASTER_AUTH/);
    assert.match(composition,
        /#elif FARCASTER_HAS_FARCASTER_AUTH[\s\S]*?runtime->farcasterAuthentication\.get\(\)[\s\S]*?#else\s*nullptr/);
    assert.match(session, /request\.route == capability::auth::SignInRoute::Farcaster/);
    assert.match(adapter,
        /return request\.route == capability::auth::SignInRoute::OpenProof\s*\?\s*beginOpenProof\(\)\s*:\s*beginFarcaster\(\)/);
    assert.match(adapter, /"\/auth\/web3\/complete"/);
    assert.match(adapter, /loadCookieAccount\(/);
});

test('native CREATE FID remains protocol-only until a separately verified OPP grant exists', () => {
    const bindings = read('ui/app/runtime/bindings.cpp');
    const session = read('src/application/session.cpp');

    // This existing native registration intentionally must not fabricate a
    // canonical OpenProof subject from a bare custody/FID tuple.
    assert.match(bindings, /HyperSnapRegistrationPublisher::nativeAccountConfirmed/);
    assert.match(bindings, /NeynarRegistrationController::farcasterFidConfirmed/);
    assert.match(bindings, /auto commitVerifiedIdentity = [\s\S]*?const std::string issuer = farcaster/);
    assert.match(bindings,
        /const std::string issuer = farcaster\s*\?\s*"farcaster:optimism-id-registry"\s*:\s*"hypersnap:native-registry"/);
    assert.match(bindings, /acceptVerifiedNativeCustody\(/);
    assert.match(bindings, /acceptVerifiedFarcasterCustody\(/);
    assert.match(session, /constexpr std::string_view NativeIssuer = "hypersnap:native-registry"/);
    assert.match(session, /constexpr std::string_view FarcasterIssuer =\s*"farcaster:optimism-id-registry"/);
    assert.match(session, /"native-fid:" \+ std::to_string\(session\.fid\.value\)/);
});

test('browser OpenProof routes require verified subject; device-only continuity is not OPP', () => {
    const browser = read('ui/app/identity/browser_auth.cpp');
    const bindings = read('ui/app/runtime/bindings.cpp');
    const policy = read('cmake/platform/WebAssembly/configure.cmake');
    assert.match(browser, /BrowserOpenProofAuth::beginFarcaster\(/);
    assert.match(browser, /"\/v1\/browser\/farcaster\/poll"/);
    assert.match(browser, /m_subject = object\.value\(QStringLiteral\("sub"\)\)/);
    assert.match(bindings, /const bool canonicalOpenProof = !nativeHyperSnap && !restoredDevice/);
    assert.match(bindings, /nativeHyperSnap\s*\? std::string\{"hypersnap:native-registry"\}/);
    assert.match(bindings, /std::string\{"genycaster:device-local"\}/);
    assert.match(bindings, /\.identityIssuer = std::string\{genycaster::deployment::openProofIssuer\}/);
    assert.match(policy, /set\(GENYCASTER_ENABLE_OPENPROOF_AUTH OFF CACHE BOOL "" FORCE\)/);
});

test('OPP enrollment is never conflated with an app-service session or protocol signing', () => {
    const guide = read('docs/OPENPROOF_AND_SERVICES.md');
    const roadmap = read('docs/12-roadmap.md');
    const account = read('services/account-sync/README.md');
    assert.match(guide, /Only then is a canonical OpenProof-backed session saved/);
    assert.match(guide, /no HyperSnap-native/);
    assert.match(guide, /Retry repeats only the identity challenge/);
    assert.match(roadmap, /Cross-protocol identity convergence remains partial/);
    assert.match(account, /Genycaster.*own[\s\S]*PostgreSQL database/);
});
