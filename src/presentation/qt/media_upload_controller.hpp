/*!
 * @file        src/presentation/qt/media_upload_controller.hpp
 * @brief       Qt media upload presentation adapter.
 *
 * @details
 * Reports asynchronous upload progress and failures to QML without
 * exposing transport credentials or performing upload I/O on the GUI
 * thread.
 *
 * @author      Kambiz Asadzadeh
 * @since       31 Aug 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual compatibility boundary: Qt MOC requires this QObject declaration in a .hpp file.
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

/**
 * @brief QML-facing state adapter for one media upload.
 *
 * The controller owns presentation state only. File inspection, network I/O,
 * signer authorization and object-storage behavior remain outside the QML
 * layer and are supplied through signals by the executable composition root.
 */
class MediaUploadController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY stateChanged)
    Q_PROPERTY(QUrl localPreviewUrl READ localPreviewUrl NOTIFY stateChanged)
    Q_PROPERTY(QString previewKind READ previewKind NOTIFY stateChanged)
    Q_PROPERTY(QString selectedContentType READ selectedContentType NOTIFY stateChanged)
    Q_PROPERTY(qulonglong transferredBytes READ transferredBytes NOTIFY stateChanged)
    Q_PROPERTY(qulonglong totalBytes READ totalBytes NOTIFY stateChanged)

public:
    /** @brief Creates an idle media-upload presentation adapter. */
    explicit MediaUploadController(QObject* parent = nullptr);

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] bool canRetry() const noexcept;
    [[nodiscard]] double progress() const noexcept;
    [[nodiscard]] QString phase() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QString fileName() const;
    [[nodiscard]] QUrl localPreviewUrl() const;

    /** @brief Returns image, video, or empty for the current local selection. */
    [[nodiscard]] QString previewKind() const;

    /** @brief Returns the canonical MIME type expected by the media gateway. */
    [[nodiscard]] QString selectedContentType() const;
    [[nodiscard]] qulonglong transferredBytes() const noexcept;
    [[nodiscard]] qulonglong totalBytes() const noexcept;

    /**
     * @brief Starts uploading one local file selected by the user.
     * @param localFile Local file URL supplied by Qt FileDialog or DropArea.
     * @param afterIndex Rich-composer block index after which the media will be inserted.
     * @return false when upload is unavailable, already busy, invalid, unreadable, or unsupported.
     */
    Q_INVOKABLE bool beginUpload(const QUrl& localFile, int afterIndex = -1);

    /** @brief Open the browser's native image picker without requiring QFile URLs. */
    Q_INVOKABLE bool browseBrowserImage(int afterIndex = -1);

    /** @brief Apply an explicitly selected browser file without exposing it to QML. */
    void applyBrowserSelection(QString fileName, QString contentType, qulonglong sizeBytes, int afterIndex);

    /** @brief Apply a bounded, locally generated browser preview thumbnail. */
    void applyBrowserPreview(const QString& dataUrl);

    /** @brief Requests cooperative cancellation of the current upload. */
    Q_INVOKABLE void cancel();

    /** @brief Retries the last failed/cancelled local selection. */
    Q_INVOKABLE bool retry();

    /** @brief Clears completed/failed transient presentation state. */
    Q_INVOKABLE void clear();

    /** @brief Updates whether a native upload runtime is composed and usable. */
    void setAvailable(bool available, QString unavailableReason = {});

    /** @brief Applies native lifecycle progress on the Qt main thread. */
    void applyProgress(QString phase, qulonglong transferred, qulonglong total);

    /** @brief Completes the upload and emits the stable public asset URL for the rich composer. */
    void applyCompleted(QString assetUrl, QString contentType, QString fileName, QString managementToken, qulonglong sizeBytes);

    /** @brief Applies a localized user-visible upload failure. */
    void applyFailed(QString message);

    /** @brief Applies cooperative cancellation completion. */
    void applyCancelled();

signals:
    void stateChanged();

    /** @brief Requests native upload orchestration for the local filesystem path. */
    void uploadRequested(QString localPath, int afterIndex);

    /** @brief Requests cancellation of the native in-flight upload. */
    void cancelRequested();
    void browserImagePickRequested(int afterIndex);

    /** @brief Delivers a verified stable media URL to the rich composer. */
    void assetReady(QString url, QString mediaType, QString managementToken, int afterIndex);

private:
    void resetProgress();
    void clearSelection();

    bool m_available{false};
    bool m_busy{false};
    bool m_canRetry{false};
    double m_progress{};
    QString m_phase{};
    QString m_errorText{};
    QString m_unavailableReason{};
    QString m_fileName{};
    QUrl m_localPreviewUrl{};
    QString m_previewKind{};
    QString m_selectedContentType{};
    QString m_localPath{};
    int m_afterIndex{-1};
    qulonglong m_transferredBytes{};
    qulonglong m_totalBytes{};
};
