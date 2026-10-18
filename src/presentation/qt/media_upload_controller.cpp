#include "media_upload_controller.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <QFileInfo>
#include <QStringView>
#include <utility>

namespace {

struct SupportedMedia final {
    QStringView extension;
    QStringView contentType;
    QStringView previewKind;
};

constexpr std::array SupportedMediaTypes{
    SupportedMedia{u"jpg", u"image/jpeg", u"image"},
    SupportedMedia{u"jpeg", u"image/jpeg", u"image"},
    SupportedMedia{u"png", u"image/png", u"image"},
    SupportedMedia{u"webp", u"image/webp", u"image"},
    SupportedMedia{u"gif", u"image/gif", u"image"},
    SupportedMedia{u"avif", u"image/avif", u"image"},
    SupportedMedia{u"mp4", u"video/mp4", u"video"},
    SupportedMedia{u"webm", u"video/webm", u"video"},
    SupportedMedia{u"mov", u"video/quicktime", u"video"},
};

[[nodiscard]] const SupportedMedia* supportedMediaFor(const QFileInfo& fileInfo) noexcept
{
    const QString suffix = fileInfo.suffix().toLower();
    const auto iterator = std::ranges::find_if(
        SupportedMediaTypes,
        [&suffix](const SupportedMedia& candidate) { return candidate.extension == suffix; });
    return iterator == SupportedMediaTypes.end() ? nullptr : std::addressof(*iterator);
}

[[nodiscard]] QString unsupportedMediaMessage()
{
    return qtTrId("genycaster.media.unsupported_file_type");
}

} // namespace

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

QString MediaUploadController::previewKind() const
{
    return m_previewKind;
}

QString MediaUploadController::selectedContentType() const
{
    return m_selectedContentType;
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
    const QFileInfo fileInfo{localPath};
    if (localPath.trimmed().isEmpty() || !fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        m_busy = false;
        m_canRetry = false;
        m_phase = QStringLiteral("failed");
        m_errorText = qtTrId("genycaster.media.file_unreadable");
        m_fileName = localFile.fileName();
        m_localPreviewUrl.clear();
        m_previewKind.clear();
        m_selectedContentType.clear();
        m_localPath.clear();
        resetProgress();
        emit stateChanged();
        return false;
    }

    const SupportedMedia* media = supportedMediaFor(fileInfo);
    if (media == nullptr) {
        m_busy = false;
        m_canRetry = false;
        m_phase = QStringLiteral("failed");
        m_errorText = unsupportedMediaMessage();
        m_fileName = fileInfo.fileName();
        m_localPreviewUrl.clear();
        m_previewKind.clear();
        m_selectedContentType.clear();
        m_localPath.clear();
        m_afterIndex = afterIndex;
        resetProgress();
        emit stateChanged();
        return false;
    }

    m_busy = true;
    m_canRetry = false;
    m_errorText.clear();
    m_phase = QStringLiteral("queued");
    m_localPreviewUrl = localFile;
    m_previewKind = media->previewKind.toString();
    m_selectedContentType = media->contentType.toString();
    m_localPath = localPath;
    m_fileName = fileInfo.fileName();
    m_afterIndex = afterIndex;
    resetProgress();
    const qint64 localSize = fileInfo.size();
    m_totalBytes = localSize > 0 ? static_cast<qulonglong>(localSize) : 0U;
    emit stateChanged();
    emit uploadRequested(m_localPath, m_afterIndex);
    return true;
}

bool MediaUploadController::browseBrowserImage(const int afterIndex)
{
    if (!m_available || m_busy) return false;
    emit browserImagePickRequested(afterIndex);
    return true;
}

void MediaUploadController::applyBrowserSelection(
    QString fileName, QString contentType, const qulonglong sizeBytes, const int afterIndex)
{
    if (!m_available || m_busy) return;
    m_busy = true;
    m_canRetry = false;
    m_errorText.clear();
    m_phase = QStringLiteral("preparing");
    m_fileName = std::move(fileName);
    m_selectedContentType = std::move(contentType);
    m_previewKind = QStringLiteral("image");
    m_localPreviewUrl.clear(); // Never pass a browser File or blob: URL into Qt.
    m_localPath.clear();
    m_afterIndex = afterIndex;
    resetProgress();
    m_totalBytes = sizeBytes;
    emit stateChanged();
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
    clearSelection();
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
    m_totalBytes = total > 0U ? total : m_totalBytes;
    m_progress = m_totalBytes == 0U
        ? 0.0
        : std::clamp(static_cast<double>(transferred) / static_cast<double>(m_totalBytes), 0.0, 1.0);
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
    if (!contentType.isEmpty()) {
        m_selectedContentType = contentType;
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

void MediaUploadController::clearSelection()
{
    m_fileName.clear();
    m_localPreviewUrl.clear();
    m_previewKind.clear();
    m_selectedContentType.clear();
    m_localPath.clear();
    m_afterIndex = -1;
}
