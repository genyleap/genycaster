#include "media_upload_controller.hpp"

#include <algorithm>
#include <QFileInfo>
#include <utility>

MediaUploadController::MediaUploadController(QObject* parent)
    : QObject{parent}
{
}

bool MediaUploadController::available() const noexcept
{
    return m_available;
}

bool MediaUploadController::busy() const noexcept
{
    return m_busy;
}

bool MediaUploadController::canRetry() const noexcept
{
    return m_canRetry;
}

double MediaUploadController::progress() const noexcept
{
    return m_progress;
}

QString MediaUploadController::phase() const
{
    return m_phase;
}

QString MediaUploadController::errorText() const
{
    return m_errorText;
}

QString MediaUploadController::fileName() const
{
    return m_fileName;
}

QUrl MediaUploadController::localPreviewUrl() const
{
    return m_localPreviewUrl;
}

bool MediaUploadController::imagePreview() const noexcept
{
    return m_imagePreview;
}

qulonglong MediaUploadController::transferredBytes() const noexcept
{
    return m_transferredBytes;
}

qulonglong MediaUploadController::totalBytes() const noexcept
{
    return m_totalBytes;
}

bool MediaUploadController::beginUpload(const QUrl& localFile, const int afterIndex)
{
    if (!m_available || m_busy || !localFile.isValid() || !localFile.isLocalFile()) {
        return false;
    }
    const QString localPath = localFile.toLocalFile();
    if (localPath.trimmed().isEmpty()) {
        return false;
    }

    m_busy = true;
    m_canRetry = false;
    m_errorText.clear();
    m_phase = QStringLiteral("queued");
    m_localPreviewUrl = localFile;
    const QString suffix = QFileInfo{localPath}.suffix().toLower();
    m_imagePreview = suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg") ||
        suffix == QStringLiteral("png") || suffix == QStringLiteral("webp") ||
        suffix == QStringLiteral("gif") || suffix == QStringLiteral("avif");
    m_localPath = localPath;
    const QFileInfo fileInfo{localPath};
    m_fileName = localFile.fileName();
    m_afterIndex = afterIndex;
    resetProgress();
    const qint64 localSize = fileInfo.size();
    m_totalBytes = localSize > 0 ? static_cast<qulonglong>(localSize) : 0U;
    emit stateChanged();
    emit uploadRequested(m_localPath, m_afterIndex);
    return true;
}

void MediaUploadController::cancel()
{
    if (!m_busy) {
        return;
    }
    m_phase = QStringLiteral("cancelling");
    emit stateChanged();
    emit cancelRequested();
}

bool MediaUploadController::retry()
{
    if (!m_available || m_busy || !m_canRetry || m_localPath.isEmpty()) {
        return false;
    }
    return beginUpload(QUrl::fromLocalFile(m_localPath), m_afterIndex);
}

void MediaUploadController::clear()
{
    if (m_busy) {
        return;
    }
    m_canRetry = false;
    m_errorText.clear();
    m_phase.clear();
    m_fileName.clear();
    m_localPreviewUrl.clear();
    m_imagePreview = false;
    m_localPath.clear();
    m_afterIndex = -1;
    resetProgress();
    emit stateChanged();
}

void MediaUploadController::setAvailable(const bool available, QString unavailableReason)
{
    if (m_available == available && m_unavailableReason == unavailableReason) {
        return;
    }
    const QString previousReason = m_unavailableReason;
    m_available = available;
    m_unavailableReason = std::move(unavailableReason);
    if (!m_available && !m_busy && !m_unavailableReason.isEmpty()) {
        m_errorText = m_unavailableReason;
    } else if (m_available && !m_busy && m_errorText == previousReason) {
        m_errorText.clear();
    }
    emit stateChanged();
}

void MediaUploadController::applyProgress(QString phase, const qulonglong transferred, const qulonglong total)
{
    if (!m_busy) {
        return;
    }
    m_phase = std::move(phase);
    m_transferredBytes = transferred;
    m_totalBytes = total;
    m_progress = total == 0U
        ? 0.0
        : std::clamp(static_cast<double>(transferred) / static_cast<double>(total), 0.0, 1.0);
    emit stateChanged();
}

void MediaUploadController::applyCompleted(
    QString assetUrl,
    QString contentType,
    QString fileName,
    QString managementToken,
    const qulonglong sizeBytes)
{
    if (!m_busy) {
        return;
    }
    const int afterIndex = m_afterIndex;
    m_busy = false;
    m_canRetry = false;
    m_errorText.clear();
    m_phase = QStringLiteral("completed");
    m_transferredBytes = sizeBytes;
    m_totalBytes = sizeBytes;
    m_progress = 1.0;
    if (!fileName.isEmpty()) {
        m_fileName = std::move(fileName);
    }
    emit stateChanged();
    emit assetReady(std::move(assetUrl), std::move(contentType), std::move(managementToken), afterIndex);
}

void MediaUploadController::applyFailed(QString message)
{
    m_busy = false;
    m_canRetry = !m_localPath.isEmpty();
    m_phase = QStringLiteral("failed");
    m_errorText = std::move(message);
    emit stateChanged();
}

void MediaUploadController::applyCancelled()
{
    m_busy = false;
    m_canRetry = !m_localPath.isEmpty();
    m_phase = QStringLiteral("cancelled");
    m_errorText.clear();
    emit stateChanged();
}

void MediaUploadController::resetProgress()
{
    m_progress = 0.0;
    m_transferredBytes = 0U;
    m_totalBytes = 0U;
}
