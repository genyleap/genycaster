// Resolves persisted and operator-supplied network endpoints without
// leaking credentials into QML or logging their contents.
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
