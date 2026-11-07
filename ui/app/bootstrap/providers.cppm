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
class ProviderEndpoints final {
public:
    ProviderEndpoints(QSettings& settings, const QStringList& arguments);
    ~ProviderEndpoints();
    ProviderEndpoints(const ProviderEndpoints&) = delete;
    ProviderEndpoints& operator=(const ProviderEndpoints&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool proxyDirect() const noexcept;
    [[nodiscard]] const QString& proxyMode() const noexcept;
    [[nodiscard]] const QString& hypersnapBaseUrl() const noexcept;
    [[nodiscard]] const QString& farcasterReadBaseUrl() const noexcept;
    [[nodiscard]] const QString& farcasterReadApiKey() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
