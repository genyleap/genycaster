/*!
 * @file        ui/app/bootstrap/providers.cppm
 * @brief       Provider endpoint bootstrap policy.
 *
 * @details
 * Resolves proxy and provider endpoint precedence and restricts optional hosted read credentials to an approved origin.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"
#include <memory>

export module genycaster.ui.bootstrap.providers;

export namespace genycaster::ui::bootstrap {
/**
 * @brief Validated network policy captured during application startup.
 *
 * Read provider overrides do not select a write adapter. An optional indexed
 * API key is accepted only for the canonical host and is never loaded from
 * persistent QSettings or exposed to QML.
 */
class ProviderEndpoints final {
public:
    /** @brief Resolve CLI, environment and persisted non-secret settings.
     *  CLI values override environment values, which override saved settings.
     */
    ProviderEndpoints(QSettings& settings, const QStringList& arguments);
    ~ProviderEndpoints();
    ProviderEndpoints(const ProviderEndpoints&) = delete;
    ProviderEndpoints& operator=(const ProviderEndpoints&) = delete;

    /** @brief False when an invalid proxy or credential destination fails closed. */
    [[nodiscard]] bool valid() const noexcept;

    /** @brief True only for explicit direct, rather than system, proxy mode. */
    [[nodiscard]] bool proxyDirect() const noexcept;
    [[nodiscard]] const QString& proxyMode() const noexcept;

    /** @brief Optional HyperSnap override; empty means provider-owned default. */
    [[nodiscard]] const QString& hypersnapBaseUrl() const noexcept;
    [[nodiscard]] const QString& farcasterReadBaseUrl() const noexcept;

    /** @brief Ephemeral canonical hosted-read credential; never persist it. */
    [[nodiscard]] const QString& farcasterReadApiKey() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
