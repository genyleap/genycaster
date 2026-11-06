// Explicit UI font policy module. Qt types are globally attached via
// forward declarations; font loading and Qt platform APIs stay private.
module;
#include "qt_fwd.hpp"
#include <memory>
export module genycaster.ui.bootstrap.fonts;
export namespace genycaster::ui::bootstrap {
class FontFamilies final {
public:
    FontFamilies();
    ~FontFamilies();
    FontFamilies(const FontFamilies&) = delete;
    FontFamilies& operator=(const FontFamilies&) = delete;

    // Preserve the exact installed-resource checks from native startup.
    [[nodiscard]] bool load();
    [[nodiscard]] const QString& persian() const noexcept;
    [[nodiscard]] const QString& devanagari() const noexcept;
    [[nodiscard]] const QString& japanese() const noexcept;
    [[nodiscard]] const QString& korean() const noexcept;
    [[nodiscard]] const QString& chinese() const noexcept;
    [[nodiscard]] const QString& iconSolid() const noexcept;
    [[nodiscard]] const QString& iconRegular() const noexcept;
    [[nodiscard]] const QString& systemText() const noexcept;
    [[nodiscard]] const QString& systemRtl() const noexcept;
    [[nodiscard]] const QString& systemDevanagari() const noexcept;
    [[nodiscard]] const QString& systemJapanese() const noexcept;
    [[nodiscard]] const QString& systemKorean() const noexcept;
    [[nodiscard]] const QString& systemChinese() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
