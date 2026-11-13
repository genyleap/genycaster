#include "native_prepare.hpp"

#include <QtConcurrentRun>
#include <QTimer>

#include <cstdint>
#include <string>
#include <utility>

import farcaster.platform.custody_recovery;

NativeRegistrationPreparationController::NativeRegistrationPreparationController(
    NativePreparationPorts ports, QObject* parent)
    : QObject{parent}, m_ports{std::move(ports)}
{
    QObject::connect(&m_watcher, &QFutureWatcher<QString>::finished,
                     this, &NativeRegistrationPreparationController::finish);
}

NativeRegistrationPreparationController::~NativeRegistrationPreparationController()
{
    cancel();
}

void NativeRegistrationPreparationController::erasePrepared() noexcept
{
    if (!m_envelope.isEmpty()) {
        m_envelope.fill('\0'); // No signature retained after cancellation.
        m_envelope.clear();
    }
}

bool NativeRegistrationPreparationController::prepare()
{
    if (m_busy) return false;
    erasePrepared();
    m_failed = false;
    m_challenge = m_ports.challenge ? m_ports.challenge() : std::nullopt;
    if (!m_ports.backupVerified || !m_ports.backupVerified()
        || !m_ports.custodyAddress || !m_ports.seal || !m_challenge) {
        m_failed = true;
        emit changed();
        return false;
    }
    m_walletAddress = m_ports.custodyAddress();
    if (m_walletAddress.size() != 42 || !m_ports.walletIsUnregistered
        || !m_ports.walletIsUnregistered(m_walletAddress)) {
        m_failed = true;
        emit changed();
        return false;
    }

    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancellation = m_cancel;
    const std::string address = m_walletAddress.toStdString();
    const std::string anchor = m_challenge->anchorHash.toStdString();
    const auto difficulty = m_challenge->difficultyBits;
    m_busy = true;
    emit changed();

    // Run at most 24 million hashes per explicit attempt. This is bounded
    // for energy and UI responsiveness, with cancellation checked inside
    // OpenSSL's native hashing loop every 16,384 nonces.
    constexpr std::uint64_t Chunk = 1ULL << 18;
    constexpr std::uint64_t Budget = 24ULL << 20;
    m_watcher.setFuture(QtConcurrent::run(
        [cancellation, address, anchor, difficulty]() -> QString {
            for (std::uint64_t offset{}; offset < Budget; offset += Chunk) {
                if (cancellation->load(std::memory_order_relaxed))
                    return {};
                auto solution =
                    farcaster::platform::custody_recovery::solveNativeOnboardPow(
                        address, anchor, difficulty, Chunk, offset,
                        cancellation.get());
                if (solution)
                    return QString::fromStdString(*solution);
            }
            return {};
        }));
    return true;
}

void NativeRegistrationPreparationController::finish()
{
    m_busy = false;
    const auto nonce = m_watcher.result();
    if (!m_cancel || m_cancel->load(std::memory_order_relaxed)) {
        m_failed = false;
        emit changed();
        return;
    }
    const auto fresh = m_ports.challenge ? m_ports.challenge() : std::nullopt;
    if (!fresh || !m_challenge || !m_ports.backupVerified()
        || m_ports.custodyAddress() != m_walletAddress
        || !m_ports.walletIsUnregistered(m_walletAddress) || nonce.isEmpty()
        || fresh->anchorHeight != m_challenge->anchorHeight
        || fresh->anchorHash != m_challenge->anchorHash
        || fresh->difficultyBits != m_challenge->difficultyBits) {
        m_failed = true;
        emit changed();
        return;
    }

    // This callback is provided by the trusted native custody component;
    // the mnemonic is not passed to QtConcurrent, the QML engine or here.
    QByteArray envelope = m_ports.seal(*fresh, nonce);
    if (envelope.isEmpty() || envelope.size() > 8192) {
        m_failed = true;
        emit changed();
        return;
    }
    m_envelope = std::move(envelope);
    m_failed = false;
    const auto revision = ++m_proofRevision;
    // A prepared anchor must never look permanently valid in the UI.
    QTimer::singleShot(60'000, this, [this, revision] {
        if (revision == m_proofRevision && !m_busy)
            cancel();
    });
    emit changed();
}

void NativeRegistrationPreparationController::cancel()
{
    ++m_proofRevision;
    if (m_cancel)
        m_cancel->store(true, std::memory_order_relaxed);
    erasePrepared();
    m_challenge.reset();
    m_walletAddress.clear();
    m_failed = false;
    emit changed();
}
