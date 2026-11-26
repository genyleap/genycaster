/*!
 * @file        tests/providers/account_session_principal_tests.cpp
 * @brief       Canonical issuer/subject/fid cookie identity regression.
 */
#include "../../ui/app/identity/account_session_principal.hpp"

#include <QJsonObject>
#include <QUrl>
#include <cstdio>

namespace {

[[nodiscard]] bool check(const bool condition, const char* description)
{
    if (!condition) std::fprintf(stderr, "Failed: %s\n", description);
    return condition;
}

[[nodiscard]] QJsonObject owner(const QString& fid,
                                const QString& issuer,
                                const QString& subject)
{
    return {{QStringLiteral("fid"), fid},
            {QStringLiteral("issuer"), issuer},
            {QStringLiteral("subject"), subject}};
}

} // namespace

int main()
{
    using genycaster::identity::matchesAccountSessionPrincipal;
    const QUrl issuer{QStringLiteral("https://openproof.genyleap.com")};
    const QString verifiedIssuer = issuer.toString(QUrl::FullyEncoded);
    constexpr qulonglong farcasterFid = 321969;
    constexpr qulonglong nativeFid = 4294967296ULL;
    const auto farcaster = owner(QString::number(farcasterFid), verifiedIssuer,
                                QStringLiteral("farcaster:321969"));
    const auto hypersnap = owner(QString::number(nativeFid),
                                 QStringLiteral("hypersnap:native-registry"),
                                 QStringLiteral("hypersnap:4294967296"));
    bool pass = true;
    pass &= check(matchesAccountSessionPrincipal(farcaster, farcasterFid, issuer, QStringLiteral("farcaster")),
                  "verified Farcaster principal accepted");
    pass &= check(matchesAccountSessionPrincipal(hypersnap, nativeFid, issuer, QStringLiteral("hypersnap")),
                  "verified protocol-only HyperSnap principal accepted");
    pass &= check(!matchesAccountSessionPrincipal(farcaster, nativeFid, issuer, QStringLiteral("hypersnap")),
                  "different numeric FID cannot inherit Farcaster identity");
    pass &= check(!matchesAccountSessionPrincipal(hypersnap, farcasterFid, issuer, QStringLiteral("farcaster")),
                  "different numeric FID cannot inherit HyperSnap identity");
    pass &= check(!matchesAccountSessionPrincipal(
                      owner(QString::number(farcasterFid),
                            QStringLiteral("hypersnap:native-registry"),
                            QStringLiteral("hypersnap:321969")), farcasterFid, issuer, QStringLiteral("farcaster")),
                  "native issuer cannot borrow a Farcaster-range numeric FID");
    pass &= check(!matchesAccountSessionPrincipal(
                      owner(QString::number(farcasterFid),
                            QStringLiteral("hypersnap:native-registry"),
                            QStringLiteral("farcaster:321969")), farcasterFid, issuer, QStringLiteral("farcaster")),
                  "issuer and protocol subject cannot be mixed");
    pass &= check(!matchesAccountSessionPrincipal(
                      owner(QString::number(farcasterFid), verifiedIssuer,
                            QStringLiteral("hypersnap:321969")), farcasterFid, issuer, QStringLiteral("farcaster")),
                  "an OpenProof issuer never upgrades a native subject");
    pass &= check(!matchesAccountSessionPrincipal(
                      owner(QString::number(farcasterFid),
                            QStringLiteral("https://attacker.invalid"),
                            QStringLiteral("farcaster:321969")), farcasterFid, issuer, QStringLiteral("farcaster")),
                  "untrusted issuer rejected");
    pass &= check(!matchesAccountSessionPrincipal(
                      owner(QStringLiteral("0321969"), verifiedIssuer,
                            QStringLiteral("farcaster:321969")), farcasterFid, issuer, QStringLiteral("farcaster")),
                  "noncanonical FID encoding rejected");
    pass &= check(!matchesAccountSessionPrincipal(farcaster, 0, issuer, QStringLiteral("farcaster")),
                  "zero FID rejected");
    pass &= check(!matchesAccountSessionPrincipal(
                      {{QStringLiteral("fid"), 321969},
                       {QStringLiteral("issuer"), verifiedIssuer},
                       {QStringLiteral("subject"), QStringLiteral("farcaster:321969")}},
                      farcasterFid, issuer, QStringLiteral("farcaster")),
                  "lossy JSON-number FID cannot enter string-only identity contract");
    pass &= check(!matchesAccountSessionPrincipal(hypersnap,nativeFid,issuer,QStringLiteral("farcaster")),
                  "same numeric FID in different protocol cannot inherit private session");
    pass &= check(!matchesAccountSessionPrincipal(owner(QString::number(nativeFid),verifiedIssuer,
        QStringLiteral("farcaster:4294967296")),nativeFid,issuer,QStringLiteral("hypersnap")),
        "Farcaster issuer cannot authenticate a native viewer");
    return pass ? 0 : 1;
}
