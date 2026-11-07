/*!
 * @file        ui/app/network/link_preview.hpp
 * @brief       External link preview resolver.
 *
 * @details
 * Fetches bounded public URL metadata asynchronously and exposes cached
 * display details to Qt Quick without network calls in QML.
 *
 * @author      Kambiz Asadzadeh
 * @since       24 Sep 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

/**
 * @brief Resolves public HTTP(S) links into cached presentation-only preview metadata.
 *
 * QML never performs metadata network I/O. The resolver asynchronously fetches a bounded HTML
 * prefix, follows only validated public HTTP(S) redirects, extracts Open Graph/Twitter metadata,
 * and exposes remote images through Genycaster's C++ image-provider URL. Canonical cast/embed
 * state is never modified by this adapter.
 */
class LinkPreviewResolver final : public QObject {
    Q_OBJECT
    Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)

public:
    /**
     * @brief Creates a process-local resolver and metadata cache.
     * @param useDirectProxy True to bypass the system proxy, false to honor system proxy policy.
     * @param parent Optional QObject owner.
     */
    explicit LinkPreviewResolver(bool useDirectProxy, QObject* parent = nullptr);

    /** @brief Cancels outstanding link-preview work and releases network resources. */
    ~LinkPreviewResolver() override;

    LinkPreviewResolver(const LinkPreviewResolver&) = delete;
    auto operator=(const LinkPreviewResolver&) -> LinkPreviewResolver& = delete;
    LinkPreviewResolver(LinkPreviewResolver&&) = delete;
    auto operator=(LinkPreviewResolver&&) -> LinkPreviewResolver& = delete;

    /** @brief Monotonic value changed whenever cached preview metadata changes. */
    [[nodiscard]] qulonglong revision() const noexcept;

    /**
     * @brief Returns immediately available preview metadata for a URL.
     * @param rawUrl Public HTTP(S) URL.
     * @return Cached/provider-derived preview fields, or an empty map for invalid URLs.
     */
    Q_INVOKABLE [[nodiscard]] QVariantMap previewFor(const QString& rawUrl) const;

    /**
     * @brief Converts a validated public HTTP(S) image URL to Genycaster's C++ image-provider URL.
     * @param rawUrl Public HTTP(S), local, qrc, or existing image-provider URL.
     * @return Presentation-safe image source, or an empty string for unsupported URLs.
     */
    Q_INVOKABLE [[nodiscard]] QString imageSourceForUrl(const QString& rawUrl) const;

    /**
     * @brief Schedules bounded metadata resolution for a public HTTP(S) URL.
     * @param rawUrl URL to resolve. Invalid/private destinations are ignored.
     */
    Q_INVOKABLE void requestPreview(const QString& rawUrl);

signals:
    /** @brief Emitted whenever one URL gains richer cached metadata. */
    void previewReady(const QString& url, const QVariantMap& preview);

    /** @brief Emitted after the process-local metadata cache changes. */
    void revisionChanged();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
