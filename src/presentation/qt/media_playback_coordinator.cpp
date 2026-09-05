#include "media_playback_coordinator.hpp"

#include <QMetaMethod>
#include <QMetaObject>

MediaPlaybackCoordinator::MediaPlaybackCoordinator(QObject* const parent)
    : QObject(parent)
{
}

bool MediaPlaybackCoordinator::muted() const noexcept
{
    return m_muted;
}

void MediaPlaybackCoordinator::setMuted(const bool muted)
{
    if (m_muted == muted) {
        return;
    }
    m_muted = muted;
    emit sessionChanged();
}

QObject* MediaPlaybackCoordinator::activePlayer() const noexcept
{
    return m_activePlayer.data();
}

void MediaPlaybackCoordinator::suspendPlayer(QObject* const player)
{
    if (player == nullptr) {
        return;
    }
    const QMetaObject* const meta = player->metaObject();
    const int index = meta == nullptr
        ? -1 : meta->indexOfMethod("suspendFromPlaybackCoordinator()");
    if (index < 0) {
        return;
    }
    QMetaMethod method = meta->method(index);
    method.invoke(player, Qt::DirectConnection);
}

bool MediaPlaybackCoordinator::claimPrimary(QObject* const player)
{
    if (player == nullptr) {
        return false;
    }
    if (m_activePlayer == player) {
        return true;
    }
    if (m_activePlayer != nullptr) {
        suspendPlayer(m_activePlayer);
    }
    m_activePlayer = player;
    emit activePlayerChanged();
    return true;
}

void MediaPlaybackCoordinator::release(QObject* const player)
{
    if (player == nullptr || m_activePlayer != player) {
        return;
    }
    m_activePlayer = nullptr;
    emit activePlayerChanged();
}

bool MediaPlaybackCoordinator::isPrimary(QObject* const player) const
{
    return player != nullptr && m_activePlayer == player;
}
