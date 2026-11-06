/*!
 * @file        ui/mobile/remote_image_provider.hpp
 * @brief       Asynchronous remote image provider.
 *
 * @details
 * Delivers bounded decoded image data to Qt Quick with controlled HTTP
 * fetching and caching outside QML delegates.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

// Textual Qt boundary: QQuickAsyncImageProvider is supplied by Qt rather than a project module.
#pragma once

#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QString>

#include <memory>

/**
 * @brief Loads remote HTTP(S) images through a bounded asynchronous pipeline.
 *
 * Network I/O is event-driven and never blocks QQuickPixmapReader. The provider
 * owns one Qt Network reactor and a bounded image-decode executor. Encoded
 * response bytes, decoded-image cache memory, dimensions, retries and failure
 * backoff are bounded. QML addresses images through
 * `image://remote/v22/<category>/<mode>/<base64url>` (with legacy
 * v21.5 keys still accepted) and never
 * performs network I/O directly.
 */
class RemoteImageProvider final : public QQuickAsyncImageProvider {
public:
    class Impl;
    /**
     * @brief Creates the provider using either direct or system proxy routing.
     * @param useDirectProxy true to bypass the system proxy.
     * @param storageStateHandle Opaque shared handle to the module-owned media storage
     * runtime state.
     *
     * The Qt textual boundary intentionally type-erases the named-module-owned RuntimeState.
     * Redeclaring that type in a textual header creates a distinct module-ownership identity on
     * Clang and can produce otherwise source-identical constructor symbols that fail to link.
     */
    explicit RemoteImageProvider(
        bool useDirectProxy,
        std::shared_ptr<void> storageStateHandle);

    /** @brief Cancels outstanding work and releases provider resources. */
    ~RemoteImageProvider() override;

    /**
     * @brief Starts one asynchronous image load.
     * @param id URL-safe base64-encoded HTTP(S) URL.
     * @param requestedSize Optional target size supplied by Qt Quick.
     * @return Response object owned by Qt Quick.
     */
    [[nodiscard]] QQuickImageResponse* requestImageResponse(
        const QString& id,
        const QSize& requestedSize) override;

private:
    std::shared_ptr<Impl> m_impl;
};
