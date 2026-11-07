/*!
 * @file        ui/app/desktop/instance.cppm
 * @brief       Desktop single-instance and deep-link handoff.
 *
 * @details
 * Coordinates one process per operating-system user, forwards safe incoming URLs and activates the existing Qt window.
 *
 * @author      Kambiz Asadzadeh
 * @since       07 Nov 2026
 * @copyright   Copyright (c) 2026 Genyleap.
 * @license     See LICENSE in repository root (GPL-3.0-only).
 */

module;
#include "qt_fwd.hpp"
#include <functional>
#include <memory>

export module genycaster.ui.desktop.instance;

export namespace genycaster::ui {
class DesktopSingleInstance final {
public:
    enum class Result { Primary, ActivatedExisting, Error };
    DesktopSingleInstance();
    ~DesktopSingleInstance();
    DesktopSingleInstance(const DesktopSingleInstance&) = delete;
    DesktopSingleInstance& operator=(const DesktopSingleInstance&) = delete;

    [[nodiscard]] Result acquire(const QString& requestedUrl);
    void setWindow(QQuickWindow* window);
    void setOpenUrlHandler(std::function<void(const QString&)> handler);
    [[nodiscard]] QString errorString() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
