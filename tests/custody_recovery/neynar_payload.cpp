#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <iostream>

#include "../../ui/app/identity/neynar_registration.hpp"

int main()
{
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    using genycaster::identity::neynar::ReservedFidTransfer;
    using genycaster::identity::neynar::buildReservedFidTransferBody;

    ReservedFidTransfer reserved{
        .fid = 1777777,
        .reservedAtUnixSeconds = now - 55,
        .deadlineUnixSeconds = now + 900,
        .custodyAddress = QStringLiteral(
            "0xf278cf59f82edcf871d630f28ecc8056f25c1cdb"),
        .eip712Signature = QStringLiteral("0x")
            + QString(128, QLatin1Char('a')) + QStringLiteral("1b"),
        .requestedFname = QStringLiteral("genytest")
    };
    auto valid = buildReservedFidTransferBody(reserved, now);
    assert(valid);
    assert(valid->value(QStringLiteral("fid")).toDouble() == 1777777);
    assert(valid->value(QStringLiteral("requested_user_custody_address"))
           .toString() == reserved.custodyAddress);
    assert(valid->contains(QStringLiteral("signature")));
    assert(valid->contains(QStringLiteral("fname")));
    assert(!valid->contains(QStringLiteral("x-api-key")));
    assert(!valid->contains(QStringLiteral("x-wallet-id")));

    reserved.reservedAtUnixSeconds = now - 601;
    assert(!buildReservedFidTransferBody(reserved, now));
    reserved.reservedAtUnixSeconds = now - 55;
    reserved.requestedFname = QStringLiteral("bad username!");
    assert(!buildReservedFidTransferBody(reserved, now));
    reserved.requestedFname = QStringLiteral("genytest");
    reserved.eip712Signature = QStringLiteral("broken");
    assert(!buildReservedFidTransferBody(reserved, now));
    reserved.eip712Signature = QStringLiteral("0x")
        + QString(128, QLatin1Char('a')) + QStringLiteral("1b");
    reserved.fid = 0;
    assert(!buildReservedFidTransferBody(reserved, now));
    reserved.fid = 1777777;
    reserved.deadlineUnixSeconds = now - 1;
    assert(!buildReservedFidTransferBody(reserved, now));
    std::cout << "Neynar public payload, strict signature and reservation TTL passed\n";
}
